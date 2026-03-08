/*
 * forge-mesh-tool — CLI mesh processing tool
 *
 * Reads an OBJ file, generates an indexed mesh via vertex deduplication,
 * optimizes the index/vertex layout for GPU efficiency, generates MikkTSpace
 * tangent vectors, produces LOD levels via mesh simplification, and writes
 * a binary .fmesh file with a .meta.json sidecar.
 *
 * Built as a reusable project-level tool at tools/mesh/ — not scoped to
 * a single lesson.  The Python pipeline plugin invokes it as a subprocess.
 * Asset Lesson 03 teaches how it works.
 *
 * Usage:
 *   forge-mesh-tool <input.obj> <output.fmesh> [options]
 *     --lod-levels 1.0,0.5,0.25   LOD target ratios (comma-separated)
 *     --no-deduplicate             Skip vertex deduplication
 *     --no-tangents                Skip tangent generation
 *     --no-optimize                Skip index/vertex optimization
 *     --verbose                    Print statistics
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "obj/forge_obj.h"
#include "math/forge_math.h"
#include "meshoptimizer.h"
#include "mikktspace.h"

/* ── Constants ───────────────────────────────────────────────────────────── */

#define FMESH_MAGIC         "FMSH"
#define FMESH_MAGIC_SIZE    4
#define FMESH_VERSION       1
#define FMESH_FLAG_TANGENTS (1u << 0)

#define MAX_LOD_LEVELS      8
#define DEFAULT_LOD_RATIOS  { 1.0f }

#define HEADER_SIZE         32  /* bytes */
#define LOD_ENTRY_SIZE      12  /* bytes per LOD */

/* meshopt_optimizeOverdraw threshold: allow up to 5% vertex-cache degradation
 * to improve overdraw.  1.0 = no overdraw optimization; higher = more
 * aggressive overdraw reduction at the cost of cache efficiency. */
#define OVERDRAW_THRESHOLD  1.05f

/* meshopt_simplify target error: maximum geometric deviation allowed during
 * edge collapse, expressed in object-space units. Smaller values preserve
 * more detail but may prevent reaching the target index count. */
#define SIMPLIFY_TARGET_ERROR 0.01f

/* Vertex stride depends on whether tangents are present:
 *   No tangents: position(3) + normal(3) + uv(2) = 8 floats = 32 bytes
 *   Tangents:    position(3) + normal(3) + uv(2) + tangent(4) = 12 floats = 48 bytes */
#define VERTEX_STRIDE_NO_TAN  (sizeof(float) * 8)
#define VERTEX_STRIDE_TAN     (sizeof(float) * 12)

/* ── Output vertex layout ────────────────────────────────────────────────── */

typedef struct MeshVertex {
    float position[3];
    float normal[3];
    float uv[2];
    float tangent[4]; /* xyz = tangent direction, w = sign/handedness */
} MeshVertex;

/* ── LOD entry for the binary header ─────────────────────────────────────── */

typedef struct LodEntry {
    unsigned int index_count;
    unsigned int index_offset;  /* byte offset from start of index data */
    float        target_error;
} LodEntry;

/* ── Command-line options ────────────────────────────────────────────────── */

typedef struct ToolOptions {
    const char *input_path;                  /* path to input .obj file */
    const char *output_path;                 /* path to output .fmesh file */
    float       lod_ratios[MAX_LOD_LEVELS];  /* target triangle ratios per LOD (0,1] */
    int         lod_count;                   /* number of active LOD levels */
    bool        deduplicate;                 /* run vertex deduplication */
    bool        generate_tangents;           /* run MikkTSpace tangent generation */
    bool        optimize;                    /* run index/vertex optimization passes */
    bool        verbose;                     /* print per-stage statistics */
} ToolOptions;

/* ── Forward declarations ────────────────────────────────────────────────── */

static bool parse_args(int argc, char *argv[], ToolOptions *opts);
static bool process_mesh(const ToolOptions *opts);
static bool write_fmesh(const char *path, const MeshVertex *vertices,
                         unsigned int vertex_count, unsigned int vertex_stride,
                         const unsigned int *indices,
                         const LodEntry *lods, int lod_count,
                         unsigned int flags);
