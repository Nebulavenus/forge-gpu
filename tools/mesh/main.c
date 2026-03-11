/*
 * forge-mesh-tool — CLI mesh processing tool
 *
 * Reads an OBJ or glTF/GLB file, generates an indexed mesh via vertex
 * deduplication, optimizes the index/vertex layout for GPU efficiency,
 * generates MikkTSpace tangent vectors, produces LOD levels via mesh
 * simplification, and writes a binary .fmesh v2 file with submesh tables,
 * a .fmat material sidecar, and a .meta.json metadata sidecar.
 *
 * v2 format adds multi-primitive support: a single .fmesh can contain
 * multiple submeshes (one per glTF primitive), each with its own material
 * index.  LOD generation and optimization operate per-submesh.
 *
 * Built as a reusable project-level tool at tools/mesh/ — not scoped to
 * a single lesson.  The Python pipeline plugin invokes it as a subprocess.
 * Asset Lesson 03 teaches how it works.
 *
 * Usage:
 *   forge-mesh-tool <input> <output.fmesh> [options]
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
/* stdio.h no longer needed — all I/O uses SDL_IOStream */
#include <stdlib.h>
#include <string.h>

#include "obj/forge_obj.h"
#include "gltf/forge_gltf.h"
#include "math/forge_math.h"
#include "meshoptimizer.h"
#include "mikktspace.h"

/* ── Constants ───────────────────────────────────────────────────────────── */

#define FMESH_MAGIC         "FMSH"
#define FMESH_MAGIC_SIZE    4
#define FMESH_VERSION       2
#define FMESH_FLAG_TANGENTS (1u << 0)

#define MAX_LOD_LEVELS      8
#define MAX_SUBMESHES       64
#define DEFAULT_LOD_RATIOS  { 1.0f }

#define HEADER_SIZE         32  /* bytes */

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

/* ── Submesh entry — tracks one primitive's index range and material ────── */

typedef struct SubmeshEntry {
    unsigned int first_index;     /* first index in the merged index buffer */
    unsigned int index_count;     /* number of indices for this submesh */
    int          material_index;  /* index into scene materials, -1 = none */
} SubmeshEntry;

/* ── LOD-submesh entry for the v2 binary format ──────────────────────────── */
/* One entry per (LOD, submesh) pair.  Stored as lod_count * submesh_count
 * entries in row-major order: all submeshes for LOD 0, then LOD 1, etc. */

typedef struct LodSubmeshEntry {
    unsigned int index_count;     /* number of indices for this submesh at this LOD */
    unsigned int index_offset;    /* byte offset into the index data section */
    int          material_index;  /* material for this submesh */
} LodSubmeshEntry;

/* ── Input format detection ──────────────────────────────────────────────── */

typedef enum InputFormat {
    FORMAT_OBJ,   /* Wavefront .obj (de-indexed, needs deduplication) */
    FORMAT_GLTF,  /* glTF .gltf or .glb (already indexed) */
} InputFormat;

/* Return the input format based on file extension, or -1 if unsupported.
 * Caller must check for -1 and handle the error. */
static int detect_format(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return -1; /* no extension = unsupported */

    if (SDL_strcasecmp(dot, ".obj") == 0) {
        return FORMAT_OBJ;
    }
    if (SDL_strcasecmp(dot, ".gltf") == 0 || SDL_strcasecmp(dot, ".glb") == 0) {
        return FORMAT_GLTF;
    }
    return -1; /* unsupported extension */
}

/* ── Command-line options ────────────────────────────────────────────────── */

