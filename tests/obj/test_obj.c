/*
 * OBJ Parser Tests
 *
 * Automated tests for common/obj/forge_obj.h
 * Writes small OBJ files to a temp directory, parses them, and verifies
 * the output vertex data (positions, normals, UVs, triangle counts).
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"
#include "obj/forge_obj.h"

/* ── Test Framework (same pattern as test_math.c) ─────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define EPSILON 0.0001f

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

#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: %s was false", #expr); \
        fail_count++; \
        return; \
    }

#define ASSERT_FALSE(expr) \
    if (expr) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: %s was true", #expr); \
        fail_count++; \
        return; \
    }

#define ASSERT_UINT_EQ(a, b) \
    if ((a) != (b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected %u, got %u", \
                     (unsigned int)(b), (unsigned int)(a)); \
        fail_count++; \
        return; \
    }

#define ASSERT_FLOAT_EQ(a, b) \
    if (!float_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected %.6f, got %.6f", \
                     (double)(b), (double)(a)); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC2_EQ(a, b) \
    if (!vec2_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f), got (%.3f, %.3f)", \
                     (double)b.x, (double)b.y, (double)a.x, (double)a.y); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC3_EQ(a, b) \
    if (!vec3_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f, %.3f), " \
                     "got (%.3f, %.3f, %.3f)", \
                     (double)b.x, (double)b.y, (double)b.z, \
                     (double)a.x, (double)a.y, (double)a.z); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ── Helper: write a temp OBJ file ────────────────────────────────────────── */
/* Writes a string to a temporary file and returns the path.
 * Caller must SDL_free() the returned path. */

static char *write_temp_obj(const char *obj_content, const char *name)
{
    /* Build path: <temp_dir>/<name>.obj */
    const char *tmp = SDL_GetBasePath();
    if (!tmp) {
        SDL_Log("write_temp_obj: SDL_GetBasePath failed: %s", SDL_GetError());
        return NULL;
    }
    size_t path_len = SDL_strlen(tmp) + SDL_strlen(name) + 8;
    char *path = (char *)SDL_malloc(path_len);
    if (!path) {
        SDL_Log("write_temp_obj: SDL_malloc(%zu) failed", path_len);
        return NULL;
    }
    SDL_snprintf(path, (int)path_len, "%s%s.obj", tmp, name);

    /* Write the file */
    SDL_IOStream *io = SDL_IOFromFile(path, "w");
    if (!io) {
        SDL_Log("write_temp_obj: SDL_IOFromFile failed for '%s': %s",
                path, SDL_GetError());
        SDL_free(path);
        return NULL;
    }
    size_t content_len = SDL_strlen(obj_content);
    size_t written = SDL_WriteIO(io, obj_content, content_len);
    if (written < content_len) {
        SDL_Log("write_temp_obj: SDL_WriteIO failed for '%s': %s",
                path, SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("write_temp_obj: SDL_CloseIO also failed: %s",
                    SDL_GetError());
        }
        SDL_free(path);
        return NULL;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("write_temp_obj: SDL_CloseIO failed for '%s': %s",
                path, SDL_GetError());
        SDL_free(path);
        return NULL;
    }
    return path;
}

/* ── Helper: clean up temp file ───────────────────────────────────────────── */