static bool write_meta_json(const char *fmesh_path, const char *source_path,
                             unsigned int vertex_count, unsigned int vertex_stride,
                             bool has_tangents, const LodEntry *lods, int lod_count,
                             const float *lod_ratios,
                             unsigned int original_vertex_count);
static bool generate_tangents(MeshVertex *vertices, unsigned int vertex_count,
                               const unsigned int *indices, unsigned int index_count);

/* ── MikkTSpace callbacks ────────────────────────────────────────────────── */

/* MikkTSpace needs access to both the vertex buffer and index buffer.
 * We pass this context through the user-data pointer so the callbacks
 * can look up vertex data by triangle index. */
typedef struct MikkContext {
    MeshVertex         *vertices;    /* vertex buffer (tangents written in-place) */
    const unsigned int *indices;     /* index buffer for triangle lookup */
    unsigned int        index_count; /* total number of indices (triangles * 3) */
} MikkContext;

static int mikk_get_num_faces(const SMikkTSpaceContext *ctx)
{
    const MikkContext *mc = (const MikkContext *)ctx->m_pUserData;
    return (int)(mc->index_count / 3);
}

static int mikk_get_num_verts_of_face(const SMikkTSpaceContext *ctx, int face)
{
    (void)ctx;
    (void)face;
    return 3; /* all faces are triangles */
}

static void mikk_get_position(const SMikkTSpaceContext *ctx, float out[],
                               int face, int vert)
{
    const MikkContext *mc = (const MikkContext *)ctx->m_pUserData;
    unsigned int idx = mc->indices[face * 3 + vert];
    out[0] = mc->vertices[idx].position[0];
    out[1] = mc->vertices[idx].position[1];
    out[2] = mc->vertices[idx].position[2];
}

static void mikk_get_normal(const SMikkTSpaceContext *ctx, float out[],
                              int face, int vert)
{
    const MikkContext *mc = (const MikkContext *)ctx->m_pUserData;
    unsigned int idx = mc->indices[face * 3 + vert];
    out[0] = mc->vertices[idx].normal[0];
    out[1] = mc->vertices[idx].normal[1];
    out[2] = mc->vertices[idx].normal[2];
}

static void mikk_get_texcoord(const SMikkTSpaceContext *ctx, float out[],
                                int face, int vert)
{
    const MikkContext *mc = (const MikkContext *)ctx->m_pUserData;
    unsigned int idx = mc->indices[face * 3 + vert];
    out[0] = mc->vertices[idx].uv[0];
    out[1] = mc->vertices[idx].uv[1];
}

/* setTSpaceBasic receives the tangent direction and its sign (handedness).
 * The bitangent can be reconstructed in the shader:
 *   bitangent = cross(normal, tangent.xyz) * tangent.w */
static void mikk_set_tspace_basic(const SMikkTSpaceContext *ctx,
                                   const float tangent[], float sign,
                                   int face, int vert)
{
    MikkContext *mc = (MikkContext *)ctx->m_pUserData;
    unsigned int idx = mc->indices[face * 3 + vert];
    mc->vertices[idx].tangent[0] = tangent[0];
    mc->vertices[idx].tangent[1] = tangent[1];
    mc->vertices[idx].tangent[2] = tangent[2];
    mc->vertices[idx].tangent[3] = sign;
}

/* ── Argument parsing ────────────────────────────────────────────────────── */