typedef struct ToolOptions {
    const char *input_path;                  /* path to input mesh file */
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
static bool write_fmesh_v2(const char *path, const MeshVertex *vertices,
                            unsigned int vertex_count, unsigned int vertex_stride,
                            const unsigned int *indices,
                            int lod_count, int submesh_count,
                            const LodSubmeshEntry *lod_submeshes,
                            const float *lod_errors,
                            unsigned int flags);
static bool write_fmat(const char *fmesh_path, const ForgeGltfScene *scene);
static bool write_meta_json(const char *fmesh_path, const char *source_path,
                             unsigned int vertex_count, unsigned int vertex_stride,
                             bool has_tangents, int lod_count,
                             const float *lod_ratios, int submesh_count,
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
        SDL_Log("Usage: forge-mesh-tool <input> <output.fmesh> [options]");
        SDL_Log("  Supported input formats: .obj, .gltf, .glb");
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
    unsigned int i;
    for (i = 0; i < vertex_count; i++) {
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

/* ── Binary helper ───────────────────────────────────────────────────────── */

/* Write a uint32 in little-endian byte order to an SDL I/O stream.
 * SDL_IOStream handles locale-independent output and consistent
 * newline behavior across platforms. */
static bool write_u32_le(SDL_IOStream *io, uint32_t val)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(val);
    bytes[1] = (uint8_t)(val >> 8);
    bytes[2] = (uint8_t)(val >> 16);
    bytes[3] = (uint8_t)(val >> 24);
    return SDL_WriteIO(io, bytes, 4) == 4;
}

/* Write a signed int32 in little-endian byte order. Material indices
 * can be -1 (no material), so we need signed support. */
static bool write_i32_le(SDL_IOStream *io, int32_t val)
{
    uint32_t uval;
    memcpy(&uval, &val, sizeof(uval));
    return write_u32_le(io, uval);
}

/* ── Filename extraction helper ──────────────────────────────────────────── */

/* Return a pointer to the filename portion of a path (after the last
 * directory separator).  Returns the original pointer if no separator. */
static const char *basename_from_path(const char *path)
{
    const char *name = path;
    const char *slash = strrchr(path, '/');
    if (slash) name = slash + 1;
    const char *backslash = strrchr(name, '\\');
    if (backslash) name = backslash + 1;
    return name;
}

/* ── Load OBJ into MeshVertex + index buffers ───────────────────────────── */

static bool load_obj(const char *path, bool deduplicate, bool verbose,
                     MeshVertex **out_vertices, unsigned int *out_vertex_count,
                     unsigned int **out_indices, unsigned int *out_index_count,
                     unsigned int *out_original_vertex_count)
{
    /* The OBJ parser gives us a flat de-indexed array: every 3 vertices
     * form one triangle with no shared vertices. We need to deduplicate
     * these into a proper indexed mesh for the GPU. */
    ForgeObjMesh obj_mesh;
    if (!forge_obj_load(path, &obj_mesh)) {
        SDL_Log("Error: failed to load OBJ '%s'", path);
        return false;
    }

    unsigned int original_vertex_count = obj_mesh.vertex_count;
    if (original_vertex_count == 0 || original_vertex_count % 3 != 0) {
        SDL_Log("Error: OBJ has %u vertices (expected multiple of 3)",
                original_vertex_count);
        forge_obj_free(&obj_mesh);
        return false;
    }
    *out_original_vertex_count = original_vertex_count;

    if (verbose) {
        SDL_Log("Input: %u vertices (%u triangles) from '%s'",
                original_vertex_count, original_vertex_count / 3, path);
    }

    /* Convert OBJ vertices to our output layout.
     * Copy position/normal/uv from ForgeObjVertex into MeshVertex.
     * Tangent data starts zeroed and gets filled by MikkTSpace later. */
    MeshVertex *raw_vertices = (MeshVertex *)SDL_calloc(
        original_vertex_count, sizeof(MeshVertex));
    if (!raw_vertices) {
        SDL_Log("Error: allocation failed for %u vertices", original_vertex_count);
        forge_obj_free(&obj_mesh);
        return false;
    }

    {
        unsigned int i;
        for (i = 0; i < original_vertex_count; i++) {
            raw_vertices[i].position[0] = obj_mesh.vertices[i].position.x;
            raw_vertices[i].position[1] = obj_mesh.vertices[i].position.y;
            raw_vertices[i].position[2] = obj_mesh.vertices[i].position.z;
            raw_vertices[i].normal[0]   = obj_mesh.vertices[i].normal.x;
            raw_vertices[i].normal[1]   = obj_mesh.vertices[i].normal.y;
            raw_vertices[i].normal[2]   = obj_mesh.vertices[i].normal.z;
            raw_vertices[i].uv[0]       = obj_mesh.vertices[i].uv.x;
            raw_vertices[i].uv[1]       = obj_mesh.vertices[i].uv.y;
        }
    }

    /* OBJ data no longer needed — we've copied everything into MeshVertex */
    forge_obj_free(&obj_mesh);

    /* Vertex deduplication: meshopt_generateVertexRemap identifies unique
     * vertices by comparing all attributes byte-for-byte. Since tangents
     * are all zero at this point, they don't affect deduplication. */
    if (deduplicate) {
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

        *out_vertices = (MeshVertex *)SDL_calloc(
            unique_vertex_count, sizeof(MeshVertex));
        *out_indices = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * original_vertex_count);

        if (!*out_vertices || !*out_indices) {
            SDL_Log("Error: allocation failed for remapped buffers");
            SDL_free(remap);
            SDL_free(raw_vertices);
            SDL_free(*out_vertices);
            SDL_free(*out_indices);
            *out_vertices = NULL;
            *out_indices = NULL;
            return false;
        }

        meshopt_remapVertexBuffer(*out_vertices, raw_vertices,
                                   original_vertex_count,
                                   sizeof(MeshVertex), remap);
        meshopt_remapIndexBuffer(*out_indices, NULL,
                                  original_vertex_count, remap);

        SDL_free(remap);
        SDL_free(raw_vertices);
        *out_vertex_count = (unsigned int)unique_vertex_count;
        *out_index_count  = original_vertex_count;
    } else {
        /* No dedup: use the raw vertices directly with identity indices */
        *out_vertices = raw_vertices;
        *out_vertex_count = original_vertex_count;
        *out_index_count  = original_vertex_count;

        *out_indices = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * original_vertex_count);
        if (!*out_indices) {
            SDL_Log("Error: allocation failed for identity index buffer");
            SDL_free(*out_vertices);
            *out_vertices = NULL;
            return false;
        }
        {
            unsigned int i;
            for (i = 0; i < original_vertex_count; i++) {
                (*out_indices)[i] = i;
            }
        }
    }

    if (verbose) {
        if (deduplicate) {
            float dedup_ratio = (float)*out_vertex_count / (float)original_vertex_count;
            SDL_Log("Deduplication: %u -> %u vertices (%.1f%% of original)",
                    original_vertex_count, *out_vertex_count,
                    (double)(dedup_ratio * 100.0f));
        } else {
            SDL_Log("Deduplication: skipped (--no-deduplicate)");
        }
    }

    return true;
}

/* ── Load glTF/GLB into MeshVertex + index buffers (multi-primitive) ───── */

