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
#include <math.h>
/* SDL provides SDL_memset, SDL_memcpy, SDL_strcmp, etc. — no <string.h> needed */

#include "math/forge_math.h"

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
    SDL_memcpy(buf + offset, &value, sizeof(value));
}

/* Write a float to a buffer at the given offset */
static void write_f32(uint8_t *buf, size_t offset, float value)
{
    SDL_memcpy(buf + offset, &value, sizeof(value));
}

/* Write a signed int32 to a buffer at the given offset */
static void write_i32(uint8_t *buf, size_t offset, int32_t value)
{
    SDL_memcpy(buf + offset, &value, sizeof(value));
}

/* Write a minimal v2 .fmesh file to disk.
 * Returns true on success. Caller provides the path.
 *
 * with_tangents:  if true, stride = 48 and tangent data is written
 * lod_count:      number of LOD levels (each gets 3 indices per submesh)
 * submesh_count:  number of submeshes (each gets the same 3-index triangle)
 */
static bool write_test_fmesh_ex(const char *path, bool with_tangents,
                                 int lod_count, int submesh_count)
{
    uint32_t vertex_count = 3;
    uint32_t stride = with_tangents ? 48 : 32;
    uint32_t flags  = with_tangents ? FORGE_PIPELINE_FLAG_TANGENTS : 0;
    uint32_t indices_per_submesh = 3;
    uint32_t total_indices = (uint32_t)(lod_count * submesh_count)
                           * indices_per_submesh;

    /* v2 LOD-submesh table: per LOD = 4 (target_error) + submesh_count * 12 */
    size_t per_lod_size = sizeof(uint32_t)
                        + (size_t)submesh_count * 3 * sizeof(uint32_t);
    size_t header_size      = FORGE_PIPELINE_HEADER_SIZE;
    size_t lod_section      = (size_t)lod_count * per_lod_size;
    size_t vertex_data_size = (size_t)vertex_count * stride;
    size_t index_data_size  = (size_t)total_indices * sizeof(uint32_t);
    size_t total_size       = header_size + lod_section
                            + vertex_data_size + index_data_size;

    uint8_t *buf = (uint8_t *)SDL_calloc(1, total_size);
    if (!buf) return false;

    /* Header (32 bytes) */
    SDL_memcpy(buf, "FMSH", 4);                       /* magic */
    write_u32(buf, 4,  FORGE_PIPELINE_FMESH_VERSION); /* version = 2 */
    write_u32(buf, 8,  vertex_count);              /* vertex_count */
    write_u32(buf, 12, stride);                    /* vertex_stride */
    write_u32(buf, 16, (uint32_t)lod_count);       /* lod_count */
    write_u32(buf, 20, flags);                     /* flags */
    write_u32(buf, 24, (uint32_t)submesh_count);   /* submesh_count */
    /* bytes 28-31: reserved (already zero from calloc) */

    /* LOD-submesh table */
    size_t p = header_size;
    uint32_t running_index_offset = 0;
    for (int lod = 0; lod < lod_count; lod++) {
        /* target_error */
        write_f32(buf, p, (float)lod * 0.01f);
        p += sizeof(uint32_t);

        /* Per-submesh entries */
        for (int s = 0; s < submesh_count; s++) {
            write_u32(buf, p + 0, indices_per_submesh);       /* index_count */
            write_u32(buf, p + 4, running_index_offset);      /* index_offset (bytes) */
            write_i32(buf, p + 8, (int32_t)s);                /* material_index */
            p += 3 * sizeof(uint32_t);
            running_index_offset += indices_per_submesh * sizeof(uint32_t);
        }
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

    /* Index data: (0, 1, 2) repeated for each LOD × submesh */
    size_t idata_off = vdata_off + vertex_data_size;
    for (uint32_t i = 0; i < total_indices / indices_per_submesh; i++) {
        size_t base = idata_off + (size_t)i * indices_per_submesh
                    * sizeof(uint32_t);
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

/* Convenience wrapper: single-submesh v2 .fmesh */
static bool write_test_fmesh(const char *path, bool with_tangents,
                              int lod_count)
{
    return write_test_fmesh_ex(path, with_tangents, lod_count, 1);
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
    size_t magic_len = SDL_strlen(magic);
    if (magic_len > 4) magic_len = 4;
    if (file_size >= 4)  SDL_memcpy(buf, magic, magic_len);
    if (file_size >= 8)  write_u32(buf, 4,  version);
    if (file_size >= 12) write_u32(buf, 8,  vertex_count);
    if (file_size >= 16) write_u32(buf, 12, stride);
    if (file_size >= 20) write_u32(buf, 16, lod_count);
    if (file_size >= 24) write_u32(buf, 20, flags);
    if (file_size >= 28) write_u32(buf, 24, 1); /* submesh_count (v2) */
    /* bytes 28-31: reserved (already zero) */

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
    TEST("load_mesh_no_tangents");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_no_tan.fmesh");

    ASSERT_TRUE(write_test_fmesh(path, false, 1));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_UINT_EQ(mesh.vertex_stride, 32);
    ASSERT_UINT_EQ(mesh.lod_count, 1);
    ASSERT_UINT_EQ(mesh.submesh_count, 1);
    ASSERT_TRUE((mesh.flags & FORGE_PIPELINE_FLAG_TANGENTS) == 0);
    ASSERT_NOT_NULL(mesh.vertices);
    ASSERT_NOT_NULL(mesh.indices);
    ASSERT_NOT_NULL(mesh.submeshes);

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
    TEST("load_mesh_with_tangents");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_tan.fmesh");

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
    TEST("load_mesh_multiple_lods");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_multi_lod.fmesh");

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
    TEST("mesh_lod_accessor");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_acc.fmesh");

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
    TEST("mesh_lod_indices");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_idx.fmesh");

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
    TEST("mesh_vertex_data_integrity");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_vdata.fmesh");

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
    TEST("load_mesh_null_mesh");
    char path[512];
    temp_path(path, sizeof(path), "whatever.fmesh");
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, NULL));
    END_TEST();
}

static void test_load_mesh_nonexistent(void)
{
    TEST("load_mesh_nonexistent");
    char path[512];
    temp_path(path, sizeof(path), "nonexistent_xyz_42.fmesh");
    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));
    END_TEST();
}

