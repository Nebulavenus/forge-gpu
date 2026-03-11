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
/* SDL provides SDL_memset, SDL_memcpy, SDL_strcmp, etc. — no <string.h> needed */

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
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_no_tan.fmesh");
    TEST("load_mesh_no_tangents");

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

    ASSERT_TRUE(write_broken_fmesh(path, "NOPE", 2, 3, 32, 1, 0, 128));

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

    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 64, 1, 0, 128));

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
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 32, 1, 0, 16));

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
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_too_many_lods.fmesh");
    TEST("load_mesh_too_many_lods");

    /* lod_count = 99, exceeds FORGE_PIPELINE_MAX_LODS (8) */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 32, 99, 0, 128));

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
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 2, 3, 48, 1, 0, 256));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_v1_rejected(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_v1_reject.fmesh");
    TEST("load_mesh_v1_rejected");

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
 * Submesh Accessors
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_submesh_single(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_sub1.fmesh");
    TEST("submesh_single");

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
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_sub3.fmesh");
    TEST("submesh_multiple");

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
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats.fmat");
    TEST("load_materials_valid");

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
    char path[512];
    temp_path(path, sizeof(path), "whatever.fmat");
    TEST("load_materials_null_args");

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(!forge_pipeline_load_materials(NULL, &set));
    ASSERT_TRUE(!forge_pipeline_load_materials(path, NULL));

    END_TEST();
}

static void test_load_materials_bad_version(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_badver.fmat");
    TEST("load_materials_bad_version");

    const char *json = "{ \"version\": 99, \"materials\": [] }";
    ASSERT_TRUE(SDL_SaveFile(path, json, SDL_strlen(json)));

    ForgePipelineMaterialSet set;
    ASSERT_TRUE(!forge_pipeline_load_materials(path, &set));

    cleanup_file(path);
    END_TEST();
}

static void test_load_materials_defaults(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_defaults.fmat");
    TEST("load_materials_defaults");

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
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_malformed.fmat");
    TEST("load_materials_malformed_arrays");

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
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_mats_nonnumeric.fmat");
    TEST("load_materials_non_numeric_factors");

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_CloseIO(io);

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
    SDL_WriteIO(io, buf, size);
    SDL_CloseIO(io);
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
    SDL_WriteIO(io, buf, size);
    SDL_CloseIO(io);
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
    SDL_WriteIO(io, buf, size);
    SDL_CloseIO(io);
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
    SDL_CloseIO(io);

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
    SDL_WriteIO(io, buf, size);
    SDL_CloseIO(io);
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

    /* ── Summary ── */
    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    SDL_Quit();
    return fail_count > 0 ? 1 : 0;
}