static bool load_gltf(const char *path, bool deduplicate, bool verbose,
                      MeshVertex **out_vertices, unsigned int *out_vertex_count,
                      unsigned int **out_indices, unsigned int *out_index_count,
                      unsigned int *out_original_vertex_count,
                      bool *out_has_tangents,
                      SubmeshEntry *out_submeshes, int *out_submesh_count,
                      ForgeGltfScene *out_scene, ForgeArena *arena)
{
    if (!forge_gltf_load(path, out_scene, arena)) {
        SDL_Log("Error: failed to load glTF '%s'", path);
        return false;
    }

    if (out_scene->primitive_count == 0) {
        SDL_Log("Error: glTF scene has no primitives");
        return false;
    }

    if (out_scene->primitive_count > MAX_SUBMESHES) {
        SDL_Log("Error: glTF has %d primitives (max %d)",
                out_scene->primitive_count, MAX_SUBMESHES);
        return false;
    }

    /* ── First pass: count total vertices and indices across all primitives
     * so we can allocate merged buffers upfront.  Use 64-bit accumulators
     * to detect overflow before truncating to 32-bit. */
    Uint64 total_verts_64 = 0;
    Uint64 total_indices_64 = 0;
    bool any_has_tangents = false;
    bool all_have_tangents = true;
    int prim_count = out_scene->primitive_count;
    int p;

    for (p = 0; p < prim_count; p++) {
        ForgeGltfPrimitive *prim = &out_scene->primitives[p];
        total_verts_64 += prim->vertex_count;

        if (prim->index_count > 0 && prim->indices) {
            total_indices_64 += prim->index_count;
        } else {
            /* Non-indexed primitive: will generate identity indices */
            total_indices_64 += prim->vertex_count;
        }

        if (prim->has_tangents) {
            any_has_tangents = true;
        } else {
            all_have_tangents = false;
        }
    }

    if (total_verts_64 > SDL_MAX_UINT32 || total_indices_64 > SDL_MAX_UINT32) {
        SDL_Log("Error: merged vertex/index counts overflow 32-bit "
                "(verts=%" SDL_PRIu64 ", indices=%" SDL_PRIu64 ")",
                total_verts_64, total_indices_64);
        return false;
    }

    unsigned int total_verts   = (unsigned int)total_verts_64;
    unsigned int total_indices = (unsigned int)total_indices_64;

    if (total_verts == 0) {
        SDL_Log("Error: glTF scene has no vertices across %d primitives",
                prim_count);
        return false;
    }

    *out_original_vertex_count = total_verts;
    /* If any primitive is missing tangents, MikkTSpace should regenerate
     * for the whole mesh — so we need ALL primitives to have tangents. */
    *out_has_tangents = all_have_tangents;

    if (verbose) {
        SDL_Log("Input: %u vertices, %u indices (%u triangles) across "
                "%d primitives from '%s'",
                total_verts, total_indices, total_indices / 3,
                prim_count, path);
        if (all_have_tangents) {
            SDL_Log("  glTF provides tangent vectors on all primitives");
        } else if (any_has_tangents) {
            SDL_Log("  glTF provides tangent vectors on some primitives "
                    "(MikkTSpace will regenerate for consistency)");
        }
    }

    /* ── Allocate merged buffers ─────────────────────────────────────────── */
    MeshVertex *verts = (MeshVertex *)SDL_calloc(total_verts, sizeof(MeshVertex));
    if (!verts) {
        SDL_Log("Error: allocation failed for %u vertices", total_verts);
        return false;
    }

    unsigned int *idx_buf = (unsigned int *)SDL_malloc(
        sizeof(unsigned int) * total_indices);
    if (!idx_buf) {
        SDL_Log("Error: allocation failed for %u indices", total_indices);
        SDL_free(verts);
        return false;
    }

    /* ── Second pass: copy vertex/index data from each primitive ─────────
     * Each primitive's indices are offset by the cumulative vertex count
     * so they reference the correct position in the merged vertex buffer. */
    unsigned int vert_offset = 0;
    unsigned int idx_offset = 0;
    *out_submesh_count = 0;

    for (p = 0; p < prim_count; p++) {
        ForgeGltfPrimitive *prim = &out_scene->primitives[p];
        unsigned int pv_count = prim->vertex_count;
        unsigned int pi_count;
        unsigned int i;

        /* Copy vertices into the merged buffer */
        for (i = 0; i < pv_count; i++) {
            unsigned int dst = vert_offset + i;
            verts[dst].position[0] = prim->vertices[i].position.x;
            verts[dst].position[1] = prim->vertices[i].position.y;
            verts[dst].position[2] = prim->vertices[i].position.z;
            verts[dst].normal[0]   = prim->vertices[i].normal.x;
            verts[dst].normal[1]   = prim->vertices[i].normal.y;
            verts[dst].normal[2]   = prim->vertices[i].normal.z;
            verts[dst].uv[0]       = prim->vertices[i].uv.x;
            verts[dst].uv[1]       = prim->vertices[i].uv.y;

            /* Only copy tangents into the merged buffer when ALL primitives
             * supply them.  If some are missing, tangents will be regenerated
             * by MikkTSpace later.  Including partial tangent data in the
             * remap key would prevent deduplication of otherwise-identical
             * vertices across primitive boundaries. */
            if (all_have_tangents && prim->has_tangents && prim->tangents) {
                verts[dst].tangent[0] = prim->tangents[i].x;
                verts[dst].tangent[1] = prim->tangents[i].y;
                verts[dst].tangent[2] = prim->tangents[i].z;
                verts[dst].tangent[3] = prim->tangents[i].w;
            }
        }

        /* Copy indices, adjusting by the vertex offset so they reference
         * the correct vertices in the merged buffer */
        if (prim->index_count > 0 && prim->indices) {
            pi_count = prim->index_count;
            if (prim->index_stride == 2) {
                const Uint16 *src = (const Uint16 *)prim->indices;
                for (i = 0; i < pi_count; i++) {
                    idx_buf[idx_offset + i] = (unsigned int)src[i] + vert_offset;
                }
            } else if (prim->index_stride == 4) {
                const Uint32 *src = (const Uint32 *)prim->indices;
                for (i = 0; i < pi_count; i++) {
                    idx_buf[idx_offset + i] = (unsigned int)src[i] + vert_offset;
                }
            } else {
                SDL_Log("Error: primitive %d has unexpected index stride %u "
                        "(expected 2 or 4)", p, prim->index_stride);
                SDL_free(verts);
                SDL_free(idx_buf);
                return false;
            }
        } else {
            /* Non-indexed primitive: generate identity indices offset by
             * the cumulative vertex count */
            pi_count = pv_count;
            for (i = 0; i < pi_count; i++) {
                idx_buf[idx_offset + i] = vert_offset + i;
            }
        }

        /* Validate: this primitive's index count must be a multiple of 3 */
        if (pi_count == 0 || pi_count % 3 != 0) {
            SDL_Log("Error: primitive %d has %u indices "
                    "(expected non-zero multiple of 3)", p, pi_count);
            SDL_free(verts);
            SDL_free(idx_buf);
            return false;
        }

        /* Record submesh entry for this primitive */
        out_submeshes[*out_submesh_count].first_index    = idx_offset;
        out_submeshes[*out_submesh_count].index_count    = pi_count;
        out_submeshes[*out_submesh_count].material_index = prim->material_index;
        (*out_submesh_count)++;

        if (verbose) {
            SDL_Log("  Primitive %d: %u verts, %u indices, material %d",
                    p, pv_count, pi_count, prim->material_index);
        }

        vert_offset += pv_count;
        idx_offset  += pi_count;
    }

    /* NOTE: We do NOT free the scene here — caller needs the material data
     * for write_fmat().  Caller is responsible for forge_arena_destroy(). */

    /* ── Optional deduplication on the merged buffer ─────────────────────
     * glTF vertices are already indexed per-primitive, but deduplication
     * can still merge vertices shared across primitive boundaries. */
    if (deduplicate) {
        unsigned int *remap = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * total_verts);
        if (!remap) {
            SDL_Log("Error: allocation failed for remap table");
            SDL_free(verts);
            SDL_free(idx_buf);
            return false;
        }

        size_t unique = meshopt_generateVertexRemap(
            remap, idx_buf, total_indices,
            verts, total_verts, sizeof(MeshVertex));

        MeshVertex *deduped = (MeshVertex *)SDL_calloc(
            unique, sizeof(MeshVertex));
        unsigned int *new_indices = (unsigned int *)SDL_malloc(
            sizeof(unsigned int) * total_indices);

        if (!deduped || !new_indices) {
            SDL_Log("Error: allocation failed for remapped buffers");
            SDL_free(remap);
            SDL_free(verts);
            SDL_free(idx_buf);
            SDL_free(deduped);
            SDL_free(new_indices);
            return false;
        }

        meshopt_remapVertexBuffer(deduped, verts, total_verts,
                                   sizeof(MeshVertex), remap);
        meshopt_remapIndexBuffer(new_indices, idx_buf, total_indices, remap);

        SDL_free(remap);
        SDL_free(verts);
        SDL_free(idx_buf);
        verts   = deduped;
        idx_buf = new_indices;
        *out_vertex_count = (unsigned int)unique;
        *out_index_count  = total_indices;

        if (verbose) {
            float ratio = (float)unique / (float)total_verts;
            SDL_Log("Deduplication: %u -> %zu vertices (%.1f%% of original)",
                    total_verts, unique, (double)(ratio * 100.0f));
        }
    } else {
        *out_vertex_count = total_verts;
        *out_index_count  = total_indices;

        if (verbose) {
            SDL_Log("Deduplication: skipped (--no-deduplicate)");
        }
    }

    *out_vertices = verts;
    *out_indices  = idx_buf;
    return true;
}