static bool parse_lod_ratios(const char *str, float *ratios, int *count)
{
    *count = 0;
    const char *p = str;

    while (*p) {
        if (*count == MAX_LOD_LEVELS) {
            SDL_Log("Error: at most %d LOD levels are supported",
                    MAX_LOD_LEVELS);
            return false;
        }
        char *end = NULL;
        float val = strtof(p, &end);
        if (end == p) {
            SDL_Log("Error: invalid LOD ratio near '%s'", p);
            return false;
        }
        if (val <= 0.0f || val > 1.0f) {
            SDL_Log("Error: LOD ratio %.3f out of range (0, 1]", (double)val);
            return false;
        }
        ratios[(*count)++] = val;
        p = end;
        if (*p == ',') {
            p++;
        } else if (*p != '\0') {
            SDL_Log("Error: unexpected character '%c' in LOD ratios", *p);
            return false;
        }
    }

    if (*count == 0) {
        SDL_Log("Error: no LOD ratios specified");
        return false;
    }
    if (ratios[0] != 1.0f) {
        SDL_Log("Error: first LOD ratio must be 1.0 (full detail)");
        return false;
    }
    for (int i = 1; i < *count; i++) {
        if (ratios[i] >= ratios[i - 1]) {
            SDL_Log("Error: LOD ratios must be strictly descending "
                    "(ratio %d: %.3f >= ratio %d: %.3f)",
                    i, (double)ratios[i], i - 1, (double)ratios[i - 1]);
            return false;
        }
    }
    return true;
}

static bool parse_args(int argc, char *argv[], ToolOptions *opts)
{
    /* Defaults */
    opts->input_path        = NULL;
    opts->output_path       = NULL;
    opts->lod_ratios[0]     = 1.0f;
    opts->lod_count         = 1;
    opts->deduplicate       = true;
    opts->generate_tangents = true;
    opts->optimize          = true;
    opts->verbose           = false;

    int positional = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lod-levels") == 0) {
            if (i + 1 >= argc) {
                SDL_Log("Error: --lod-levels requires an argument");
                return false;
            }
            i++;
            if (!parse_lod_ratios(argv[i], opts->lod_ratios, &opts->lod_count)) {
                return false;
            }
        } else if (strcmp(argv[i], "--no-deduplicate") == 0) {
            opts->deduplicate = false;
        } else if (strcmp(argv[i], "--no-tangents") == 0) {
            opts->generate_tangents = false;
        } else if (strcmp(argv[i], "--no-optimize") == 0) {
            opts->optimize = false;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = true;
        } else if (argv[i][0] == '-') {
            SDL_Log("Error: unknown option '%s'", argv[i]);
            return false;
        } else {
            if (positional == 0)      opts->input_path  = argv[i];
            else if (positional == 1) opts->output_path = argv[i];
            else {
                SDL_Log("Error: unexpected argument '%s'", argv[i]);
                return false;
            }
            positional++;
        }
    }

    if (!opts->input_path || !opts->output_path) {
        SDL_Log("Usage: forge-mesh-tool <input.obj> <output.fmesh> [options]");
        SDL_Log("  --lod-levels 1.0,0.5,0.25   LOD target ratios");
        SDL_Log("  --no-deduplicate             Skip vertex deduplication");
        SDL_Log("  --no-tangents                Skip tangent generation");
        SDL_Log("  --no-optimize                Skip index/vertex optimization");
        SDL_Log("  --verbose                    Print statistics");
        return false;
    }

    return true;
}

/* ── Tangent generation ──────────────────────────────────────────────────── */

static bool generate_tangents(MeshVertex *vertices, unsigned int vertex_count,
                               const unsigned int *indices, unsigned int index_count)
{
    /* Zero out tangent data before MikkTSpace writes to it */
    for (unsigned int i = 0; i < vertex_count; i++) {
        vertices[i].tangent[0] = 0.0f;
        vertices[i].tangent[1] = 0.0f;
        vertices[i].tangent[2] = 0.0f;
        vertices[i].tangent[3] = 1.0f;
    }

    MikkContext mc;
    mc.vertices    = vertices;
    mc.indices     = indices;
    mc.index_count = index_count;

    SMikkTSpaceInterface iface;
    memset(&iface, 0, sizeof(iface));
    iface.m_getNumFaces          = mikk_get_num_faces;
    iface.m_getNumVerticesOfFace = mikk_get_num_verts_of_face;
    iface.m_getPosition          = mikk_get_position;
    iface.m_getNormal            = mikk_get_normal;
    iface.m_getTexCoord          = mikk_get_texcoord;
    iface.m_setTSpaceBasic       = mikk_set_tspace_basic;

    SMikkTSpaceContext context;
    context.m_pInterface = &iface;
    context.m_pUserData  = &mc;

    if (!genTangSpaceDefault(&context)) {
        SDL_Log("Error: MikkTSpace tangent generation failed");
        return false;
    }
    return true;
}

