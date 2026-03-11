/*
 * Fuzz harness for forge_gltf.h glTF parser
 *
 * Exercises the glTF parsing paths with random mutations to detect:
 *   - Integer overflow in count * stride / element_size calculations
 *   - Negative JSON values cast to unsigned types
 *   - Out-of-range array indices (accessor, bufferView, material, mesh, node)
 *   - Buffer overruns from mismatched binary data sizes
 *   - Component type / accessor type mismatches
 *   - Missing required fields
 *   - Circular node hierarchies
 *   - Memory corruption (canary sentinel detection)
 *
 * Architecture:
 *   - Deterministic xorshift32 PRNG (seeded from argv or fixed default)
 *   - Each iteration builds a minimal valid glTF JSON + binary, applies
 *     random mutations, writes temp files, calls forge_gltf_load(), and
 *     checks invariants on the result.
 *   - On crash or invariant violation, prints seed + iteration for repro.
 *
 * Usage:
 *   fuzz_gltf [seed] [iterations]
 *
 *   seed:       PRNG seed (default: 0xDEADBEEF)
 *   iterations: number of test rounds (default: FORGE_FUZZ_ITERATIONS
 *               or 100000)
 *
 * Exit code: 0 on success, 1 on any invariant violation.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "gltf/forge_gltf.h"
#include "arena/forge_arena.h"

/* ── Configurable iteration count ──────────────────────────────────────── */

#ifndef FORGE_FUZZ_ITERATIONS
#define FORGE_FUZZ_ITERATIONS 100000
#endif

/* ── Default PRNG seed ─────────────────────────────────────────────────── */

#define DEFAULT_SEED 0xDEADBEEFu

/* ── Log suppression during parsing ───────────────────────────────────── */
/* The parser emits SDL_Log messages for every rejected mutation (expected
 * behavior).  These drown out the fuzzer's progress lines, so we install a
 * silent callback before each parse and restore the default after. */

static SDL_LogOutputFunction g_original_log_fn;
static void *g_original_log_userdata;

static void silent_log(void *userdata, int category,
                        SDL_LogPriority priority, const char *message)
{
    (void)userdata; (void)category; (void)priority; (void)message;
}

static void suppress_logs(void)
{
    SDL_GetLogOutputFunction(&g_original_log_fn, &g_original_log_userdata);
    SDL_SetLogOutputFunction(silent_log, NULL);
}

static void restore_logs(void)
{
    SDL_SetLogOutputFunction(g_original_log_fn, g_original_log_userdata);
}

/* ── Canary sentinel for detecting out-of-bounds writes ────────────────── */

#define CANARY_SIZE   16       /* bytes past the ForgeGltfScene allocation */
#define CANARY_BYTE   0xDE     /* sentinel value */

/* ── JSON buffer size ─────────────────────────────────────────────────── */

#define JSON_BUF_SIZE 4096     /* max generated JSON length */

/* ── Binary buffer size (positions + indices for up to 4 triangles) ──── */

#define BIN_BUF_SIZE  512      /* generous for a few primitives */

/* ── Mutation categories ──────────────────────────────────────────────── */

typedef enum MutKind {
    MUT_NONE = 0,              /* no mutation — parse valid scene */
    MUT_NEGATIVE_COUNT,        /* accessor count = -N */
    MUT_HUGE_COUNT,            /* accessor count = 2 billion */
    MUT_NEGATIVE_BV_OFFSET,    /* bufferView byteOffset = -100 */
    MUT_HUGE_BV_OFFSET,        /* bufferView byteOffset = 4294967200 */
    MUT_NEGATIVE_ACC_OFFSET,   /* accessor byteOffset = -4 */
    MUT_WRONG_COMP_TYPE,       /* componentType = 5120 (BYTE) for positions */
    MUT_WRONG_ACC_TYPE,        /* type = "SCALAR" for positions */
    MUT_OOB_MATERIAL_IDX,      /* material index = 9999 */
    MUT_NEGATIVE_MATERIAL_IDX, /* material index = -5 */
    MUT_OOB_MESH_IDX,          /* node mesh index = 9999 */
    MUT_OOB_BV_IDX,            /* accessor bufferView = 9999 */
    MUT_OOB_BUFFER_IDX,        /* bufferView buffer = 9999 */
    MUT_ZERO_BV_LENGTH,        /* bufferView byteLength = 0 */
    MUT_TINY_BV_LENGTH,        /* bufferView byteLength = 1 (too small) */
    MUT_STRIDE_MISALIGN,       /* byteStride = 3 (not aligned to float) */
    MUT_HUGE_STRIDE,           /* byteStride = 1000000 */
    MUT_MISSING_POSITION,      /* mesh primitive has no POSITION attribute */
    MUT_EXTRA_NODES,           /* nodes array has more items than expected */
    MUT_CIRCULAR_CHILDREN,     /* node 0 is child of itself */
    MUT_NEGATIVE_SCENE_IDX,    /* "scene": -1 */
    MUT_OOB_SCENE_IDX,         /* "scene": 9999 */
    MUT_EMPTY_MESHES,          /* meshes array is empty */
    MUT_EMPTY_ACCESSORS,       /* accessors array is empty */
    MUT_NEGATIVE_CHILD_IDX,    /* children: [-1] */
    MUT_OOB_CHILD_IDX,         /* children: [9999] */
    MUT_HUGE_INDEX_COUNT,      /* index accessor count = 2 billion */
    MUT_MISSING_BUFFER_URI,    /* buffer has no uri field */
    MUT_SHORT_BINARY,          /* binary file shorter than declared */
    MUT_WRONG_UV_TYPE,         /* type = "VEC3" for TEXCOORD_0 (should be VEC2) */
    MUT_WITH_TANGENT,          /* add valid TANGENT attribute to exercise tangent alloc */
    MUT_MULTI_MUT,             /* apply 2-4 mutations at once */
    MUT_COUNT                  /* total mutation types */
} MutKind;