/* ── Main processing pipeline ────────────────────────────────────────────── */

static bool process_mesh(const ToolOptions *opts)
{
    int format = detect_format(opts->input_path);
    if (format < 0) {
        SDL_Log("Error: unsupported input format for '%s' "
                "(expected .obj, .gltf, or .glb)", opts->input_path);
        return false;
    }

    MeshVertex   *vertices = NULL;
    unsigned int *indices  = NULL;
    unsigned int  vertex_count = 0;
    unsigned int  index_count  = 0;
    unsigned int  original_vertex_count = 0;
    bool          gltf_has_tangents = false;

    /* Submesh tracking — OBJ produces 1 submesh, glTF can produce many */
    SubmeshEntry  submeshes[MAX_SUBMESHES];
    int           submesh_count = 0;

    /* glTF scene kept alive for material extraction (write_fmat). */
    ForgeGltfScene *gltf_scene = NULL;
    ForgeArena      gltf_arena;
    bool            has_gltf_scene = false;

    memset(submeshes, 0, sizeof(submeshes));

    /* ── Step 1–3: Load and deduplicate ──────────────────────────────────
     * OBJ: de-indexed -> deduplicate -> indexed mesh (single submesh)
     * glTF: already indexed -> optional deduplication (multi-submesh) */
    if (format == FORMAT_GLTF) {
        gltf_scene = (ForgeGltfScene *)SDL_calloc(1, sizeof(ForgeGltfScene));
        if (!gltf_scene) {
            SDL_Log("Error: allocation failed for glTF scene");
            return false;
        }
        gltf_arena = forge_arena_create(0);
        if (!gltf_arena.first) {
            SDL_Log("Error: arena creation failed (out of memory)");
            SDL_free(gltf_scene);
            return false;
        }
        if (!load_gltf(opts->input_path, opts->deduplicate, opts->verbose,
                       &vertices, &vertex_count, &indices, &index_count,
                       &original_vertex_count, &gltf_has_tangents,
                       submeshes, &submesh_count, gltf_scene, &gltf_arena)) {
            forge_arena_destroy(&gltf_arena);
            SDL_free(gltf_scene);
            return false;
        }
        has_gltf_scene = true;
    } else {
        if (!load_obj(opts->input_path, opts->deduplicate, opts->verbose,
                      &vertices, &vertex_count, &indices, &index_count,
                      &original_vertex_count)) {
            return false;
        }
        /* OBJ produces a single submesh covering all indices */
        submesh_count = 1;
        submeshes[0].first_index    = 0;
        submeshes[0].index_count    = index_count;
        submeshes[0].material_index = -1;
    }

    /* ── Step 4: Index/vertex optimization ───────────────────────────────
     * Vertex cache and overdraw run per-submesh because each submesh is
     * a separate draw call.  Vertex fetch runs on the full merged buffer
     * because all submeshes share one vertex buffer. */
    if (opts->optimize) {
        int s;
        for (s = 0; s < submesh_count; s++) {
            unsigned int *sub_idx = indices + submeshes[s].first_index;
            unsigned int  sub_cnt = submeshes[s].index_count;

            /* Pass 1: Vertex cache optimization — reorder triangles so
             * consecutive triangles share vertices in the post-transform cache */
            meshopt_optimizeVertexCache(sub_idx, sub_idx, sub_cnt, vertex_count);

            /* Pass 2: Overdraw optimization — reorder triangles to reduce
             * pixels shaded multiple times */
            meshopt_optimizeOverdraw(sub_idx, sub_idx, sub_cnt,
                                      &vertices[0].position[0],
                                      vertex_count, sizeof(MeshVertex),
                                      OVERDRAW_THRESHOLD);
        }

        /* Pass 3: Vertex fetch optimization — reorders the vertex buffer
         * so vertices are accessed sequentially, improving memory locality.
         * Runs on the entire merged buffer (all submeshes share vertices). */
        meshopt_optimizeVertexFetch(vertices, indices, index_count,
                                     vertices, vertex_count, sizeof(MeshVertex));

        if (opts->verbose) {
            SDL_Log("Optimization: per-submesh cache/overdraw + "
                    "global fetch complete");
        }
    }

    /* ── Step 5: Tangent generation ──────────────────────────────────────
     * MikkTSpace computes per-vertex tangent vectors from the mesh's
     * positions, normals, and UVs. We run this after optimization because
     * the index buffer is now in its final order. */
    bool has_tangents = gltf_has_tangents;

    if (opts->generate_tangents && !gltf_has_tangents) {
        if (!generate_tangents(vertices, vertex_count, indices, index_count)) {
            SDL_free(vertices);
            SDL_free(indices);
            if (has_gltf_scene) {
                forge_arena_destroy(&gltf_arena);
                SDL_free(gltf_scene);
            }
            return false;
        }
        has_tangents = true;

        if (opts->verbose) {
            SDL_Log("Tangent generation: complete (MikkTSpace)");
        }
    } else if (gltf_has_tangents && opts->verbose) {
        SDL_Log("Tangent generation: skipped (glTF provides tangent vectors)");
    }

    /* ── Step 6: LOD generation (per-submesh per LOD) ────────────────────
     * For each LOD level, simplify each submesh independently. The result
     * is a 2D table: lod_count x submesh_count LodSubmeshEntry values.
     *
     * LOD 0 is always the full-detail mesh. Additional LODs simplify each
     * submesh to a reduced triangle count using meshopt_simplify. */
    int total_lod_submeshes = opts->lod_count * submesh_count;
    LodSubmeshEntry *lod_submeshes = (LodSubmeshEntry *)SDL_calloc(
        (size_t)total_lod_submeshes, sizeof(LodSubmeshEntry));
    float *lod_errors = (float *)SDL_calloc(
        (size_t)opts->lod_count, sizeof(float));

    if (!lod_submeshes || !lod_errors) {
        SDL_Log("Error: allocation failed for LOD-submesh table");
        SDL_free(lod_submeshes);
        SDL_free(lod_errors);
        SDL_free(vertices);
        SDL_free(indices);
        if (has_gltf_scene) {
            forge_arena_destroy(&gltf_arena);
            SDL_free(gltf_scene);
        }
        return false;
    }

    /* Worst case: each LOD has as many indices as the base mesh.
     * Guard against byte-offset overflow: the serialized submesh
     * index_offset is a 32-bit byte offset, so total LOD index data
     * must fit in UINT32_MAX bytes. */
    Uint64 max_total_bytes = (Uint64)index_count
                           * (Uint64)opts->lod_count
                           * (Uint64)sizeof(unsigned int);
    if (max_total_bytes > SDL_MAX_UINT32) {
        SDL_Log("Error: LOD index data would exceed 32-bit byte offset "
                "(%u indices x %d LODs x 4 bytes = %" SDL_PRIu64 ")",
                index_count, opts->lod_count, max_total_bytes);
        SDL_free(lod_submeshes);
        SDL_free(lod_errors);
        SDL_free(vertices);
        SDL_free(indices);
        if (has_gltf_scene) {
            forge_arena_destroy(&gltf_arena);
            SDL_free(gltf_scene);
        }
        return false;
    }

    unsigned int max_total_indices = index_count * (unsigned int)opts->lod_count;
    unsigned int *all_indices = (unsigned int *)SDL_malloc(
        sizeof(unsigned int) * max_total_indices);
    if (!all_indices) {
        SDL_Log("Error: allocation failed for LOD index data");
        SDL_free(lod_submeshes);
        SDL_free(lod_errors);
        SDL_free(vertices);
        SDL_free(indices);
        if (has_gltf_scene) {
            forge_arena_destroy(&gltf_arena);
            SDL_free(gltf_scene);
        }
        return false;
    }

    unsigned int total_output_indices = 0;

    {
        int lod;
        for (lod = 0; lod < opts->lod_count; lod++) {
            float ratio = opts->lod_ratios[lod];
            float max_error_this_lod = 0.0f;
            int s;

            for (s = 0; s < submesh_count; s++) {
                int entry_idx = lod * submesh_count + s;
                unsigned int sub_first = submeshes[s].first_index;
                unsigned int sub_count = submeshes[s].index_count;
                unsigned int *dest = all_indices + total_output_indices;

                if (ratio >= 1.0f) {
                    /* Full detail: copy this submesh's indices directly */
                    memcpy(dest, indices + sub_first,
                           sizeof(unsigned int) * sub_count);

                    lod_submeshes[entry_idx].index_count  = sub_count;
                    lod_submeshes[entry_idx].index_offset =
                        total_output_indices * (unsigned int)sizeof(unsigned int);
                    lod_submeshes[entry_idx].material_index =
                        submeshes[s].material_index;
                } else {
                    /* Simplified: target a reduced triangle count for
                     * this submesh */
                    unsigned int target = (unsigned int)(
                        (float)sub_count * ratio);
                    /* Round down to a multiple of 3 (complete triangles) */
                    target = (target / 3) * 3;
                    if (target < 3) target = 3;

                    float result_error = 0.0f;

                    unsigned int simplified = (unsigned int)meshopt_simplify(
                        dest,
                        indices + sub_first, sub_count,
                        &vertices[0].position[0],
                        vertex_count,
                        sizeof(MeshVertex),
                        target,
                        SIMPLIFY_TARGET_ERROR,
                        0,      /* options: default */
                        &result_error);

                    lod_submeshes[entry_idx].index_count  = simplified;
                    lod_submeshes[entry_idx].index_offset =
                        total_output_indices * (unsigned int)sizeof(unsigned int);
                    lod_submeshes[entry_idx].material_index =
                        submeshes[s].material_index;

                    if (result_error > max_error_this_lod) {
                        max_error_this_lod = result_error;
                    }
                }

                if (opts->verbose) {
                    SDL_Log("LOD %d submesh %d: %u indices (ratio %.2f)",
                            lod, s, lod_submeshes[entry_idx].index_count,
                            (double)ratio);
                }

                total_output_indices += lod_submeshes[entry_idx].index_count;
            }

            lod_errors[lod] = max_error_this_lod;
        }
    }

    /* Base indices no longer needed — LOD data is in all_indices */
    SDL_free(indices);

    /* ── Step 7: Determine vertex stride and flags ───────────────────────
     * The output format stores only the attributes that are present.
     * Without tangents, each vertex is 32 bytes (pos + normal + uv).
     * With tangents, each vertex is 48 bytes (pos + normal + uv + tangent). */
    unsigned int flags = 0;
    unsigned int vertex_stride;

    if (has_tangents) {
        flags |= FMESH_FLAG_TANGENTS;
        vertex_stride = (unsigned int)VERTEX_STRIDE_TAN;
    } else {
        vertex_stride = (unsigned int)VERTEX_STRIDE_NO_TAN;
    }

    /* ── Step 8: Write binary .fmesh v2 ──────────────────────────────────*/
    bool ok = write_fmesh_v2(opts->output_path, vertices, vertex_count,
                              vertex_stride, all_indices,
                              opts->lod_count, submesh_count,
                              lod_submeshes, lod_errors, flags);
    if (!ok) {
        SDL_free(vertices);
        SDL_free(all_indices);
        SDL_free(lod_submeshes);
        SDL_free(lod_errors);
        if (has_gltf_scene) {
            forge_arena_destroy(&gltf_arena);
            SDL_free(gltf_scene);
        }
        return false;
    }

    /* ── Step 9: Write .fmat material sidecar (glTF only) ────────────────*/
    if (has_gltf_scene && gltf_scene->material_count > 0) {
        ok = write_fmat(opts->output_path, gltf_scene);
        if (!ok) {
            SDL_Log("Error: failed to write .fmat sidecar");
            SDL_free(vertices);
            SDL_free(all_indices);
            SDL_free(lod_submeshes);
            SDL_free(lod_errors);
            if (has_gltf_scene) {
                forge_arena_destroy(&gltf_arena);
                SDL_free(gltf_scene);
            }
            return false;
        }
    }

    /* ── Step 10: Write .meta.json sidecar ───────────────────────────────*/
    ok = write_meta_json(opts->output_path, opts->input_path,
                          vertex_count, vertex_stride, has_tangents,
                          opts->lod_count, opts->lod_ratios, submesh_count,
                          original_vertex_count);

    if (ok && opts->verbose) {
        SDL_Log("Output: '%s' (%u vertices, %d submeshes, %d LODs, "
                "stride %u bytes)",
                opts->output_path, vertex_count, submesh_count,
                opts->lod_count, vertex_stride);
    }

    SDL_free(vertices);
    SDL_free(all_indices);
    SDL_free(lod_submeshes);
    SDL_free(lod_errors);
    if (has_gltf_scene) {
        forge_arena_destroy(&gltf_arena);
        SDL_free(gltf_scene);
    }
    return ok;
}