static void test_load_mesh_invalid_magic(void)
{
    TEST("load_mesh_invalid_magic");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_magic.fmesh");

    ASSERT_TRUE(write_broken_fmesh(path, "NOPE", 2, 3, 32, 1, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_invalid_version(void)
{
    TEST("load_mesh_invalid_version");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_ver.fmesh");

    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 99, 3, 32, 1, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_invalid_stride(void)
{
    TEST("load_mesh_invalid_stride");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_stride.fmesh");

    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 64, 1, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_truncated_header(void)
{
    TEST("load_mesh_truncated_header");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_trunc_hdr.fmesh");

    /* Write only 16 bytes — less than the 32-byte header */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 32, 1, 0, 16));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_truncated_data(void)
{
    TEST("load_mesh_truncated_data");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_trunc_data.fmesh");

    /* Write a valid header with 3 vertices, stride 32, 1 LOD, 1 submesh,
     * but only 48 bytes total — not enough for LOD-submesh table + vertex data.
     * Need: 32 (header) + 16 (1 LOD × 1 submesh) + 96 (3*32 verts) + 12 (3 idx)
     *     = 156 bytes minimum.  We provide only 48. */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 32, 1, 0, 48));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_too_many_lods(void)
{
    TEST("load_mesh_too_many_lods");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_too_many_lods.fmesh");

    /* lod_count = 99, exceeds FORGE_PIPELINE_MAX_LODS (8) */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 32, 99, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_stride_tan_without_flag(void)
{
    TEST("load_mesh_stride_tan_without_flag");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_stride_noflag.fmesh");

    /* stride = 48 (tangent stride) but flags = 0 (no TANGENTS flag) */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 48, 1, 0, 256));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_v1_rejected(void)
{
    TEST("load_mesh_v1_rejected");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_v1_reject.fmesh");

    /* Write a v1 header — should be rejected since we only support v2 */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 1, 3, 32, 1, 0, 128));

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
    TEST("free_mesh_zeroes");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_free_zero.fmesh");

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
    ASSERT_NULL(mesh.submeshes);
    ASSERT_UINT_EQ(mesh.vertex_count, 0);
    ASSERT_UINT_EQ(mesh.vertex_stride, 0);
    ASSERT_UINT_EQ(mesh.lod_count, 0);
    ASSERT_UINT_EQ(mesh.flags, 0);
    ASSERT_UINT_EQ(mesh.submesh_count, 0);

    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Helper Functions (tests 18-20)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_has_tangents(void)
{
    TEST("has_tangents");
    char path_tan[512];
    char path_no_tan[512];
    temp_path(path_tan, sizeof(path_tan), "test_pipeline_has_tan.fmesh");
    temp_path(path_no_tan, sizeof(path_no_tan), "test_pipeline_no_tan2.fmesh");

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
    TEST("lod_index_count_out_of_range");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_oor.fmesh");

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
    TEST("lod_indices_out_of_range");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_ptr_oor.fmesh");

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
    TEST("load_texture_single_mip");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex.meta.json");

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
    TEST("load_texture_with_mip_levels");
    char img_path[512];
    char meta_path[512];
    char mip1_path[512];
    char mip2_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_texm.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_texm.meta.json");
    temp_path(mip1_path, sizeof(mip1_path), "test_pipeline_texm_mip1.png");
    temp_path(mip2_path, sizeof(mip2_path), "test_pipeline_texm_mip2.png");

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
    TEST("load_texture_format_default");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_texfmt.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_texfmt.meta.json");

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
    TEST("load_texture_null_tex");
    char path[512];
    temp_path(path, sizeof(path), "whatever.png");
    ASSERT_TRUE(!forge_pipeline_load_texture(path, NULL));
    END_TEST();
}

static void test_load_texture_no_meta(void)
{
    TEST("load_texture_no_meta");
    char img_path[512];
    char meta_cleanup_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_nometa.png");
    temp_path(meta_cleanup_path, sizeof(meta_cleanup_path), "test_pipeline_tex_nometa.meta.json");

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
    TEST("load_texture_invalid_json");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_badjson.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_badjson.meta.json");

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
    TEST("load_texture_missing_dimensions");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_nodim.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_nodim.meta.json");

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
    TEST("load_texture_missing_mip_file");
    char img_path[512];
    char meta_path[512];
    char mip1_cleanup_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_mipmiss.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_mipmiss.meta.json");
    temp_path(mip1_cleanup_path, sizeof(mip1_cleanup_path), "test_pipeline_tex_mipmiss_mip1.png");

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
    TEST("load_texture_zero_dimensions");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_zerodim.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_zerodim.meta.json");

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
    TEST("load_texture_negative_dimensions");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_negdim.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_negdim.meta.json");

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
    TEST("free_texture_zeroes");
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_texfree.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_texfree.meta.json");

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
 * Submesh Accessors
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_submesh_single(void)
{
    TEST("submesh_single");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_sub1.fmesh");

    ASSERT_TRUE(write_test_fmesh(path, false, 1));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    ASSERT_UINT_EQ(forge_pipeline_submesh_count(&mesh), 1);
    ASSERT_NOT_NULL(mesh.submeshes);

    const ForgePipelineSubmesh *sub = forge_pipeline_lod_submesh(&mesh, 0, 0);
    ASSERT_NOT_NULL(sub);
    ASSERT_UINT_EQ(sub->index_count, 3);
    ASSERT_UINT_EQ(sub->index_offset, 0);
    ASSERT_INT_EQ(sub->material_index, 0);

    /* Out-of-range returns NULL */
    ASSERT_NULL(forge_pipeline_lod_submesh(&mesh, 0, 1));
    ASSERT_NULL(forge_pipeline_lod_submesh(&mesh, 1, 0));

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_submesh_multiple(void)
{
    TEST("submesh_multiple");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_sub3.fmesh");

    ASSERT_TRUE(write_test_fmesh_ex(path, false, 2, 3));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    ASSERT_UINT_EQ(mesh.submesh_count, 3);
    ASSERT_UINT_EQ(mesh.lod_count, 2);

    /* Each submesh in each LOD should have 3 indices */
    for (uint32_t lod = 0; lod < mesh.lod_count; lod++) {
        for (uint32_t s = 0; s < mesh.submesh_count; s++) {
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&mesh, lod, s);
            ASSERT_NOT_NULL(sub);
            ASSERT_UINT_EQ(sub->index_count, 3);
            ASSERT_INT_EQ(sub->material_index, (int32_t)s);
        }
    }

    /* LOD aggregate: 3 submeshes × 3 indices = 9 per LOD */
    ASSERT_UINT_EQ(mesh.lods[0].index_count, 9);
    ASSERT_UINT_EQ(mesh.lods[1].index_count, 9);

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_submesh_count_null(void)
{
    TEST("submesh_count_null");
    ASSERT_UINT_EQ(forge_pipeline_submesh_count(NULL), 0);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Material Loading
 * ══════════════════════════════════════════════════════════════════════════ */

/* Helper: write a .fmat JSON file for testing */
static bool write_test_fmat(const char *path)
{
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"materials\": [\n"
        "    {\n"
        "      \"name\": \"Metal\",\n"
        "      \"base_color_factor\": [0.8, 0.2, 0.1, 1.0],\n"
        "      \"base_color_texture\": \"metal_albedo.png\",\n"
        "      \"metallic_factor\": 0.9,\n"
        "      \"roughness_factor\": 0.3,\n"
        "      \"metallic_roughness_texture\": \"metal_orm.png\",\n"
        "      \"normal_texture\": \"metal_normal.png\",\n"
        "      \"normal_scale\": 1.5,\n"
        "      \"occlusion_texture\": \"metal_orm.png\",\n"
        "      \"occlusion_strength\": 0.8,\n"
        "      \"emissive_factor\": [1.0, 0.5, 0.0],\n"
        "      \"emissive_texture\": \"metal_emissive.png\",\n"
        "      \"alpha_mode\": \"OPAQUE\",\n"
        "      \"alpha_cutoff\": 0.5,\n"
        "      \"double_sided\": false\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"Glass\",\n"
        "      \"base_color_factor\": [1.0, 1.0, 1.0, 0.5],\n"
        "      \"base_color_texture\": null,\n"
        "      \"metallic_factor\": 0.0,\n"
        "      \"roughness_factor\": 0.1,\n"
        "      \"metallic_roughness_texture\": null,\n"
        "      \"normal_texture\": null,\n"
        "      \"normal_scale\": 1.0,\n"
        "      \"occlusion_texture\": null,\n"
        "      \"occlusion_strength\": 1.0,\n"
        "      \"emissive_factor\": [0.0, 0.0, 0.0],\n"
        "      \"emissive_texture\": null,\n"
        "      \"alpha_mode\": \"BLEND\",\n"
        "      \"alpha_cutoff\": 0.5,\n"
        "      \"double_sided\": true\n"
        "    }\n"
        "  ]\n"
        "}\n";

    return SDL_SaveFile(path, json, SDL_strlen(json));
}

static void test_load_materials_valid(void)
{
    TEST("load_materials_valid");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats.fmat");

    ASSERT_TRUE(write_test_fmat(path));

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(forge_pipeline_load_materials(path, &set));

    ASSERT_UINT_EQ(set.material_count, 2);
    ASSERT_NOT_NULL(set.materials);

    /* Material 0: Metal */
    ForgePipelineMaterial *m0 = &set.materials[0];
    ASSERT_TRUE(SDL_strcmp(m0->name, "Metal") == 0);
    ASSERT_FLOAT_EQ(m0->base_color_factor[0], 0.8f);
    ASSERT_FLOAT_EQ(m0->base_color_factor[3], 1.0f);
    ASSERT_TRUE(SDL_strcmp(m0->base_color_texture, "metal_albedo.png") == 0);
    ASSERT_FLOAT_EQ(m0->metallic_factor, 0.9f);
    ASSERT_FLOAT_EQ(m0->roughness_factor, 0.3f);
    ASSERT_TRUE(SDL_strcmp(m0->metallic_roughness_texture, "metal_orm.png") == 0);
    ASSERT_TRUE(SDL_strcmp(m0->normal_texture, "metal_normal.png") == 0);
    ASSERT_FLOAT_EQ(m0->normal_scale, 1.5f);
    ASSERT_FLOAT_EQ(m0->occlusion_strength, 0.8f);
    ASSERT_FLOAT_EQ(m0->emissive_factor[0], 1.0f);
    ASSERT_FLOAT_EQ(m0->emissive_factor[1], 0.5f);
    ASSERT_FLOAT_EQ(m0->emissive_factor[2], 0.0f);
    ASSERT_TRUE(SDL_strcmp(m0->emissive_texture, "metal_emissive.png") == 0);
    ASSERT_INT_EQ(m0->alpha_mode, FORGE_PIPELINE_ALPHA_OPAQUE);
    ASSERT_FLOAT_EQ(m0->alpha_cutoff, 0.5f);
    ASSERT_TRUE(!m0->double_sided);

    /* Material 1: Glass */
    ForgePipelineMaterial *m1 = &set.materials[1];
    ASSERT_TRUE(SDL_strcmp(m1->name, "Glass") == 0);
    ASSERT_FLOAT_EQ(m1->base_color_factor[3], 0.5f);
    ASSERT_TRUE(m1->base_color_texture[0] == '\0');  /* null texture */
    ASSERT_FLOAT_EQ(m1->metallic_factor, 0.0f);
    ASSERT_INT_EQ(m1->alpha_mode, FORGE_PIPELINE_ALPHA_BLEND);
    ASSERT_TRUE(m1->double_sided);

    forge_pipeline_free_materials(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_materials_null_args(void)
{
    TEST("load_materials_null_args");
    char path[512];
    temp_path(path, sizeof(path), "whatever.fmat");

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(!forge_pipeline_load_materials(NULL, &set));
    ASSERT_TRUE(!forge_pipeline_load_materials(path, NULL));

    END_TEST();
}

static void test_load_materials_bad_version(void)
{
    TEST("load_materials_bad_version");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_badver.fmat");

    const char *json = "{ \"version\": 99, \"materials\": [] }";
    ASSERT_TRUE(SDL_SaveFile(path, json, SDL_strlen(json)));

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(!forge_pipeline_load_materials(path, &set));

    cleanup_file(path);
    END_TEST();
}

static void test_load_materials_defaults(void)
{
    TEST("load_materials_defaults");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_defaults.fmat");

    /* Minimal material — omit most optional fields */
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"materials\": [\n"
        "    { \"name\": \"Bare\" }\n"
        "  ]\n"
        "}\n";
    ASSERT_TRUE(SDL_SaveFile(path, json, SDL_strlen(json)));

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(forge_pipeline_load_materials(path, &set));

    ASSERT_UINT_EQ(set.material_count, 1);
    ForgePipelineMaterial *m = &set.materials[0];

    /* Verify defaults */
    ASSERT_FLOAT_EQ(m->base_color_factor[0], 1.0f);
    ASSERT_FLOAT_EQ(m->base_color_factor[3], 1.0f);
    ASSERT_FLOAT_EQ(m->metallic_factor, 1.0f);
    ASSERT_FLOAT_EQ(m->roughness_factor, 1.0f);
    ASSERT_FLOAT_EQ(m->normal_scale, 1.0f);
    ASSERT_FLOAT_EQ(m->occlusion_strength, 1.0f);
    ASSERT_FLOAT_EQ(m->emissive_factor[0], 0.0f);
    ASSERT_INT_EQ(m->alpha_mode, FORGE_PIPELINE_ALPHA_OPAQUE);
    ASSERT_FLOAT_EQ(m->alpha_cutoff, 0.5f);
    ASSERT_TRUE(!m->double_sided);
    ASSERT_TRUE(m->base_color_texture[0] == '\0');

    forge_pipeline_free_materials(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_materials_malformed_arrays(void)
{
    TEST("load_materials_malformed_arrays");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_malformed.fmat");

    /* base_color_factor has a string element and emissive_factor has only 2
     * elements.  The loader must not crash — non-numeric elements fall back
     * to defaults and short arrays still produce valid output. */
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"materials\": [\n"
        "    {\n"
        "      \"name\": \"Bad\",\n"
        "      \"base_color_factor\": [0.5, \"oops\", null, 0.9],\n"
        "      \"emissive_factor\": [0.1, 0.2]\n"
        "    }\n"
        "  ]\n"
        "}\n";
    ASSERT_TRUE(SDL_SaveFile(path, json, SDL_strlen(json)));

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(forge_pipeline_load_materials(path, &set));

    ASSERT_UINT_EQ(set.material_count, 1);
    ForgePipelineMaterial *m = &set.materials[0];

    /* Element 0 is numeric — should parse */
    ASSERT_FLOAT_EQ(m->base_color_factor[0], 0.5f);
    /* Element 1 is a string — falls back to default (1.0) */
    ASSERT_FLOAT_EQ(m->base_color_factor[1], 1.0f);
    /* Element 2 is null — falls back to default (1.0) */
    ASSERT_FLOAT_EQ(m->base_color_factor[2], 1.0f);
    /* Element 3 is numeric — should parse */
    ASSERT_FLOAT_EQ(m->base_color_factor[3], 0.9f);

    /* emissive_factor has only 2 elements — the loader requires >= 3,
     * so the entire array is rejected and all components get default 0.0 */
    ASSERT_FLOAT_EQ(m->emissive_factor[0], 0.0f);
    ASSERT_FLOAT_EQ(m->emissive_factor[1], 0.0f);
    ASSERT_FLOAT_EQ(m->emissive_factor[2], 0.0f);

    forge_pipeline_free_materials(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_materials_non_numeric_factors(void)
{
    TEST("load_materials_non_numeric_factors");
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_nonnumeric.fmat");

    /* All numeric factor fields set to strings or null — must not crash,
     * must fall back to spec defaults. */
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"materials\": [\n"
        "    {\n"
        "      \"name\": \"Weird\",\n"
        "      \"metallic_factor\": \"high\",\n"
        "      \"roughness_factor\": null,\n"
        "      \"normal_scale\": false,\n"
        "      \"occlusion_strength\": [1, 2, 3],\n"
        "      \"alpha_cutoff\": \"half\"\n"
        "    }\n"
        "  ]\n"
        "}\n";
    ASSERT_TRUE(SDL_SaveFile(path, json, SDL_strlen(json)));

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(forge_pipeline_load_materials(path, &set));

    ForgePipelineMaterial *m = &set.materials[0];
    ASSERT_FLOAT_EQ(m->metallic_factor, 1.0f);
    ASSERT_FLOAT_EQ(m->roughness_factor, 1.0f);
    ASSERT_FLOAT_EQ(m->normal_scale, 1.0f);
    ASSERT_FLOAT_EQ(m->occlusion_strength, 1.0f);
    ASSERT_FLOAT_EQ(m->alpha_cutoff, 0.5f);

    forge_pipeline_free_materials(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_free_materials_null(void)
{
    TEST("free_materials_null");
    forge_pipeline_free_materials(NULL);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Scene loader tests
 *
 * Write .fscene binaries to disk and verify the loader reads them back
 * correctly, including world transform computation.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Write a .fscene file with a given hierarchy.
 * Returns true on success.  Caller provides the path.
 *
 * Layout:
 *   root_count root indices (u32 each)
 *   mesh_count mesh entries (first_submesh u32, submesh_count u32)
 *   node_count nodes (192 bytes each)
 *   children array (total_children u32 each)
 */
static bool write_test_fscene(const char *path,
                               uint32_t node_count,
                               uint32_t mesh_count,
                               uint32_t root_count,
                               const uint32_t *root_indices,
                               const uint32_t *mesh_first_submesh,
                               const uint32_t *mesh_submesh_count,
                               const int32_t *node_parent,
                               const int32_t *node_mesh,
                               const int32_t *node_skin,
                               const uint32_t *node_first_child,
                               const uint32_t *node_child_count,
                               const float node_locals[][16],
                               const uint32_t *children_array,
                               uint32_t total_children)
{
    /* Calculate buffer size */
    size_t size = FORGE_PIPELINE_FSCENE_HEADER_SIZE;
    size += root_count * 4;
    size += mesh_count * 8;
    size += node_count * FORGE_PIPELINE_FSCENE_NODE_SIZE;
    size += total_children * 4;

    uint8_t *buf = (uint8_t *)SDL_calloc(1, size);
    if (!buf) return false;

    size_t off = 0;

    /* Header */
    SDL_memcpy(buf + off, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    off += FORGE_PIPELINE_FSCENE_MAGIC_SIZE;
    write_u32(buf, off, FORGE_PIPELINE_FSCENE_VERSION);  off += 4;
    write_u32(buf, off, node_count); off += 4;
    write_u32(buf, off, mesh_count); off += 4;
    write_u32(buf, off, root_count); off += 4;
    write_u32(buf, off, 0);         off += 4;  /* reserved */

    /* Root indices */
    for (uint32_t i = 0; i < root_count; i++) {
        write_u32(buf, off, root_indices[i]);
        off += 4;
    }

    /* Mesh table */
    for (uint32_t i = 0; i < mesh_count; i++) {
        write_u32(buf, off, mesh_first_submesh[i]); off += 4;
        write_u32(buf, off, mesh_submesh_count[i]); off += 4;
    }

    /* Node table: each node is FORGE_PIPELINE_FSCENE_NODE_SIZE bytes */
    for (uint32_t i = 0; i < node_count; i++) {
        size_t node_start = off;

        /* name: 64 bytes (zero-filled) */
        off += 64;

        /* parent, mesh_index, skin_index */
        write_i32(buf, off, node_parent[i]);  off += 4;
        write_i32(buf, off, node_mesh[i]);    off += 4;
        write_i32(buf, off, node_skin[i]);    off += 4;

        /* first_child, child_count */
        write_u32(buf, off, node_first_child[i]); off += 4;
        write_u32(buf, off, node_child_count[i]); off += 4;

        /* has_trs */
        write_u32(buf, off, 0); off += 4;

        /* translation[3] */
        off += 12;

        /* rotation[4] */
        off += 16;

        /* scale[3] */
        off += 12;

        /* local_transform[16] */
        for (int j = 0; j < 16; j++) {
            write_f32(buf, off, node_locals[i][j]);
            off += 4;
        }

        /* Ensure we advanced exactly FORGE_PIPELINE_FSCENE_NODE_SIZE bytes */
        if (off - node_start != FORGE_PIPELINE_FSCENE_NODE_SIZE) {
            SDL_free(buf);
            return false;
        }
    }

    /* Children array */
    for (uint32_t i = 0; i < total_children; i++) {
        write_u32(buf, off, children_array[i]);
        off += 4;
    }

    /* Write to disk */
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("write_test_fscene: SDL_IOFromFile failed for '%s': %s",
                path, SDL_GetError());
        SDL_free(buf);
        return false;
    }
    bool ok = (SDL_WriteIO(io, buf, size) == size);
    if (!ok) {
        SDL_Log("write_test_fscene: SDL_WriteIO failed for '%s': %s",
                path, SDL_GetError());
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("write_test_fscene: SDL_CloseIO failed for '%s': %s",
                path, SDL_GetError());
        ok = false;
    }
    SDL_free(buf);
    return ok;
}

/* Identity matrix (column-major) */
static const float IDENTITY_16[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

/* Translation matrix: translate by (tx, ty, tz) */
static void make_translation(float *out, float tx, float ty, float tz)
{
    SDL_memcpy(out, IDENTITY_16, sizeof(IDENTITY_16));
    out[12] = tx;
    out[13] = ty;
    out[14] = tz;
}

/* ── Scene loading: valid files ─────────────────────────────────────────── */

static void test_load_scene_single_node(void)
{
    TEST("load_scene_single_node");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_single.fscene");

    /* One root node with mesh 0, identity transform */
    uint32_t roots[] = { 0 };
    uint32_t mesh_first[] = { 0 };
    uint32_t mesh_count_arr[] = { 2 };
    int32_t parent[] = { -1 };
    int32_t mesh_idx[] = { 0 };
    int32_t skin[] = { -1 };
    uint32_t first_child[] = { 0 };
    uint32_t child_count[] = { 0 };
    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
    };

    bool wrote = write_test_fscene(path, 1, 1, 1,
                                    roots, mesh_first, mesh_count_arr,
                                    parent, mesh_idx, skin,
                                    first_child, child_count,
                                    locals, NULL, 0);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(scene.node_count, 1);
    ASSERT_UINT_EQ(scene.mesh_count, 1);
    ASSERT_UINT_EQ(scene.root_count, 1);
    ASSERT_UINT_EQ(scene.roots[0], 0);
    ASSERT_INT_EQ(scene.nodes[0].parent, -1);
    ASSERT_INT_EQ(scene.nodes[0].mesh_index, 0);

    /* World transform should be identity (root with identity local) */
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[0], 1.0f);
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[5], 1.0f);
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[10], 1.0f);
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[15], 1.0f);
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[12], 0.0f);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_parent_child(void)
{
    TEST("load_scene_parent_child — world transform computation");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_parent_child.fscene");

    /* Two nodes: root translates by (10,0,0), child translates by (0,5,0).
     * Child's world transform should be translate(10,5,0). */
    uint32_t roots[] = { 0 };
    uint32_t mesh_first[] = { 0 };
    uint32_t mesh_count_arr[] = { 1 };
    int32_t parent[] = { -1, 0 };
    int32_t mesh_idx[] = { -1, 0 };
    int32_t skin[] = { -1, -1 };
    uint32_t first_child[] = { 0, 0 };
    uint32_t child_count_arr[] = { 1, 0 };
    float locals[2][16];
    make_translation(locals[0], 10.0f, 0.0f, 0.0f);
    make_translation(locals[1], 0.0f, 5.0f, 0.0f);
    uint32_t children[] = { 1 };

    bool wrote = write_test_fscene(path, 2, 1, 1,
                                    roots, mesh_first, mesh_count_arr,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, children, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(scene.node_count, 2);

    /* Root: world = identity * translate(10,0,0) = translate(10,0,0) */
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[12], 10.0f);
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[13], 0.0f);
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[14], 0.0f);

    /* Child: world = translate(10,0,0) * translate(0,5,0) = translate(10,5,0) */
    ASSERT_FLOAT_EQ(scene.nodes[1].world_transform[12], 10.0f);
    ASSERT_FLOAT_EQ(scene.nodes[1].world_transform[13], 5.0f);
    ASSERT_FLOAT_EQ(scene.nodes[1].world_transform[14], 0.0f);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_three_levels(void)
{
    TEST("load_scene_three_levels — deep hierarchy transform chain");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_3level.fscene");

    /* Three nodes: root -> child -> grandchild, each translating +1 on X.
     * Grandchild world X should be 3.0. */
    uint32_t roots[] = { 0 };
    int32_t parent[] = { -1, 0, 1 };
    int32_t mesh_idx[] = { -1, -1, 0 };
    int32_t skin[] = { -1, -1, -1 };
    uint32_t first_child[] = { 0, 1, 0 };
    uint32_t child_count_arr[] = { 1, 1, 0 };
    float locals[3][16];
    make_translation(locals[0], 1.0f, 0.0f, 0.0f);
    make_translation(locals[1], 1.0f, 0.0f, 0.0f);
    make_translation(locals[2], 1.0f, 0.0f, 0.0f);
    uint32_t children[] = { 1, 2 };
    uint32_t mesh_first[] = { 0 };
    uint32_t mesh_count_arr[] = { 1 };

    bool wrote = write_test_fscene(path, 3, 1, 1,
                                    roots, mesh_first, mesh_count_arr,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, children, 2);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);

    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[12], 1.0f);
    ASSERT_FLOAT_EQ(scene.nodes[1].world_transform[12], 2.0f);
    ASSERT_FLOAT_EQ(scene.nodes[2].world_transform[12], 3.0f);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_instanced_mesh(void)
{
    TEST("load_scene_instanced_mesh — same mesh, two nodes, different transforms");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_instance.fscene");

    /* Three nodes: root (no mesh), two children both referencing mesh 0
     * at different positions.  Like two wheels sharing a mesh. */
    uint32_t roots[] = { 0 };
    uint32_t mesh_first[] = { 0 };
    uint32_t mesh_count_arr[] = { 1 };
    int32_t parent[] = { -1, 0, 0 };
    int32_t mesh_idx[] = { -1, 0, 0 };  /* nodes 1 and 2 share mesh 0 */
    int32_t skin[] = { -1, -1, -1 };
    uint32_t first_child[] = { 0, 0, 0 };
    uint32_t child_count_arr[] = { 2, 0, 0 };
    float locals[3][16];
    make_translation(locals[0], 0.0f, 0.0f, 0.0f);  /* root at origin */
    make_translation(locals[1], 5.0f, 0.0f, 0.0f);   /* left wheel */
    make_translation(locals[2], -5.0f, 0.0f, 0.0f);  /* right wheel */
    uint32_t children[] = { 1, 2 };

    bool wrote = write_test_fscene(path, 3, 1, 1,
                                    roots, mesh_first, mesh_count_arr,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, children, 2);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);

    /* Both nodes reference mesh 0 */
    ASSERT_INT_EQ(scene.nodes[1].mesh_index, 0);
    ASSERT_INT_EQ(scene.nodes[2].mesh_index, 0);

    /* But at different world positions */
    ASSERT_FLOAT_EQ(scene.nodes[1].world_transform[12], 5.0f);
    ASSERT_FLOAT_EQ(scene.nodes[2].world_transform[12], -5.0f);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_mesh_table(void)
{
    TEST("load_scene_mesh_table — get_mesh accessor");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_meshtable.fscene");

    /* Two meshes: mesh 0 has 1 submesh starting at 0,
     * mesh 1 has 3 submeshes starting at 1. */
    uint32_t roots[] = { 0 };
    uint32_t mesh_first[] = { 0, 1 };
    uint32_t mesh_count_arr[] = { 1, 3 };
    int32_t parent[] = { -1 };
    int32_t mesh_idx[] = { 1 };
    int32_t skin[] = { -1 };
    uint32_t first_child[] = { 0 };
    uint32_t child_count_arr[] = { 0 };
    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
    };

    bool wrote = write_test_fscene(path, 1, 2, 1,
                                    roots, mesh_first, mesh_count_arr,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, NULL, 0);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);

    const ForgePipelineSceneMesh *m0 = forge_pipeline_scene_get_mesh(&scene, 0);
    ASSERT_NOT_NULL(m0);
    ASSERT_UINT_EQ(m0->first_submesh, 0);
    ASSERT_UINT_EQ(m0->submesh_count, 1);

    const ForgePipelineSceneMesh *m1 = forge_pipeline_scene_get_mesh(&scene, 1);
    ASSERT_NOT_NULL(m1);
    ASSERT_UINT_EQ(m1->first_submesh, 1);
    ASSERT_UINT_EQ(m1->submesh_count, 3);

    /* Out of range returns NULL */
    ASSERT_NULL(forge_pipeline_scene_get_mesh(&scene, 2));
    ASSERT_NULL(forge_pipeline_scene_get_mesh(&scene, 999));

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_multiple_roots(void)
{
    TEST("load_scene_multiple_roots");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_multi_root.fscene");

    /* Two independent root nodes */
    uint32_t roots[] = { 0, 1 };
    int32_t parent[] = { -1, -1 };
    int32_t mesh_idx[] = { 0, 0 };
    int32_t skin[] = { -1, -1 };
    uint32_t first_child[] = { 0, 0 };
    uint32_t child_count_arr[] = { 0, 0 };
    float locals[2][16];
    make_translation(locals[0], 1.0f, 0.0f, 0.0f);
    make_translation(locals[1], 0.0f, 0.0f, 2.0f);
    uint32_t mesh_first[] = { 0 };
    uint32_t mesh_count_arr[] = { 1 };

    bool wrote = write_test_fscene(path, 2, 1, 2,
                                    roots, mesh_first, mesh_count_arr,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, NULL, 0);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(scene.root_count, 2);
    ASSERT_UINT_EQ(scene.roots[0], 0);
    ASSERT_UINT_EQ(scene.roots[1], 1);

    /* Each root computed independently */
    ASSERT_FLOAT_EQ(scene.nodes[0].world_transform[12], 1.0f);
    ASSERT_FLOAT_EQ(scene.nodes[1].world_transform[14], 2.0f);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_empty(void)
{
    TEST("load_scene_empty — zero nodes");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_empty.fscene");

    bool wrote = write_test_fscene(path, 0, 0, 0,
                                    NULL, NULL, NULL,
                                    NULL, NULL, NULL,
                                    NULL, NULL,
                                    NULL, NULL, 0);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(scene.node_count, 0);
    ASSERT_UINT_EQ(scene.mesh_count, 0);
    ASSERT_UINT_EQ(scene.root_count, 0);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

/* ── Scene loading: error cases ─────────────────────────────────────────── */

static void test_load_scene_null_path(void)
{
    TEST("load_scene_null_path");
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(NULL, &scene));
    END_TEST();
}

static void test_load_scene_null_scene(void)
{
    TEST("load_scene_null_scene");
    ASSERT_TRUE(!forge_pipeline_load_scene("nonexistent.fscene", NULL));
    END_TEST();
}

static void test_load_scene_both_null(void)
{
    TEST("load_scene_both_null");
    ASSERT_TRUE(!forge_pipeline_load_scene(NULL, NULL));
    END_TEST();
}

static void test_load_scene_nonexistent(void)
{
    TEST("load_scene_nonexistent");
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene("no_such_file.fscene", &scene));
    END_TEST();
}

static void test_load_scene_invalid_magic(void)
{
    TEST("load_scene_invalid_magic");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badmagic.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, "BAAD", FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_invalid_version(void)
{
    TEST("load_scene_invalid_version");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badver.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, 99);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_version_zero(void)
{
    TEST("load_scene_version_zero");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_ver0.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, 0);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_truncated_header(void)
{
    TEST("load_scene_truncated_header — 10 bytes < 24-byte header");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_truncated.fscene");

    uint8_t buf[10];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_truncated_node_data(void)
{
    TEST("load_scene_truncated_node_data — header says 5 nodes but file too small");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_truncdata.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 5);  /* 5 nodes need 5*NODE_SIZE bytes — file too small */
    write_u32(buf, 12, 0);
    write_u32(buf, 16, 0);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_node_count_exceeds_max(void)
{
    TEST("load_scene_node_count_exceeds_max");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_maxnodes.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, FORGE_PIPELINE_MAX_NODES + 1);
    write_u32(buf, 12, 0);
    write_u32(buf, 16, 0);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_root_count_exceeds_max(void)
{
    TEST("load_scene_root_count_exceeds_max");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_maxroots.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 0);
    write_u32(buf, 12, 0);
    write_u32(buf, 16, FORGE_PIPELINE_MAX_ROOTS + 1);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_mesh_count_exceeds_max(void)
{
    TEST("load_scene_mesh_count_exceeds_max");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_maxmesh.fscene");

    uint8_t buf[FORGE_PIPELINE_FSCENE_HEADER_SIZE];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 0);
    write_u32(buf, 12, FORGE_PIPELINE_MAX_SCENE_MESHES + 1);
    write_u32(buf, 16, 0);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, buf, sizeof(buf));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_root_index_out_of_range(void)
{
    TEST("load_scene_root_index_out_of_range — root[0]=5, only 1 node");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badroot.fscene");

    /* Header: 1 node, 0 meshes, 1 root.  Root index will be 5 (invalid). */
    size_t size = FORGE_PIPELINE_FSCENE_HEADER_SIZE + 4 + FORGE_PIPELINE_FSCENE_NODE_SIZE; /* header + 1 root u32 + 1 node */
    uint8_t *buf = (uint8_t *)SDL_calloc(1, size);
    ASSERT_NOT_NULL(buf);
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 1);  /* node_count */
    write_u32(buf, 12, 0); /* mesh_count */
    write_u32(buf, 16, 1); /* root_count */
    write_u32(buf, 24, 5); /* root index 5 — out of range! */
    /* Identity in local_transform at offset 24+4+64+12+8+4+12+16+12 = 156 */
    /* (just leave zeroed — the load should fail before using it) */

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, size) == size);
    ASSERT_TRUE(SDL_CloseIO(io));
    SDL_free(buf);

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_child_index_out_of_range(void)
{
    TEST("load_scene_child_index_out_of_range — children[0]=99, only 2 nodes");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badchild.fscene");

    /* 2 nodes, 0 meshes, 1 root.  Node 0 has 1 child, children[0] = 99. */
    uint32_t roots[] = { 0 };
    int32_t parent[] = { -1, 0 };
    int32_t mesh_idx[] = { -1, -1 };
    int32_t skin[] = { -1, -1 };
    uint32_t first_child[] = { 0, 0 };
    uint32_t child_count_arr[] = { 1, 0 };
    float locals[2][16];
    make_translation(locals[0], 0.0f, 0.0f, 0.0f);
    make_translation(locals[1], 0.0f, 0.0f, 0.0f);
    uint32_t children[] = { 99 }; /* invalid! */

    bool wrote = write_test_fscene(path, 2, 0, 1,
                                    roots, NULL, NULL,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, children, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_first_child_overflow(void)
{
    TEST("load_scene_first_child_overflow — first_child + child_count wraps");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_childovf.fscene");

    /* Craft a file where node 0 has first_child=0xFFFFFFF0, child_count=32
     * which overflows to a small value.  The loader must reject this. */
    size_t size = FORGE_PIPELINE_FSCENE_HEADER_SIZE + 4 + FORGE_PIPELINE_FSCENE_NODE_SIZE; /* header + root + node (no children in file) */
    uint8_t *buf = (uint8_t *)SDL_calloc(1, size);
    ASSERT_NOT_NULL(buf);
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 1);   /* node_count */
    write_u32(buf, 12, 0);  /* mesh_count */
    write_u32(buf, 16, 1);  /* root_count */
    write_u32(buf, 24, 0);  /* root index */

    /* Node at offset 28 */
    size_t node_off = 28;
    /* Skip name (64 bytes), parent (4), mesh (4), skin (4) */
    write_i32(buf, node_off + 64, -1); /* parent */
    write_i32(buf, node_off + 68, -1); /* mesh */
    write_i32(buf, node_off + 72, -1); /* skin */
    write_u32(buf, node_off + 76, 0xFFFFFFF0u); /* first_child */
    write_u32(buf, node_off + 80, 32);          /* child_count */
    /* rest is zero (identity-ish, but we'll fail before transforms) */

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, size) == size);
    ASSERT_TRUE(SDL_CloseIO(io));
    SDL_free(buf);

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_truncated_children(void)
{
    TEST("load_scene_truncated_children — file ends before children array");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_truncchildren.fscene");

    /* 1 node with 3 children, but file has no children array data. */
    size_t size = FORGE_PIPELINE_FSCENE_HEADER_SIZE + 4 + FORGE_PIPELINE_FSCENE_NODE_SIZE; /* header + root + node (NO children) */
    uint8_t *buf = (uint8_t *)SDL_calloc(1, size);
    ASSERT_NOT_NULL(buf);
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 1);   /* node_count */
    write_u32(buf, 12, 0);  /* mesh_count */
    write_u32(buf, 16, 1);  /* root_count */
    write_u32(buf, 24, 0);  /* root index */

    size_t node_off = 28;
    write_i32(buf, node_off + 64, -1); /* parent */
    write_i32(buf, node_off + 68, -1); /* mesh */
    write_i32(buf, node_off + 72, -1); /* skin */
    write_u32(buf, node_off + 76, 0);  /* first_child */
    write_u32(buf, node_off + 80, 3);  /* child_count = 3, but no data! */

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, size) == size);
    ASSERT_TRUE(SDL_CloseIO(io));
    SDL_free(buf);

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_empty_file(void)
{
    TEST("load_scene_empty_file — 0 bytes");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_empty_file.fscene");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_mesh_index_out_of_range(void)
{
    TEST("load_scene_mesh_index_out_of_range — mesh_index >= mesh_count");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badmeshidx.fscene");

    /* 1 node, 1 mesh, 1 root — but node's mesh_index = 5 (out of range) */
    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1 };
    int32_t mesh_idx[]      = { 5 };  /* only 1 mesh, index 5 is invalid */
    int32_t skins[]         = { -1 };
    uint32_t first_child[]  = { 0 };
    uint32_t child_count[]  = { 0 };

    float locals[][16] = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }};
    bool wrote = write_test_fscene(path, 1, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_parent_out_of_range(void)
{
    TEST("load_scene_parent_out_of_range — parent >= node_count");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badparent.fscene");

    /* 1 node with parent = 5 (only 1 node exists) */
    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { 5 };  /* invalid: only 1 node */
    int32_t mesh_idx[]      = { 0 };
    int32_t skins[]         = { -1 };
    uint32_t first_child[]  = { 0 };
    uint32_t child_count[]  = { 0 };

    float locals[][16] = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }};
    bool wrote = write_test_fscene(path, 1, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_unreachable_node(void)
{
    TEST("load_scene_unreachable_node — node not reachable from roots");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_unreachable.fscene");

    /* 2 nodes but only node 0 is a root; node 1 is not a child of anyone */
    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1, -1 };  /* both claim root parent */
    int32_t mesh_idx[]      = { 0,  0 };
    int32_t skins[]         = { -1, -1 };
    uint32_t first_child[]  = { 0,  0 };
    uint32_t child_count[]  = { 0,  0 };

    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
    };
    bool wrote = write_test_fscene(path, 2, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_duplicate_child(void)
{
    TEST("load_scene_duplicate_child — same node referenced by two parents");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_dupchlid.fscene");

    /* 3 nodes: root (0) has children [1, 1] — node 1 visited twice */
    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1, 0, -1 };
    int32_t mesh_idx[]      = { 0,  0,  0 };
    int32_t skins[]         = { -1, -1, -1 };
    uint32_t first_child[]  = { 0,  0,  0 };
    uint32_t child_count[]  = { 2,  0,  0 };
    uint32_t children[]     = { 1,  1 };  /* duplicate: node 1 twice */

    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
    };
    bool wrote = write_test_fscene(path, 3, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, children, 2);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    /* Should fail: node 1 is visited twice (duplicate child) */
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_parent_mismatch(void)
{
    TEST("load_scene_parent_mismatch — node parent != traversal parent");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_parentmis.fscene");

    /* Node 0 is root, has child [1]. Node 1 claims parent=-1 (root) but
     * is actually a child of node 0. Traversal should detect the mismatch. */
    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1, -1 };  /* node 1 says parent=-1, wrong */
    int32_t mesh_idx[]      = { 0,  0 };
    int32_t skins[]         = { -1, -1 };
    uint32_t first_child[]  = { 0,  0 };
    uint32_t child_count[]  = { 1,  0 };
    uint32_t children[]     = { 1 };

    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
    };
    bool wrote = write_test_fscene(path, 2, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, children, 1);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_negative_mesh_index(void)
{
    TEST("load_scene_negative_mesh_index — mesh_index < -1 rejected");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_negmesh.fscene");

    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1 };
    int32_t mesh_idx[]      = { -2 };  /* invalid: only -1 or [0,N) allowed */
    int32_t skins[]         = { -1 };
    uint32_t first_child[]  = { 0 };
    uint32_t child_count[]  = { 0 };

    float locals[][16] = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }};
    bool wrote = write_test_fscene(path, 1, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_negative_skin_index(void)
{
    TEST("load_scene_negative_skin_index — skin_index < -1 rejected");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_negskin.fscene");

    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1 };
    int32_t mesh_idx[]      = { 0 };
    int32_t skins[]         = { -5 };  /* invalid: only -1 or >= 0 allowed */
    uint32_t first_child[]  = { 0 };
    uint32_t child_count[]  = { 0 };

    float locals[][16] = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }};
    bool wrote = write_test_fscene(path, 1, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_submesh_out_of_range(void)
{
    TEST("load_scene_submesh_out_of_range — submesh span exceeds max rejected");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badsub.fscene");

    uint32_t roots[]        = { 0 };
    /* first_submesh=60, submesh_count=10 => end=70 > MAX_SUBMESHES(64) */
    uint32_t mesh_first[]   = { 60 };
    uint32_t mesh_sub[]     = { 10 };
    int32_t parents[]       = { -1 };
    int32_t mesh_idx[]      = { 0 };
    int32_t skins[]         = { -1 };
    uint32_t first_child[]  = { 0 };
    uint32_t child_count[]  = { 0 };

    float locals[][16] = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }};
    bool wrote = write_test_fscene(path, 1, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_invalid_has_trs(void)
{
    TEST("load_scene_invalid_has_trs — has_trs > 1 rejected");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_badtrs.fscene");

    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1 };
    int32_t mesh_idx[]      = { 0 };
    int32_t skins[]         = { -1 };
    uint32_t first_child[]  = { 0 };
    uint32_t child_count[]  = { 0 };

    float locals[][16] = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }};
    bool wrote = write_test_fscene(path, 1, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, NULL, 0);
    ASSERT_TRUE(wrote);

    /* Patch has_trs field to invalid value (2).
     * has_trs is at byte offset: header(24) + roots(4) + meshes(8)
     *   + node fields before has_trs: name(64) + parent(4) + mesh(4)
     *   + skin(4) + first_child(4) + child_count(4) = 84
     *   => offset = 24 + 4 + 8 + 84 = 120 */
    {
        size_t fsize = 0;
        void *fdata = SDL_LoadFile(path, &fsize);
        ASSERT_TRUE(fdata != NULL);
        uint8_t *buf = (uint8_t *)fdata;
        size_t trs_off = FORGE_PIPELINE_FSCENE_HEADER_SIZE + 4 + 8 + 64 + 4 + 4 + 4 + 4 + 4;
        ASSERT_TRUE(trs_off + 4 <= fsize);
        write_u32(buf, (uint32_t)trs_off, 2);  /* has_trs = 2 (invalid) */
        ASSERT_TRUE(SDL_SaveFile(path, buf, fsize));
        SDL_free(fdata);
    }

    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_children_exceed_max_edges(void)
{
    TEST("load_scene_children_exceed_max_edges — total_children > N-R rejected");

    /* A forest with 2 nodes and 1 root can have at most 1 edge.
     * Set child_count = 2 for the root to exceed that limit. */
    char path[512];
    temp_path(path, sizeof(path), "test_scene_maxedge.fscene");

    uint32_t roots[]        = { 0 };
    uint32_t mesh_first[]   = { 0 };
    uint32_t mesh_sub[]     = { 1 };
    int32_t parents[]       = { -1, 0 };
    int32_t mesh_idx[]      = { -1, 0 };
    int32_t skins[]         = { -1, -1 };
    uint32_t first_child[]  = { 0, 0 };
    uint32_t child_count[]  = { 2, 0 };  /* root claims 2 children but max is 1 */

    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
    };
    uint32_t child_indices[] = { 1, 1 };  /* duplicated to fill the 2 slots */
    bool wrote = write_test_fscene(path, 2, 1, 1,
        roots, mesh_first, mesh_sub,
        parents, mesh_idx, skins,
        first_child, child_count,
        locals, child_indices, 2);

    ASSERT_TRUE(wrote);
    ForgePipelineScene scene;
    ASSERT_TRUE(!forge_pipeline_load_scene(path, &scene));

    cleanup_file(path);
    END_TEST();
}