/* ── Main processing pipeline ────────────────────────────────────────────── */

static bool process_mesh(const ToolOptions *opts)
{
    /* ── Step 1: Load OBJ ────────────────────────────────────────────────
     * The OBJ parser gives us a flat de-indexed array: every 3 vertices
     * form one triangle with no shared vertices. We need to deduplicate
     * these into a proper indexed mesh for the GPU. */
    ForgeObjMesh obj_mesh;
    if (!forge_obj_load(opts->input_path, &obj_mesh)) {
        SDL_Log("Error: failed to load OBJ '%s'", opts->input_path);
        return false;
    }

    unsigned int original_vertex_count = obj_mesh.vertex_count;
    if (original_vertex_count == 0 || original_vertex_count % 3 != 0) {
        SDL_Log("Error: OBJ has %u vertices (expected multiple of 3)",
                original_vertex_count);
        forge_obj_free(&obj_mesh);
        return false;
    }

    if (opts->verbose) {
        SDL_Log("Input: %u vertices (%u triangles) from '%s'",
                original_vertex_count, original_vertex_count / 3,
                opts->input_path);
    }

    /* ── Step 2: Convert OBJ vertices to our output layout ───────────────
     * Copy position/normal/uv from ForgeObjVertex into MeshVertex.
     * Tangent data starts zeroed and gets filled by MikkTSpace later. */
    MeshVertex *raw_vertices = (MeshVertex *)SDL_calloc(
        original_vertex_count, sizeof(MeshVertex));
    if (!raw_vertices) {
        SDL_Log("Error: allocation failed for %u vertices", original_vertex_count);
        forge_obj_free(&obj_mesh);
        return false;
    }

    for (unsigned int i = 0; i < original_vertex_count; i++) {
        raw_vertices[i].position[0] = obj_mesh.vertices[i].position.x;
        raw_vertices[i].position[1] = obj_mesh.vertices[i].position.y;
        raw_vertices[i].position[2] = obj_mesh.vertices[i].position.z;
        raw_vertices[i].normal[0]   = obj_mesh.vertices[i].normal.x;
        raw_vertices[i].normal[1]   = obj_mesh.vertices[i].normal.y;
        raw_vertices[i].normal[2]   = obj_mesh.vertices[i].normal.z;
        raw_vertices[i].uv[0]       = obj_mesh.vertices[i].uv.x;
        raw_vertices[i].uv[1]       = obj_mesh.vertices[i].uv.y;
    }

    /* OBJ data no longer needed — we've copied everything into MeshVertex */
    forge_obj_free(&obj_mesh);

    /* ── Step 3: Vertex deduplication ────────────────────────────────────
     * meshopt_generateVertexRemap identifies unique vertices by comparing
     * all attributes byte-for-byte. Vertices that are bitwise identical
     * get mapped to the same index, producing a compact index buffer.
     *
     * We use the full MeshVertex stride here (including the zeroed tangent
     * fields) so that the comparison is consistent. Since all tangents are
     * zero at this point, they don't affect deduplication — two vertices
     * with the same position/normal/uv will still merge correctly.
     *
     * When --no-deduplicate is set, we skip remap and create an identity
     * index buffer instead (each vertex referenced exactly once). */
    MeshVertex   *vertices = NULL;
    unsigned int *indices  = NULL;
    unsigned int  vertex_count;
    unsigned int  index_count = original_vertex_count;

    if (opts->deduplicate) {
        unsigned int *remap = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * original_vertex_count);
        if (!remap) {
            SDL_Log("Error: allocation failed for remap table");
            SDL_free(raw_vertices);
            return false;
        }

        size_t unique_vertex_count = meshopt_generateVertexRemap(
            remap, NULL, original_vertex_count,
            raw_vertices, original_vertex_count, sizeof(MeshVertex));

        vertices = (MeshVertex *)SDL_calloc(
            unique_vertex_count, sizeof(MeshVertex));
        indices = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * original_vertex_count);

        if (!vertices || !indices) {
            SDL_Log("Error: allocation failed for remapped buffers");
            SDL_free(remap);
            SDL_free(raw_vertices);
            SDL_free(vertices);
            SDL_free(indices);
            return false;
        }

        meshopt_remapVertexBuffer(vertices, raw_vertices, original_vertex_count,
                                   sizeof(MeshVertex), remap);
        meshopt_remapIndexBuffer(indices, NULL, original_vertex_count, remap);

        SDL_free(remap);
        SDL_free(raw_vertices);
        vertex_count = (unsigned int)unique_vertex_count;
    } else {
        /* No dedup: use the raw vertices directly with identity indices */
        vertices = raw_vertices;
        raw_vertices = NULL; /* transfer ownership */
        vertex_count = original_vertex_count;

        indices = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * original_vertex_count);
        if (!indices) {
            SDL_Log("Error: allocation failed for identity index buffer");
            SDL_free(vertices);
            return false;
        }
        for (unsigned int i = 0; i < original_vertex_count; i++) {
            indices[i] = i;
        }
    }

    if (opts->verbose) {
        if (opts->deduplicate) {
            float dedup_ratio = (float)vertex_count / (float)original_vertex_count;
            SDL_Log("Deduplication: %u -> %u vertices (%.1f%% of original)",
                    original_vertex_count, vertex_count,
                    (double)(dedup_ratio * 100.0f));
        } else {
            SDL_Log("Deduplication: skipped (--no-deduplicate)");
        }
    }

    /* ── Step 4: Index/vertex optimization ───────────────────────────────
     * Three passes that reorder data for better GPU performance:
     *
     * 1. Vertex cache: Reorder triangles so consecutive triangles share
     *    vertices that are still in the GPU's post-transform cache.
     *
     * 2. Overdraw: Reorder triangles to minimize pixels shaded multiple
     *    times. Requires position data for occlusion estimation.
     *
     * 3. Vertex fetch: Reorder the vertex buffer itself so vertices are
     *    accessed in roughly sequential order, improving memory locality. */
    if (opts->optimize) {
        /* Pass 1: Vertex cache optimization */
        meshopt_optimizeVertexCache(indices, indices, index_count, vertex_count);

        /* Pass 2: Overdraw optimization — needs positions and the stride
         * between consecutive position entries in the vertex buffer */
        meshopt_optimizeOverdraw(indices, indices, index_count,
                                  &vertices[0].position[0],
                                  vertex_count, sizeof(MeshVertex),
                                  OVERDRAW_THRESHOLD);

        /* Pass 3: Vertex fetch optimization — reorders the vertex buffer
         * and updates indices to match */
        meshopt_optimizeVertexFetch(vertices, indices, index_count,
                                     vertices, vertex_count, sizeof(MeshVertex));

        if (opts->verbose) {
            SDL_Log("Optimization: vertex cache + overdraw + fetch complete");
        }
    }

    /* ── Step 5: Tangent generation ──────────────────────────────────────
     * MikkTSpace computes per-vertex tangent vectors from the mesh's
     * positions, normals, and UVs. The tangent frame (tangent, bitangent,
     * normal) is essential for normal mapping — it defines the coordinate
     * system that transforms tangent-space normal map values into world
     * space.
     *
     * We run this after optimization because the index buffer is now in
     * its final order, and MikkTSpace uses the indices to identify shared
     * vertices and average their tangent contributions. */
    if (opts->generate_tangents) {
        if (!generate_tangents(vertices, vertex_count, indices, index_count)) {
            SDL_free(vertices);
            SDL_free(indices);
            return false;
        }

        if (opts->verbose) {
            SDL_Log("Tangent generation: complete (MikkTSpace)");
        }
    }

    /* ── Step 6: LOD generation ──────────────────────────────────────────
     * Each LOD level simplifies the base mesh to a target triangle count.
     * meshopt_simplify uses edge collapse to progressively remove vertices
     * while preserving the mesh's visual shape as much as possible.
     *
     * LOD 0 is always the full-detail mesh. Additional LODs are generated
     * for each ratio < 1.0 in the --lod-levels list. */
    LodEntry lods[MAX_LOD_LEVELS];
    memset(lods, 0, sizeof(lods));

    /* We'll concatenate all LOD index buffers into one allocation.
     * Worst case: each LOD has as many indices as the base mesh.
     * Guard against integer overflow for very large meshes. */
    if (index_count > SDL_MAX_UINT32 / (unsigned int)opts->lod_count) {
        SDL_Log("Error: index count %u too large for %d LOD levels",
                index_count, opts->lod_count);
        SDL_free(vertices);
        SDL_free(indices);
        return false;
    }
    unsigned int max_total_indices = index_count * (unsigned int)opts->lod_count;
    unsigned int *all_indices = (unsigned int *)SDL_malloc(
        sizeof(unsigned int) * max_total_indices);
    if (!all_indices) {
        SDL_Log("Error: allocation failed for LOD index data");
        SDL_free(vertices);
        SDL_free(indices);
        return false;
    }

    unsigned int total_index_count = 0;

    for (int lod = 0; lod < opts->lod_count; lod++) {
        float ratio = opts->lod_ratios[lod];

        if (ratio >= 1.0f) {
            /* LOD 0 (full detail): copy the base indices directly */
            memcpy(all_indices + total_index_count, indices,
                   sizeof(unsigned int) * index_count);

            lods[lod].index_count  = index_count;
            lods[lod].index_offset = total_index_count * (unsigned int)sizeof(unsigned int);
            lods[lod].target_error = 0.0f;

            if (opts->verbose) {
                SDL_Log("LOD %d: %u indices (ratio %.2f, full detail)",
                        lod, index_count, (double)ratio);
            }
        } else {
            /* Simplified LOD: target a reduced triangle count */
            unsigned int target_index_count =
                (unsigned int)((float)index_count * ratio);
            /* Round down to a multiple of 3 (complete triangles) */
            target_index_count = (target_index_count / 3) * 3;
            if (target_index_count < 3) target_index_count = 3;

            float result_error = 0.0f;

            unsigned int simplified_count = (unsigned int)meshopt_simplify(
                all_indices + total_index_count,
                indices, index_count,
                &vertices[0].position[0],
                vertex_count,
                sizeof(MeshVertex),
                target_index_count,
                SIMPLIFY_TARGET_ERROR,
                0,      /* options: default */
                &result_error);

            lods[lod].index_count  = simplified_count;
            lods[lod].index_offset = total_index_count * (unsigned int)sizeof(unsigned int);
            lods[lod].target_error = result_error;

            if (opts->verbose) {
                SDL_Log("LOD %d: %u indices (ratio %.2f, target %u, error %.6f)",
                        lod, simplified_count, (double)ratio,
                        target_index_count, (double)result_error);
            }
        }

        total_index_count += lods[lod].index_count;
    }

    /* Base indices no longer needed — LOD data is in all_indices */
    SDL_free(indices);

    /* ── Step 7: Determine vertex stride and flags ───────────────────────
     * The output format stores only the attributes that are present.
     * Without tangents, each vertex is 32 bytes (pos + normal + uv).
     * With tangents, each vertex is 48 bytes (pos + normal + uv + tangent). */
    unsigned int flags = 0;
    unsigned int vertex_stride;

    if (opts->generate_tangents) {
        flags |= FMESH_FLAG_TANGENTS;
        vertex_stride = (unsigned int)VERTEX_STRIDE_TAN;
    } else {
        vertex_stride = (unsigned int)VERTEX_STRIDE_NO_TAN;
    }

    /* ── Step 8: Write binary .fmesh ─────────────────────────────────────*/
    bool ok = write_fmesh(opts->output_path, vertices, vertex_count,
                           vertex_stride, all_indices,
                           lods, opts->lod_count, flags);
    if (!ok) {
        SDL_free(vertices);
        SDL_free(all_indices);
        return false;
    }

    /* ── Step 9: Write .meta.json sidecar ────────────────────────────────*/
    ok = write_meta_json(opts->output_path, opts->input_path,
                          vertex_count, vertex_stride,
                          opts->generate_tangents,
                          lods, opts->lod_count, opts->lod_ratios,
                          original_vertex_count);

    if (ok && opts->verbose) {
        SDL_Log("Output: '%s' (%u vertices, %u LODs, stride %u bytes)",
                opts->output_path, vertex_count, opts->lod_count, vertex_stride);
    }

    SDL_free(vertices);
    SDL_free(all_indices);
    return ok;
}