/* ── Binary .fmesh v2 writer ─────────────────────────────────────────────── */

static bool write_fmesh_v2(const char *path, const MeshVertex *vertices,
                            unsigned int vertex_count, unsigned int vertex_stride,
                            const unsigned int *indices,
                            int lod_count, int submesh_count,
                            const LodSubmeshEntry *lod_submeshes,
                            const float *lod_errors,
                            unsigned int flags)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                path, SDL_GetError());
        return false;
    }

    /* ── Header (32 bytes) ───────────────────────────────────────────────
     *   magic:          4 bytes  "FMSH"
     *   version:        4 bytes  uint32 (2)
     *   vertex_count:   4 bytes  uint32
     *   vertex_stride:  4 bytes  uint32
     *   lod_count:      4 bytes  uint32
     *   flags:          4 bytes  uint32
     *   submesh_count:  4 bytes  uint32  (NEW in v2)
     *   reserved:       4 bytes  padding (shrunk from 8 to 4) */
    uint8_t reserved[4];
    memset(reserved, 0, sizeof(reserved));

    bool ok = true;
    ok = ok && (SDL_WriteIO(io, FMESH_MAGIC, FMESH_MAGIC_SIZE) == FMESH_MAGIC_SIZE);
    ok = ok && write_u32_le(io, FMESH_VERSION);
    ok = ok && write_u32_le(io, vertex_count);
    ok = ok && write_u32_le(io, vertex_stride);
    ok = ok && write_u32_le(io, (uint32_t)lod_count);
    ok = ok && write_u32_le(io, flags);
    ok = ok && write_u32_le(io, (uint32_t)submesh_count);
    ok = ok && (SDL_WriteIO(io, reserved, sizeof(reserved)) == sizeof(reserved));

    if (!ok) {
        SDL_Log("Error: failed writing .fmesh header");
        SDL_CloseIO(io);
        return false;
    }

    /* ── LOD-submesh table ───────────────────────────────────────────────
     * For each LOD:
     *   target_error (float as u32)
     *   For each submesh:
     *     index_count (u32)
     *     index_offset (u32) — byte offset into index section
     *     material_index (i32) */
    {
        int lod;
        for (lod = 0; lod < lod_count; lod++) {
            /* Write target error as float bits */
            uint32_t error_bits;
            memcpy(&error_bits, &lod_errors[lod], sizeof(error_bits));
            ok = ok && write_u32_le(io, error_bits);

            int s;
            for (s = 0; s < submesh_count; s++) {
                int entry_idx = lod * submesh_count + s;
                ok = ok && write_u32_le(io, lod_submeshes[entry_idx].index_count);
                ok = ok && write_u32_le(io, lod_submeshes[entry_idx].index_offset);
                ok = ok && write_i32_le(io, (int32_t)lod_submeshes[entry_idx].material_index);
            }
        }
    }

    if (!ok) {
        SDL_Log("Error: failed writing LOD-submesh table");
        SDL_CloseIO(io);
        return false;
    }

    /* ── Vertex data ─────────────────────────────────────────────────────
     * Write each vertex using only the fields indicated by vertex_stride.
     * Without tangents we write 8 floats; with tangents we write 12. */
    {
        unsigned int i;
        for (i = 0; i < vertex_count; i++) {
            if (SDL_WriteIO(io, &vertices[i], (size_t)vertex_stride) != (size_t)vertex_stride) {
                SDL_Log("Error: failed writing vertex %u", i);
                SDL_CloseIO(io);
                return false;
            }
        }
    }

    /* ── Index data ──────────────────────────────────────────────────────
     * All LOD index buffers are concatenated. Each index is a uint32
     * referencing the shared vertex buffer above. */
    {
        unsigned int total_indices = 0;
        int lod, s;
        for (lod = 0; lod < lod_count; lod++) {
            for (s = 0; s < submesh_count; s++) {
                int entry_idx = lod * submesh_count + s;
                total_indices += lod_submeshes[entry_idx].index_count;
            }
        }

        unsigned int i;
        for (i = 0; i < total_indices; i++) {
            if (!write_u32_le(io, indices[i])) {
                SDL_Log("Error: failed writing index %u", i);
                SDL_CloseIO(io);
                return false;
            }
        }
    }

    if (!SDL_FlushIO(io)) {
        SDL_Log("Error: flush failed on '%s': %s", path, SDL_GetError());
        SDL_CloseIO(io);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", path, SDL_GetError());
        return false;
    }
    return true;
}