/* ── Scene data integrity ───────────────────────────────────────────────── */

static void test_load_scene_trs_roundtrip(void)
{
    TEST("load_scene_trs_roundtrip — TRS values survive serialization");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_trs.fscene");

    /* Build a single-node scene with specific TRS values to verify
     * the loader reads them back correctly. */
    size_t size = FORGE_PIPELINE_FSCENE_HEADER_SIZE + 4 + FORGE_PIPELINE_FSCENE_NODE_SIZE;
    uint8_t *buf = (uint8_t *)SDL_calloc(1, size);
    ASSERT_NOT_NULL(buf);
    SDL_memcpy(buf, FORGE_PIPELINE_FSCENE_MAGIC, FORGE_PIPELINE_FSCENE_MAGIC_SIZE);
    write_u32(buf, 4, FORGE_PIPELINE_FSCENE_VERSION);
    write_u32(buf, 8, 1);   /* node_count */
    write_u32(buf, 12, 0);  /* mesh_count */
    write_u32(buf, 16, 1);  /* root_count */
    write_u32(buf, 24, 0);  /* root[0] = 0 */

    /* Node at offset 28 */
    size_t n = 28;

    /* Name: "TestNode" */
    SDL_memcpy(buf + n, "TestNode", 8);
    n += 64;

    /* parent, mesh, skin */
    write_i32(buf, n, -1);  n += 4;  /* parent */
    write_i32(buf, n, -1);  n += 4;  /* mesh_index (-1 = no mesh) */
    write_i32(buf, n, -1);  n += 4;  /* skin_index (-1 = no skin) */

    /* first_child=0, child_count=0 */
    write_u32(buf, n, 0);   n += 4;
    write_u32(buf, n, 0);   n += 4;

    /* has_trs = 1 */
    write_u32(buf, n, 1);   n += 4;

    /* translation = (1.5, 2.5, 3.5) */
    write_f32(buf, n, 1.5f);  n += 4;
    write_f32(buf, n, 2.5f);  n += 4;
    write_f32(buf, n, 3.5f);  n += 4;

    /* rotation = (0.0, 0.707, 0.0, 0.707) — 90° Y */
    write_f32(buf, n, 0.0f);    n += 4;
    write_f32(buf, n, 0.707f);  n += 4;
    write_f32(buf, n, 0.0f);    n += 4;
    write_f32(buf, n, 0.707f);  n += 4;

    /* scale = (2.0, 2.0, 2.0) */
    write_f32(buf, n, 2.0f);  n += 4;
    write_f32(buf, n, 2.0f);  n += 4;
    write_f32(buf, n, 2.0f);  n += 4;

    /* local_transform — identity for this test */
    float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    int fi;
    for (fi = 0; fi < 16; fi++) {
        write_f32(buf, n, ident[fi]);
        n += 4;
    }

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, size) == size);
    ASSERT_TRUE(SDL_CloseIO(io));
    SDL_free(buf);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(scene.node_count, 1);

    ForgePipelineSceneNode *node = &scene.nodes[0];

    /* Verify name */
    ASSERT_TRUE(SDL_strcmp(node->name, "TestNode") == 0);

    /* Verify indices */
    ASSERT_INT_EQ(node->parent, -1);
    ASSERT_INT_EQ(node->mesh_index, -1);
    ASSERT_INT_EQ(node->skin_index, -1);

    /* Verify TRS */
    ASSERT_UINT_EQ(node->has_trs, 1);
    ASSERT_FLOAT_EQ(node->translation[0], 1.5f);
    ASSERT_FLOAT_EQ(node->translation[1], 2.5f);
    ASSERT_FLOAT_EQ(node->translation[2], 3.5f);
    ASSERT_FLOAT_EQ(node->rotation[0], 0.0f);
    ASSERT_FLOAT_EQ(node->rotation[1], 0.707f);
    ASSERT_FLOAT_EQ(node->rotation[2], 0.0f);
    ASSERT_FLOAT_EQ(node->rotation[3], 0.707f);
    ASSERT_FLOAT_EQ(node->scale[0], 2.0f);
    ASSERT_FLOAT_EQ(node->scale[1], 2.0f);
    ASSERT_FLOAT_EQ(node->scale[2], 2.0f);

    /* Verify world transform is identity (root with identity local) */
    ASSERT_FLOAT_EQ(node->world_transform[0], 1.0f);
    ASSERT_FLOAT_EQ(node->world_transform[5], 1.0f);
    ASSERT_FLOAT_EQ(node->world_transform[10], 1.0f);
    ASSERT_FLOAT_EQ(node->world_transform[15], 1.0f);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_children_array_integrity(void)
{
    TEST("load_scene_children_array — verify children indices roundtrip");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_children_rt.fscene");

    /* Root (node 0) with 3 children: nodes 1, 2, 3 */
    uint32_t roots[] = { 0 };
    int32_t parent[] = { -1, 0, 0, 0 };
    int32_t mesh_idx[] = { -1, -1, -1, -1 };
    int32_t skin[] = { -1, -1, -1, -1 };
    uint32_t first_child[] = { 0, 0, 0, 0 };
    uint32_t child_count_arr[] = { 3, 0, 0, 0 };
    float locals[4][16];
    int ni;
    for (ni = 0; ni < 4; ni++) {
        make_translation(locals[ni], 0.0f, 0.0f, 0.0f);
    }
    uint32_t children[] = { 1, 2, 3 };

    bool wrote = write_test_fscene(path, 4, 0, 1,
                                    roots, NULL, NULL,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, children, 3);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(scene.child_count, 3);
    ASSERT_UINT_EQ(scene.children[0], 1);
    ASSERT_UINT_EQ(scene.children[1], 2);
    ASSERT_UINT_EQ(scene.children[2], 3);
    ASSERT_UINT_EQ(scene.nodes[0].first_child, 0);
    ASSERT_UINT_EQ(scene.nodes[0].child_count, 3);

    forge_pipeline_free_scene(&scene);
    cleanup_file(path);
    END_TEST();
}

static void test_load_scene_double_free(void)
{
    TEST("load_scene_double_free — free_scene is safe to call twice");

    char path[512];
    temp_path(path, sizeof(path), "test_scene_dblf.fscene");

    uint32_t roots[] = { 0 };
    int32_t parent[] = { -1 };
    int32_t mesh_idx[] = { -1 };
    int32_t skin[] = { -1 };
    uint32_t first_child[] = { 0 };
    uint32_t child_count_arr[] = { 0 };
    float locals[][16] = {
        { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
    };

    bool wrote = write_test_fscene(path, 1, 0, 1,
                                    roots, NULL, NULL,
                                    parent, mesh_idx, skin,
                                    first_child, child_count_arr,
                                    locals, NULL, 0);
    ASSERT_TRUE(wrote);

    ForgePipelineScene scene;
    bool loaded = forge_pipeline_load_scene(path, &scene);
    ASSERT_TRUE(loaded);
    forge_pipeline_free_scene(&scene);
    /* Second free should be safe (pointers zeroed by first free) */
    forge_pipeline_free_scene(&scene);
    ASSERT_NULL(scene.nodes);

    cleanup_file(path);
    END_TEST();
}

/* ── Scene free ─────────────────────────────────────────────────────────── */

static void test_free_scene_null(void)
{
    TEST("free_scene_null");
    forge_pipeline_free_scene(NULL);
    END_TEST();
}

static void test_free_scene_zeroes(void)
{
    TEST("free_scene_zeroes — free a zero-initialized scene");
    ForgePipelineScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    forge_pipeline_free_scene(&scene);
    ASSERT_UINT_EQ(scene.node_count, 0);
    ASSERT_NULL(scene.nodes);
    END_TEST();
}

/* ── Scene get_mesh edge cases ──────────────────────────────────────────── */

static void test_scene_get_mesh_null(void)
{
    TEST("scene_get_mesh_null — NULL scene pointer");
    ASSERT_NULL(forge_pipeline_scene_get_mesh(NULL, 0));
    END_TEST();
}

static void test_scene_get_mesh_empty(void)
{
    TEST("scene_get_mesh_empty — scene with 0 meshes");
    ForgePipelineScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ASSERT_NULL(forge_pipeline_scene_get_mesh(&scene, 0));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Animation loader tests
 * ══════════════════════════════════════════════════════════════════════════ */

/* Write a test .fanim file with the given parameters.
 * Creates a valid binary with `clip_count` clips, each having
 * `sampler_count` samplers (translation, 2 keyframes) and
 * `channel_count` channels. */
static bool write_test_fanim(const char *path, int clip_count,
                              int sampler_count, int channel_count)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;

    bool ok = true;

    /* Header */
    ok = ok && (SDL_WriteIO(io, "FANM", 4) == 4);
    {
        uint8_t buf[4];
        write_u32(buf, 0, 1); /* version */
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
        write_u32(buf, 0, (uint32_t)clip_count);
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    }

    int ci;
    for (ci = 0; ci < clip_count; ci++) {
        uint8_t buf[4];

        /* Name: 64 bytes */
        char name[64];
        SDL_memset(name, 0, sizeof(name));
        SDL_snprintf(name, sizeof(name), "clip_%d", ci);
        ok = ok && (SDL_WriteIO(io, name, 64) == 64);

        /* Duration */
        write_f32(buf, 0, 1.0f + (float)ci);
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

        /* Sampler count */
        write_u32(buf, 0, (uint32_t)sampler_count);
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

        /* Channel count */
        write_u32(buf, 0, (uint32_t)channel_count);
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

        /* Samplers */
        int si;
        for (si = 0; si < sampler_count; si++) {
            /* keyframe_count = 2 */
            write_u32(buf, 0, 2);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
            /* value_components = 3 (translation) */
            write_u32(buf, 0, 3);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
            /* interpolation = 0 (LINEAR) */
            write_u32(buf, 0, 0);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

            /* timestamps: 0.0, 1.0 */
            write_f32(buf, 0, 0.0f);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
            write_f32(buf, 0, 1.0f);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

            /* values: 2 keyframes × 3 components = 6 floats */
            int vi;
            for (vi = 0; vi < 6; vi++) {
                write_f32(buf, 0, (float)vi * 0.5f);
                ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
            }
        }

        /* Channels */
        int chi;
        for (chi = 0; chi < channel_count; chi++) {
            /* target_node */
            write_i32(buf, 0, (int32_t)chi);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
            /* target_path = 0 (translation) */
            write_u32(buf, 0, 0);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
            /* sampler_index = 0 (first sampler) */
            write_u32(buf, 0, 0);
            ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
        }
    }

    if (!SDL_CloseIO(io)) return false;
    return ok;
}

/* Write a .fanim with a rotation sampler (value_components = 4). */
static bool write_test_fanim_rotation(const char *path)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;

    bool ok = true;
    uint8_t buf[4];

    /* Header */
    ok = ok && (SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* 1 clip */

    /* Clip header */
    char name[64];
    SDL_memset(name, 0, sizeof(name));
    SDL_strlcpy(name, "rotate", sizeof(name));
    ok = ok && (SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 2.0f); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* duration */
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* 1 sampler */
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* 1 channel */

    /* Sampler: rotation, 2 keyframes, LINEAR */
    write_u32(buf, 0, 2); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* keyframes */
    write_u32(buf, 0, 4); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* components */
    write_u32(buf, 0, 0); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* LINEAR */

    /* Timestamps */
    write_f32(buf, 0, 0.0f); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_f32(buf, 0, 2.0f); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

    /* Values: 2 × 4 = 8 floats (identity quaternion → 90° rotation) */
    float vals[8] = { 0.0f, 0.0f, 0.0f, 1.0f,   /* identity quat */
                      0.0f, 0.7071f, 0.0f, 0.7071f }; /* 90° Y */
    int vi;
    for (vi = 0; vi < 8; vi++) {
        write_f32(buf, 0, vals[vi]);
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    }

    /* Channel: node 0, rotation, sampler 0 */
    write_i32(buf, 0, 0); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* ROTATION */
    write_u32(buf, 0, 0); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

    if (!SDL_CloseIO(io)) return false;
    return ok;
}

/* Write a .fanim with STEP interpolation. */
static bool write_test_fanim_step(const char *path)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;

    bool ok = true;
    uint8_t buf[4];

    ok = ok && (SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

    char name[64];
    SDL_memset(name, 0, sizeof(name));
    SDL_strlcpy(name, "step_anim", sizeof(name));
    ok = ok && (SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

    /* Sampler: translation, 2 keyframes, STEP */
    write_u32(buf, 0, 2); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 3); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 1); ok = ok && (SDL_WriteIO(io, buf, 4) == 4); /* STEP */

    write_f32(buf, 0, 0.0f); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_f32(buf, 0, 1.0f); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

    int vi;
    for (vi = 0; vi < 6; vi++) {
        write_f32(buf, 0, (float)(vi + 1));
        ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    }

    write_i32(buf, 0, 0); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 0); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 0); ok = ok && (SDL_WriteIO(io, buf, 4) == 4);

    if (!SDL_CloseIO(io)) return false;
    return ok;
}