/* ── Binary .fmesh writer ────────────────────────────────────────────────── */

/* Write a uint32 in little-endian byte order using standard C I/O.
 * This avoids depending on SDL_IOStream / SDL_WriteU32LE, which are not
 * available in the SDL3 shim used for console-only builds. */
static bool write_u32_le(FILE *fp, uint32_t val)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(val);
    bytes[1] = (uint8_t)(val >> 8);
    bytes[2] = (uint8_t)(val >> 16);
    bytes[3] = (uint8_t)(val >> 24);
    return fwrite(bytes, 1, 4, fp) == 4;
}

static bool write_fmesh(const char *path, const MeshVertex *vertices,
                         unsigned int vertex_count, unsigned int vertex_stride,
                         const unsigned int *indices,
                         const LodEntry *lods, int lod_count,
                         unsigned int flags)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        SDL_Log("Error: failed to open '%s' for writing", path);
        return false;
    }

    /* ── Header (32 bytes) ───────────────────────────────────────────────
     *   magic:         4 bytes  "FMSH"
     *   version:       4 bytes  uint32
     *   vertex_count:  4 bytes  uint32
     *   vertex_stride: 4 bytes  uint32
     *   lod_count:     4 bytes  uint32
     *   flags:         4 bytes  uint32
     *   reserved:      8 bytes  padding */
    uint8_t reserved[8];
    memset(reserved, 0, sizeof(reserved));

    bool ok = true;
    ok = ok && (fwrite(FMESH_MAGIC, 1, FMESH_MAGIC_SIZE, fp) == FMESH_MAGIC_SIZE);
    ok = ok && write_u32_le(fp, FMESH_VERSION);
    ok = ok && write_u32_le(fp, vertex_count);
    ok = ok && write_u32_le(fp, vertex_stride);
    ok = ok && write_u32_le(fp, (uint32_t)lod_count);
    ok = ok && write_u32_le(fp, flags);
    ok = ok && (fwrite(reserved, 1, sizeof(reserved), fp) == sizeof(reserved));

    if (!ok) {
        SDL_Log("Error: failed writing .fmesh header");
        fclose(fp);
        return false;
    }

    /* ── LOD entries (12 bytes each) ─────────────────────────────────────
     * Each entry stores the index count, byte offset into the index data
     * section, and the simplification error metric. */
    for (int i = 0; i < lod_count; i++) {
        ok = ok && write_u32_le(fp, lods[i].index_count);
        ok = ok && write_u32_le(fp, lods[i].index_offset);

        /* Write float as raw bytes in little-endian. */
        uint32_t error_bits;
        memcpy(&error_bits, &lods[i].target_error, sizeof(error_bits));
        ok = ok && write_u32_le(fp, error_bits);
    }

    if (!ok) {
        SDL_Log("Error: failed writing LOD entries");
        fclose(fp);
        return false;
    }

    /* ── Vertex data ─────────────────────────────────────────────────────
     * Write each vertex using only the fields indicated by vertex_stride.
     * Without tangents we write 8 floats; with tangents we write 12. */
    for (unsigned int i = 0; i < vertex_count; i++) {
        if (fwrite(&vertices[i], 1, (size_t)vertex_stride, fp) != (size_t)vertex_stride) {
            SDL_Log("Error: failed writing vertex %u", i);
            fclose(fp);
            return false;
        }
    }

    /* ── Index data ──────────────────────────────────────────────────────
     * All LOD index buffers are concatenated. Each index is a uint32
     * referencing the shared vertex buffer above. */
    unsigned int total_indices = 0;
    for (int i = 0; i < lod_count; i++) {
        total_indices += lods[i].index_count;
    }

    for (unsigned int i = 0; i < total_indices; i++) {
        if (!write_u32_le(fp, indices[i])) {
            SDL_Log("Error: failed writing index %u", i);
            fclose(fp);
            return false;
        }
    }

    fclose(fp);
    return true;
}