/* ── Material sidecar writer (.fmat) ─────────────────────────────────────── */

/* Write a JSON sidecar file containing material data extracted from the
 * glTF scene.  The .fmat file sits next to the .fmesh with the same stem:
 *   model.fmesh -> model.fmat
 *
 * GPU lesson code reads this to set up material uniforms and bind textures
 * for each submesh draw call. */
/* Write a JSON-escaped string value (with surrounding quotes) to an SDL
 * I/O stream.  Handles backslash, double-quote, and control characters
 * (U+0000..U+001F). */
static void write_json_string(SDL_IOStream *io, const char *s)
{
    SDL_WriteIO(io, "\"", 1);
    if (s) {
        for (const char *c = s; *c != '\0'; c++) {
            switch (*c) {
            case '"':  SDL_WriteIO(io, "\\\"", 2); break;
            case '\\': SDL_WriteIO(io, "\\\\", 2); break;
            case '\b': SDL_WriteIO(io, "\\b", 2);  break;
            case '\f': SDL_WriteIO(io, "\\f", 2);  break;
            case '\n': SDL_WriteIO(io, "\\n", 2);  break;
            case '\r': SDL_WriteIO(io, "\\r", 2);  break;
            case '\t': SDL_WriteIO(io, "\\t", 2);  break;
            default:
                if ((unsigned char)*c < 0x20) {
                    SDL_IOprintf(io, "\\u%04x", (unsigned char)*c);
                } else {
                    SDL_WriteIO(io, c, 1);
                }
                break;
            }
        }
    }
    SDL_WriteIO(io, "\"", 1);
}