/* ── Animation loading: valid files ─────────────────────────────────────── */

static void test_load_anim_single_clip(void)
{
    TEST("load_anim_single_clip — 1 clip, 1 sampler, 1 channel");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_single.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clip_count, 1);
    ASSERT_TRUE(SDL_strcmp(file.clips[0].name, "clip_0") == 0);
    ASSERT_FLOAT_EQ(file.clips[0].duration, 1.0f);
    ASSERT_UINT_EQ(file.clips[0].sampler_count, 1);
    ASSERT_UINT_EQ(file.clips[0].channel_count, 1);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_rotation_sampler(void)
{
    TEST("load_anim_rotation — rotation sampler with 4 components");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_rot.fanim");
    bool wrote = write_test_fanim_rotation(path);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clips[0].samplers[0].value_components, 4);
    ASSERT_UINT_EQ(file.clips[0].samplers[0].keyframe_count, 2);
    ASSERT_UINT_EQ(file.clips[0].channels[0].target_path,
                   FORGE_PIPELINE_ANIM_ROTATION);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_multi_sampler(void)
{
    TEST("load_anim_multi_sampler — clip with 3 samplers");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_multisamp.fanim");
    bool wrote = write_test_fanim(path, 1, 3, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clips[0].sampler_count, 3);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_multi_channel(void)
{
    TEST("load_anim_multi_channel — clip with 3 channels");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_multich.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 3);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clips[0].channel_count, 3);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_multi_clip(void)
{
    TEST("load_anim_multi_clip — 3 clips in one file");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_multiclip.fanim");
    bool wrote = write_test_fanim(path, 3, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clip_count, 3);
    ASSERT_TRUE(SDL_strcmp(file.clips[0].name, "clip_0") == 0);
    ASSERT_TRUE(SDL_strcmp(file.clips[1].name, "clip_1") == 0);
    ASSERT_TRUE(SDL_strcmp(file.clips[2].name, "clip_2") == 0);
    ASSERT_FLOAT_EQ(file.clips[1].duration, 2.0f);
    ASSERT_FLOAT_EQ(file.clips[2].duration, 3.0f);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_step_interp(void)
{
    TEST("load_anim_step — STEP interpolation roundtrip");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_step.fanim");
    bool wrote = write_test_fanim_step(path);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clips[0].samplers[0].interpolation,
                   FORGE_PIPELINE_INTERP_STEP);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_name_roundtrip(void)
{
    TEST("load_anim_name — clip name roundtrip");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_name.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(SDL_strcmp(file.clips[0].name, "clip_0") == 0);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_duration_roundtrip(void)
{
    TEST("load_anim_duration — duration roundtrip for multi-clip");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_dur.fanim");
    bool wrote = write_test_fanim(path, 2, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_FLOAT_EQ(file.clips[0].duration, 1.0f);
    ASSERT_FLOAT_EQ(file.clips[1].duration, 2.0f);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

/* ── Animation loading: error cases ─────────────────────────────────────── */

static void test_load_anim_null_path(void)
{
    TEST("load_anim_null_path");
    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(NULL, &file));
    END_TEST();
}

static void test_load_anim_null_file(void)
{
    TEST("load_anim_null_file");
    ASSERT_TRUE(!forge_pipeline_load_animation("dummy.fanim", NULL));
    END_TEST();
}

static void test_load_anim_both_null(void)
{
    TEST("load_anim_both_null");
    ASSERT_TRUE(!forge_pipeline_load_animation(NULL, NULL));
    END_TEST();
}

static void test_load_anim_nonexistent(void)
{
    TEST("load_anim_nonexistent");
    ForgePipelineAnimFile file;
    char path[512];
    temp_path(path, sizeof(path), "no_such_file.fanim");
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    END_TEST();
}

static void test_load_anim_bad_magic(void)
{
    TEST("load_anim_bad_magic");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_badmagic.fanim");

    /* Write file with wrong magic */
    uint8_t buf[12];
    SDL_memcpy(buf, "XXXX", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 0);
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 12) == 12);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_bad_version(void)
{
    TEST("load_anim_bad_version");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_badver.fanim");

    uint8_t buf[12];
    SDL_memcpy(buf, "FANM", 4);
    write_u32(buf, 4, 99);
    write_u32(buf, 8, 0);
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 12) == 12);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_version_zero(void)
{
    TEST("load_anim_version_zero");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_ver0.fanim");

    uint8_t buf[12];
    SDL_memcpy(buf, "FANM", 4);
    write_u32(buf, 4, 0);
    write_u32(buf, 8, 0);
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 12) == 12);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_truncated_header(void)
{
    TEST("load_anim_truncated_header — file too small for header");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_trunchdr.fanim");

    uint8_t buf[8];
    SDL_memcpy(buf, "FANM", 4);
    write_u32(buf, 4, 1);
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 8) == 8);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_truncated_clip(void)
{
    TEST("load_anim_truncated_clip — header says 1 clip but no clip data");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_truncclip.fanim");

    uint8_t buf[12];
    SDL_memcpy(buf, "FANM", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 1); /* 1 clip, but no clip data follows */
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 12) == 12);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_truncated_sampler(void)
{
    TEST("load_anim_truncated_sampler — clip header ok but sampler data missing");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_truncsamp.fanim");

    /* Write header + clip header (says 1 sampler) but no sampler data */
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* version */
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 clip */

    /* Clip header */
    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4); /* duration */
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 sampler */
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4); /* 0 channels */
    /* No sampler data follows — truncated */

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_truncated_channel(void)
{
    TEST("load_anim_truncated_channel — samplers ok but channels truncated");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_truncch.fanim");

    /* Write a complete clip with 0 samplers but 1 channel (no channel data) */
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4); /* 0 samplers */
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 channel — but no data */

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_clip_count_exceeds_max(void)
{
    TEST("load_anim_clip_count_exceeds_max");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_maxclips.fanim");

    uint8_t buf[12];
    SDL_memcpy(buf, "FANM", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, FORGE_PIPELINE_MAX_ANIM_CLIPS + 1);
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 12) == 12);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_sampler_count_exceeds_max(void)
{
    TEST("load_anim_sampler_count_exceeds_max");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_maxsamp.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, FORGE_PIPELINE_MAX_ANIM_SAMPLERS + 1);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_channel_count_exceeds_max(void)
{
    TEST("load_anim_channel_count_exceeds_max");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_maxch.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, FORGE_PIPELINE_MAX_ANIM_CHANNELS + 1);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 4) == 4);

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_channel_sampler_oob(void)
{
    TEST("load_anim_channel_sampler_oob — channel references nonexistent sampler");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_choob.fanim");

    /* 1 clip, 1 sampler, 1 channel — channel sampler_index = 5 (out of bounds) */
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 sampler */
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 channel */

    /* Valid sampler */
    write_u32(buf, 0, 2); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 3); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);
    write_f32(buf, 0, 0.0f); SDL_WriteIO(io, buf, 4);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    int vi;
    for (vi = 0; vi < 6; vi++) {
        write_f32(buf, 0, 0.0f); SDL_WriteIO(io, buf, 4);
    }

    /* Channel with sampler_index = 5 (out of bounds) */
    write_i32(buf, 0, 0); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 5); SDL_WriteIO(io, buf, 4); /* OOB */

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

/* ── Animation data integrity ───────────────────────────────────────────── */

static void test_load_anim_timestamps(void)
{
    TEST("load_anim_timestamps — timestamp values roundtrip");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_ts.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);

    ForgePipelineAnimSampler *s = &file.clips[0].samplers[0];
    ASSERT_FLOAT_EQ(s->timestamps[0], 0.0f);
    ASSERT_FLOAT_EQ(s->timestamps[1], 1.0f);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_keyframe_values(void)
{
    TEST("load_anim_keyframe_values — value array roundtrip");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_vals.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);

    ForgePipelineAnimSampler *s = &file.clips[0].samplers[0];
    /* Values were written as vi * 0.5f for vi in 0..5 */
    ASSERT_FLOAT_EQ(s->values[0], 0.0f);
    ASSERT_FLOAT_EQ(s->values[1], 0.5f);
    ASSERT_FLOAT_EQ(s->values[2], 1.0f);
    ASSERT_FLOAT_EQ(s->values[3], 1.5f);
    ASSERT_FLOAT_EQ(s->values[4], 2.0f);
    ASSERT_FLOAT_EQ(s->values[5], 2.5f);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_channel_targets(void)
{
    TEST("load_anim_channel_targets — target_node and path roundtrip");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_targets.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 3);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);

    ASSERT_INT_EQ(file.clips[0].channels[0].target_node, 0);
    ASSERT_INT_EQ(file.clips[0].channels[1].target_node, 1);
    ASSERT_INT_EQ(file.clips[0].channels[2].target_node, 2);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_empty_file(void)
{
    TEST("load_anim_empty — 0 clips is valid");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_empty.fanim");

    uint8_t buf[12];
    SDL_memcpy(buf, "FANM", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 0);
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 12) == 12);
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clip_count, 0);
    ASSERT_NULL(file.clips);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_zero_keyframes(void)
{
    TEST("load_anim_zero_keyframes — sampler with 0 keyframes");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_zerokf.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 0.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 sampler */
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4); /* 0 channels */

    /* Sampler with 0 keyframes */
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4); /* keyframe_count = 0 */
    write_u32(buf, 0, 3); SDL_WriteIO(io, buf, 4); /* value_components = 3 */
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4); /* LINEAR */

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(file.clips[0].samplers[0].keyframe_count, 0);
    ASSERT_NULL(file.clips[0].samplers[0].timestamps);
    ASSERT_NULL(file.clips[0].samplers[0].values);

    forge_pipeline_free_animation(&file);
    cleanup_file(path);
    END_TEST();
}

/* ── Animation loading: value validation ────────────────────────────────── */

static void test_load_anim_keyframe_count_exceeds_max(void)
{
    TEST("load_anim_keyframe_count_exceeds_max");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_maxkf.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    write_u32(buf, 0, FORGE_PIPELINE_MAX_KEYFRAMES + 1);
    ASSERT_TRUE(SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 3); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_invalid_interpolation(void)
{
    TEST("load_anim_invalid_interpolation — not LINEAR or STEP");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_badinterp.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    write_u32(buf, 0, 2); SDL_WriteIO(io, buf, 4); /* 2 keyframes */
    write_u32(buf, 0, 3); SDL_WriteIO(io, buf, 4); /* 3 components */
    write_u32(buf, 0, 99); SDL_WriteIO(io, buf, 4); /* invalid interp */

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_invalid_target_path(void)
{
    TEST("load_anim_invalid_target_path — not translation/rotation/scale");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_badpath.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 sampler */
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4); /* 1 channel */

    /* Valid sampler */
    write_u32(buf, 0, 2); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 3); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4); /* LINEAR */
    write_f32(buf, 0, 0.0f); SDL_WriteIO(io, buf, 4);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    int vi;
    for (vi = 0; vi < 6; vi++) {
        write_f32(buf, 0, 0.0f); SDL_WriteIO(io, buf, 4);
    }

    /* Channel with invalid target_path = 42 */
    write_i32(buf, 0, 0); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 42); SDL_WriteIO(io, buf, 4); /* invalid path */
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_invalid_value_components(void)
{
    TEST("load_anim_invalid_value_components — not 3 or 4");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_badcomp.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    uint8_t buf[4];

    ASSERT_TRUE(SDL_WriteIO(io, "FANM", 4) == 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);

    char name[64];
    SDL_memset(name, 0, 64);
    ASSERT_TRUE(SDL_WriteIO(io, name, 64) == 64);
    write_f32(buf, 0, 1.0f); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 1); SDL_WriteIO(io, buf, 4);
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    write_u32(buf, 0, 2); SDL_WriteIO(io, buf, 4); /* 2 keyframes */
    write_u32(buf, 0, 5); SDL_WriteIO(io, buf, 4); /* invalid: 5 components */
    write_u32(buf, 0, 0); SDL_WriteIO(io, buf, 4);

    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineAnimFile file;
    ASSERT_TRUE(!forge_pipeline_load_animation(path, &file));
    cleanup_file(path);
    END_TEST();
}

/* ── Animation free safety ──────────────────────────────────────────────── */

static void test_free_anim_null(void)
{
    TEST("free_anim_null — NULL pointer");
    forge_pipeline_free_animation(NULL);
    END_TEST();
}

static void test_free_anim_zeroed(void)
{
    TEST("free_anim_zeroed — zero-initialized struct");
    ForgePipelineAnimFile file;
    SDL_memset(&file, 0, sizeof(file));
    forge_pipeline_free_animation(&file);
    ASSERT_UINT_EQ(file.clip_count, 0);
    ASSERT_NULL(file.clips);
    END_TEST();
}

static void test_free_anim_double(void)
{
    TEST("free_anim_double — double free is safe");
    char path[512];
    temp_path(path, sizeof(path), "test_anim_dblf.fanim");
    bool wrote = write_test_fanim(path, 1, 1, 1);
    ASSERT_TRUE(wrote);

    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_animation(path, &file);
    ASSERT_TRUE(loaded);
    forge_pipeline_free_animation(&file);
    forge_pipeline_free_animation(&file);
    ASSERT_NULL(file.clips);

    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Animation manifest (.fanims) tests
 * ══════════════════════════════════════════════════════════════════════════ */

/* Helper: write a .fanims manifest JSON file for testing */
static bool write_test_fanims(const char *path, const char *json)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("write_test_fanims: SDL_IOFromFile failed for '%s': %s",
                path, SDL_GetError());
        return false;
    }
    size_t written = SDL_IOprintf(io, "%s", json);
    if (written == 0 && SDL_strlen(json) > 0) {
        SDL_Log("write_test_fanims: SDL_IOprintf failed for '%s': %s",
                path, SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("write_test_fanims: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("write_test_fanims: SDL_CloseIO failed for '%s': %s",
                path, SDL_GetError());
        return false;
    }
    return true;
}

/* ── Valid loading (6 tests) ── */