static void remove_temp_obj(char *path)
{
    if (path) {
        if (!SDL_RemovePath(path)) {
            SDL_Log("remove_temp_obj: SDL_RemovePath failed for '%s': %s",
                    path, SDL_GetError());
        }
        SDL_free(path);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Single triangle ──────────────────────────────────────────────────────── */
/* The simplest possible OBJ: one triangle with only positions. */

static void test_single_triangle(void)
{
    TEST("single triangle (positions only)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3\n";

    char *path = write_temp_obj(obj, "test_tri");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);

    /* Check positions */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[1].position, vec3_create(1.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[2].position, vec3_create(0.0f, 1.0f, 0.0f));

    /* Normals and UVs should be zero (not present in file) */
    ASSERT_VEC3_EQ(mesh.vertices[0].normal, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC2_EQ(mesh.vertices[0].uv, vec2_create(0.0f, 0.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Triangle with UVs and normals ────────────────────────────────────────── */

static void test_triangle_with_uvs_and_normals(void)
{
    TEST("triangle with positions, UVs, and normals");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "f 1/1/1 2/2/1 3/3/1\n";

    char *path = write_temp_obj(obj, "test_tri_uvn");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);

    /* Check positions */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[1].position, vec3_create(1.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[2].position, vec3_create(0.0f, 1.0f, 0.0f));

    /* Check normals — all share the same normal */
    ASSERT_VEC3_EQ(mesh.vertices[0].normal, vec3_create(0.0f, 0.0f, 1.0f));
    ASSERT_VEC3_EQ(mesh.vertices[1].normal, vec3_create(0.0f, 0.0f, 1.0f));
    ASSERT_VEC3_EQ(mesh.vertices[2].normal, vec3_create(0.0f, 0.0f, 1.0f));

    /* Check UVs — flipped V (OBJ V=0 at bottom → GPU V=0 at top) */
    ASSERT_VEC2_EQ(mesh.vertices[0].uv, vec2_create(0.0f, 1.0f));  /* 1-0=1 */
    ASSERT_VEC2_EQ(mesh.vertices[1].uv, vec2_create(1.0f, 1.0f));  /* 1-0=1 */
    ASSERT_VEC2_EQ(mesh.vertices[2].uv, vec2_create(0.0f, 0.0f));  /* 1-1=0 */

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Quad triangulation ───────────────────────────────────────────────────── */
/* A quad should be split into 2 triangles (6 vertices). */

static void test_quad_triangulation(void)
{
    TEST("quad triangulation (4 verts → 2 triangles)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3 4\n";

    char *path = write_temp_obj(obj, "test_quad");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 6);  /* 2 triangles × 3 verts */

    /* Triangle 1: vertices 0, 1, 2 (fan from vertex 0) */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[1].position, vec3_create(1.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[2].position, vec3_create(1.0f, 1.0f, 0.0f));

    /* Triangle 2: vertices 0, 2, 3 (fan from vertex 0) */
    ASSERT_VEC3_EQ(mesh.vertices[3].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[4].position, vec3_create(1.0f, 1.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[5].position, vec3_create(0.0f, 1.0f, 0.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── De-indexing: same position, different UVs ────────────────────────────── */
/* Two triangles sharing a position but with different UVs — each face corner
 * should get its own vertex with the correct UV. */

static void test_deindexing(void)
{
    TEST("de-indexing (same position, different UVs)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 0.0 1.0\n"
        "vt 0.5 0.5\n"
        "f 1/1 2/2 3/3\n"
        "f 2/4 4/2 3/3\n";

    char *path = write_temp_obj(obj, "test_deindex");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 6);  /* 2 triangles */

    /* Triangle 1: position 0 has UV (0, 1-0=1) */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC2_EQ(mesh.vertices[0].uv, vec2_create(0.0f, 1.0f));

    /* Triangle 2: position 1 (vertex 1) has UV #4 = (0.5, 1-0.5=0.5) */
    ASSERT_VEC3_EQ(mesh.vertices[3].position, vec3_create(1.0f, 0.0f, 0.0f));
    ASSERT_VEC2_EQ(mesh.vertices[3].uv, vec2_create(0.5f, 0.5f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── 1-based indexing ─────────────────────────────────────────────────────── */
/* Verify that OBJ's 1-based indices correctly map to 0-based array positions.
 * Use non-sequential face indices to catch off-by-one errors. */

static void test_one_based_indexing(void)
{
    TEST("1-based indexing (face references last vertex)");

    const char *obj =
        "v 10.0 20.0 30.0\n"
        "v 40.0 50.0 60.0\n"
        "v 70.0 80.0 90.0\n"
        "f 3 1 2\n";   /* Intentionally out of order */

    char *path = write_temp_obj(obj, "test_onebase");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);

    /* f 3 1 2 → position[2], position[0], position[1] */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(70.0f, 80.0f, 90.0f));
    ASSERT_VEC3_EQ(mesh.vertices[1].position, vec3_create(10.0f, 20.0f, 30.0f));
    ASSERT_VEC3_EQ(mesh.vertices[2].position, vec3_create(40.0f, 50.0f, 60.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Comments and ignored lines ───────────────────────────────────────────── */
/* OBJ files can contain comments (#), material refs (mtllib, usemtl),
 * groups (g, o), and smooth shading (s) — all should be skipped. */

static void test_comments_and_ignored_lines(void)
{
    TEST("comments and ignored lines (mtllib, usemtl, g, s, o)");

    const char *obj =
        "# This is a comment\n"
        "mtllib material.mtl\n"
        "o MyObject\n"
        "g MyGroup\n"
        "s 1\n"
        "usemtl MyMaterial\n"
        "v 1.0 2.0 3.0\n"
        "v 4.0 5.0 6.0\n"
        "v 7.0 8.0 9.0\n"
        "# Another comment\n"
        "f 1 2 3\n";

    char *path = write_temp_obj(obj, "test_comments");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(1.0f, 2.0f, 3.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Windows line endings (\r\n) ──────────────────────────────────────────── */

static void test_crlf_line_endings(void)
{
    TEST("Windows line endings (\\r\\n)");

    const char *obj =
        "v 1.0 0.0 0.0\r\n"
        "v 0.0 1.0 0.0\r\n"
        "v 0.0 0.0 1.0\r\n"
        "f 1 2 3\r\n";

    char *path = write_temp_obj(obj, "test_crlf");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(1.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[1].position, vec3_create(0.0f, 1.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[2].position, vec3_create(0.0f, 0.0f, 1.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Multiple faces ───────────────────────────────────────────────────────── */
/* Mix of triangles and quads. */

static void test_multiple_faces(void)
{
    TEST("multiple faces (2 triangles + 1 quad = 4 triangles)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "v 2.0 0.0 0.0\n"
        "v 2.0 1.0 0.0\n"
        "f 1 2 3\n"        /* triangle 1 */
        "f 1 3 4\n"        /* triangle 2 */
        "f 2 5 6 3\n";     /* quad → 2 more triangles */

    char *path = write_temp_obj(obj, "test_multi");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 12);  /* 4 triangles × 3 verts */

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── UV flip verification ─────────────────────────────────────────────────── */
/* OBJ V=0 is at the bottom, GPU V=0 is at the top.
 * The parser should flip: v_gpu = 1.0 - v_obj. */

static void test_uv_flip(void)
{
    TEST("UV V-coordinate flip (OBJ bottom-up → GPU top-down)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.25 0.75\n"
        "vt 0.5 0.0\n"
        "vt 1.0 1.0\n"
        "f 1/1 2/2 3/3\n";

    char *path = write_temp_obj(obj, "test_uvflip");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);

    /* vt 0.25 0.75 → (0.25, 1.0 - 0.75) = (0.25, 0.25) */
    ASSERT_VEC2_EQ(mesh.vertices[0].uv, vec2_create(0.25f, 0.25f));
    /* vt 0.5  0.0  → (0.5,  1.0 - 0.0)  = (0.5,  1.0)  */
    ASSERT_VEC2_EQ(mesh.vertices[1].uv, vec2_create(0.5f, 1.0f));
    /* vt 1.0  1.0  → (1.0,  1.0 - 1.0)  = (1.0,  0.0)  */
    ASSERT_VEC2_EQ(mesh.vertices[2].uv, vec2_create(1.0f, 0.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── v/vt format (no normals) ─────────────────────────────────────────────── */

static void test_v_vt_format(void)
{
    TEST("v/vt face format (positions + UVs, no normals)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 0.0 1.0\n"
        "f 1/1 2/2 3/3\n";

    char *path = write_temp_obj(obj, "test_v_vt");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);

    /* Has positions and UVs */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC2_EQ(mesh.vertices[0].uv, vec2_create(0.0f, 1.0f));

    /* Normals should be zero */
    ASSERT_VEC3_EQ(mesh.vertices[0].normal, vec3_create(0.0f, 0.0f, 0.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── v//vn format (no UVs) ────────────────────────────────────────────────── */

static void test_v_vn_format(void)
{
    TEST("v//vn face format (positions + normals, no UVs)");

    const char *obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vn 0.0 0.0 1.0\n"
        "f 1//1 2//1 3//1\n";

    char *path = write_temp_obj(obj, "test_v_vn");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);

    /* Has positions and normals */
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(mesh.vertices[0].normal, vec3_create(0.0f, 0.0f, 1.0f));

    /* UVs should be zero */
    ASSERT_VEC2_EQ(mesh.vertices[0].uv, vec2_create(0.0f, 0.0f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Negative coordinates ─────────────────────────────────────────────────── */

static void test_negative_coordinates(void)
{
    TEST("negative vertex coordinates");

    const char *obj =
        "v -1.5 -2.5 -3.5\n"
        "v  1.0  0.0  0.0\n"
        "v  0.0  1.0  0.0\n"
        "f 1 2 3\n";

    char *path = write_temp_obj(obj, "test_neg");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_VEC3_EQ(mesh.vertices[0].position, vec3_create(-1.5f, -2.5f, -3.5f));

    forge_obj_free(&mesh);
    END_TEST();
}

/* ── Empty/invalid file ───────────────────────────────────────────────────── */

static void test_empty_file(void)
{
    TEST("empty file returns false");

    const char *obj = "# Just a comment, no geometry\n";

    char *path = write_temp_obj(obj, "test_empty");
    ASSERT_TRUE(path != NULL);

    ForgeObjMesh mesh;
    bool ok = forge_obj_load(path, &mesh);
    remove_temp_obj(path);

    ASSERT_FALSE(ok);
    END_TEST();
}

/* ── Nonexistent file ─────────────────────────────────────────────────────── */

static void test_nonexistent_file(void)
{
    TEST("nonexistent file returns false");

    ForgeObjMesh mesh;
    bool ok = forge_obj_load("this_file_does_not_exist_12345.obj", &mesh);

    ASSERT_FALSE(ok);
    END_TEST();
}

/* ── forge_obj_free on zeroed mesh ────────────────────────────────────────── */

static void test_free_null_mesh(void)
{
    TEST("forge_obj_free on zeroed mesh is safe");

    ForgeObjMesh mesh;
    mesh.vertices     = NULL;
    mesh.vertex_count = 0;

    /* Should not crash */
    forge_obj_free(&mesh);

    ASSERT_TRUE(mesh.vertices == NULL);
    ASSERT_UINT_EQ(mesh.vertex_count, 0);
    END_TEST();
}

/* ── Real-world model (space shuttle) ─────────────────────────────────────── */
/* If the shuttle model is accessible, verify it loads with the expected
 * vertex count.  This test is skipped if the file isn't found. */

static void test_space_shuttle_model(void)
{
    TEST("space shuttle model (real-world OBJ)");

    /* Try to find the model relative to the executable */
    const char *base = SDL_GetBasePath();
    if (base != NULL) {
        char path[512];
        SDL_snprintf(path, sizeof(path),
                     "%s../../../lessons/gpu/08-mesh-loading/models/"
                     "space-shuttle/space-shuttle.obj", base);

        ForgeObjMesh mesh;
        bool ok = forge_obj_load(path, &mesh);

        if (ok) {
            /* 1032 quads + 172 triangles = 2236 triangles = 6708 vertices */
            ASSERT_UINT_EQ(mesh.vertex_count, 6708);
            forge_obj_free(&mesh);
        } else {
            /* Model not found — not a failure, just skip.
             * END_TEST() below handles the pass_count increment. */
            SDL_Log("    SKIP (model not found at %s)", path);
        }
    } else {
        SDL_Log("    SKIP (SDL_GetBasePath failed: %s)", SDL_GetError());
    }
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

    SDL_Log("=== OBJ Parser Tests ===\n");

    /* Basic parsing */
    test_single_triangle();
    test_triangle_with_uvs_and_normals();
    test_one_based_indexing();
    test_negative_coordinates();

    /* Face formats */
    test_v_vt_format();
    test_v_vn_format();

    /* Triangulation */
    test_quad_triangulation();
    test_multiple_faces();

    /* De-indexing */
    test_deindexing();

    /* UV handling */
    test_uv_flip();

    /* Robustness */
    test_comments_and_ignored_lines();
    test_crlf_line_endings();
    test_empty_file();
    test_nonexistent_file();
    test_free_null_mesh();

    /* Real model */
    test_space_shuttle_model();

    /* Summary */
    SDL_Log("\n=== Test Summary ===");
    SDL_Log("Total:  %d", test_count);
    SDL_Log("Passed: %d", pass_count);
    SDL_Log("Failed: %d", fail_count);

    if (fail_count > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "\nSome tests FAILED!");
        SDL_Quit();
        return 1;
    }

    SDL_Log("\nAll tests PASSED!");
    SDL_Quit();
    return 0;
}