/* ── Meta JSON sidecar writer ────────────────────────────────────────────── */

static bool write_meta_json(const char *fmesh_path, const char *source_path,
                             unsigned int vertex_count, unsigned int vertex_stride,
                             bool has_tangents, const LodEntry *lods, int lod_count,
                             const float *lod_ratios,
                             unsigned int original_vertex_count)
{
    /* Build the .meta.json path by replacing the .fmesh extension.
     * Example: "model.fmesh" -> "model.meta.json"
     * This matches the Python pipeline convention where the sidecar sits
     * next to the output with the same stem. */
    size_t path_len = strlen(fmesh_path);

    /* Find the last '.' to strip the extension */
    const char *dot = strrchr(fmesh_path, '.');
    size_t stem_len = dot ? (size_t)(dot - fmesh_path) : path_len;

    size_t meta_len = stem_len + 11; /* ".meta.json\0" */
    char *meta_path = (char *)SDL_malloc(meta_len);
    if (!meta_path) {
        SDL_Log("Error: allocation failed for meta path");
        return false;
    }
    snprintf(meta_path, meta_len, "%.*s.meta.json", (int)stem_len, fmesh_path);

    FILE *fp = fopen(meta_path, "w");
    if (!fp) {
        SDL_Log("Error: failed to open '%s' for writing", meta_path);
        SDL_free(meta_path);
        return false;
    }

    /* Extract just the filename from the source path for the JSON.
     * Reject filenames with characters that would break JSON strings. */
    const char *source_name = source_path;
    const char *slash = strrchr(source_path, '/');
    if (slash) source_name = slash + 1;
    const char *backslash = strrchr(source_name, '\\');
    if (backslash) source_name = backslash + 1;

    for (const char *p = source_name; *p; p++) {
        if (*p == '"' || *p == '\\' || (unsigned char)*p < 0x20) {
            SDL_Log("Error: source filename contains characters that "
                    "cannot be safely embedded in JSON: '%s'", source_name);
            fclose(fp);
            SDL_free(meta_path);
            return false;
        }
    }

    float dedup_ratio = (original_vertex_count > 0)
        ? (float)vertex_count / (float)original_vertex_count
        : 0.0f;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"source\": \"%s\",\n", source_name);
    fprintf(fp, "  \"vertex_count\": %u,\n", vertex_count);
    fprintf(fp, "  \"vertex_stride\": %u,\n", vertex_stride);
    fprintf(fp, "  \"has_tangents\": %s,\n", has_tangents ? "true" : "false");
    fprintf(fp, "  \"lods\": [\n");

    for (int i = 0; i < lod_count; i++) {
        fprintf(fp, "    {\"level\": %d, \"index_count\": %u, "
                     "\"target_ratio\": %.2f, \"error\": %.6f}",
                i, lods[i].index_count,
                (double)lod_ratios[i],
                (double)lods[i].target_error);
        if (i + 1 < lod_count) fprintf(fp, ",");
        fprintf(fp, "\n");
    }

    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"original_vertex_count\": %u,\n", original_vertex_count);
    fprintf(fp, "  \"dedup_ratio\": %.2f\n", (double)dedup_ratio);
    fprintf(fp, "}\n");

    fclose(fp);
    SDL_Log("Wrote metadata: '%s'", meta_path);
    SDL_free(meta_path);
    return true;
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* SDL_Init is required for SDL_Log and SDL_LoadFile (used by the OBJ
     * parser).  We don't need video or audio — just the base subsystem. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    ToolOptions opts;
    if (!parse_args(argc, argv, &opts)) {
        SDL_Quit();
        return 1;
    }

    bool ok = process_mesh(&opts);

    SDL_Quit();
    return ok ? 0 : 1;
}