static void test_load_anim_set_single_clip(void)
{
    TEST("load_anim_set_single_clip");
    char path[512];
    temp_path(path, sizeof(path), "test_set_single.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"TestModel\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.5, "
        "\"loop\": true, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(set.clip_count, 1);
    ASSERT_TRUE(SDL_strcmp(set.model, "TestModel") == 0);
    ASSERT_TRUE(SDL_strcmp(set.clips[0].name, "walk") == 0);
    ASSERT_TRUE(SDL_strcmp(set.clips[0].file, "walk.fanim") == 0);
    ASSERT_FLOAT_EQ(set.clips[0].duration, 1.5f);
    ASSERT_TRUE(set.clips[0].loop == true);
    ASSERT_UINT_EQ(set.clips[0].tag_count, 0);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_multiple_clips(void)
{
    TEST("load_anim_set_multiple_clips");
    char path[512];
    temp_path(path, sizeof(path), "test_set_multi.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"Hero\", \"clips\": {"
        "  \"run\":  { \"file\": \"run.fanim\",  \"duration\": 1.25, \"loop\": true, \"tags\": [] },"
        "  \"idle\": { \"file\": \"idle.fanim\", \"duration\": 2.0,  \"loop\": true, \"tags\": [] },"
        "  \"jump\": { \"file\": \"jump.fanim\", \"duration\": 0.8,  \"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(set.clip_count, 3);
    ASSERT_TRUE(SDL_strcmp(set.model, "Hero") == 0);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_with_tags(void)
{
    TEST("load_anim_set_with_tags");
    char path[512];
    temp_path(path, sizeof(path), "test_set_tags.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"run\": { \"file\": \"run.fanim\", \"duration\": 1.0, "
        "\"loop\": true, \"tags\": [\"locomotion\", \"ground\"] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(set.clips[0].tag_count, 2);
    ASSERT_TRUE(SDL_strcmp(set.clips[0].tags[0], "locomotion") == 0);
    ASSERT_TRUE(SDL_strcmp(set.clips[0].tags[1], "ground") == 0);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_loop_flags(void)
{
    TEST("load_anim_set_loop_flags");
    char path[512];
    temp_path(path, sizeof(path), "test_set_loop.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"idle\": { \"file\": \"idle.fanim\", \"duration\": 2.0, \"loop\": true, \"tags\": [] },"
        "  \"die\":  { \"file\": \"die.fanim\",  \"duration\": 1.0, \"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    const ForgePipelineAnimClipInfo *idle = forge_pipeline_find_clip(&set, "idle");
    const ForgePipelineAnimClipInfo *die = forge_pipeline_find_clip(&set, "die");
    ASSERT_NOT_NULL(idle);
    ASSERT_NOT_NULL(die);
    ASSERT_TRUE(idle->loop == true);
    ASSERT_TRUE(die->loop == false);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_model_name(void)
{
    TEST("load_anim_set_model_name");
    char path[512];
    temp_path(path, sizeof(path), "test_set_model.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"CesiumMan\", \"clips\": {}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(SDL_strcmp(set.model, "CesiumMan") == 0);
    ASSERT_UINT_EQ(set.clip_count, 0);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_base_dir(void)
{
    TEST("load_anim_set_base_dir");
    char path[512];
    temp_path(path, sizeof(path), "test_set_basedir.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(SDL_strlen(set.base_dir) > 0);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

/* ── Named lookup (5 tests) ── */

static void test_find_clip_exists(void)
{
    TEST("find_clip_exists");
    char path[512];
    temp_path(path, sizeof(path), "test_find_exists.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] },"
        "  \"run\":  { \"file\": \"run.fanim\",  \"duration\": 0.5, "
        "\"loop\": true, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    const ForgePipelineAnimClipInfo *clip = forge_pipeline_find_clip(&set, "run");
    ASSERT_NOT_NULL(clip);
    ASSERT_TRUE(SDL_strcmp(clip->name, "run") == 0);
    ASSERT_TRUE(SDL_strcmp(clip->file, "run.fanim") == 0);
    ASSERT_FLOAT_EQ(clip->duration, 0.5f);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_find_clip_not_found(void)
{
    TEST("find_clip_not_found");
    char path[512];
    temp_path(path, sizeof(path), "test_find_notfound.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    const ForgePipelineAnimClipInfo *clip = forge_pipeline_find_clip(&set, "dance");
    ASSERT_NULL(clip);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_find_clip_null_set(void)
{
    TEST("find_clip_null_set");
    const ForgePipelineAnimClipInfo *clip = forge_pipeline_find_clip(NULL, "walk");
    ASSERT_NULL(clip);
    END_TEST();
}

static void test_find_clip_null_name(void)
{
    TEST("find_clip_null_name");
    char path[512];
    temp_path(path, sizeof(path), "test_find_nullname.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    const ForgePipelineAnimClipInfo *clip = forge_pipeline_find_clip(&set, NULL);
    ASSERT_NULL(clip);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_find_clip_empty_set(void)
{
    TEST("find_clip_empty_set");
    char path[512];
    temp_path(path, sizeof(path), "test_find_empty.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    const ForgePipelineAnimClipInfo *clip = forge_pipeline_find_clip(&set, "walk");
    ASSERT_NULL(clip);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

/* ── Error cases (8 tests) ── */

static void test_load_anim_set_null_path(void)
{
    TEST("load_anim_set_null_path");
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(NULL, &set);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_anim_set_null_set(void)
{
    TEST("load_anim_set_null_set");
    bool loaded = forge_pipeline_load_anim_set("dummy.fanims", NULL);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_anim_set_nonexistent(void)
{
    TEST("load_anim_set_nonexistent");
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set("nonexistent.fanims", &set);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_anim_set_invalid_json(void)
{
    TEST("load_anim_set_invalid_json");
    char path[512];
    temp_path(path, sizeof(path), "test_set_badjson.fanims");
    ASSERT_TRUE(write_test_fanims(path, "{ not valid json }}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_missing_version(void)
{
    TEST("load_anim_set_missing_version");
    char path[512];
    temp_path(path, sizeof(path), "test_set_noversion.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"model\": \"M\", \"clips\": {} }"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_bad_version(void)
{
    TEST("load_anim_set_bad_version");
    char path[512];
    temp_path(path, sizeof(path), "test_set_badversion.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 99, \"model\": \"M\", \"clips\": {} }"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_missing_clips(void)
{
    TEST("load_anim_set_missing_clips");
    char path[512];
    temp_path(path, sizeof(path), "test_set_noclips.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\" }"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_anim_set_clip_missing_file(void)
{
    TEST("load_anim_set_clip_missing_file");
    char path[512];
    temp_path(path, sizeof(path), "test_set_nofile.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"duration\": 1.0, \"loop\": false }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

/* ── Free safety (3 tests) ── */

static void test_free_anim_set_null(void)
{
    TEST("free_anim_set_null");
    forge_pipeline_free_anim_set(NULL);
    /* no crash = pass */
    END_TEST();
}

static void test_free_anim_set_zeroed(void)
{
    TEST("free_anim_set_zeroed");
    ForgePipelineAnimSet set;
    SDL_memset(&set, 0, sizeof(set));
    forge_pipeline_free_anim_set(&set);
    ASSERT_NULL(set.clips);
    END_TEST();
}

static void test_free_anim_set_double(void)
{
    TEST("free_anim_set_double");
    char path[512];
    temp_path(path, sizeof(path), "test_set_double_free.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    forge_pipeline_free_anim_set(&set);
    forge_pipeline_free_anim_set(&set);
    ASSERT_NULL(set.clips);
    cleanup_file(path);
    END_TEST();
}

/* ── Load clip convenience (4 tests) ── */

static void test_load_clip_not_found(void)
{
    TEST("load_clip_not_found");
    char path[512];
    temp_path(path, sizeof(path), "test_loadclip_notfound.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"walk.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_clip(&set, "dance", &file);
    ASSERT_TRUE(!loaded);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_clip_missing_fanim(void)
{
    TEST("load_clip_missing_fanim");
    char path[512];
    temp_path(path, sizeof(path), "test_loadclip_missing.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"walk\": { \"file\": \"nonexistent.fanim\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(path, &set));
    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_clip(&set, "walk", &file);
    ASSERT_TRUE(!loaded);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_clip_null_args(void)
{
    TEST("load_clip_null_args");
    ForgePipelineAnimFile file;
    bool loaded = forge_pipeline_load_clip(NULL, "walk", &file);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_clip_success(void)
{
    TEST("load_clip_success");
    /* Write a valid .fanim binary for the clip */
    char fanim_path[512];
    temp_path(fanim_path, sizeof(fanim_path), "dance.fanim");
    ASSERT_TRUE(write_test_fanim(fanim_path, 1, 1, 1));

    /* Write a .fanims manifest referencing it */
    char fanims_path[512];
    temp_path(fanims_path, sizeof(fanims_path), "test_loadclip_ok.fanims");
    ASSERT_TRUE(write_test_fanims(fanims_path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"dance\": { \"file\": \"dance.fanim\", \"duration\": 1.0, "
        "\"loop\": true, \"tags\": [\"combat\"] }"
        "}}"));

    ForgePipelineAnimSet set;
    ASSERT_TRUE(forge_pipeline_load_anim_set(fanims_path, &set));

    ForgePipelineAnimFile anim;
    bool clip_loaded = forge_pipeline_load_clip(&set, "dance", &anim);
    ASSERT_TRUE(clip_loaded);
    ASSERT_UINT_EQ(anim.clip_count, 1);
    ASSERT_TRUE(SDL_strcmp(anim.clips[0].name, "clip_0") == 0);
    ASSERT_FLOAT_EQ(anim.clips[0].duration, 1.0f);
    ASSERT_UINT_EQ(anim.clips[0].sampler_count, 1);
    ASSERT_UINT_EQ(anim.clips[0].channel_count, 1);

    forge_pipeline_free_animation(&anim);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(fanim_path);
    cleanup_file(fanims_path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Skin (.fskin) tests
 * ══════════════════════════════════════════════════════════════════════════ */

/* Helper: write raw bytes to a file for error testing */
static bool write_raw_file(const char *path, const void *data, size_t size)
{
    bool ok = SDL_SaveFile(path, data, size);
    if (!ok) {
        SDL_Log("write_raw_file: SDL_SaveFile failed for '%s': %s",
                path, SDL_GetError());
    }
    return ok;
}

/* Helper: write a test .fskin binary file with given skin/joint count. */
static bool write_test_fskin(const char *path, uint32_t skin_count,
                              uint32_t joint_count, const char *skin_name)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("write_test_fskin: SDL_IOFromFile failed: %s", SDL_GetError());
        return false;
    }

    /* Header */
    bool ok = true;
    ok = ok && (SDL_WriteIO(io, "FSKN", 4) == 4);
    uint32_t version = 1;
    ok = ok && (SDL_WriteIO(io, &version, 4) == 4);
    ok = ok && (SDL_WriteIO(io, &skin_count, 4) == 4);

    uint32_t si;
    for (si = 0; si < skin_count && ok; si++) {
        char name_buf[64];
        SDL_memset(name_buf, 0, sizeof(name_buf));
        if (skin_name) {
            SDL_strlcpy(name_buf, skin_name, sizeof(name_buf));
        }
        ok = ok && (SDL_WriteIO(io, name_buf, 64) == 64);

        ok = ok && (SDL_WriteIO(io, &joint_count, 4) == 4);
        int32_t skeleton = -1;
        ok = ok && (SDL_WriteIO(io, &skeleton, 4) == 4);

        uint32_t ji;
        for (ji = 0; ji < joint_count && ok; ji++) {
            int32_t joint = (int32_t)ji;
            ok = ok && (SDL_WriteIO(io, &joint, 4) == 4);
        }

        uint32_t mi;
        for (mi = 0; mi < joint_count && ok; mi++) {
            float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
            ok = ok && (SDL_WriteIO(io, identity, sizeof(identity)) == sizeof(identity));
        }
    }

    if (!ok) {
        SDL_Log("write_test_fskin: write error: %s", SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("write_test_fskin: SDL_CloseIO failed: %s", SDL_GetError());
        }
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("write_test_fskin: SDL_CloseIO failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

/* ── Skin valid loading (5 tests) ── */

static void test_load_skin_single(void)
{
    TEST("load_skin_single");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_single.fskin");
    write_test_fskin(path, 1, 4, "TestSkin");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(set.skin_count, 1);
    ASSERT_TRUE(SDL_strcmp(set.skins[0].name, "TestSkin") == 0);
    ASSERT_UINT_EQ(set.skins[0].joint_count, 4);
    ASSERT_INT_EQ(set.skins[0].skeleton, -1);
    forge_pipeline_free_skins(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_multiple(void)
{
    TEST("load_skin_multiple");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_multi.fskin");
    write_test_fskin(path, 3, 2, "Skin");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(set.skin_count, 3);
    forge_pipeline_free_skins(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_ibm_roundtrip(void)
{
    TEST("load_skin_ibm_roundtrip");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_ibm.fskin");
    write_test_fskin(path, 1, 2, "IBMTest");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_NOT_NULL(set.skins[0].inverse_bind_matrices);
    ASSERT_FLOAT_EQ(set.skins[0].inverse_bind_matrices[0], 1.0f);
    ASSERT_FLOAT_EQ(set.skins[0].inverse_bind_matrices[5], 1.0f);
    ASSERT_FLOAT_EQ(set.skins[0].inverse_bind_matrices[10], 1.0f);
    ASSERT_FLOAT_EQ(set.skins[0].inverse_bind_matrices[15], 1.0f);
    ASSERT_FLOAT_EQ(set.skins[0].inverse_bind_matrices[1], 0.0f);
    forge_pipeline_free_skins(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_joint_indices(void)
{
    TEST("load_skin_joint_indices");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_joints.fskin");
    write_test_fskin(path, 1, 4, "JointTest");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_NOT_NULL(set.skins[0].joints);
    ASSERT_INT_EQ(set.skins[0].joints[0], 0);
    ASSERT_INT_EQ(set.skins[0].joints[1], 1);
    ASSERT_INT_EQ(set.skins[0].joints[2], 2);
    ASSERT_INT_EQ(set.skins[0].joints[3], 3);
    forge_pipeline_free_skins(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_skeleton_root(void)
{
    TEST("load_skin_skeleton_root");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_skeleton.fskin");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_NOT_NULL(io);
    ASSERT_TRUE(SDL_WriteIO(io, "FSKN", 4) == 4);
    uint32_t version = 1, count = 1;
    ASSERT_TRUE(SDL_WriteIO(io, &version, 4) == 4);
    ASSERT_TRUE(SDL_WriteIO(io, &count, 4) == 4);
    char name_buf[64];
    SDL_memset(name_buf, 0, sizeof(name_buf));
    SDL_strlcpy(name_buf, "RootTest", sizeof(name_buf));
    ASSERT_TRUE(SDL_WriteIO(io, name_buf, 64) == 64);
    uint32_t jc = 1;
    ASSERT_TRUE(SDL_WriteIO(io, &jc, 4) == 4);
    int32_t skel = 5;
    ASSERT_TRUE(SDL_WriteIO(io, &skel, 4) == 4);
    int32_t joint = 0;
    ASSERT_TRUE(SDL_WriteIO(io, &joint, 4) == 4);
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    ASSERT_TRUE(SDL_WriteIO(io, identity, sizeof(identity)) == sizeof(identity));
    ASSERT_TRUE(SDL_CloseIO(io));

    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_INT_EQ(set.skins[0].skeleton, 5);
    forge_pipeline_free_skins(&set);
    cleanup_file(path);
    END_TEST();
}

/* ── Skin error cases (10 tests) ── */

static void test_load_skin_null_path(void)
{
    TEST("load_skin_null_path");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(NULL, &set);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_skin_null_set(void)
{
    TEST("load_skin_null_set");
    bool loaded = forge_pipeline_load_skins("dummy.fskin", NULL);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_skin_nonexistent(void)
{
    TEST("load_skin_nonexistent");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins("nonexistent.fskin", &set);
    ASSERT_TRUE(!loaded);
    END_TEST();
}

static void test_load_skin_bad_magic(void)
{
    TEST("load_skin_bad_magic");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_badmagic.fskin");
    uint8_t buf[12];
    SDL_memcpy(buf, "BAAD", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 0);
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_bad_version(void)
{
    TEST("load_skin_bad_version");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_badver.fskin");
    uint8_t buf[12];
    SDL_memcpy(buf, "FSKN", 4);
    write_u32(buf, 4, 99);
    write_u32(buf, 8, 0);
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_truncated_header(void)
{
    TEST("load_skin_truncated_header");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_trunchdr.fskin");
    uint8_t buf[8];
    SDL_memcpy(buf, "FSKN", 4);
    write_u32(buf, 4, 1);
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_truncated_joints(void)
{
    TEST("load_skin_truncated_joints");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_truncjoints.fskin");
    /* Header(12) + name(64) + joint_count(4) + skeleton(4) = 84 bytes
     * But joint data missing */
    uint8_t buf[84];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, "FSKN", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 1);
    write_u32(buf, 76, 4);  /* joint_count = 4 */
    write_i32(buf, 80, -1);
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_truncated_ibm(void)
{
    TEST("load_skin_truncated_ibm");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_truncibm.fskin");
    /* Header(12) + name(64) + jc(4) + skel(4) + 1 joint(4) = 88 bytes
     * But no IBM data */
    uint8_t buf[88];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, "FSKN", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 1);
    write_u32(buf, 76, 1);  /* joint_count = 1 */
    write_i32(buf, 80, -1);
    write_i32(buf, 84, 0);  /* joint[0] = 0 */
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_count_exceeds_max(void)
{
    TEST("load_skin_count_exceeds_max");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_maxcount.fskin");
    uint8_t buf[12];
    SDL_memcpy(buf, "FSKN", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 999);
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skin_joint_count_exceeds_max(void)
{
    TEST("load_skin_joint_count_exceeds_max");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_maxjoints.fskin");
    uint8_t buf[84];
    SDL_memset(buf, 0, sizeof(buf));
    SDL_memcpy(buf, "FSKN", 4);
    write_u32(buf, 4, 1);
    write_u32(buf, 8, 1);
    write_u32(buf, 76, 999);
    write_i32(buf, 80, -1);
    write_raw_file(path, buf, sizeof(buf));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

/* ── Skin free safety (3 tests) ── */

static void test_free_skin_null(void)
{
    TEST("free_skin_null");
    forge_pipeline_free_skins(NULL);
    /* no crash = pass */
    END_TEST();
}

static void test_free_skin_zeroed(void)
{
    TEST("free_skin_zeroed");
    ForgePipelineSkinSet set;
    SDL_memset(&set, 0, sizeof(set));
    forge_pipeline_free_skins(&set);
    ASSERT_NULL(set.skins);
    END_TEST();
}

static void test_free_skin_double(void)
{
    TEST("free_skin_double");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_dblf.fskin");
    write_test_fskin(path, 1, 2, "DoubleFree");
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    forge_pipeline_free_skins(&set);
    forge_pipeline_free_skins(&set);
    ASSERT_NULL(set.skins);
    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Skinned mesh loading tests
 * ══════════════════════════════════════════════════════════════════════════ */

/* Helper: write a minimal v3 .fmesh with the given stride and flags. */
static bool write_test_fmesh_v3(const char *path, uint32_t stride,
                                 uint32_t flags)
{
    uint32_t vertex_count = 3;
    uint32_t lod_count = 1;
    uint32_t submesh_count = 1;

    uint8_t header[32];
    SDL_memset(header, 0, sizeof(header));
    SDL_memcpy(header, "FMSH", 4);
    write_u32(header, 4, 3);
    write_u32(header, 8, vertex_count);
    write_u32(header, 12, stride);
    write_u32(header, 16, lod_count);
    write_u32(header, 20, flags);
    write_u32(header, 24, submesh_count);

    uint8_t lod_data[12];
    write_u32(lod_data, 0, 3);
    write_u32(lod_data, 4, 0);
    write_f32(lod_data, 8, 0.0f);

    uint8_t submesh_data[12];
    write_u32(submesh_data, 0, 3);
    write_u32(submesh_data, 4, 0);
    write_i32(submesh_data, 8, -1);

    size_t verts_size = (size_t)vertex_count * stride;
    uint8_t *verts = (uint8_t *)SDL_calloc(1, verts_size);
    if (!verts) {
        SDL_Log("write_test_fmesh_v3: SDL_calloc failed for '%s': %s",
                path, SDL_GetError());
        return false;
    }

    uint8_t indices[12];
    write_u32(indices, 0, 0);
    write_u32(indices, 4, 1);
    write_u32(indices, 8, 2);

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("write_test_fmesh_v3: SDL_IOFromFile failed for '%s': %s",
                path, SDL_GetError());
        SDL_free(verts);
        return false;
    }
    bool ok = true;
    ok = ok && (SDL_WriteIO(io, header, sizeof(header)) == sizeof(header));
    ok = ok && (SDL_WriteIO(io, lod_data, sizeof(lod_data)) == sizeof(lod_data));
    ok = ok && (SDL_WriteIO(io, submesh_data, sizeof(submesh_data)) == sizeof(submesh_data));
    ok = ok && (SDL_WriteIO(io, verts, verts_size) == verts_size);
    ok = ok && (SDL_WriteIO(io, indices, sizeof(indices)) == sizeof(indices));
    SDL_free(verts);
    if (!ok) {
        SDL_Log("write_test_fmesh_v3: write error for '%s': %s",
                path, SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("write_test_fmesh_v3: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("write_test_fmesh_v3: SDL_CloseIO failed for '%s': %s",
                path, SDL_GetError());
        return false;
    }
    return true;
}

/* Write a minimal v3 .fmesh with morph target data appended after indices.
 * morph_target_count targets with the specified morph_attr_flags.
 * vertex_count is fixed at 3. */
static bool write_test_fmesh_v3_morph(const char *path, uint32_t stride,
                                       uint32_t base_flags,
                                       uint32_t morph_target_count,
                                       uint32_t morph_attr_flags)
{
    uint32_t vertex_count = 3;
    uint32_t lod_count = 1;
    uint32_t submesh_count = 1;
    uint32_t flags = base_flags | FORGE_PIPELINE_FLAG_MORPHS;

    uint8_t header[32];
    SDL_memset(header, 0, sizeof(header));
    SDL_memcpy(header, "FMSH", 4);
    write_u32(header, 4, 3);  /* version */
    write_u32(header, 8, vertex_count);
    write_u32(header, 12, stride);
    write_u32(header, 16, lod_count);
    write_u32(header, 20, flags);
    write_u32(header, 24, submesh_count);

    /* LOD-submesh table: for each LOD: target_error(f32), then per submesh:
     * index_count(u32), index_offset(u32), material_index(i32).
     * 1 LOD × (4 + 1 submesh × 12) = 16 bytes total. */
    uint8_t lod_table[16];
    write_f32(lod_table, 0, 0.0f);  /* target_error */
    write_u32(lod_table, 4, 3);     /* submesh index_count */
    write_u32(lod_table, 8, 0);     /* submesh index_offset */
    write_i32(lod_table, 12, -1);   /* submesh material_index */

    size_t verts_size = (size_t)vertex_count * stride;
    uint8_t *verts = (uint8_t *)SDL_calloc(1, verts_size);
    if (!verts) return false;

    uint8_t indices[12];
    write_u32(indices, 0, 0);
    write_u32(indices, 4, 1);
    write_u32(indices, 8, 2);

    /* Morph header: target_count(u32) + attr_flags(u32) */
    uint8_t morph_header[FORGE_PIPELINE_MORPH_HEADER_SIZE];
    write_u32(morph_header, 0, morph_target_count);
    write_u32(morph_header, 4, morph_attr_flags);

    /* Per-target metadata: name + default_weight = MORPH_META_SIZE each */
    if (morph_target_count > SIZE_MAX / FORGE_PIPELINE_MORPH_META_SIZE) {
        SDL_Log("write_test_fmesh_v3_morph: meta_size overflow");
        SDL_free(verts);
        return false;
    }
    size_t meta_size = (size_t)morph_target_count * FORGE_PIPELINE_MORPH_META_SIZE;
    uint8_t *morph_meta = NULL;
    if (meta_size > 0) {
        morph_meta = (uint8_t *)SDL_calloc(1, meta_size);
        if (!morph_meta) { SDL_free(verts); return false; }
    }
    for (uint32_t ti = 0; ti < morph_target_count; ti++) {
        size_t off = (size_t)ti * FORGE_PIPELINE_MORPH_META_SIZE;
        SDL_snprintf((char *)&morph_meta[off],
                     FORGE_PIPELINE_MORPH_NAME_LEN, "target_%u", ti);
        write_f32(morph_meta, (uint32_t)(off + FORGE_PIPELINE_MORPH_NAME_LEN),
                  0.5f); /* default_weight */
    }

    /* Delta data: count attrs set × target_count × vertex_count × 3 floats */
    uint32_t attrs_per_target = 0;
    if (morph_attr_flags & FORGE_PIPELINE_MORPH_ATTR_POSITION) attrs_per_target++;
    if (morph_attr_flags & FORGE_PIPELINE_MORPH_ATTR_NORMAL) attrs_per_target++;
    if (morph_attr_flags & FORGE_PIPELINE_MORPH_ATTR_TANGENT) attrs_per_target++;
    size_t delta_floats = 0;
    size_t delta_bytes = 0;
    if (attrs_per_target > 0) {
        size_t t = (size_t)morph_target_count;
        size_t a = (size_t)attrs_per_target;
        size_t v = (size_t)vertex_count;
        if (t > SIZE_MAX / a ||
            t * a > SIZE_MAX / v ||
            t * a * v > SIZE_MAX / 3u ||
            t * a * v * 3u > SIZE_MAX / sizeof(float)) {
            SDL_Log("write_test_fmesh_v3_morph: delta_floats overflow");
            SDL_free(verts);
            SDL_free(morph_meta);
            return false;
        }
        delta_floats = t * a * v * 3u;
        delta_bytes = delta_floats * sizeof(float);
    }
    uint8_t *deltas = NULL;
    if (delta_bytes > 0) {
        deltas = (uint8_t *)SDL_calloc(1, delta_bytes);
        if (!deltas) { SDL_free(verts); SDL_free(morph_meta); return false; }
        /* Fill position deltas with recognizable values */
        float *df = (float *)deltas;
        for (size_t i = 0; i < delta_floats; i++) {
            df[i] = (float)(i + 1) * 0.1f;
        }
    }

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("write_test_fmesh_v3_morph: SDL_IOFromFile failed for "
                "'%s': %s", path, SDL_GetError());
        SDL_free(verts); SDL_free(morph_meta); SDL_free(deltas);
        return false;
    }
    bool ok = true;
    ok = ok && (SDL_WriteIO(io, header, sizeof(header)) == sizeof(header));
    ok = ok && (SDL_WriteIO(io, lod_table, sizeof(lod_table)) == sizeof(lod_table));
    ok = ok && (SDL_WriteIO(io, verts, verts_size) == verts_size);
    ok = ok && (SDL_WriteIO(io, indices, sizeof(indices)) == sizeof(indices));
    ok = ok && (SDL_WriteIO(io, morph_header, sizeof(morph_header)) == sizeof(morph_header));
    if (meta_size > 0)
        ok = ok && (SDL_WriteIO(io, morph_meta, meta_size) == meta_size);
    if (delta_bytes > 0 && deltas)
        ok = ok && (SDL_WriteIO(io, deltas, delta_bytes) == delta_bytes);

    SDL_free(verts);
    SDL_free(morph_meta);
    SDL_free(deltas);
    bool close_ok = SDL_CloseIO(io);
    if (!ok || !close_ok) {
        if (!ok) {
            SDL_Log("write_test_fmesh_v3_morph: SDL_WriteIO failed for "
                    "'%s': %s", path, SDL_GetError());
        }
        if (!close_ok) {
            SDL_Log("write_test_fmesh_v3_morph: SDL_CloseIO failed: %s",
                    SDL_GetError());
        }
        return false;
    }
    return true;
}

static void test_load_skinned_mesh_stride_56(void)
{
    TEST("load_skinned_mesh_stride_56");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_mesh_56.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 56, FORGE_PIPELINE_FLAG_SKINNED));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(mesh.vertex_stride, 56);
    ASSERT_TRUE(forge_pipeline_has_skin_data(&mesh));
    ASSERT_TRUE(!forge_pipeline_has_tangents(&mesh));
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skinned_mesh_stride_72(void)
{
    TEST("load_skinned_mesh_stride_72");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_mesh_72.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 72,
        FORGE_PIPELINE_FLAG_SKINNED | FORGE_PIPELINE_FLAG_TANGENTS));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(mesh.vertex_stride, 72);
    ASSERT_TRUE(forge_pipeline_has_skin_data(&mesh));
    ASSERT_TRUE(forge_pipeline_has_tangents(&mesh));
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skinned_mesh_flag_set(void)
{
    TEST("load_skinned_mesh_flag_set");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_mesh_flag.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 56, FORGE_PIPELINE_FLAG_SKINNED));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE((mesh.flags & FORGE_PIPELINE_FLAG_SKINNED) != 0);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skinned_mesh_v2_compat(void)
{
    TEST("load_skinned_mesh_v2_compat");
    char path[512];
    temp_path(path, sizeof(path), "test_v2_compat.fmesh");
    ASSERT_TRUE(write_test_fmesh(path, false, 1));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(!forge_pipeline_has_skin_data(&mesh));
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skinned_mesh_v3_no_skin(void)
{
    TEST("load_skinned_mesh_v3_no_skin");
    char path[512];
    temp_path(path, sizeof(path), "test_v3_noskin.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 32, 0));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(!forge_pipeline_has_skin_data(&mesh));
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skinned_mesh_invalid_stride(void)
{
    TEST("load_skinned_mesh_invalid_stride");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_badstride.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 32, FORGE_PIPELINE_FLAG_SKINNED));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_has_skin_data_null(void)
{
    TEST("has_skin_data_null");
    ASSERT_TRUE(!forge_pipeline_has_skin_data(NULL));
    END_TEST();
}

static void test_load_skinned_mesh_v3_header(void)
{
    TEST("load_skinned_mesh_v3_header");
    char path[512];
    temp_path(path, sizeof(path), "test_v3_header.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 56, FORGE_PIPELINE_FLAG_SKINNED));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_UINT_EQ(mesh.lod_count, 1);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_skinned_stride_no_flag(void)
{
    TEST("load_skinned_stride_no_flag");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_noflag.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 56, 0));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_load_v2_no_skin_flag(void)
{
    TEST("load_v2_no_skin_flag");
    char path[512];
    temp_path(path, sizeof(path), "test_v2_noskinflag.fmesh");
    ASSERT_TRUE(write_test_fmesh(path, false, 1));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE((mesh.flags & FORGE_PIPELINE_FLAG_SKINNED) == 0);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

/* ── Unknown flag bits rejection (1 test) ── */

static void test_load_mesh_unknown_flags(void)
{
    TEST("load_mesh_unknown_flags");
    /* Build a v3 .fmesh with an unknown flag bit set (bit 3 = 0x8) */
    char path[512];
    temp_path(path, sizeof(path), "test_unknown_flags.fmesh");
    enum { TEST_UNKNOWN_MESH_FLAG = (1u << 3) };
    ASSERT_TRUE(write_test_fmesh_v3(path, 32, TEST_UNKNOWN_MESH_FLAG));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

/* ── Morph target mesh tests ── */

/* Forward declaration — init_identity_node is defined later in the file
 * (animation runtime test section) but needed by morph anim tests. */
static void init_identity_node(ForgePipelineSceneNode *node, int32_t parent);

static void test_morph_load_position_only(void)
{
    TEST("morph_load_position_only");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_pos.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 2,
        FORGE_PIPELINE_MORPH_ATTR_POSITION));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(forge_pipeline_has_morph_data(&mesh));
    ASSERT_TRUE(mesh.morph_target_count == 2);
    ASSERT_TRUE(mesh.morph_attribute_flags == FORGE_PIPELINE_MORPH_ATTR_POSITION);
    ASSERT_TRUE(mesh.morph_targets != NULL);
    ASSERT_TRUE(mesh.morph_targets[0].position_deltas != NULL);
    ASSERT_TRUE(mesh.morph_targets[0].normal_deltas == NULL);
    ASSERT_TRUE(mesh.morph_targets[0].tangent_deltas == NULL);
    /* Check default weight */
    ASSERT_TRUE(float_eq(mesh.morph_targets[0].default_weight, 0.5f));
    /* Check first delta value */
    ASSERT_TRUE(float_eq(mesh.morph_targets[0].position_deltas[0], 0.1f));
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_load_pos_and_normal(void)
{
    TEST("morph_load_pos_and_normal");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_pos_norm.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 1,
        FORGE_PIPELINE_MORPH_ATTR_POSITION | FORGE_PIPELINE_MORPH_ATTR_NORMAL));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(mesh.morph_target_count == 1);
    ASSERT_TRUE(mesh.morph_attribute_flags ==
        (FORGE_PIPELINE_MORPH_ATTR_POSITION | FORGE_PIPELINE_MORPH_ATTR_NORMAL));
    ASSERT_TRUE(mesh.morph_targets[0].position_deltas != NULL);
    ASSERT_TRUE(mesh.morph_targets[0].normal_deltas != NULL);
    ASSERT_TRUE(mesh.morph_targets[0].tangent_deltas == NULL);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_load_all_attrs(void)
{
    TEST("morph_load_all_attrs");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_all.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 48,
        FORGE_PIPELINE_FLAG_TANGENTS, 1,
        FORGE_PIPELINE_MORPH_ATTR_POSITION | FORGE_PIPELINE_MORPH_ATTR_NORMAL |
        FORGE_PIPELINE_MORPH_ATTR_TANGENT));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(mesh.morph_target_count == 1);
    ASSERT_TRUE(mesh.morph_attribute_flags ==
        (FORGE_PIPELINE_MORPH_ATTR_POSITION | FORGE_PIPELINE_MORPH_ATTR_NORMAL |
         FORGE_PIPELINE_MORPH_ATTR_TANGENT));
    ASSERT_TRUE(mesh.morph_targets[0].position_deltas != NULL);
    ASSERT_TRUE(mesh.morph_targets[0].normal_deltas != NULL);
    ASSERT_TRUE(mesh.morph_targets[0].tangent_deltas != NULL);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_with_skin(void)
{
    TEST("morph_with_skin");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_skin.fmesh");
    /* FLAG_SKINNED + FLAG_MORPHS, skin stride 56 */
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 56,
        FORGE_PIPELINE_FLAG_SKINNED, 1,
        FORGE_PIPELINE_MORPH_ATTR_POSITION));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(forge_pipeline_has_skin_data(&mesh));
    ASSERT_TRUE(forge_pipeline_has_morph_data(&mesh));
    ASSERT_TRUE(mesh.morph_target_count == 1);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_bad_target_count(void)
{
    TEST("morph_bad_target_count");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_bad_count.fmesh");
    /* morph_target_count exceeds MAX_MORPH_TARGETS */
    enum { TEST_TOO_MANY_MORPHS = FORGE_PIPELINE_MAX_MORPH_TARGETS + 1u };
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, TEST_TOO_MANY_MORPHS,
        FORGE_PIPELINE_MORPH_ATTR_POSITION));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_bad_target_count_zero(void)
{
    TEST("morph_bad_target_count_zero");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_zero_count.fmesh");
    /* morph_target_count = 0 with FLAG_MORPHS set is invalid */
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 0,
        FORGE_PIPELINE_MORPH_ATTR_POSITION));
    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));
    cleanup_file(path);
    END_TEST();
}

static void test_morph_bad_attr_flags_zero(void)
{
    TEST("morph_bad_attr_flags_zero");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_zero_flags.fmesh");
    /* morph_attr_flags = 0 is invalid (no attributes) */
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 1, 0x0));
    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));
    cleanup_file(path);
    END_TEST();
}

static void test_morph_bad_attr_flags_unknown(void)
{
    TEST("morph_bad_attr_flags_unknown");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_unk_flags.fmesh");
    /* Include POSITION so the file isn't rejected for missing required
     * attributes — isolates validation of the unknown bit. */
    enum { TEST_UNKNOWN_MORPH_ATTR_FLAG = (1u << 3) };
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 1,
        FORGE_PIPELINE_MORPH_ATTR_POSITION | TEST_UNKNOWN_MORPH_ATTR_FLAG));
    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));
    cleanup_file(path);
    END_TEST();
}

static void test_morph_truncated_data(void)
{
    TEST("morph_truncated_data");
    /* Write a valid header but truncate before morph delta data */
    char path[512];
    temp_path(path, sizeof(path), "test_morph_trunc.fmesh");

    uint32_t vertex_count = 3, stride = 32, lod_count = 1, submesh_count = 1;
    uint32_t flags = FORGE_PIPELINE_FLAG_MORPHS;

    uint8_t header[32];
    SDL_memset(header, 0, sizeof(header));
    SDL_memcpy(header, "FMSH", 4);
    write_u32(header, 4, 3);
    write_u32(header, 8, vertex_count);
    write_u32(header, 12, stride);
    write_u32(header, 16, lod_count);
    write_u32(header, 20, flags);
    write_u32(header, 24, submesh_count);

    /* LOD-submesh table: target_error + submesh (index_count, offset, mat) */
    uint8_t lod_table[16];
    write_f32(lod_table, 0, 0.0f);
    write_u32(lod_table, 4, 3);
    write_u32(lod_table, 8, 0);
    write_i32(lod_table, 12, -1);

    size_t verts_size = (size_t)vertex_count * stride;
    uint8_t *verts = (uint8_t *)SDL_calloc(1, verts_size);
    ASSERT_TRUE(verts != NULL);

    uint8_t indices[12];
    write_u32(indices, 0, 0);
    write_u32(indices, 4, 1);
    write_u32(indices, 8, 2);

    /* Morph header says 2 targets with position, but no delta data follows */
    uint8_t morph_header[FORGE_PIPELINE_MORPH_HEADER_SIZE];
    write_u32(morph_header, 0, 2);
    write_u32(morph_header, 4, FORGE_PIPELINE_MORPH_ATTR_POSITION);

    /* Only write metadata, no actual delta data — should cause truncation */
    uint8_t morph_meta[2 * FORGE_PIPELINE_MORPH_META_SIZE];
    SDL_memset(morph_meta, 0, sizeof(morph_meta));

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("test_morph_truncated_data: SDL_IOFromFile failed: %s",
                SDL_GetError());
        SDL_free(verts);
        cleanup_file(path);
        fail_count++;
        return;
    }
    bool wok = true;
    wok = wok && (SDL_WriteIO(io, header, sizeof(header)) == sizeof(header));
    wok = wok && (SDL_WriteIO(io, lod_table, sizeof(lod_table)) == sizeof(lod_table));
    wok = wok && (SDL_WriteIO(io, verts, verts_size) == verts_size);
    SDL_free(verts);
    wok = wok && (SDL_WriteIO(io, indices, sizeof(indices)) == sizeof(indices));
    wok = wok && (SDL_WriteIO(io, morph_header, sizeof(morph_header)) == sizeof(morph_header));
    wok = wok && (SDL_WriteIO(io, morph_meta, sizeof(morph_meta)) == sizeof(morph_meta));
    bool close_ok = SDL_CloseIO(io);
    if (!close_ok) {
        SDL_Log("test_morph_truncated_data: SDL_CloseIO failed: %s",
                SDL_GetError());
    }
    ASSERT_TRUE(close_ok);
    ASSERT_TRUE(wok);

    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(!loaded);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_flag_without_data(void)
{
    TEST("morph_flag_without_data");
    /* FLAG_MORPHS set but file ends at indices — no morph section */
    char path[512];
    temp_path(path, sizeof(path), "test_morph_no_data.fmesh");
    /* Use write_test_fmesh_v3 with FLAG_MORPHS bit but no morph data */
    ASSERT_TRUE(write_test_fmesh_v3(path, 32, FORGE_PIPELINE_FLAG_MORPHS));
    ForgePipelineMesh mesh;
    bool loaded = forge_pipeline_load_mesh(path, &mesh);
    ASSERT_TRUE(!loaded); /* should fail — truncated at morph header */
    cleanup_file(path);
    END_TEST();
}

static void test_morph_free_null(void)
{
    TEST("morph_free_null");
    /* Verify free_mesh handles zero morph data gracefully */
    ForgePipelineMesh mesh;
    SDL_memset(&mesh, 0, sizeof(mesh));
    forge_pipeline_free_mesh(&mesh);
    ASSERT_TRUE(mesh.morph_targets == NULL);
    ASSERT_TRUE(mesh.morph_target_count == 0);
    END_TEST();
}

static void test_morph_double_free(void)
{
    TEST("morph_double_free");
    char path[512];
    temp_path(path, sizeof(path), "test_morph_dbl.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 1,
        FORGE_PIPELINE_MORPH_ATTR_POSITION));
    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));
    forge_pipeline_free_mesh(&mesh);
    /* Second free should be safe (struct zeroed) */
    forge_pipeline_free_mesh(&mesh);
    ASSERT_TRUE(mesh.morph_targets == NULL);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_v3_flag_accepted(void)
{
    TEST("morph_v3_flag_accepted");
    /* Verify that FLAG_MORPHS is accepted on v3 files and morph data
     * loads correctly with a single position-only target. */
    char path[512];
    temp_path(path, sizeof(path), "test_morph_v3_ok.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3_morph(path, 32, 0, 1,
        FORGE_PIPELINE_MORPH_ATTR_POSITION));
    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));
    ASSERT_TRUE(mesh.morph_target_count == 1);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_anim_path_accepted(void)
{
    TEST("morph_anim_path_accepted");
    /* Verify MORPH_WEIGHTS path (3) is accepted by the animation loader.
     * Build a minimal .fanim with a morph weight channel. */
    char path[512];
    temp_path(path, sizeof(path), "test_morph_anim.fanim");

    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("test_morph_anim_path_accepted: SDL_IOFromFile failed: %s",
                SDL_GetError());
        cleanup_file(path);
        fail_count++;
        return;
    }
    bool wok = true;

    /* Header: magic + version + clip_count */
    wok = wok && (SDL_WriteIO(io, "FANM", 4) == 4);
    uint8_t buf[4];
    write_u32(buf, 0, 1); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* version */
    write_u32(buf, 0, 1); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* clip_count */

    /* Clip header: name(64) + duration(f32) + sampler_count + channel_count */
    uint8_t name[64];
    SDL_memset(name, 0, sizeof(name));
    SDL_strlcpy((char *)name, "morph_test", sizeof(name));
    wok = wok && (SDL_WriteIO(io, name, sizeof(name)) == sizeof(name));
    write_f32(buf, 0, 1.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* duration */
    write_u32(buf, 0, 1); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* sampler_count */
    write_u32(buf, 0, 1); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* channel_count */

    /* Sampler: 2 keyframes, 2 components (2 morph targets) */
    write_u32(buf, 0, 2); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* keyframe_count */
    write_u32(buf, 0, 2); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* value_components */
    write_u32(buf, 0, 0); wok = wok && (SDL_WriteIO(io, buf, 4) == 4); /* interpolation = LINEAR */
    /* timestamps */
    write_f32(buf, 0, 0.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    write_f32(buf, 0, 1.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    /* values: 2 keyframes × 2 components = 4 floats */
    write_f32(buf, 0, 0.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    write_f32(buf, 0, 0.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    write_f32(buf, 0, 1.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    write_f32(buf, 0, 1.0f); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);

    /* Channel: target_node=0, target_path=MORPH_WEIGHTS, sampler=0 */
    write_i32(buf, 0, 0); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, FORGE_PIPELINE_ANIM_MORPH_WEIGHTS);
    wok = wok && (SDL_WriteIO(io, buf, 4) == 4);
    write_u32(buf, 0, 0); wok = wok && (SDL_WriteIO(io, buf, 4) == 4);

    bool close_ok = SDL_CloseIO(io);
    if (!close_ok) {
        SDL_Log("test_morph_anim_path_accepted: SDL_CloseIO failed: %s",
                SDL_GetError());
    }
    ASSERT_TRUE(close_ok);
    ASSERT_TRUE(wok);

    ForgePipelineAnimFile anim;
    bool loaded = forge_pipeline_load_animation(path, &anim);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(anim.clip_count == 1);
    ASSERT_TRUE(anim.clips[0].channel_count == 1);
    ASSERT_TRUE(anim.clips[0].channels[0].target_path ==
                FORGE_PIPELINE_ANIM_MORPH_WEIGHTS);
    ASSERT_TRUE(anim.clips[0].samplers[0].value_components == 2);
    forge_pipeline_free_animation(&anim);
    cleanup_file(path);
    END_TEST();
}

static void test_morph_anim_apply_skips_weights(void)
{
    TEST("morph_anim_apply_skips_weights");
    /* Verify that morph weight channels don't modify node TRS */
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);
    float orig_tx = node.translation[0];

    static float timestamps[2] = { 0.0f, 1.0f };
    static float values[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 2;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_MORPH_WEIGHTS;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    /* Node translation should be unchanged */
    ASSERT_TRUE(float_eq(node.translation[0], orig_tx));
    END_TEST();
}

static void test_morph_backward_compat_no_flag(void)
{
    TEST("morph_backward_compat_no_flag");
    /* v3 .fmesh without morph flag should load with zero morph data */
    char path[512];
    temp_path(path, sizeof(path), "test_no_morph.fmesh");
    ASSERT_TRUE(write_test_fmesh_v3(path, 32, 0));
    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));
    ASSERT_TRUE(!forge_pipeline_has_morph_data(&mesh));
    ASSERT_TRUE(mesh.morph_target_count == 0);
    ASSERT_TRUE(mesh.morph_targets == NULL);
    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

/* ── Path traversal rejection in load_clip (3 tests) ── */

static void test_load_clip_path_traversal(void)
{
    TEST("load_clip_path_traversal");
    char path[512];
    temp_path(path, sizeof(path), "test_clip_traversal.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"evil\": { \"file\": \"../secret.bin\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ForgePipelineAnimFile file;
    bool clip_loaded = forge_pipeline_load_clip(&set, "evil", &file);
    ASSERT_TRUE(!clip_loaded);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_clip_absolute_path(void)
{
    TEST("load_clip_absolute_path");
    char path[512];
    temp_path(path, sizeof(path), "test_clip_abspath.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"abs\": { \"file\": \"/etc/passwd\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ForgePipelineAnimFile file;
    bool clip_loaded = forge_pipeline_load_clip(&set, "abs", &file);
    ASSERT_TRUE(!clip_loaded);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

static void test_load_clip_windows_drive_path(void)
{
    TEST("load_clip_windows_drive_path");
    char path[512];
    temp_path(path, sizeof(path), "test_clip_drive.fanims");
    ASSERT_TRUE(write_test_fanims(path,
        "{ \"version\": 1, \"model\": \"M\", \"clips\": {"
        "  \"win\": { \"file\": \"C:\\\\evil.bin\", \"duration\": 1.0, "
        "\"loop\": false, \"tags\": [] }"
        "}}"));
    ForgePipelineAnimSet set;
    bool loaded = forge_pipeline_load_anim_set(path, &set);
    ASSERT_TRUE(loaded);
    ForgePipelineAnimFile file;
    bool clip_loaded = forge_pipeline_load_clip(&set, "win", &file);
    ASSERT_TRUE(!clip_loaded);
    forge_pipeline_free_anim_set(&set);
    cleanup_file(path);
    END_TEST();
}

/* ── Zero-joint skin handling (1 test) ── */

static void test_load_skin_zero_joints(void)
{
    TEST("load_skin_zero_joints");
    char path[512];
    temp_path(path, sizeof(path), "test_skin_zero_joints.fskin");
    /* Write a .fskin with 1 skin that has 0 joints */
    ASSERT_TRUE(write_test_fskin(path, 1, 0, "EmptySkin"));
    ForgePipelineSkinSet set;
    bool loaded = forge_pipeline_load_skins(path, &set);
    ASSERT_TRUE(loaded);
    ASSERT_INT_EQ(set.skin_count, 1);
    ASSERT_INT_EQ(set.skins[0].joint_count, 0);
    ASSERT_TRUE(set.skins[0].joints == NULL);
    ASSERT_TRUE(set.skins[0].inverse_bind_matrices == NULL);
    forge_pipeline_free_skins(&set);
    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Helper: initialise a scene node to identity
 * ══════════════════════════════════════════════════════════════════════════ */

static void init_identity_node(ForgePipelineSceneNode *node, int32_t parent)
{
    mat4 id;
    SDL_memset(node, 0, sizeof(*node));
    node->parent = parent;
    node->mesh_index = -1;
    node->skin_index = -1;
    node->has_trs = 1;
    node->translation[0] = 0.0f; node->translation[1] = 0.0f; node->translation[2] = 0.0f;
    node->rotation[0] = 0.0f; node->rotation[1] = 0.0f; node->rotation[2] = 0.0f; node->rotation[3] = 1.0f;
    node->scale[0] = 1.0f; node->scale[1] = 1.0f; node->scale[2] = 1.0f;
    /* Set local_transform and world_transform to identity */
    id = mat4_identity();
    SDL_memcpy(node->local_transform, &id, sizeof(node->local_transform));
    SDL_memcpy(node->world_transform, &id, sizeof(node->world_transform));
}

/* ══════════════════════════════════════════════════════════════════════════
 * Animation runtime (8 tests)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_anim_apply_null_args(void)
{
    TEST("anim_apply_null_args");
    /* NULL anim */
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);
    forge_pipeline_anim_apply(NULL, &node, 1, 0.0f, false);
    /* NULL nodes */
    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    forge_pipeline_anim_apply(&anim, NULL, 1, 0.0f, false);
    /* node_count 0 */
    forge_pipeline_anim_apply(&anim, &node, 0, 0.0f, false);
    /* channel_count 0 */
    anim.channel_count = 0;
    forge_pipeline_anim_apply(&anim, &node, 1, 0.0f, false);
    /* If we get here, nothing crashed */
    ASSERT_TRUE(true);
    END_TEST();
}

static void test_anim_apply_translation(void)
{
    TEST("anim_apply_translation");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Sampler: 2 keyframes, translation (3 components) */
    static float timestamps[2] = { 0.0f, 1.0f };
    static float values[6] = { 0.0f, 0.0f, 0.0f,   /* t=0: origin */
                                10.0f, 0.0f, 0.0f }; /* t=1: x=10 */

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_TRUE(float_eq(node.translation[0], 5.0f));
    ASSERT_TRUE(float_eq(node.translation[1], 0.0f));
    ASSERT_TRUE(float_eq(node.translation[2], 0.0f));
    END_TEST();
}

static void test_anim_apply_rotation(void)
{
    TEST("anim_apply_rotation");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Sampler: 2 keyframes, rotation (4 components, quaternion xyzw) */
    float sin45 = SDL_sinf(45.0f * FORGE_DEG2RAD);
    float cos45 = SDL_cosf(45.0f * FORGE_DEG2RAD);
    static float timestamps[2] = { 0.0f, 1.0f };
    float values[8];
    /* t=0: identity quaternion */
    values[0] = 0.0f; values[1] = 0.0f; values[2] = 0.0f; values[3] = 1.0f;
    /* t=1: 90deg around Y */
    values[4] = 0.0f; values[5] = sin45; values[6] = 0.0f; values[7] = cos45;

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 4;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_ROTATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* At t=0, rotation should stay identity */
    forge_pipeline_anim_apply(&anim, &node, 1, 0.0f, false);
    ASSERT_TRUE(float_eq(node.rotation[0], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[1], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[2], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[3], 1.0f));

    /* At t=0.5, rotation should be halfway (slerp between identity and
     * 90deg around Y).  Half of 90deg = 45deg, so:
     *   q = (0, sin(22.5deg), 0, cos(22.5deg))                        */
    float sin22 = SDL_sinf(22.5f * FORGE_DEG2RAD);
    float cos22 = SDL_cosf(22.5f * FORGE_DEG2RAD);
    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_TRUE(float_eq(node.rotation[0], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[1], sin22));
    ASSERT_TRUE(float_eq(node.rotation[2], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[3], cos22));

    /* At t=1.0, rotation should be the full 90deg around Y */
    forge_pipeline_anim_apply(&anim, &node, 1, 1.0f, false);
    ASSERT_TRUE(float_eq(node.rotation[0], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[1], sin45));
    ASSERT_TRUE(float_eq(node.rotation[2], 0.0f));
    ASSERT_TRUE(float_eq(node.rotation[3], cos45));
    END_TEST();
}

static void test_anim_apply_exceeds_max_nodes(void)
{
    TEST("anim_apply_exceeds_max_nodes");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);
    node.translation[0] = 42.0f;

    static float timestamps[2] = { 0.0f, 1.0f };
    static float values[6] = { 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* Call with node_count exceeding max — should return without modifying */
    forge_pipeline_anim_apply(&anim, &node, FORGE_PIPELINE_MAX_NODES + 1,
                              0.5f, false);
    ASSERT_TRUE(float_eq(node.translation[0], 42.0f));
    END_TEST();
}

static void test_anim_apply_loop(void)
{
    TEST("anim_apply_loop");
    ForgePipelineSceneNode node_loop, node_direct;
    init_identity_node(&node_loop, -1);
    init_identity_node(&node_direct, -1);

    static float timestamps[2] = { 0.0f, 2.0f };
    static float values[6] = { 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 2.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* t=3.0 with loop wraps to t=1.0 */
    forge_pipeline_anim_apply(&anim, &node_loop, 1, 3.0f, true);
    /* t=1.0 directly */
    forge_pipeline_anim_apply(&anim, &node_direct, 1, 1.0f, false);
    ASSERT_TRUE(float_eq(node_loop.translation[0], node_direct.translation[0]));
    END_TEST();
}

static void test_anim_apply_clamp(void)
{
    TEST("anim_apply_clamp");
    ForgePipelineSceneNode node_clamp, node_end;
    init_identity_node(&node_clamp, -1);
    init_identity_node(&node_end, -1);

    static float timestamps[2] = { 0.0f, 2.0f };
    static float values[6] = { 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 2.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* t=3.0 without loop clamps to t=2.0 */
    forge_pipeline_anim_apply(&anim, &node_clamp, 1, 3.0f, false);
    /* t=2.0 directly */
    forge_pipeline_anim_apply(&anim, &node_end, 1, 2.0f, false);
    ASSERT_TRUE(float_eq(node_clamp.translation[0], node_end.translation[0]));
    ASSERT_TRUE(float_eq(node_clamp.translation[0], 10.0f));
    END_TEST();
}

static void test_anim_apply_step_interpolation(void)
{
    TEST("anim_apply_step_interpolation");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Sampler: 3 keyframes with STEP interpolation — should hold the
     * preceding keyframe's value, not interpolate between them. */
    static float timestamps[3] = { 0.0f, 1.0f, 2.0f };
    static float values[9] = { 1.0f, 0.0f, 0.0f,   /* t=0: x=1 */
                                5.0f, 0.0f, 0.0f,   /* t=1: x=5 */
                                9.0f, 0.0f, 0.0f };  /* t=2: x=9 */

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 3;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_STEP;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 2.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* t=0.5 should hold at keyframe 0 value (x=1), not interpolate to x=3 */
    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_TRUE(float_eq(node.translation[0], 1.0f));

    /* t=1.5 should hold at keyframe 1 value (x=5), not interpolate to x=7 */
    init_identity_node(&node, -1);
    forge_pipeline_anim_apply(&anim, &node, 1, 1.5f, false);
    ASSERT_TRUE(float_eq(node.translation[0], 5.0f));
    END_TEST();
}

static void test_anim_apply_scale(void)
{
    TEST("anim_apply_scale");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Sampler: 2 keyframes, scale (3 components) */
    static float timestamps[2] = { 0.0f, 1.0f };
    static float values[6] = { 1.0f, 1.0f, 1.0f,   /* t=0: unit scale */
                                2.0f, 3.0f, 4.0f };  /* t=1: non-uniform */

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_SCALE;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* t=0.5: should interpolate to (1.5, 2.0, 2.5) */
    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_TRUE(float_eq(node.scale[0], 1.5f));
    ASSERT_TRUE(float_eq(node.scale[1], 2.0f));
    ASSERT_TRUE(float_eq(node.scale[2], 2.5f));

    /* Verify TRS rebuild produced a local_transform with the scale.
     * Column-major layout: m[0]=sx, m[5]=sy, m[10]=sz for a pure scale. */
    ASSERT_TRUE(float_eq(node.local_transform[0],  1.5f));
    ASSERT_TRUE(float_eq(node.local_transform[5],  2.0f));
    ASSERT_TRUE(float_eq(node.local_transform[10], 2.5f));
    END_TEST();
}

static void test_anim_apply_non_finite_time(void)
{
    TEST("anim_apply_non_finite_time");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Two keyframes: t=0 → (0,0,0), t=1 → (10,10,10).
     * NaN/Inf time should be sanitized to 0, producing (0,0,0). */
    static float timestamps[2] = { 0.0f, 1.0f };
    static float values[6] = { 0.0f, 0.0f, 0.0f,
                                10.0f, 10.0f, 10.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    /* NaN time — should be sanitized to 0 */
    float nan_val = NAN;
    forge_pipeline_anim_apply(&anim, &node, 1, nan_val, false);
    ASSERT_TRUE(float_eq(node.translation[0], 0.0f));
    ASSERT_TRUE(float_eq(node.translation[1], 0.0f));

    /* Inf time — should be sanitized to 0 */
    init_identity_node(&node, -1);
    float inf_val = INFINITY;
    forge_pipeline_anim_apply(&anim, &node, 1, inf_val, false);
    ASSERT_TRUE(float_eq(node.translation[0], 0.0f));
    ASSERT_TRUE(float_eq(node.translation[1], 0.0f));
    END_TEST();
}

static void test_anim_apply_single_keyframe(void)
{
    TEST("anim_apply_single_keyframe");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Sampler with exactly 1 keyframe — the find_keyframe binary search
     * returns 0 for count<2, but eval functions handle this via boundary
     * checks (t <= ts[0]).  Verify the single value IS applied. */
    static float timestamps[1] = { 0.0f };
    static float values[3] = { 7.0f, 8.0f, 9.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 1;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_TRUE(float_eq(node.translation[0], 7.0f));
    ASSERT_TRUE(float_eq(node.translation[1], 8.0f));
    ASSERT_TRUE(float_eq(node.translation[2], 9.0f));
    END_TEST();
}

static void test_anim_apply_invalid_path(void)
{
    TEST("anim_apply_invalid_path");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Save original TRS to verify no modification */
    float orig_tx = node.translation[0];
    float orig_s0 = node.scale[0];

    static float timestamps[2] = { 0.0f, 1.0f };
    static float values[6] = { 1.0f, 2.0f, 3.0f,
                                4.0f, 5.0f, 6.0f };

    ForgePipelineAnimSampler sampler;
    SDL_memset(&sampler, 0, sizeof(sampler));
    sampler.timestamps = timestamps;
    sampler.values = values;
    sampler.keyframe_count = 2;
    sampler.value_components = 3;
    sampler.interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    ForgePipelineAnimChannel channel;
    SDL_memset(&channel, 0, sizeof(channel));
    channel.target_node = 0;
    channel.target_path = 99; /* invalid path — hits default case */
    channel.sampler_index = 0;

    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    anim.duration = 1.0f;
    anim.samplers = &sampler;
    anim.sampler_count = 1;
    anim.channels = &channel;
    anim.channel_count = 1;

    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    /* Node should be unchanged — invalid path hits the default continue */
    ASSERT_TRUE(float_eq(node.translation[0], orig_tx));
    ASSERT_TRUE(float_eq(node.scale[0], orig_s0));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * World transform computation (5 tests)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_world_transform_single_root(void)
{
    TEST("world_transform_single_root");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Set local_transform to translate(5,0,0) */
    mat4 t = mat4_translate(vec3_create(5.0f, 0.0f, 0.0f));
    SDL_memcpy(node.local_transform, &t, sizeof(node.local_transform));

    uint32_t roots[1] = { 0 };
    forge_pipeline_scene_compute_world_transforms(&node, 1, roots, 1, NULL, 0);

    /* World should match local for a root node */
    mat4 world;
    SDL_memcpy(&world, node.world_transform, sizeof(world));
    ASSERT_TRUE(float_eq(world.m[12], 5.0f));
    ASSERT_TRUE(float_eq(world.m[13], 0.0f));
    ASSERT_TRUE(float_eq(world.m[14], 0.0f));
    END_TEST();
}

static void test_world_transform_parent_child(void)
{
    TEST("world_transform_parent_child");
    ForgePipelineSceneNode nodes[2];
    init_identity_node(&nodes[0], -1);
    init_identity_node(&nodes[1], 0);

    /* Parent: translate(5,0,0) */
    mat4 t0 = mat4_translate(vec3_create(5.0f, 0.0f, 0.0f));
    SDL_memcpy(nodes[0].local_transform, &t0, sizeof(nodes[0].local_transform));

    /* Child: translate(0,3,0) */
    mat4 t1 = mat4_translate(vec3_create(0.0f, 3.0f, 0.0f));
    SDL_memcpy(nodes[1].local_transform, &t1, sizeof(nodes[1].local_transform));

    /* Set up hierarchy: node 0 has 1 child (node 1) */
    nodes[0].first_child = 0;
    nodes[0].child_count = 1;
    uint32_t children[1] = { 1 };
    uint32_t roots[1] = { 0 };

    forge_pipeline_scene_compute_world_transforms(
        nodes, 2, roots, 1, children, 1);

    /* Child world should be translate(5,3,0) */
    mat4 world;
    SDL_memcpy(&world, nodes[1].world_transform, sizeof(world));
    ASSERT_TRUE(float_eq(world.m[12], 5.0f));
    ASSERT_TRUE(float_eq(world.m[13], 3.0f));
    ASSERT_TRUE(float_eq(world.m[14], 0.0f));
    END_TEST();
}

static void test_world_transform_cycle_detection(void)
{
    TEST("world_transform_cycle_detection");
    ForgePipelineSceneNode nodes[2];
    init_identity_node(&nodes[0], -1);
    init_identity_node(&nodes[1], -1);

    /* Create a cycle: node 0 -> child 1, node 1 -> child 0 */
    nodes[0].first_child = 0;
    nodes[0].child_count = 1;
    nodes[1].first_child = 1;
    nodes[1].child_count = 1;
    uint32_t children[2] = { 1, 0 };
    uint32_t roots[2] = { 0, 1 };

    /* Should not hang — cycle detection should prevent infinite recursion */
    forge_pipeline_scene_compute_world_transforms(
        nodes, 2, roots, 2, children, 2);

    /* If we get here, no hang occurred */
    ASSERT_TRUE(true);
    END_TEST();
}

static void test_world_transform_null_args(void)
{
    TEST("world_transform_null_args");
    /* NULL nodes */
    forge_pipeline_scene_compute_world_transforms(NULL, 0, NULL, 0, NULL, 0);
    /* node_count 0 */
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);
    forge_pipeline_scene_compute_world_transforms(&node, 0, NULL, 0, NULL, 0);
    /* Should not crash */
    ASSERT_TRUE(true);
    END_TEST();
}

static void test_world_transform_no_roots(void)
{
    TEST("world_transform_no_roots");
    ForgePipelineSceneNode nodes[2];
    init_identity_node(&nodes[0], -1);
    init_identity_node(&nodes[1], -1);

    /* Both nodes are roots (parent=-1) but no explicit root array */
    mat4 t0 = mat4_translate(vec3_create(1.0f, 0.0f, 0.0f));
    mat4 t1 = mat4_translate(vec3_create(0.0f, 2.0f, 0.0f));
    SDL_memcpy(nodes[0].local_transform, &t0, sizeof(nodes[0].local_transform));
    SDL_memcpy(nodes[1].local_transform, &t1, sizeof(nodes[1].local_transform));

    /* Pass NULL root array — should use fallback (parent == -1) */
    forge_pipeline_scene_compute_world_transforms(nodes, 2, NULL, 0, NULL, 0);

    mat4 w0, w1;
    SDL_memcpy(&w0, nodes[0].world_transform, sizeof(w0));
    SDL_memcpy(&w1, nodes[1].world_transform, sizeof(w1));
    ASSERT_TRUE(float_eq(w0.m[12], 1.0f));
    ASSERT_TRUE(float_eq(w1.m[13], 2.0f));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Joint matrix computation (5 tests)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_joint_matrices_null_args(void)
{
    TEST("joint_matrices_null_args");
    mat4 out;
    /* NULL skin */
    uint32_t r = forge_pipeline_compute_joint_matrices(NULL, NULL, 0, 0, &out, 1);
    ASSERT_UINT_EQ(r, 0);
    /* NULL nodes */
    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    r = forge_pipeline_compute_joint_matrices(&skin, NULL, 0, 0, &out, 1);
    ASSERT_UINT_EQ(r, 0);
    /* NULL out_matrices */
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);
    r = forge_pipeline_compute_joint_matrices(&skin, &node, 1, 0, NULL, 1);
    ASSERT_UINT_EQ(r, 0);
    END_TEST();
}

static void test_joint_matrices_identity(void)
{
    TEST("joint_matrices_identity");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* 1 joint at node 0, IBM is identity */
    static int32_t joints[1] = { 0 };
    static float ibm[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    skin.joints = joints;
    skin.inverse_bind_matrices = ibm;
    skin.joint_count = 1;

    mat4 out;
    uint32_t r = forge_pipeline_compute_joint_matrices(
        &skin, &node, 1, 0, &out, 1);
    ASSERT_UINT_EQ(r, 1);

    /* Result should be identity: inv(I) * I * I = I */
    {
        mat4 id = mat4_identity();
        int i;
        for (i = 0; i < 16; i++) {
            ASSERT_TRUE(float_eq(out.m[i], id.m[i]));
        }
    }
    END_TEST();
}

static void test_joint_matrices_with_transform(void)
{
    TEST("joint_matrices_with_transform");
    ForgePipelineSceneNode nodes[2];
    init_identity_node(&nodes[0], -1);  /* mesh node — translate(2,0,0) */
    init_identity_node(&nodes[1], -1);  /* joint node — translate(5,0,0) */

    mat4 mesh_world = mat4_translate(vec3_create(2.0f, 0.0f, 0.0f));
    mat4 joint_world = mat4_translate(vec3_create(5.0f, 0.0f, 0.0f));
    SDL_memcpy(nodes[0].world_transform, &mesh_world,
               sizeof(nodes[0].world_transform));
    SDL_memcpy(nodes[1].world_transform, &joint_world,
               sizeof(nodes[1].world_transform));

    /* 1 joint at node 1, IBM is identity */
    static int32_t joints[1] = { 1 };
    static float ibm[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    skin.joints = joints;
    skin.inverse_bind_matrices = ibm;
    skin.joint_count = 1;

    mat4 out;
    uint32_t r = forge_pipeline_compute_joint_matrices(
        &skin, nodes, 2, 0, &out, 1);
    ASSERT_UINT_EQ(r, 1);

    /* Result: inv(translate(2,0,0)) * translate(5,0,0) * I = translate(3,0,0) */
    ASSERT_TRUE(float_eq(out.m[12], 3.0f));
    ASSERT_TRUE(float_eq(out.m[13], 0.0f));
    ASSERT_TRUE(float_eq(out.m[14], 0.0f));
    END_TEST();
}

static void test_joint_matrices_invalid_joint_index(void)
{
    TEST("joint_matrices_invalid_joint_index");
    ForgePipelineSceneNode node;
    init_identity_node(&node, -1);

    /* Joint points to node index 999 (out of range) */
    static int32_t joints[1] = { 999 };
    static float ibm[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    skin.joints = joints;
    skin.inverse_bind_matrices = ibm;
    skin.joint_count = 1;

    mat4 out;
    uint32_t r = forge_pipeline_compute_joint_matrices(
        &skin, &node, 1, 0, &out, 1);
    ASSERT_UINT_EQ(r, 1);

    /* Should write identity for invalid joint */
    {
        mat4 id = mat4_identity();
        int i;
        for (i = 0; i < 16; i++) {
            ASSERT_TRUE(float_eq(out.m[i], id.m[i]));
        }
    }
    END_TEST();
}

static void test_joint_matrices_max_cap(void)
{
    TEST("joint_matrices_max_cap");
    ForgePipelineSceneNode nodes[3];
    init_identity_node(&nodes[0], -1);
    init_identity_node(&nodes[1], -1);
    init_identity_node(&nodes[2], -1);

    static int32_t joints[3] = { 0, 1, 2 };
    static float ibm[48] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
    };

    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    skin.joints = joints;
    skin.inverse_bind_matrices = ibm;
    skin.joint_count = 3;

    /* Only request 2 matrices even though skin has 3 joints.
     * Allocate a third slot with a sentinel to verify no overwrite. */
    mat4 out[3];
    mat4 sentinel = mat4_translate(vec3_create(123.0f, 456.0f, 789.0f));
    out[2] = sentinel;
    uint32_t r = forge_pipeline_compute_joint_matrices(
        &skin, nodes, 3, 0, out, 2);
    ASSERT_UINT_EQ(r, 2);
    /* Sentinel must be untouched — proves the cap prevented overwrite */
    ASSERT_TRUE(float_eq(out[2].m[12], sentinel.m[12]));
    ASSERT_TRUE(float_eq(out[2].m[13], sentinel.m[13]));
    ASSERT_TRUE(float_eq(out[2].m[14], sentinel.m[14]));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════════════════
 * Atlas metadata loading tests
 *
 * Tests for forge_pipeline_load_atlas() and forge_pipeline_free_atlas().
 * Writes temporary atlas.json files and verifies correct parsing.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Helper: write a JSON string to a temp file */
static bool write_atlas_json(const char *filename, const char *json)
{
    char path[512];
    temp_path(path, sizeof(path), filename);
    if (!SDL_SaveFile(path, json, SDL_strlen(json))) {
        SDL_Log("write_atlas_json: SDL_SaveFile failed for '%s': %s",
                path, SDL_GetError());
        return false;
    }
    return true;
}

static void test_load_atlas_valid(void)
{
    TEST("load atlas — valid 2-entry file");
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"width\": 2048,\n"
        "  \"height\": 2048,\n"
        "  \"padding\": 4,\n"
        "  \"utilization\": 0.499,\n"
        "  \"entries\": {\n"
        "    \"bark\": {\n"
        "      \"x\": 4, \"y\": 4, \"width\": 256, \"height\": 256,\n"
        "      \"u_offset\": 0.002, \"v_offset\": 0.002,\n"
        "      \"u_scale\": 0.125, \"v_scale\": 0.125\n"
        "    },\n"
        "    \"marble\": {\n"
        "      \"x\": 268, \"y\": 4, \"width\": 256, \"height\": 256,\n"
        "      \"u_offset\": 0.1309, \"v_offset\": 0.002,\n"
        "      \"u_scale\": 0.125, \"v_scale\": 0.125\n"
        "    }\n"
        "  }\n"
        "}";
    ASSERT_TRUE(write_atlas_json("atlas_valid.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_valid.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(forge_pipeline_load_atlas(path, &atlas));
    ASSERT_INT_EQ(atlas.width, 2048);
    ASSERT_INT_EQ(atlas.height, 2048);
    ASSERT_INT_EQ(atlas.padding, 4);
    ASSERT_FLOAT_EQ(atlas.utilization, 0.499f);
    ASSERT_INT_EQ(atlas.entry_count, 2);
    ASSERT_NOT_NULL(atlas.entries);

    /* Verify entry data (cJSON iterates in insertion order) */
    ASSERT_TRUE(SDL_strcmp(atlas.entries[0].name, "bark") == 0);
    ASSERT_INT_EQ(atlas.entries[0].x, 4);
    ASSERT_INT_EQ(atlas.entries[0].y, 4);
    ASSERT_INT_EQ(atlas.entries[0].width, 256);
    ASSERT_INT_EQ(atlas.entries[0].height, 256);
    ASSERT_FLOAT_EQ(atlas.entries[0].u_offset, 0.002f);
    ASSERT_FLOAT_EQ(atlas.entries[0].v_offset, 0.002f);
    ASSERT_FLOAT_EQ(atlas.entries[0].u_scale, 0.125f);
    ASSERT_FLOAT_EQ(atlas.entries[0].v_scale, 0.125f);

    ASSERT_TRUE(SDL_strcmp(atlas.entries[1].name, "marble") == 0);
    ASSERT_INT_EQ(atlas.entries[1].x, 268);
    ASSERT_FLOAT_EQ(atlas.entries[1].u_offset, 0.1309f);

    forge_pipeline_free_atlas(&atlas);
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_null_path(void)
{
    TEST("load atlas — NULL path");
    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(NULL, &atlas));
    END_TEST();
}

static void test_load_atlas_null_atlas(void)
{
    TEST("load atlas — NULL atlas");
    ASSERT_TRUE(!forge_pipeline_load_atlas("dummy.json", NULL));
    END_TEST();
}

static void test_load_atlas_nonexistent(void)
{
    TEST("load atlas — nonexistent file");
    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas("no_such_atlas.json", &atlas));
    END_TEST();
}

static void test_load_atlas_invalid_json(void)
{
    TEST("load atlas — invalid JSON");
    ASSERT_TRUE(write_atlas_json("atlas_bad.json", "{ not valid json !!!"));
    char path[512];
    temp_path(path, sizeof(path), "atlas_bad.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_missing_width(void)
{
    TEST("load atlas — missing width/height");
    const char *json =
        "{ \"version\": 1,"
        "  \"entries\": { \"a\": { \"x\": 0, \"y\": 0 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_no_dim.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_no_dim.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_bad_version(void)
{
    TEST("load atlas — wrong version rejected");
    const char *json =
        "{ \"version\": 99, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 0, \"y\": 0, \"width\": 64,"
        "    \"height\": 64, \"u_offset\": 0.0, \"v_offset\": 0.0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_bad_ver.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_bad_ver.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_missing_entries(void)
{
    TEST("load atlas — missing entries object");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512 }";
    ASSERT_TRUE(write_atlas_json("atlas_no_entries.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_no_entries.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_empty_entries(void)
{
    TEST("load atlas — empty entries object");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512, \"entries\": {} }";
    ASSERT_TRUE(write_atlas_json("atlas_empty.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_empty.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_optional_fields(void)
{
    TEST("load atlas — optional padding/utilization/UV defaults");
    /* Omit padding, utilization, AND per-entry UV fields to exercise
     * all default paths in the loader */
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"width\": 1024, \"height\": 1024,\n"
        "  \"entries\": {\n"
        "    \"stone\": { \"x\": 0, \"y\": 0, \"width\": 128, \"height\": 128 }\n"
        "  }\n"
        "}";
    ASSERT_TRUE(write_atlas_json("atlas_defaults.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_defaults.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(forge_pipeline_load_atlas(path, &atlas));
    /* padding and utilization should default to 0 when missing */
    ASSERT_INT_EQ(atlas.padding, 0);
    ASSERT_FLOAT_EQ(atlas.utilization, 0.0f);
    ASSERT_INT_EQ(atlas.entry_count, 1);
    /* Per-entry UV fields should default to identity transform */
    ASSERT_FLOAT_EQ(atlas.entries[0].u_offset, 0.0f);
    ASSERT_FLOAT_EQ(atlas.entries[0].v_offset, 0.0f);
    ASSERT_FLOAT_EQ(atlas.entries[0].u_scale, 1.0f);
    ASSERT_FLOAT_EQ(atlas.entries[0].v_scale, 1.0f);
    forge_pipeline_free_atlas(&atlas);
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_name_truncation(void)
{
    TEST("load atlas — long name truncated to buffer size");
    /* Name longer than FORGE_PIPELINE_ATLAS_NAME_LEN (128) */
    char long_name[256];
    SDL_memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    char json[1024];
    SDL_snprintf(json, sizeof(json),
        "{ \"version\": 1, \"width\": 512, \"height\": 512, \"entries\": {"
        "\"%s\": { \"x\": 0, \"y\": 0, \"width\": 64, \"height\": 64,"
        " \"u_offset\": 0, \"v_offset\": 0, \"u_scale\": 0.125,"
        " \"v_scale\": 0.125 } } }", long_name);
    ASSERT_TRUE(write_atlas_json("atlas_longname.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_longname.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(forge_pipeline_load_atlas(path, &atlas));
    /* Name should be truncated, not overflow */
    ASSERT_TRUE(SDL_strlen(atlas.entries[0].name) <
                FORGE_PIPELINE_ATLAS_NAME_LEN);
    forge_pipeline_free_atlas(&atlas);
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_entry_missing_fields(void)
{
    TEST("load atlas — entry missing x/y/width/height rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 0, \"y\": 0 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_entry_missing.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_entry_missing.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_entry_negative_coords(void)
{
    TEST("load atlas — entry with negative coordinates rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": -1, \"y\": 0, \"width\": 64,"
        "    \"height\": 64, \"u_offset\": 0, \"v_offset\": 0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_neg_coords.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_neg_coords.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_entry_zero_size(void)
{
    TEST("load atlas — entry with zero width rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 0, \"y\": 0, \"width\": 0,"
        "    \"height\": 64, \"u_offset\": 0, \"v_offset\": 0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_zero_size.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_zero_size.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_entry_oob(void)
{
    TEST("load atlas — entry exceeding atlas bounds rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 500, \"y\": 0, \"width\": 64,"
        "    \"height\": 64, \"u_offset\": 0, \"v_offset\": 0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_oob.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_oob.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_entry_fractional_x(void)
{
    TEST("load atlas — entry with fractional x rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 0.5, \"y\": 0, \"width\": 64,"
        "    \"height\": 64, \"u_offset\": 0, \"v_offset\": 0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_frac_x.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_frac_x.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_entry_fractional_y(void)
{
    TEST("load atlas — entry with fractional y rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 0, \"y\": 0.5, \"width\": 64,"
        "    \"height\": 64, \"u_offset\": 0, \"v_offset\": 0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_frac_y.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_frac_y.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_load_atlas_fractional_dimensions(void)
{
    TEST("load atlas — fractional width/height rejected");
    const char *json =
        "{ \"version\": 1, \"width\": 512.5, \"height\": 512,"
        "  \"entries\": { \"a\": { \"x\": 0, \"y\": 0, \"width\": 64,"
        "    \"height\": 64, \"u_offset\": 0, \"v_offset\": 0,"
        "    \"u_scale\": 0.125, \"v_scale\": 0.125 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_frac_dim.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_frac_dim.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(!forge_pipeline_load_atlas(path, &atlas));
    cleanup_file(path);
    END_TEST();
}

static void test_free_atlas_null(void)
{
    TEST("free atlas — NULL pointer");
    forge_pipeline_free_atlas(NULL);  /* must not crash */
    END_TEST();
}

static void test_free_atlas_zeroed(void)
{
    TEST("free atlas — zeroed struct");
    ForgePipelineAtlas atlas;
    SDL_memset(&atlas, 0, sizeof(atlas));
    forge_pipeline_free_atlas(&atlas);  /* must not crash */
    ASSERT_INT_EQ(atlas.entry_count, 0);
    ASSERT_NULL(atlas.entries);
    END_TEST();
}

static void test_free_atlas_double(void)
{
    TEST("free atlas — double free safety");
    const char *json =
        "{ \"version\": 1, \"width\": 256, \"height\": 256, \"entries\": {"
        "\"test\": { \"x\": 0, \"y\": 0, \"width\": 64, \"height\": 64,"
        " \"u_offset\": 0, \"v_offset\": 0, \"u_scale\": 0.25,"
        " \"v_scale\": 0.25 } } }";
    ASSERT_TRUE(write_atlas_json("atlas_dbl.json", json));
    char path[512];
    temp_path(path, sizeof(path), "atlas_dbl.json");

    ForgePipelineAtlas atlas;
    ASSERT_TRUE(forge_pipeline_load_atlas(path, &atlas));
    forge_pipeline_free_atlas(&atlas);
    forge_pipeline_free_atlas(&atlas);  /* must not crash */
    ASSERT_NULL(atlas.entries);
    cleanup_file(path);
    END_TEST();
}

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
    test_load_mesh_v1_rejected();

    /* ── Mesh free (2 tests) ── */
    SDL_Log("\nMesh free:");
    test_free_mesh_null();
    test_free_mesh_zeroes();

    /* ── Helper functions (3 tests) ── */
    SDL_Log("\nHelper functions:");
    test_has_tangents();
    test_lod_index_count_out_of_range();
    test_lod_indices_out_of_range();

    /* ── Submesh accessors (3 tests) ── */
    SDL_Log("\nSubmesh accessors:");
    test_submesh_single();
    test_submesh_multiple();
    test_submesh_count_null();

    /* ── Material loading (5 tests) ── */
    SDL_Log("\nMaterial loading:");
    test_load_materials_valid();
    test_load_materials_null_args();
    test_load_materials_bad_version();
    test_load_materials_defaults();
    test_load_materials_malformed_arrays();
    test_load_materials_non_numeric_factors();
    test_free_materials_null();

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

    /* ── Scene loading: valid files (7 tests) ── */
    SDL_Log("\nScene loading — valid files:");
    test_load_scene_single_node();
    test_load_scene_parent_child();
    test_load_scene_three_levels();
    test_load_scene_instanced_mesh();
    test_load_scene_mesh_table();
    test_load_scene_multiple_roots();
    test_load_scene_empty();

    /* ── Scene loading: error cases (27 tests) ── */
    SDL_Log("\nScene loading — error cases:");
    test_load_scene_null_path();
    test_load_scene_null_scene();
    test_load_scene_both_null();
    test_load_scene_nonexistent();
    test_load_scene_invalid_magic();
    test_load_scene_invalid_version();
    test_load_scene_version_zero();
    test_load_scene_truncated_header();
    test_load_scene_truncated_node_data();
    test_load_scene_node_count_exceeds_max();
    test_load_scene_root_count_exceeds_max();
    test_load_scene_mesh_count_exceeds_max();
    test_load_scene_root_index_out_of_range();
    test_load_scene_child_index_out_of_range();
    test_load_scene_first_child_overflow();
    test_load_scene_truncated_children();
    test_load_scene_empty_file();
    test_load_scene_mesh_index_out_of_range();
    test_load_scene_parent_out_of_range();
    test_load_scene_unreachable_node();
    test_load_scene_duplicate_child();
    test_load_scene_parent_mismatch();
    test_load_scene_negative_mesh_index();
    test_load_scene_negative_skin_index();
    test_load_scene_children_exceed_max_edges();
    test_load_scene_invalid_has_trs();
    test_load_scene_submesh_out_of_range();

    /* ── Scene data integrity (3 tests) ── */
    SDL_Log("\nScene data integrity:");
    test_load_scene_trs_roundtrip();
    test_load_scene_children_array_integrity();
    test_load_scene_double_free();

    /* ── Scene free (2 tests) ── */
    SDL_Log("\nScene free:");
    test_free_scene_null();
    test_free_scene_zeroes();

    /* ── Scene accessors (2 tests) ── */
    SDL_Log("\nScene accessors:");
    test_scene_get_mesh_null();
    test_scene_get_mesh_empty();

    /* ── Animation loading: valid files (8 tests) ── */
    SDL_Log("\nAnimation loading — valid files:");
    test_load_anim_single_clip();
    test_load_anim_rotation_sampler();
    test_load_anim_multi_sampler();
    test_load_anim_multi_channel();
    test_load_anim_multi_clip();
    test_load_anim_step_interp();
    test_load_anim_name_roundtrip();
    test_load_anim_duration_roundtrip();

    /* ── Animation loading: error cases (15 tests) ── */
    SDL_Log("\nAnimation loading — error cases:");
    test_load_anim_null_path();
    test_load_anim_null_file();
    test_load_anim_both_null();
    test_load_anim_nonexistent();
    test_load_anim_bad_magic();
    test_load_anim_bad_version();
    test_load_anim_version_zero();
    test_load_anim_truncated_header();
    test_load_anim_truncated_clip();
    test_load_anim_truncated_sampler();
    test_load_anim_truncated_channel();
    test_load_anim_clip_count_exceeds_max();
    test_load_anim_sampler_count_exceeds_max();
    test_load_anim_channel_count_exceeds_max();
    test_load_anim_channel_sampler_oob();

    /* ── Animation data integrity (5 tests) ── */
    SDL_Log("\nAnimation data integrity:");
    test_load_anim_timestamps();
    test_load_anim_keyframe_values();
    test_load_anim_channel_targets();
    test_load_anim_empty_file();
    test_load_anim_zero_keyframes();

    /* ── Animation value validation (4 tests) ── */
    SDL_Log("\nAnimation value validation:");
    test_load_anim_keyframe_count_exceeds_max();
    test_load_anim_invalid_interpolation();
    test_load_anim_invalid_target_path();
    test_load_anim_invalid_value_components();

    /* ── Animation free (3 tests) ── */
    SDL_Log("\nAnimation free:");
    test_free_anim_null();
    test_free_anim_zeroed();
    test_free_anim_double();

    /* ── Animation manifest: valid loading (6 tests) ── */
    SDL_Log("\nAnimation manifest — valid loading:");
    test_load_anim_set_single_clip();
    test_load_anim_set_multiple_clips();
    test_load_anim_set_with_tags();
    test_load_anim_set_loop_flags();
    test_load_anim_set_model_name();
    test_load_anim_set_base_dir();

    /* ── Animation manifest: named lookup (5 tests) ── */
    SDL_Log("\nAnimation manifest — named lookup:");
    test_find_clip_exists();
    test_find_clip_not_found();
    test_find_clip_null_set();
    test_find_clip_null_name();
    test_find_clip_empty_set();

    /* ── Animation manifest: error cases (8 tests) ── */
    SDL_Log("\nAnimation manifest — error cases:");
    test_load_anim_set_null_path();
    test_load_anim_set_null_set();
    test_load_anim_set_nonexistent();
    test_load_anim_set_invalid_json();
    test_load_anim_set_missing_version();
    test_load_anim_set_bad_version();
    test_load_anim_set_missing_clips();
    test_load_anim_set_clip_missing_file();

    /* ── Animation manifest: free safety (3 tests) ── */
    SDL_Log("\nAnimation manifest — free safety:");
    test_free_anim_set_null();
    test_free_anim_set_zeroed();
    test_free_anim_set_double();

    /* ── Animation manifest: load clip convenience (3 tests) ── */
    SDL_Log("\nAnimation manifest — load clip convenience:");
    test_load_clip_not_found();
    test_load_clip_missing_fanim();
    test_load_clip_null_args();
    test_load_clip_success();

    /* ── Skin valid loading (5 tests) ── */
    SDL_Log("\nSkin loading — valid files:");
    test_load_skin_single();
    test_load_skin_multiple();
    test_load_skin_ibm_roundtrip();
    test_load_skin_joint_indices();
    test_load_skin_skeleton_root();

    /* ── Skin error cases (10 tests) ── */
    SDL_Log("\nSkin loading — error cases:");
    test_load_skin_null_path();
    test_load_skin_null_set();
    test_load_skin_nonexistent();
    test_load_skin_bad_magic();
    test_load_skin_bad_version();
    test_load_skin_truncated_header();
    test_load_skin_truncated_joints();
    test_load_skin_truncated_ibm();
    test_load_skin_count_exceeds_max();
    test_load_skin_joint_count_exceeds_max();

    /* ── Skin free safety (3 tests) ── */
    SDL_Log("\nSkin free:");
    test_free_skin_null();
    test_free_skin_zeroed();
    test_free_skin_double();

    /* ── Skinned mesh loading (10 tests) ── */
    SDL_Log("\nSkinned mesh loading:");
    test_load_skinned_mesh_stride_56();
    test_load_skinned_mesh_stride_72();
    test_load_skinned_mesh_flag_set();
    test_load_skinned_mesh_v2_compat();
    test_load_skinned_mesh_v3_no_skin();
    test_load_skinned_mesh_invalid_stride();
    test_has_skin_data_null();
    test_load_skinned_mesh_v3_header();
    test_load_skinned_stride_no_flag();
    test_load_v2_no_skin_flag();

    /* ── Unknown flag bits (1 test) ── */
    SDL_Log("\nUnknown flag bits:");
    test_load_mesh_unknown_flags();

    /* ── Morph target mesh loading (16 tests) ── */
    SDL_Log("\nMorph target mesh loading:");
    test_morph_load_position_only();
    test_morph_load_pos_and_normal();
    test_morph_load_all_attrs();
    test_morph_with_skin();
    test_morph_bad_target_count();
    test_morph_bad_target_count_zero();
    test_morph_bad_attr_flags_zero();
    test_morph_bad_attr_flags_unknown();
    test_morph_truncated_data();
    test_morph_flag_without_data();
    test_morph_free_null();
    test_morph_double_free();
    test_morph_v3_flag_accepted();
    test_morph_anim_path_accepted();
    test_morph_anim_apply_skips_weights();
    test_morph_backward_compat_no_flag();

    /* ── Path traversal rejection (3 tests) ── */
    SDL_Log("\nPath traversal rejection:");
    test_load_clip_path_traversal();
    test_load_clip_absolute_path();
    test_load_clip_windows_drive_path();

    /* ── Zero-joint skin (1 test) ── */
    SDL_Log("\nZero-joint skin:");
    test_load_skin_zero_joints();

    /* ── Animation runtime (11 tests) ── */
    SDL_Log("\nAnimation runtime:");
    test_anim_apply_null_args();
    test_anim_apply_translation();
    test_anim_apply_rotation();
    test_anim_apply_exceeds_max_nodes();
    test_anim_apply_loop();
    test_anim_apply_clamp();
    test_anim_apply_step_interpolation();
    test_anim_apply_scale();
    test_anim_apply_non_finite_time();
    test_anim_apply_single_keyframe();
    test_anim_apply_invalid_path();

    /* ── World transform computation (5 tests) ── */
    SDL_Log("\nWorld transform computation:");
    test_world_transform_single_root();
    test_world_transform_parent_child();
    test_world_transform_cycle_detection();
    test_world_transform_null_args();
    test_world_transform_no_roots();

    /* ── Joint matrix computation (5 tests) ── */
    SDL_Log("\nJoint matrix computation:");
    test_joint_matrices_null_args();
    test_joint_matrices_identity();
    test_joint_matrices_with_transform();
    test_joint_matrices_invalid_joint_index();
    test_joint_matrices_max_cap();

    /* ── Atlas loading: valid files (3 tests) ── */
    SDL_Log("\nAtlas loading — valid files:");
    test_load_atlas_valid();
    test_load_atlas_optional_fields();
    test_load_atlas_name_truncation();

    /* ── Atlas loading: error cases (15 tests) ── */
    SDL_Log("\nAtlas loading — error cases:");
    test_load_atlas_null_path();
    test_load_atlas_null_atlas();
    test_load_atlas_nonexistent();
    test_load_atlas_invalid_json();
    test_load_atlas_missing_width();
    test_load_atlas_bad_version();
    test_load_atlas_missing_entries();
    test_load_atlas_empty_entries();
    test_load_atlas_entry_missing_fields();
    test_load_atlas_entry_negative_coords();
    test_load_atlas_entry_zero_size();
    test_load_atlas_entry_oob();
    test_load_atlas_entry_fractional_x();
    test_load_atlas_entry_fractional_y();
    test_load_atlas_fractional_dimensions();

    /* ── Atlas free safety (3 tests) ── */
    SDL_Log("\nAtlas free:");
    test_free_atlas_null();
    test_free_atlas_zeroed();
    test_free_atlas_double();

    /* ── Summary ── */
    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    SDL_Quit();
    return fail_count > 0 ? 1 : 0;
}