static bool write_fmat(const char *fmesh_path, const ForgeGltfScene *scene)
{
    /* Build the .fmat path by replacing the .fmesh extension */
    size_t path_len = strlen(fmesh_path);
    const char *dot = strrchr(fmesh_path, '.');
    size_t stem_len = dot ? (size_t)(dot - fmesh_path) : path_len;

    size_t fmat_len = stem_len + 6; /* ".fmat\0" */
    char *fmat_path = (char *)SDL_malloc(fmat_len);
    if (!fmat_path) {
        SDL_Log("Error: allocation failed for .fmat path");
        return false;
    }
    SDL_snprintf(fmat_path, fmat_len, "%.*s.fmat", (int)stem_len, fmesh_path);

    SDL_IOStream *io = SDL_IOFromFile(fmat_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                fmat_path, SDL_GetError());
        SDL_free(fmat_path);
        return false;
    }

    int mat_count = scene->material_count;
    int m;

    SDL_IOprintf(io, "{\n");
    SDL_IOprintf(io, "  \"version\": 1,\n");
    SDL_IOprintf(io, "  \"materials\": [\n");

    for (m = 0; m < mat_count; m++) {
        const ForgeGltfMaterial *mat = &scene->materials[m];

        /* Use full relative paths from glTF material */
        const char *base_tex = mat->has_texture
            ? mat->texture_path : NULL;
        const char *mr_tex = mat->has_metallic_roughness
            ? mat->metallic_roughness_path : NULL;
        const char *norm_tex = mat->has_normal_map
            ? mat->normal_map_path : NULL;
        const char *occ_tex = mat->has_occlusion
            ? mat->occlusion_path : NULL;
        const char *emis_tex = (mat->has_emissive && mat->emissive_path[0])
            ? mat->emissive_path : NULL;

        /* Alpha mode string */
        const char *alpha_str = "OPAQUE";
        if (mat->alpha_mode == FORGE_GLTF_ALPHA_MASK) {
            alpha_str = "MASK";
        } else if (mat->alpha_mode == FORGE_GLTF_ALPHA_BLEND) {
            alpha_str = "BLEND";
        }

        SDL_IOprintf(io, "    {\n");
        SDL_IOprintf(io, "      \"name\": ");
        write_json_string(io, mat->name);
        SDL_IOprintf(io, ",\n");
        SDL_IOprintf(io, "      \"base_color_factor\": [%.6f, %.6f, %.6f, %.6f],\n",
                (double)mat->base_color[0], (double)mat->base_color[1],
                (double)mat->base_color[2], (double)mat->base_color[3]);

        if (base_tex) {
            SDL_IOprintf(io, "      \"base_color_texture\": ");
            write_json_string(io, base_tex);
            SDL_IOprintf(io, ",\n");
        } else {
            SDL_IOprintf(io, "      \"base_color_texture\": null,\n");
        }

        SDL_IOprintf(io, "      \"metallic_factor\": %.6f,\n",
                (double)mat->metallic_factor);
        SDL_IOprintf(io, "      \"roughness_factor\": %.6f,\n",
                (double)mat->roughness_factor);

        if (mr_tex) {
            SDL_IOprintf(io, "      \"metallic_roughness_texture\": ");
            write_json_string(io, mr_tex);
            SDL_IOprintf(io, ",\n");
        } else {
            SDL_IOprintf(io, "      \"metallic_roughness_texture\": null,\n");
        }

        if (norm_tex) {
            SDL_IOprintf(io, "      \"normal_texture\": ");
            write_json_string(io, norm_tex);
            SDL_IOprintf(io, ",\n");
        } else {
            SDL_IOprintf(io, "      \"normal_texture\": null,\n");
        }
        SDL_IOprintf(io, "      \"normal_scale\": %.6f,\n",
                (double)mat->normal_scale);

        if (occ_tex) {
            SDL_IOprintf(io, "      \"occlusion_texture\": ");
            write_json_string(io, occ_tex);
            SDL_IOprintf(io, ",\n");
        } else {
            SDL_IOprintf(io, "      \"occlusion_texture\": null,\n");
        }
        SDL_IOprintf(io, "      \"occlusion_strength\": %.6f,\n",
                (double)mat->occlusion_strength);

        SDL_IOprintf(io, "      \"emissive_factor\": [%.6f, %.6f, %.6f],\n",
                (double)mat->emissive_factor[0],
                (double)mat->emissive_factor[1],
                (double)mat->emissive_factor[2]);

        if (emis_tex) {
            SDL_IOprintf(io, "      \"emissive_texture\": ");
            write_json_string(io, emis_tex);
            SDL_IOprintf(io, ",\n");
        } else {
            SDL_IOprintf(io, "      \"emissive_texture\": null,\n");
        }

        SDL_IOprintf(io, "      \"alpha_mode\": ");
        write_json_string(io, alpha_str);
        SDL_IOprintf(io, ",\n");
        SDL_IOprintf(io, "      \"alpha_cutoff\": %.6f,\n",
                (double)mat->alpha_cutoff);
        SDL_IOprintf(io, "      \"double_sided\": %s\n",
                mat->double_sided ? "true" : "false");

        if (m + 1 < mat_count) {
            SDL_IOprintf(io, "    },\n");
        } else {
            SDL_IOprintf(io, "    }\n");
        }
    }

    SDL_IOprintf(io, "  ]\n");
    SDL_IOprintf(io, "}\n");

    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", fmat_path, SDL_GetError());
        SDL_CloseIO(io);
        SDL_free(fmat_path);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", fmat_path, SDL_GetError());
        SDL_free(fmat_path);
        return false;
    }
    SDL_Log("Wrote materials: '%s' (%d materials)", fmat_path, mat_count);
    SDL_free(fmat_path);
    return true;
}