/* ── xorshift32 PRNG ──────────────────────────────────────────────────── */

static Uint32 prng_state;

static void prng_seed(Uint32 seed)
{
    prng_state = seed ? seed : 1u;
}

static Uint32 prng_next(void)
{
    Uint32 x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

static int prng_range(int lo, int hi)
{
    if (lo >= hi) return lo;
    Uint32 range = (Uint32)(hi - lo + 1);
    return lo + (int)(prng_next() % range);
}

/* ── Failure reporting ─────────────────────────────────────────────────── */

static Uint32 g_seed;
static int    g_iteration;

#define FUZZ_FAIL(msg, ...)                                               \
    do {                                                                  \
        SDL_Log("FUZZ FAIL: " msg, ##__VA_ARGS__);                        \
        SDL_Log("  seed=%u  iteration=%d",                                \
                (unsigned)g_seed, g_iteration);                           \
        return false;                                                     \
    } while (0)

/* ── Temp file helpers ─────────────────────────────────────────────────── */

typedef struct TempFiles {
    char gltf_path[512];
    char bin_path[512];
} TempFiles;

/* Cached writable path — resolved once, reused for all iterations.
 * Prefer SDL_GetPrefPath (always writable) over SDL_GetBasePath
 * (may be read-only in CI or app-bundle layouts). */
static char *g_temp_path;
static bool  g_temp_path_owned;  /* true if g_temp_path needs SDL_free */

static bool write_temp_files(const char *json, size_t json_len,
                              const void *bin, size_t bin_len,
                              int iter, TempFiles *out)
{
    SDL_IOStream *io;

    if (!g_temp_path) return false;

    SDL_snprintf(out->gltf_path, sizeof(out->gltf_path),
                 "%sfuzz_%d.gltf", g_temp_path, iter);
    SDL_snprintf(out->bin_path, sizeof(out->bin_path),
                 "%sfuzz_%d.bin", g_temp_path, iter);

    /* Write JSON */
    io = SDL_IOFromFile(out->gltf_path, "w");
    if (!io) {
        SDL_Log("fuzz: SDL_IOFromFile failed for '%s': %s",
                out->gltf_path, SDL_GetError());
        return false;
    }
    bool write_ok = (SDL_WriteIO(io, json, json_len) == json_len);
    if (!SDL_CloseIO(io)) {
        SDL_Log("fuzz: SDL_CloseIO failed for '%s': %s",
                out->gltf_path, SDL_GetError());
        return false;
    }
    if (!write_ok) {
        SDL_Log("fuzz: SDL_WriteIO failed for '%s'", out->gltf_path);
        return false;
    }

    /* Write binary (if any) */
    if (bin && bin_len > 0) {
        io = SDL_IOFromFile(out->bin_path, "wb");
        if (!io) {
            SDL_Log("fuzz: SDL_IOFromFile failed for '%s': %s",
                    out->bin_path, SDL_GetError());
            return false;
        }
        write_ok = (SDL_WriteIO(io, bin, bin_len) == bin_len);
        if (!SDL_CloseIO(io)) {
            SDL_Log("fuzz: SDL_CloseIO failed for '%s': %s",
                    out->bin_path, SDL_GetError());
            return false;
        }
        if (!write_ok) {
            SDL_Log("fuzz: SDL_WriteIO failed for '%s'", out->bin_path);
            return false;
        }
    }

    return true;
}

static void remove_temp_files(TempFiles *tf)
{
    if (tf->gltf_path[0] && !SDL_RemovePath(tf->gltf_path)) {
        SDL_Log("fuzz: SDL_RemovePath failed for '%s': %s",
                tf->gltf_path, SDL_GetError());
    }
    if (tf->bin_path[0] && !SDL_RemovePath(tf->bin_path)) {
        SDL_Log("fuzz: SDL_RemovePath failed for '%s': %s",
                tf->bin_path, SDL_GetError());
    }
}

/* ── JSON builder ─────────────────────────────────────────────────────── */
/* Builds glTF JSON with optional mutations applied.  Returns the JSON
 * string length written to buf. */

typedef struct MutFlags {
    bool negative_count;       /* accessor count < 0 */
    bool huge_count;           /* accessor count = huge */
    bool negative_bv_offset;   /* bufferView byteOffset < 0 */
    bool huge_bv_offset;       /* bufferView byteOffset = huge */
    bool negative_acc_offset;  /* accessor byteOffset < 0 */
    bool wrong_comp_type;      /* BYTE instead of FLOAT for positions */
    bool wrong_acc_type;       /* SCALAR instead of VEC3 for positions */
    bool oob_material;         /* material index out of range */
    bool negative_material;    /* material index < 0 */
    bool oob_mesh;             /* node mesh index out of range */
    bool oob_bv;               /* accessor bufferView out of range */
    bool oob_buffer;           /* bufferView buffer out of range */
    bool zero_bv_length;       /* bufferView byteLength = 0 */
    bool tiny_bv_length;       /* bufferView byteLength = 1 */
    bool stride_misalign;      /* byteStride not aligned */
    bool huge_stride;          /* byteStride = 1000000 */
    bool missing_position;     /* no POSITION attribute */
    bool circular_children;    /* node is its own child */
    bool negative_scene;       /* "scene": -1 */
    bool oob_scene;            /* "scene": 9999 */
    bool empty_meshes;         /* empty meshes array */
    bool empty_accessors;      /* empty accessors array */
    bool negative_child;       /* children: [-1] */
    bool oob_child;            /* children: [9999] */
    bool huge_index_count;     /* index count overflow */
    bool missing_uri;          /* buffer has no uri */
    bool short_binary;         /* binary shorter than declared */
    bool wrong_uv_type;        /* VEC3 instead of VEC2 for TEXCOORD_0 */
    bool with_tangent;         /* add TANGENT attribute (VEC4) */
    bool extra_nodes;          /* nodes array has an unreferenced extra node */
} MutFlags;

static int build_json(char *buf, int buf_size, const MutFlags *m,
                      int iter, size_t *out_bin_size)
{
    int n = 0;
    const char *bin_name_fmt = "fuzz_%d.bin";
    char bin_name[64];

    /* Scene index */
    int scene_idx = 0;
    if (m->negative_scene)  scene_idx = -1;
    if (m->oob_scene)       scene_idx = 9999;

    /* Accessor count */
    const char *pos_count = "3";
    if (m->negative_count)  pos_count = "-5";
    if (m->huge_count)      pos_count = "2000000000";

    /* Component type for positions */
    const char *pos_comp = "5126"; /* FLOAT */
    if (m->wrong_comp_type) pos_comp = "5120"; /* BYTE */

    /* Accessor type for positions */
    const char *pos_type = "VEC3";
    if (m->wrong_acc_type) pos_type = "SCALAR";

    /* BufferView 0: byteOffset */
    const char *bv0_offset = "0";
    if (m->negative_bv_offset) bv0_offset = "-100";
    if (m->huge_bv_offset)     bv0_offset = "4294967200";

    /* BufferView 0: byteLength */
    const char *bv0_length = "36";
    if (m->zero_bv_length)  bv0_length = "0";
    if (m->tiny_bv_length)  bv0_length = "1";

    /* BufferView 0: optional byteStride */
    char stride_str[64] = {0};
    if (m->stride_misalign)  SDL_snprintf(stride_str, sizeof(stride_str), ", \"byteStride\": 3");
    if (m->huge_stride)      SDL_snprintf(stride_str, sizeof(stride_str), ", \"byteStride\": 1000000");

    /* Accessor 0: byteOffset */
    const char *acc_offset = "";
    if (m->negative_acc_offset) acc_offset = "\"byteOffset\": -4, ";

    /* BufferView index for accessor */
    const char *bv_idx = "0";
    if (m->oob_bv) bv_idx = "9999";

    /* Buffer index for bufferView */
    const char *buf_idx = "0";
    if (m->oob_buffer) buf_idx = "9999";

    /* Material index on primitive */
    char mat_str[64] = {0};
    if (m->oob_material)       SDL_snprintf(mat_str, sizeof(mat_str), ", \"material\": 9999");
    else if (m->negative_material) SDL_snprintf(mat_str, sizeof(mat_str), ", \"material\": -5");

    /* Mesh index on node */
    const char *mesh_idx = "0";
    if (m->oob_mesh) mesh_idx = "9999";

    /* Children */
    char children_str[64] = {0};
    if (m->circular_children)   SDL_snprintf(children_str, sizeof(children_str), ", \"children\": [0]");
    else if (m->negative_child) SDL_snprintf(children_str, sizeof(children_str), ", \"children\": [-1]");
    else if (m->oob_child)      SDL_snprintf(children_str, sizeof(children_str), ", \"children\": [9999]");

    /* Extra unreferenced node — tests that the parser handles nodes not
     * referenced by any scene.  Appended after the main node entry. */
    const char *extra_node_str = "";
    if (m->extra_nodes) extra_node_str = ", {\"mesh\": 0}";

    /* Index accessor (second accessor, bufferView 1) */
    const char *idx_count = "3";
    if (m->huge_index_count) idx_count = "2000000000";

    /* Buffer URI — byteLength depends on whether tangent data is included.
     * Without tangents: positions(36) + indices(6) = 42
     * With tangents:    positions(36) + tangents(48) + indices(6) = 90 */
    int buf_byte_len = m->with_tangent ? 90 : 42;
    SDL_snprintf(bin_name, sizeof(bin_name), bin_name_fmt, iter);
    char uri_str[128];
    if (m->missing_uri) {
        SDL_snprintf(uri_str, sizeof(uri_str), "\"byteLength\": %d",
                     buf_byte_len);
    } else {
        SDL_snprintf(uri_str, sizeof(uri_str),
                     "\"uri\": \"%s\", \"byteLength\": %d",
                     bin_name, buf_byte_len);
    }

    /* Declared binary size for buffer */
    size_t actual_bin = (size_t)buf_byte_len;
    if (m->short_binary) actual_bin = 4; /* write only 4 bytes */
    *out_bin_size = actual_bin;

    /* Position attribute — the full key:value pair inside attributes {} */
    const char *pos_attr_str = "\"POSITION\": 0";
    if (m->missing_position) pos_attr_str = ""; /* no POSITION */

    /* Optional TEXCOORD_0 attribute — added when testing wrong UV type.
     * Points to accessor 2 with deliberately wrong type (VEC3 not VEC2).
     * Extra accessor and bufferView strings are injected into the normal
     * scene template only. */
    const char *uv_attr_str = "";
    const char *uv_acc_str = "";
    const char *uv_bv_str = "";
    if (m->wrong_uv_type) {
        uv_attr_str = ", \"TEXCOORD_0\": 2";
        /* VEC3 is wrong for TEXCOORD_0 (should be VEC2) */
        uv_acc_str = ",{\"bufferView\": 2, \"componentType\": 5126,"
                     " \"count\": 3, \"type\": \"VEC3\"}";
        uv_bv_str = ",{\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36}";
    }

    /* Optional TANGENT attribute — adds VEC4 accessor for tangent vectors.
     * Tangent data sits at offset 36, indices shift to offset 84.
     * Accessor/bufferView indices depend on whether UV is also present:
     *   no UV: TANGENT = accessor 2, tangent BV = bufferView 2, idx BV = 3
     *   with UV: TANGENT = accessor 3, tangent BV = 3, idx BV stays at 1 */
    const char *tang_attr_str = "";
    const char *tang_acc_str = "";
    const char *tang_bv_str = "";
    const char *tang_idx_bv_offset = "36"; /* default index offset */
    if (m->with_tangent && !m->wrong_uv_type) {
        tang_attr_str = ", \"TANGENT\": 2";
        tang_acc_str = ",{\"bufferView\": 2, \"componentType\": 5126,"
                       " \"count\": 3, \"type\": \"VEC4\"}";
        tang_bv_str = ",{\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 48}";
        tang_idx_bv_offset = "84";
    }

    /* Build JSON */
    if (m->empty_meshes) {
        n = SDL_snprintf(buf, buf_size,
            "{"
            "  \"asset\": {\"version\": \"2.0\"},"
            "  \"scene\": %d,"
            "  \"scenes\": [{\"nodes\": [0]}],"
            "  \"nodes\": [{\"mesh\": %s%s}%s],"
            "  \"meshes\": [],"
            "  \"accessors\": ["
            "    {\"bufferView\": %s, %s\"componentType\": %s,"
            "     \"count\": %s, \"type\": \"%s\"}"
            "  ],"
            "  \"bufferViews\": ["
            "    {\"buffer\": %s, \"byteOffset\": %s, \"byteLength\": %s%s}"
            "  ],"
            "  \"buffers\": [{%s}]"
            "}",
            scene_idx,
            mesh_idx, children_str, extra_node_str,
            bv_idx, acc_offset, pos_comp, pos_count, pos_type,
            buf_idx, bv0_offset, bv0_length, stride_str,
            uri_str);
    } else if (m->empty_accessors) {
        n = SDL_snprintf(buf, buf_size,
            "{"
            "  \"asset\": {\"version\": \"2.0\"},"
            "  \"scene\": %d,"
            "  \"scenes\": [{\"nodes\": [0]}],"
            "  \"nodes\": [{\"mesh\": %s%s}%s],"
            "  \"meshes\": [{\"primitives\": [{"
            "    \"attributes\": {%s}%s"
            "  }]}],"
            "  \"accessors\": [],"
            "  \"bufferViews\": ["
            "    {\"buffer\": %s, \"byteOffset\": %s, \"byteLength\": %s%s}"
            "  ],"
            "  \"buffers\": [{%s}]"
            "}",
            scene_idx,
            mesh_idx, children_str, extra_node_str,
            pos_attr_str, mat_str,
            buf_idx, bv0_offset, bv0_length, stride_str,
            uri_str);
    } else if (m->missing_position) {
        /* Mesh with no POSITION — has indices only */
        n = SDL_snprintf(buf, buf_size,
            "{"
            "  \"asset\": {\"version\": \"2.0\"},"
            "  \"scene\": %d,"
            "  \"scenes\": [{\"nodes\": [0]}],"
            "  \"nodes\": [{\"mesh\": %s%s}%s],"
            "  \"meshes\": [{\"primitives\": [{"
            "    \"attributes\": {},"
            "    \"indices\": 0%s"
            "  }]}],"
            "  \"accessors\": ["
            "    {\"bufferView\": 0, \"componentType\": 5123,"
            "     \"count\": 3, \"type\": \"SCALAR\"}"
            "  ],"
            "  \"bufferViews\": ["
            "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
            "  ],"
            "  \"buffers\": [{%s}]"
            "}",
            scene_idx,
            mesh_idx, children_str, extra_node_str,
            mat_str,
            uri_str);
    } else {
        /* Normal scene: 1 triangle, positions + indices, 1 material.
         * Optional UV accessor/bufferView appended when wrong_uv_type.
         * Optional TANGENT accessor/bufferView appended when with_tangent. */
        n = SDL_snprintf(buf, buf_size,
            "{"
            "  \"asset\": {\"version\": \"2.0\"},"
            "  \"scene\": %d,"
            "  \"scenes\": [{\"nodes\": [0]}],"
            "  \"nodes\": [{\"mesh\": %s%s}%s],"
            "  \"meshes\": [{\"primitives\": [{"
            "    \"attributes\": {%s%s%s},"
            "    \"indices\": 1%s"
            "  }]}],"
            "  \"accessors\": ["
            "    {\"bufferView\": %s, %s\"componentType\": %s,"
            "     \"count\": %s, \"type\": \"%s\"},"
            "    {\"bufferView\": 1, \"componentType\": 5123,"
            "     \"count\": %s, \"type\": \"SCALAR\"}%s%s"
            "  ],"
            "  \"bufferViews\": ["
            "    {\"buffer\": %s, \"byteOffset\": %s, \"byteLength\": %s%s},"
            "    {\"buffer\": 0, \"byteOffset\": %s, \"byteLength\": 6}%s%s"
            "  ],"
            "  \"materials\": [{\"name\": \"mat0\"}],"
            "  \"buffers\": [{%s}]"
            "}",
            scene_idx,
            mesh_idx, children_str, extra_node_str,
            pos_attr_str, uv_attr_str, tang_attr_str, mat_str,
            bv_idx, acc_offset, pos_comp, pos_count, pos_type,
            idx_count, uv_acc_str, tang_acc_str,
            buf_idx, bv0_offset, bv0_length, stride_str,
            tang_idx_bv_offset, uv_bv_str, tang_bv_str,
            uri_str);
    }

    return n;
}

/* ── Scene invariant checker ──────────────────────────────────────────── */
/* After a successful parse, verify the scene struct is internally
 * consistent.  After a failed parse, verify the scene was not partially
 * corrupted (counts should be 0 or fields should be safe to inspect). */

static bool check_scene_invariants(const ForgeGltfScene *scene, bool load_ok,
                                    const Uint8 *canary)
{
    /* Canary check — detect buffer overrun past the scene allocation */
    for (int i = 0; i < CANARY_SIZE; i++) {
        if (canary[i] != CANARY_BYTE) {
            FUZZ_FAIL("canary[%d] corrupted: expected=0x%02X actual=0x%02X",
                      i, CANARY_BYTE, (unsigned)canary[i]);
        }
    }

    /* Counts must be non-negative */
    if (scene->node_count < 0) {
        FUZZ_FAIL("node_count < 0: %d", scene->node_count);
    }
    if (scene->mesh_count < 0) {
        FUZZ_FAIL("mesh_count < 0: %d", scene->mesh_count);
    }
    if (scene->primitive_count < 0) {
        FUZZ_FAIL("primitive_count < 0: %d", scene->primitive_count);
    }
    if (scene->material_count < 0) {
        FUZZ_FAIL("material_count < 0: %d", scene->material_count);
    }
    if (scene->buffer_count < 0) {
        FUZZ_FAIL("buffer_count < 0: %d", scene->buffer_count);
    }
    if (scene->root_node_count < 0) {
        FUZZ_FAIL("root_node_count < 0: %d", scene->root_node_count);
    }
    if (scene->skin_count < 0) {
        FUZZ_FAIL("skin_count < 0: %d", scene->skin_count);
    }
    if (scene->animation_count < 0) {
        FUZZ_FAIL("animation_count < 0: %d", scene->animation_count);
    }

    if (!load_ok) {
        /* Failed load — nothing else to validate.  The key check is that
         * the canary is intact (no memory corruption during the failed parse). */
        return true;
    }

    /* Successful load — validate cross-references */

    /* NULL-array guards: if count > 0 but array is NULL, the parser has a bug
     * (returned success with an inconsistent scene struct). */
    if (scene->primitive_count > 0 && !scene->primitives) {
        FUZZ_FAIL("primitive_count=%d but primitives is NULL",
                  scene->primitive_count);
    }
    if (scene->node_count > 0 && !scene->nodes) {
        FUZZ_FAIL("node_count=%d but nodes is NULL", scene->node_count);
    }
    if (scene->root_node_count > 0 && !scene->root_nodes) {
        FUZZ_FAIL("root_node_count=%d but root_nodes is NULL",
                  scene->root_node_count);
    }
    if (scene->skin_count > 0 && !scene->skins) {
        FUZZ_FAIL("skin_count=%d but skins is NULL", scene->skin_count);
    }
    if (scene->material_count > 0 && !scene->materials) {
        FUZZ_FAIL("material_count=%d but materials is NULL",
                  scene->material_count);
    }
    if (scene->mesh_count > 0 && !scene->meshes) {
        FUZZ_FAIL("mesh_count=%d but meshes is NULL", scene->mesh_count);
    }

    /* Meshes: first_primitive + primitive_count within bounds */
    for (int i = 0; i < scene->mesh_count; i++) {
        const ForgeGltfMesh *gm = &scene->meshes[i];
        if (gm->first_primitive < 0 ||
            gm->first_primitive > scene->primitive_count) {
            FUZZ_FAIL("mesh[%d] first_primitive=%d out of range (0..%d)",
                      i, gm->first_primitive, scene->primitive_count);
        }
        if (gm->primitive_count < 0 ||
            gm->primitive_count > scene->primitive_count - gm->first_primitive) {
            FUZZ_FAIL("mesh[%d] first_primitive=%d + primitive_count=%d "
                      "exceeds scene primitive_count=%d",
                      i, gm->first_primitive, gm->primitive_count,
                      scene->primitive_count);
        }
    }

    /* Primitives: vertex_count > 0 for every primitive */
    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *gp = &scene->primitives[i];
        if (gp->vertex_count == 0) {
            FUZZ_FAIL("primitive[%d] vertex_count == 0", i);
        }
        if (gp->vertices == NULL) {
            FUZZ_FAIL("primitive[%d] vertices == NULL", i);
        }
        /* material_index must be -1 or valid */
        if (gp->material_index != -1) {
            if (gp->material_index < 0 || gp->material_index >= scene->material_count) {
                FUZZ_FAIL("primitive[%d] material_index=%d out of range (0..%d)",
                          i, gp->material_index, scene->material_count - 1);
            }
        }
        /* index_stride must be 0 (no indices), 2, or 4 */
        if (gp->index_stride != 0 && gp->index_stride != 2 && gp->index_stride != 4) {
            FUZZ_FAIL("primitive[%d] index_stride=%u (expected 0, 2, or 4)",
                      i, (unsigned)gp->index_stride);
        }
        /* has_tangents and tangents pointer must be consistent */
        if (gp->has_tangents && !gp->tangents) {
            FUZZ_FAIL("primitive[%d] has_tangents=true but tangents is NULL", i);
        }
        if (!gp->has_tangents && gp->tangents) {
            FUZZ_FAIL("primitive[%d] has_tangents=false but tangents is non-NULL", i);
        }
    }

    /* Nodes: mesh_index and parent are valid */
    for (int i = 0; i < scene->node_count; i++) {
        const ForgeGltfNode *gn = &scene->nodes[i];
        if (gn->mesh_index != -1) {
            if (!scene->meshes || gn->mesh_index < 0 ||
                gn->mesh_index >= scene->mesh_count) {
                FUZZ_FAIL("node[%d] mesh_index=%d out of range (0..%d) "
                          "or meshes is NULL",
                          i, gn->mesh_index, scene->mesh_count - 1);
            }
        }
        if (gn->parent != -1) {
            if (gn->parent < 0 || gn->parent >= scene->node_count) {
                FUZZ_FAIL("node[%d] parent=%d out of range (0..%d)",
                          i, gn->parent, scene->node_count - 1);
            }
        }
        /* Each child index must be valid */
        if (gn->child_count > 0 && !gn->children) {
            FUZZ_FAIL("node[%d] child_count=%d but children is NULL",
                      i, gn->child_count);
        }
        for (int c = 0; c < gn->child_count; c++) {
            if (gn->children[c] < 0 || gn->children[c] >= scene->node_count) {
                FUZZ_FAIL("node[%d] children[%d]=%d out of range (0..%d)",
                          i, c, gn->children[c], scene->node_count - 1);
            }
        }
    }

    /* Root nodes must be valid indices */
    for (int i = 0; i < scene->root_node_count; i++) {
        if (scene->root_nodes[i] < 0 || scene->root_nodes[i] >= scene->node_count) {
            FUZZ_FAIL("root_nodes[%d]=%d out of range (0..%d)",
                      i, scene->root_nodes[i], scene->node_count - 1);
        }
    }

    /* Skins: joint indices must be valid node indices */
    for (int i = 0; i < scene->skin_count; i++) {
        const ForgeGltfSkin *sk = &scene->skins[i];
        if (sk->joint_count > 0 && !sk->joints) {
            FUZZ_FAIL("skin[%d] joint_count=%d but joints is NULL",
                      i, sk->joint_count);
        }
        if (sk->joint_count > 0 && !sk->inverse_bind_matrices) {
            FUZZ_FAIL("skin[%d] joint_count=%d but inverse_bind_matrices is NULL",
                      i, sk->joint_count);
        }
        for (int j = 0; j < sk->joint_count; j++) {
            if (sk->joints[j] < 0 || sk->joints[j] >= scene->node_count) {
                FUZZ_FAIL("skin[%d] joints[%d]=%d out of range (0..%d)",
                          i, j, sk->joints[j], scene->node_count - 1);
            }
        }
    }

    /* Animations: pointers, sampler/channel indices must be in range */
    if (scene->animation_count > 0 && !scene->animations) {
        FUZZ_FAIL("animation_count=%d but animations is NULL",
                  scene->animation_count);
    }
    for (int i = 0; i < scene->animation_count; i++) {
        const ForgeGltfAnimation *clip = &scene->animations[i];
        if (clip->sampler_count > 0 && !clip->samplers) {
            FUZZ_FAIL("animation[%d] sampler_count=%d but samplers is NULL",
                      i, clip->sampler_count);
        }
        if (clip->channel_count > 0 && !clip->channels) {
            FUZZ_FAIL("animation[%d] channel_count=%d but channels is NULL",
                      i, clip->channel_count);
        }
        for (int j = 0; j < clip->channel_count; j++) {
            const ForgeGltfAnimChannel *ch = &clip->channels[j];
            if (ch->sampler_index < 0 || ch->sampler_index >= clip->sampler_count) {
                FUZZ_FAIL("animation[%d] channel[%d] sampler_index=%d out of range (0..%d)",
                          i, j, ch->sampler_index, clip->sampler_count - 1);
            }
            if (ch->target_node < 0 || ch->target_node >= scene->node_count) {
                FUZZ_FAIL("animation[%d] channel[%d] target_node=%d out of range (0..%d)",
                          i, j, ch->target_node, scene->node_count - 1);
            }
        }
    }

    return true;
}

/* ── Apply a single mutation ──────────────────────────────────────────── */

static void apply_mutation(MutFlags *m, MutKind kind)
{
    switch (kind) {
    case MUT_NONE:               break;
    case MUT_NEGATIVE_COUNT:     m->negative_count = true; break;
    case MUT_HUGE_COUNT:         m->huge_count = true; break;
    case MUT_NEGATIVE_BV_OFFSET: m->negative_bv_offset = true; break;
    case MUT_HUGE_BV_OFFSET:     m->huge_bv_offset = true; break;
    case MUT_NEGATIVE_ACC_OFFSET:m->negative_acc_offset = true; break;
    case MUT_WRONG_COMP_TYPE:    m->wrong_comp_type = true; break;
    case MUT_WRONG_ACC_TYPE:     m->wrong_acc_type = true; break;
    case MUT_OOB_MATERIAL_IDX:   m->oob_material = true; break;
    case MUT_NEGATIVE_MATERIAL_IDX: m->negative_material = true; break;
    case MUT_OOB_MESH_IDX:       m->oob_mesh = true; break;
    case MUT_OOB_BV_IDX:         m->oob_bv = true; break;
    case MUT_OOB_BUFFER_IDX:     m->oob_buffer = true; break;
    case MUT_ZERO_BV_LENGTH:     m->zero_bv_length = true; break;
    case MUT_TINY_BV_LENGTH:     m->tiny_bv_length = true; break;
    case MUT_STRIDE_MISALIGN:    m->stride_misalign = true; break;
    case MUT_HUGE_STRIDE:        m->huge_stride = true; break;
    case MUT_MISSING_POSITION:   m->missing_position = true; break;
    case MUT_EXTRA_NODES:        m->extra_nodes = true; break;
    case MUT_CIRCULAR_CHILDREN:  m->circular_children = true; break;
    case MUT_NEGATIVE_SCENE_IDX: m->negative_scene = true; break;
    case MUT_OOB_SCENE_IDX:      m->oob_scene = true; break;
    case MUT_EMPTY_MESHES:       m->empty_meshes = true; break;
    case MUT_EMPTY_ACCESSORS:    m->empty_accessors = true; break;
    case MUT_NEGATIVE_CHILD_IDX: m->negative_child = true; break;
    case MUT_OOB_CHILD_IDX:      m->oob_child = true; break;
    case MUT_HUGE_INDEX_COUNT:   m->huge_index_count = true; break;
    case MUT_MISSING_BUFFER_URI: m->missing_uri = true; break;
    case MUT_SHORT_BINARY:       m->short_binary = true; break;
    case MUT_WRONG_UV_TYPE:      m->wrong_uv_type = true; break;
    case MUT_WITH_TANGENT:       m->with_tangent = true; break;
    case MUT_MULTI_MUT:          break; /* handled by caller */
    case MUT_COUNT:              break;
    }
}

/* ── Must-reject predicate ────────────────────────────────────────────── */
/* Returns true if the mutation flags guarantee the glTF is invalid and
 * the parser MUST reject it (return false from forge_gltf_load).
 * If this returns true but the load succeeded, the parser has a bug. */

static bool must_reject(const MutFlags *m)
{
    /* Negative offsets in accessor or bufferView are spec-invalid.
     * The parser must reject the load, not silently normalize to 0. */
    if (m->negative_acc_offset)  return true;
    if (m->negative_bv_offset)   return true;

    /* Negative accessor count is spec-invalid.  A negative count passed
     * through to callers can cause writes before the start of an array. */
    if (m->negative_count)       return true;

    /* Out-of-range scene index — no valid scene to load. */
    if (m->oob_scene)            return true;
    if (m->negative_scene)       return true;

    /* Missing required buffer data — can't load any geometry. */
    if (m->missing_uri)          return true;

    /* Binary data shorter than declared buffer size. */
    if (m->short_binary)         return true;

    /* Huge counts that would overflow allocation size. */
    if (m->huge_count)           return true;
    if (m->huge_index_count)     return true;

    /* Huge bufferView offset exceeding buffer bounds. */
    if (m->huge_bv_offset)       return true;

    /* Zero-length bufferView is spec-invalid (byteLength required > 0). */
    if (m->zero_bv_length)       return true;

    /* Tiny bufferView (1 byte) — too small for any valid accessor data. */
    if (m->tiny_bv_length)       return true;

    /* OOB bufferView buffer index — references nonexistent buffer. */
    if (m->oob_buffer)           return true;

    /* BYTE for POSITION — parser rejects non-FLOAT component type. */
    if (m->wrong_comp_type)      return true;

    /* SCALAR for POSITION — parser rejects non-VEC3 accessor type. */
    if (m->wrong_acc_type)       return true;

    /* OOB bufferView index — accessor references nonexistent bufferView. */
    if (m->oob_bv)               return true;

    /* Circular children trigger the depth-limit check in
     * compute_world_transforms, which propagates as a load failure. */
    if (m->circular_children)    return true;

    /* Misaligned or huge byteStride — the parser rejects interleaved
     * accessors (byteStride != element_size) and misaligned strides. */
    if (m->stride_misalign)      return true;
    if (m->huge_stride)          return true;

    /* Note: wrong_uv_type is NOT a must-reject — TEXCOORD_0 is optional,
     * so a wrong type simply means the UV attribute is silently skipped.
     * The mesh loads with positions but without UVs, which is valid. */

    return false;
}

/* ── Single fuzz iteration ─────────────────────────────────────────────── */

static bool fuzz_iteration(int iter)
{
    char json_buf[JSON_BUF_SIZE];
    Uint8 bin_buf[BIN_BUF_SIZE];
    MutFlags mflags;
    TempFiles tf;
    ForgeArena gltf_arena;
    size_t bin_size;
    int json_len;
    bool wrote, load_ok;

    g_iteration = iter;
    SDL_memset(&mflags, 0, sizeof(mflags));

    /* Choose mutation(s) */
    bool is_multi = false;
    MutKind primary = (MutKind)(prng_next() % MUT_COUNT);
    if (primary == MUT_MULTI_MUT) {
        /* Apply 2-4 random mutations */
        is_multi = true;
        int n_muts = prng_range(2, 4);
        for (int m = 0; m < n_muts; m++) {
            MutKind extra = (MutKind)(prng_next() % (MUT_COUNT - 1));
            apply_mutation(&mflags, extra);
        }
    } else {
        apply_mutation(&mflags, primary);
    }

    /* Build binary data: 3 vertices (VEC3 float) + optional tangents (VEC4 float)
     * + 3 indices (uint16).
     * Without tangents: positions(36) + indices(6) = 42 bytes
     * With tangents:    positions(36) + tangents(48) + indices(6) = 90 bytes */
    SDL_memset(bin_buf, 0, sizeof(bin_buf));
    {
        float positions[9] = {0,0,0, 1,0,0, 0,1,0};
        Uint16 indices[3] = {0, 1, 2};
        SDL_memcpy(bin_buf, positions, 36);
        if (mflags.with_tangent) {
            float tangents[12] = {1,0,0,1, 1,0,0,1, 1,0,0,1};
            SDL_memcpy(bin_buf + 36, tangents, 48);
            SDL_memcpy(bin_buf + 84, indices, 6);
        } else {
            SDL_memcpy(bin_buf + 36, indices, 6);
        }
    }

    /* Build JSON */
    json_len = build_json(json_buf, JSON_BUF_SIZE, &mflags, iter, &bin_size);
    if (json_len <= 0 || json_len >= JSON_BUF_SIZE) {
        SDL_Log("WARN: JSON overflow at iteration %d, skipping", iter);
        return true; /* not a fuzzer failure */
    }

    /* Write temp files — treat staging failure as a real error so the
     * fuzzer never exits "0 issues" without actually testing anything. */
    wrote = write_temp_files(json_buf, (size_t)json_len,
                              bin_buf, bin_size, iter, &tf);
    if (!wrote) {
        remove_temp_files(&tf);
        FUZZ_FAIL("temp file write failed at iteration %d "
                  "(is temp dir writable?)", iter);
    }

    /* Allocate scene with canary.  Use SDL_malloc + manual canary rather
     * than stack allocation so we can detect overruns. */
    size_t scene_alloc = sizeof(ForgeGltfScene) + CANARY_SIZE;
    Uint8 *raw = (Uint8 *)SDL_malloc(scene_alloc);
    if (!raw) {
        remove_temp_files(&tf);
        return true; /* OOM, skip */
    }

    SDL_memset(raw, 0, sizeof(ForgeGltfScene));
    SDL_memset(raw + sizeof(ForgeGltfScene), CANARY_BYTE, CANARY_SIZE);
    Uint8 *canary = raw + sizeof(ForgeGltfScene);

    ForgeGltfScene *scene = (ForgeGltfScene *)raw;

    /* Parse — suppress parser log spam (expected rejections) */
    gltf_arena = forge_arena_create(0);
    if (!gltf_arena.first) {
        SDL_free(raw);
        remove_temp_files(&tf);
        return true; /* OOM, skip */
    }
    suppress_logs();
    load_ok = forge_gltf_load(tf.gltf_path, scene, &gltf_arena);
    restore_logs();

    /* Check that mutations which produce invalid glTF are actually rejected.
     * Skip this check for multi-mutations since combining mutations can
     * change the JSON structure in ways that bypass the target field entirely
     * (e.g. empty_accessors + negative_count — no accessor exists to be
     * negative, so the load may legitimately succeed). */
    bool ok = true;
    if (load_ok && !is_multi && must_reject(&mflags)) {
        SDL_Log("FUZZ FAIL: parser accepted input that should be rejected "
                "(mutation=%d produced invalid glTF but load_ok=true)",
                (int)primary);
        SDL_Log("  seed=%u  iteration=%d",
                (unsigned)g_seed, g_iteration);
        ok = false;
    }

    /* Check invariants */
    if (ok) {
        ok = check_scene_invariants(scene, load_ok, canary);
    }

    /* Cleanup */
    forge_arena_destroy(&gltf_arena);
    SDL_free(raw);
    remove_temp_files(&tf);

    return ok;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    g_seed = DEFAULT_SEED;
    if (argc > 1) {
        unsigned long parsed = strtoul(argv[1], NULL, 0);
        g_seed = (Uint32)parsed;
    }

    int iterations = FORGE_FUZZ_ITERATIONS;
    if (argc > 2) {
        int parsed = atoi(argv[2]);
        if (parsed > 0) iterations = parsed;
    }

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* Prefer SDL_GetPrefPath (guaranteed writable) over SDL_GetBasePath
     * (may be read-only in CI or app-bundle layouts). */
    g_temp_path = SDL_GetPrefPath("forge-gpu", "fuzz_gltf");
    if (g_temp_path) {
        g_temp_path_owned = true;
    } else {
        g_temp_path = (char *)SDL_GetBasePath();
        g_temp_path_owned = false;
    }
    if (!g_temp_path) {
        SDL_Log("Cannot find writable temp directory: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Log("=== glTF Parser Fuzz Harness ===");
    SDL_Log("  seed:       0x%08X (%u)", (unsigned)g_seed, (unsigned)g_seed);
    SDL_Log("  iterations: %d", iterations);
    SDL_Log("  mutations:  %d types", (int)MUT_COUNT);
    SDL_Log("  canary:     %d bytes (0x%02X)", CANARY_SIZE, CANARY_BYTE);
    SDL_Log("");

    prng_seed(g_seed);

    int progress_interval = iterations / 20; /* report every 5% */
    if (progress_interval < 1) progress_interval = 1;

    int issues_found = 0;

    for (int i = 0; i < iterations; i++) {
        if (i > 0 && (i % progress_interval) == 0) {
            SDL_Log("  ... %d / %d iterations (%.0f%%)",
                    i, iterations, (double)i / (double)iterations * 100.0);
        }

        if (!fuzz_iteration(i)) {
            issues_found++;
            SDL_Log("  Issue at iteration %d (seed=0x%08X)",
                    i, (unsigned)g_seed);
            /* Continue fuzzing — collect all issues rather than stopping
             * at the first one.  Cap at 20 to avoid flooding output. */
            if (issues_found >= 20) {
                SDL_Log("  ... stopping after 20 issues");
                break;
            }
        }
    }

    SDL_Log("");
    if (issues_found > 0) {
        SDL_Log("=== FOUND %d ISSUES in %d iterations ===", issues_found, iterations);
        SDL_Log("Reproduce: %s 0x%08X %d", argv[0], (unsigned)g_seed, iterations);
        if (g_temp_path_owned) SDL_free(g_temp_path);
        SDL_Quit();
        return 1;
    }

    SDL_Log("=== PASSED: %d iterations, 0 issues ===", iterations);
    if (g_temp_path_owned) SDL_free(g_temp_path);
    SDL_Quit();
    return 0;
}