/* ── Meta JSON sidecar writer ────────────────────────────────────────────── */

static bool write_meta_json(const char *fmesh_path, const char *source_path,
                             unsigned int vertex_count, unsigned int vertex_stride,
                             bool has_tangents, int lod_count,
                             const float *lod_ratios, int submesh_count,
                             unsigned int original_vertex_count)
{
    /* Build the .meta.json path by replacing the .fmesh extension.
     * Example: "model.fmesh" -> "model.meta.json" */
    size_t path_len = strlen(fmesh_path);
    const char *dot = strrchr(fmesh_path, '.');
    size_t stem_len = dot ? (size_t)(dot - fmesh_path) : path_len;

    size_t meta_len = stem_len + 11; /* ".meta.json\0" */
    char *meta_path = (char *)SDL_malloc(meta_len);
    if (!meta_path) {
        SDL_Log("Error: allocation failed for meta path");
        return false;
    }
    SDL_snprintf(meta_path, meta_len, "%.*s.meta.json", (int)stem_len, fmesh_path);

    SDL_IOStream *io = SDL_IOFromFile(meta_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                meta_path, SDL_GetError());
        SDL_free(meta_path);
        return false;
    }

    /* Extract just the filename from the source path for the JSON. */
    const char *source_name = basename_from_path(source_path);

    float dedup_ratio = (original_vertex_count > 0)
        ? (float)vertex_count / (float)original_vertex_count
        : 0.0f;

    SDL_IOprintf(io, "{\n");
    SDL_IOprintf(io, "  \"source\": ");
    write_json_string(io, source_name);
    SDL_IOprintf(io, ",\n");
    SDL_IOprintf(io, "  \"format_version\": %d,\n", FMESH_VERSION);
    SDL_IOprintf(io, "  \"vertex_count\": %u,\n", vertex_count);
    SDL_IOprintf(io, "  \"vertex_stride\": %u,\n", vertex_stride);
    SDL_IOprintf(io, "  \"has_tangents\": %s,\n", has_tangents ? "true" : "false");
    SDL_IOprintf(io, "  \"submesh_count\": %d,\n", submesh_count);
    SDL_IOprintf(io, "  \"lod_count\": %d,\n", lod_count);
    SDL_IOprintf(io, "  \"lod_ratios\": [");

    {
        int i;
        for (i = 0; i < lod_count; i++) {
            if (i > 0) SDL_IOprintf(io, ", ");
            SDL_IOprintf(io, "%.2f", (double)lod_ratios[i]);
        }
    }

    SDL_IOprintf(io, "],\n");
    SDL_IOprintf(io, "  \"original_vertex_count\": %u,\n", original_vertex_count);
    SDL_IOprintf(io, "  \"dedup_ratio\": %.2f\n", (double)dedup_ratio);
    SDL_IOprintf(io, "}\n");

    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", meta_path, SDL_GetError());
        SDL_CloseIO(io);
        SDL_free(meta_path);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", meta_path, SDL_GetError());
        SDL_free(meta_path);
        return false;
    }
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
