/*
 * forge_pipeline.h — Asset pipeline runtime library for forge-gpu
 *
 * Loads processed assets produced by the forge-gpu asset pipeline:
 *   - .fmesh v2 files (optimised meshes with submeshes, LODs, tangents)
 *   - .fmat files (PBR material sidecars — JSON)
 *   - Texture files with .meta.json sidecars (mip chains, format metadata)
 *
 * This is a header-only library.  In exactly ONE .c file, define
 * FORGE_PIPELINE_IMPLEMENTATION before including this header:
 *
 *   #define FORGE_PIPELINE_IMPLEMENTATION
 *   #include "pipeline/forge_pipeline.h"
 *
 * All other files that need the types and declarations include the header
 * without the define.
 *
 * Dependencies:
 *   - SDL3/SDL.h   (SDL_LoadFile, SDL_malloc, SDL_free, SDL_Log)
 *   - cJSON.h      (texture .meta.json parsing)
 *   - string.h     (memcpy, memcmp)
 *
 * See tools/mesh/main.c for the .fmesh writer.
 * See lessons/assets/ for walkthroughs of each pipeline stage.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_PIPELINE_H
#define FORGE_PIPELINE_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>

/* cJSON is used for parsing texture .meta.json sidecars. */
#include "cJSON.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

/* .fmesh binary format identifiers */
#define FORGE_PIPELINE_FMESH_MAGIC   "FMSH"
#define FORGE_PIPELINE_FMESH_VERSION 2

/* .fmesh header layout sizes (bytes) */
#define FORGE_PIPELINE_HEADER_SIZE    32

/* Vertex stride values — determines whether tangent data is present.
 * Stride 32: position(12) + normal(12) + uv(8)
 * Stride 48: position(12) + normal(12) + uv(8) + tangent(16) */
#define FORGE_PIPELINE_VERTEX_STRIDE_NO_TAN 32
#define FORGE_PIPELINE_VERTEX_STRIDE_TAN    48

/* Flags stored in the .fmesh header */
#define FORGE_PIPELINE_FLAG_TANGENTS (1u << 0)

/* .fmesh magic identifier size */
#define FORGE_PIPELINE_FMESH_MAGIC_SIZE 4

/* Size of the reserved padding at the end of the .fmesh v2 header (bytes).
 * In v2, 4 bytes of the original 8-byte reserved field became submesh_count,
 * leaving 4 bytes of padding. */
#define FORGE_PIPELINE_HEADER_RESERVED 4

/* Upper bounds for validation */
#define FORGE_PIPELINE_MAX_LODS       8
#define FORGE_PIPELINE_MAX_SUBMESHES  64
#define FORGE_PIPELINE_MAX_MATERIALS  256
#define FORGE_PIPELINE_MAX_MIP_LEVELS 16

/* .fmat material sidecar format version */
#define FORGE_PIPELINE_FMAT_VERSION   1

/* ── Mesh types ────────────────────────────────────────────────────────── */

/* Vertex without tangent data (stride 32). */
typedef struct ForgePipelineVertex {
    float position[3]; /* world-space position (x, y, z) */
    float normal[3];   /* unit surface normal (x, y, z) */
    float uv[2];       /* texture coordinates (u, v) */
} ForgePipelineVertex;

/* Vertex with tangent data (stride 48).
 * The tangent w component encodes the bitangent sign (+1 or -1) for
 * MikkTSpace compatibility: B = cross(N, T.xyz) * T.w */
typedef struct ForgePipelineVertexTan {
    float position[3]; /* world-space position (x, y, z) */
    float normal[3];   /* unit surface normal (x, y, z) */
    float uv[2];       /* texture coordinates (u, v) */
    float tangent[4];  /* xyz = tangent direction, w = bitangent sign (±1) */
} ForgePipelineVertexTan;

/* One LOD entry — aggregated from the per-submesh data.
 * index_count is the sum of all submesh index_counts for this LOD.
 * index_offset is the byte offset of the first submesh in this LOD. */
typedef struct ForgePipelineLod {
    uint32_t index_count;  /* total indices across all submeshes in this LOD */
    uint32_t index_offset; /* byte offset of the first submesh's indices */
    float    target_error; /* meshoptimizer simplification error metric */
} ForgePipelineLod;

/* A loaded .fmesh mesh.
 * vertices points to an array of ForgePipelineVertex or
 * ForgePipelineVertexTan depending on vertex_stride.  Cast accordingly:
 *
 *   if (forge_pipeline_has_tangents(&mesh)) {
 *       ForgePipelineVertexTan *v = (ForgePipelineVertexTan *)mesh.vertices;
 *   } else {
 *       ForgePipelineVertex *v = (ForgePipelineVertex *)mesh.vertices;
 *   }
 */
/* Per-submesh index range within a single LOD level.
 * The .fmesh v2 format stores lod_count × submesh_count of these entries. */
typedef struct ForgePipelineSubmesh {
    uint32_t index_count;     /* number of indices for this submesh */
    uint32_t index_offset;    /* byte offset into the index section */
    int32_t  material_index;  /* index into a ForgePipelineMaterialSet, -1 = none */
} ForgePipelineSubmesh;

typedef struct ForgePipelineMesh {
    void             *vertices;     /* vertex array (cast to Vertex or VertexTan) */
    uint32_t         *indices;      /* all LOD indices concatenated (uint32) */
    uint32_t          vertex_count; /* number of vertices in the array */
    uint32_t          vertex_stride;/* bytes per vertex (32 or 48) */
    ForgePipelineLod *lods;         /* target_error per LOD (lod_count entries) */
    uint32_t          lod_count;    /* number of LOD levels (1..MAX_LODS) */
    uint32_t          flags;        /* bit field (see FORGE_PIPELINE_FLAG_*) */

    /* Submesh data. Flat array indexed as
     * submeshes[lod * submesh_count + submesh_idx]. */
    ForgePipelineSubmesh *submeshes;     /* lod_count × submesh_count entries */
    uint32_t              submesh_count; /* number of submeshes (primitives) */
} ForgePipelineMesh;

/* ── Material types ───────────────────────────────────────────────────── */

/* Alpha blending mode — matches glTF 2.0 spec values. */
#define FORGE_PIPELINE_ALPHA_OPAQUE 0
#define FORGE_PIPELINE_ALPHA_MASK   1
#define FORGE_PIPELINE_ALPHA_BLEND  2

/* Maximum path length for texture references in .fmat files. */
#define FORGE_PIPELINE_MAT_PATH_SIZE 512

/* A single PBR metallic-roughness material loaded from a .fmat sidecar. */
typedef struct ForgePipelineMaterial {
    char  name[64];                                       /* material name (for debugging) */
    float base_color_factor[4];                           /* RGBA multiplier (default 1,1,1,1) */
    char  base_color_texture[FORGE_PIPELINE_MAT_PATH_SIZE]; /* relative path, empty = none */
    float metallic_factor;                                /* 0 = dielectric, 1 = metal (default 1) */
    float roughness_factor;                               /* 0 = mirror, 1 = rough (default 1) */
    char  metallic_roughness_texture[FORGE_PIPELINE_MAT_PATH_SIZE]; /* B=metallic G=roughness */
    char  normal_texture[FORGE_PIPELINE_MAT_PATH_SIZE];   /* tangent-space normal map path */
    float normal_scale;                                   /* normal map XY multiplier (default 1) */
    char  occlusion_texture[FORGE_PIPELINE_MAT_PATH_SIZE]; /* AO in R channel, empty = none */
    float occlusion_strength;                             /* AO blend: 0 = none, 1 = full (default 1) */
    float emissive_factor[3];                             /* RGB emission multiplier (default 0,0,0) */
    char  emissive_texture[FORGE_PIPELINE_MAT_PATH_SIZE]; /* emissive texture path, empty = none */
    int   alpha_mode;                                     /* FORGE_PIPELINE_ALPHA_OPAQUE/MASK/BLEND */
    float alpha_cutoff;                                   /* MASK threshold (default 0.5) */
    bool  double_sided;                                   /* render both faces if true */
} ForgePipelineMaterial;

/* Collection of materials loaded from a .fmat sidecar file. */
typedef struct ForgePipelineMaterialSet {
    ForgePipelineMaterial *materials;
    uint32_t               material_count;
} ForgePipelineMaterialSet;

/* ── Texture types ─────────────────────────────────────────────────────── */

/* Pixel format of the decoded texture data. */
typedef enum ForgePipelineTextureFormat {
    FORGE_PIPELINE_TEX_RGBA8,
    FORGE_PIPELINE_TEX_RGB8
} ForgePipelineTextureFormat;

/* One mip level of a texture.
 * data contains the raw file bytes (PNG, KTX2, etc.) as loaded from disk.
 * For current pipeline output (PNG), decode with SDL_image or stb_image.
 * For future compressed formats (BC7/BC5 in KTX2), the bytes are GPU-ready. */
typedef struct ForgePipelineMipLevel {
    void    *data;   /* raw file bytes (PNG, KTX2, etc.) */
    uint32_t width;  /* mip level width in pixels */
    uint32_t height; /* mip level height in pixels */
    uint32_t size;   /* byte count of data */
} ForgePipelineMipLevel;

/* A loaded texture with its mip chain and metadata.
 * width and height are the base (mip 0) dimensions. */
typedef struct ForgePipelineTexture {
    ForgePipelineMipLevel *mips;      /* mip chain array (mip_count entries) */
    uint32_t               mip_count; /* number of mip levels (1..MAX_MIP_LEVELS) */
    uint32_t               width;     /* base (mip 0) width in pixels */
    uint32_t               height;    /* base (mip 0) height in pixels */
    ForgePipelineTextureFormat format; /* pixel format of the texture data */
} ForgePipelineTexture;

/* ── Function declarations ─────────────────────────────────────────────── */

/*
 * forge_pipeline_load_mesh — Load a .fmesh binary file.
 *
 * Reads the file at `path`, validates the header, and populates `mesh`
 * with allocated vertex, index, and LOD data.  The caller owns the memory
 * and must call forge_pipeline_free_mesh() when done.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_mesh(const char *path, ForgePipelineMesh *mesh);

/*
 * forge_pipeline_free_mesh — Release all memory owned by a mesh.
 *
 * Safe to call on a zeroed or already-freed mesh (no-op if mesh is NULL).
 */
void forge_pipeline_free_mesh(ForgePipelineMesh *mesh);

/*
 * forge_pipeline_has_tangents — Check if the mesh has tangent data.
 */
bool forge_pipeline_has_tangents(const ForgePipelineMesh *mesh);

/*
 * forge_pipeline_lod_index_count — Get the index count for a specific LOD.
 *
 * Returns 0 if lod >= mesh->lod_count.
 */
uint32_t forge_pipeline_lod_index_count(const ForgePipelineMesh *mesh,
                                        uint32_t lod);

/*
 * forge_pipeline_lod_indices — Get a pointer to the index data for a LOD.
 *
 * The returned pointer is into mesh->indices at the correct offset.
 * Returns NULL if lod >= mesh->lod_count.
 */
const uint32_t *forge_pipeline_lod_indices(const ForgePipelineMesh *mesh,
                                           uint32_t lod);

/*
 * forge_pipeline_submesh_count — Get the number of submeshes in the mesh.
 *
 * Each submesh corresponds to a glTF primitive with its own material.
 */
uint32_t forge_pipeline_submesh_count(const ForgePipelineMesh *mesh);

/*
 * forge_pipeline_lod_submesh — Get a submesh entry for a specific LOD.
 *
 * Returns a pointer to the submesh entry at (lod, submesh_idx),
 * or NULL if either index is out of range.
 */
const ForgePipelineSubmesh *forge_pipeline_lod_submesh(
    const ForgePipelineMesh *mesh, uint32_t lod, uint32_t submesh_idx);

/*
 * forge_pipeline_load_materials — Load a .fmat JSON material sidecar.
 *
 * Reads the file at `path`, parses the material array, and populates
 * `set` with allocated material data.  The caller owns the memory and
 * must call forge_pipeline_free_materials() when done.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_materials(const char *path,
                                    ForgePipelineMaterialSet *set);

/*
 * forge_pipeline_free_materials — Release all memory owned by a material set.
 *
 * Safe to call on a zeroed or already-freed set.
 */
void forge_pipeline_free_materials(ForgePipelineMaterialSet *set);

/*
 * forge_pipeline_load_texture — Load texture metadata and raw mip files.
 *
 * Reads <stem>.meta.json to discover dimensions, format, and mip file
 * paths.  Each mip level file is loaded as raw bytes via SDL_LoadFile.
 * The caller is responsible for decoding (e.g. with SDL_image for PNG)
 * or uploading directly for GPU-ready formats.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_texture(const char *path,
                                 ForgePipelineTexture *tex);

/*
 * forge_pipeline_free_texture — Release all memory owned by a texture.
 *
 * Safe to call on a zeroed or already-freed texture (no-op if tex is NULL).
 */
void forge_pipeline_free_texture(ForgePipelineTexture *tex);

/* ── Implementation ────────────────────────────────────────────────────── */

#ifdef FORGE_PIPELINE_IMPLEMENTATION

/* ── Helper: read a uint32 from a little-endian buffer ────────────────
 *
 * Uses memcpy to avoid undefined behaviour from unaligned access.
 * The .fmesh format stores all multi-byte values in little-endian order.
 * On big-endian hosts this would need a byte swap — SDL_SwapLE32 handles
 * that, but since virtually all current targets (x86, ARM) are LE, the
 * swap is a no-op in practice. */
static uint32_t forge_pipeline__read_u32_le(const uint8_t *buf)
{
    uint32_t val;
    SDL_memcpy(&val, buf, sizeof(val));
    return SDL_Swap32LE(val);
}

/* ── Helper: read a float stored as uint32 LE bits ────────────────────
 *
 * The .fmesh format stores floats by memcpy-ing their bit pattern into
 * a uint32 and writing that in LE order (see tools/mesh/main.c).
 * We reverse the process: read the uint32 LE, then memcpy back to float. */
static float forge_pipeline__read_f32_le(const uint8_t *buf)
{
    uint32_t bits = forge_pipeline__read_u32_le(buf);
    float val;
    SDL_memcpy(&val, &bits, sizeof(val));
    return val;
}

bool forge_pipeline_load_mesh(const char *path, ForgePipelineMesh *mesh)
{
    /* ── Validate inputs ──────────────────────────────────────────────── */
    if (!path) {
        SDL_Log("forge_pipeline_load_mesh: path is NULL");
        return false;
    }
    if (!mesh) {
        SDL_Log("forge_pipeline_load_mesh: mesh is NULL");
        return false;
    }

    /* Zero the output so partial-failure cleanup is safe. */
    SDL_memset(mesh, 0, sizeof(*mesh));

    /* ── Load entire file ─────────────────────────────────────────────
     * SDL_LoadFile reads the whole file into an SDL-managed buffer.
     * We parse in memory and free the buffer before returning. */
    size_t file_size = 0;
    uint8_t *file_data = (uint8_t *)SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_mesh: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* ── Validate minimum header size ─────────────────────────────── */
    if (file_size < FORGE_PIPELINE_HEADER_SIZE) {
        SDL_Log("forge_pipeline_load_mesh: file '%s' too small for header "
                "(%zu bytes, need %d)", path, file_size,
                FORGE_PIPELINE_HEADER_SIZE);
        SDL_free(file_data);
        return false;
    }

    /* ── Parse header fields ──────────────────────────────────────── */
    const uint8_t *p = file_data;

    /* Magic: must be "FMSH" */
    if (SDL_memcmp(p, FORGE_PIPELINE_FMESH_MAGIC,
                   FORGE_PIPELINE_FMESH_MAGIC_SIZE) != 0) {
        SDL_Log("forge_pipeline_load_mesh: '%s' is not a .fmesh file "
                "(bad magic)", path);
        SDL_free(file_data);
        return false;
    }
    p += FORGE_PIPELINE_FMESH_MAGIC_SIZE;

    /* Version — v2 only */
    uint32_t version = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (version != FORGE_PIPELINE_FMESH_VERSION) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has unsupported version %u "
                "(expected %u)", path, version, FORGE_PIPELINE_FMESH_VERSION);
        SDL_free(file_data);
        return false;
    }

    /* Vertex count */
    uint32_t vertex_count = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);

    /* Vertex stride */
    uint32_t vertex_stride = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_NO_TAN &&
        vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_TAN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has invalid vertex stride %u "
                "(expected %d or %d)", path, vertex_stride,
                FORGE_PIPELINE_VERTEX_STRIDE_NO_TAN,
                FORGE_PIPELINE_VERTEX_STRIDE_TAN);
        SDL_free(file_data);
        return false;
    }

    /* LOD count */
    uint32_t lod_count = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (lod_count == 0 || lod_count > FORGE_PIPELINE_MAX_LODS) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has invalid lod_count %u "
                "(max %d)", path, lod_count, FORGE_PIPELINE_MAX_LODS);
        SDL_free(file_data);
        return false;
    }

    /* Flags */
    uint32_t flags = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);

    /* Submesh count (v2 header field) + 4 bytes reserved padding */
    uint32_t submesh_count = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (submesh_count == 0 || submesh_count > FORGE_PIPELINE_MAX_SUBMESHES) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has invalid submesh_count %u "
                "(max %d)", path, submesh_count, FORGE_PIPELINE_MAX_SUBMESHES);
        SDL_free(file_data);
        return false;
    }
    p += FORGE_PIPELINE_HEADER_RESERVED; /* remaining reserved padding */

    /* ── Validate tangent flag / stride consistency ───────────────── */
    bool has_tangents = (flags & FORGE_PIPELINE_FLAG_TANGENTS) != 0;
    if (has_tangents && vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_TAN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has TANGENTS flag but "
                "stride is %u (expected %d)", path, vertex_stride,
                FORGE_PIPELINE_VERTEX_STRIDE_TAN);
        SDL_free(file_data);
        return false;
    }
    if (!has_tangents && vertex_stride == FORGE_PIPELINE_VERTEX_STRIDE_TAN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has tangent stride %u "
                "but FORGE_PIPELINE_FLAG_TANGENTS is not set",
                path, vertex_stride);
        SDL_free(file_data);
        return false;
    }

    /* ── Validate file size against LOD-submesh table ─────────────
     *
     * v2 LOD-submesh table layout (per LOD):
     *   target_error:   4 bytes (float as u32)
     *   per submesh:    12 bytes (index_count u32, index_offset u32, material_index i32)
     *
     * Total per LOD = 4 + submesh_count * 12 */
    /* Use 64-bit intermediate arithmetic to detect overflow on 32-bit
     * builds where size_t is 32 bits.  All operands originate from
     * untrusted file data (vertex_count, vertex_stride, lod_count,
     * submesh_count) so every multiply/add must be checked. */
    uint64_t per_lod_size_64 = (uint64_t)sizeof(uint32_t)
                             + (uint64_t)submesh_count * 3 * sizeof(uint32_t);
    uint64_t lod_section_size_64 = (uint64_t)lod_count * per_lod_size_64;
    uint64_t vertex_section_size_64 = (uint64_t)vertex_count * vertex_stride;

    if (per_lod_size_64 > SIZE_MAX || lod_section_size_64 > SIZE_MAX
        || vertex_section_size_64 > SIZE_MAX) {
        SDL_Log("forge_pipeline_load_mesh: '%s' header values cause "
                "size overflow", path);
        SDL_free(file_data);
        return false;
    }

    size_t per_lod_size = (size_t)per_lod_size_64;
    size_t lod_section_size = (size_t)lod_section_size_64;
    size_t vertex_section_size = (size_t)vertex_section_size_64;

    size_t min_size_for_lods = FORGE_PIPELINE_HEADER_SIZE + lod_section_size;
    if (file_size < min_size_for_lods) {
        SDL_Log("forge_pipeline_load_mesh: '%s' too small for LOD-submesh "
                "table (%zu bytes, need %zu)", path, file_size,
                min_size_for_lods);
        SDL_free(file_data);
        return false;
    }

    /* ── Read LOD-submesh table ──────────────────────────────────── */
    ForgePipelineLod *lods = (ForgePipelineLod *)SDL_calloc(
        lod_count, sizeof(ForgePipelineLod));
    if (!lods) {
        SDL_Log("forge_pipeline_load_mesh: allocation failed for LOD entries");
        SDL_free(file_data);
        return false;
    }

    size_t total_submesh_entries = (size_t)lod_count * submesh_count;
    ForgePipelineSubmesh *submeshes = (ForgePipelineSubmesh *)SDL_calloc(
        total_submesh_entries, sizeof(ForgePipelineSubmesh));
    if (!submeshes) {
        SDL_Log("forge_pipeline_load_mesh: allocation failed for submeshes");
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }

    uint64_t total_indices = 0;
    for (uint32_t lod = 0; lod < lod_count; lod++) {
        /* target_error (float stored as uint32 bits) */
        lods[lod].target_error = forge_pipeline__read_f32_le(p);
        p += sizeof(uint32_t);

        /* Per-submesh entries */
        for (uint32_t s = 0; s < submesh_count; s++) {
            size_t idx = (size_t)lod * submesh_count + s;
            submeshes[idx].index_count  = forge_pipeline__read_u32_le(p);
            p += sizeof(uint32_t);
            submeshes[idx].index_offset = forge_pipeline__read_u32_le(p);
            p += sizeof(uint32_t);
            /* material_index is signed (int32_t), stored as uint32 bits */
            uint32_t mat_bits = forge_pipeline__read_u32_le(p);
            SDL_memcpy(&submeshes[idx].material_index, &mat_bits,
                       sizeof(int32_t));
            p += sizeof(uint32_t);

            /* Validate index_offset is 4-byte aligned (uint32 indices) */
            if ((submeshes[idx].index_offset % 4) != 0) {
                SDL_Log("forge_pipeline_load_mesh: '%s' submesh %u (lod %u) "
                        "has misaligned index_offset %u (must be 4-byte "
                        "aligned)", path, s, lod,
                        submeshes[idx].index_offset);
                SDL_free(submeshes);
                SDL_free(lods);
                SDL_free(file_data);
                return false;
            }

            total_indices += submeshes[idx].index_count;
        }

        /* Populate legacy LOD fields by summing submesh data for this LOD.
         * index_count = sum of all submesh index_counts for this LOD.
         * index_offset = minimum index_offset among submeshes. */
        uint64_t lod_idx_count  = 0;
        uint32_t lod_idx_offset = UINT32_MAX;
        for (uint32_t s = 0; s < submesh_count; s++) {
            size_t idx = (size_t)lod * submesh_count + s;
            lod_idx_count += submeshes[idx].index_count;
            if (submeshes[idx].index_offset < lod_idx_offset) {
                lod_idx_offset = submeshes[idx].index_offset;
            }
        }
        if (lod_idx_count > UINT32_MAX) {
            SDL_Log("forge_pipeline_load_mesh: '%s' lod %u index count "
                    "overflow (%" SDL_PRIu64 ")", path, lod, lod_idx_count);
            SDL_free(submeshes);
            SDL_free(lods);
            SDL_free(file_data);
            return false;
        }
        lods[lod].index_count  = (uint32_t)lod_idx_count;
        lods[lod].index_offset = (lod_idx_offset == UINT32_MAX)
                                   ? 0 : lod_idx_offset;
    }

    /* ── Validate submesh index ranges against total index count ── */
    for (uint32_t lod = 0; lod < lod_count; lod++) {
        for (uint32_t s = 0; s < submesh_count; s++) {
            size_t idx = (size_t)lod * submesh_count + s;
            uint32_t sub_offset = submeshes[idx].index_offset;
            uint32_t sub_count  = submeshes[idx].index_count;
            /* index_offset is in bytes; convert to index units.
             * Use 64-bit to avoid wraparound on 32-bit builds. */
            uint64_t first_index = (uint64_t)sub_offset / 4u;
            uint64_t range_end   = first_index + (uint64_t)sub_count;
            if (range_end > total_indices) {
                SDL_Log("forge_pipeline_load_mesh: '%s' submesh %u (lod %u) "
                        "index range [%" SDL_PRIu64 "..%" SDL_PRIu64
                        ") exceeds total index count %" SDL_PRIu64,
                        path, s, lod, first_index, range_end, total_indices);
                SDL_free(submeshes);
                SDL_free(lods);
                SDL_free(file_data);
                return false;
            }
        }
    }

    /* ── Validate contiguous submesh spans per LOD ──────────────── */
    /* Use a temporary copy so the original submeshes array keeps its file
     * order — sorting in-place would change the order returned by
     * forge_pipeline_lod_submesh(). */
    ForgePipelineSubmesh *sort_tmp = (ForgePipelineSubmesh *)SDL_malloc(
        sizeof(ForgePipelineSubmesh) * submesh_count);
    if (!sort_tmp) {
        SDL_Log("forge_pipeline_load_mesh: '%s' allocation failed for "
                "contiguity check", path);
        SDL_free(submeshes);
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }
    for (uint32_t lod = 0; lod < lod_count; lod++) {
        /* Copy this LOD's submeshes into the temp buffer */
        SDL_memcpy(sort_tmp,
                   &submeshes[(size_t)lod * submesh_count],
                   sizeof(ForgePipelineSubmesh) * submesh_count);
        /* Bubble-sort by index_offset.  Submesh count per LOD is small
         * (typically 1-8), so a simple sort is fine. */
        for (uint32_t i = 0; i < submesh_count; i++) {
            for (uint32_t j = i + 1; j < submesh_count; j++) {
                if (sort_tmp[j].index_offset < sort_tmp[i].index_offset) {
                    ForgePipelineSubmesh swap = sort_tmp[i];
                    sort_tmp[i] = sort_tmp[j];
                    sort_tmp[j] = swap;
                }
            }
        }
        /* Verify each submesh's end equals the next submesh's start.
         * Use 64-bit arithmetic to avoid overflow when index_count * 4
         * exceeds UINT32_MAX. */
        for (uint32_t s = 0; s + 1 < submesh_count; s++) {
            uint64_t cur_end = (uint64_t)sort_tmp[s].index_offset
                             + (uint64_t)sort_tmp[s].index_count * 4;
            if (cur_end != (uint64_t)sort_tmp[s + 1].index_offset) {
                SDL_Log("forge_pipeline_load_mesh: '%s' lod %u submeshes %u "
                        "and %u are not contiguous (end %" SDL_PRIu64
                        " != offset %u)",
                        path, lod, s, s + 1, cur_end,
                        sort_tmp[s + 1].index_offset);
                SDL_free(sort_tmp);
                SDL_free(submeshes);
                SDL_free(lods);
                SDL_free(file_data);
                return false;
            }
        }
    }
    SDL_free(sort_tmp);

    /* ── Validate full file size ──────────────────────────────────── */
    uint64_t index_section_size_64 = total_indices * (uint64_t)sizeof(uint32_t);
    uint64_t expected_size_64 = (uint64_t)FORGE_PIPELINE_HEADER_SIZE
                              + lod_section_size_64
                              + vertex_section_size_64
                              + index_section_size_64;
    if (index_section_size_64 > SIZE_MAX || expected_size_64 > SIZE_MAX) {
        SDL_Log("forge_pipeline_load_mesh: '%s' data sections cause "
                "size overflow", path);
        SDL_free(submeshes);
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }
    size_t index_section_size = (size_t)index_section_size_64;
    size_t expected_size = (size_t)expected_size_64;
    if (file_size < expected_size) {
        SDL_Log("forge_pipeline_load_mesh: '%s' is truncated "
                "(%zu bytes, expected %zu)", path, file_size, expected_size);
        SDL_free(submeshes);
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }

    /* ── Copy vertex data ─────────────────────────────────────────── */
    void *vertices = SDL_malloc(vertex_section_size);
    if (!vertices) {
        SDL_Log("forge_pipeline_load_mesh: allocation failed for vertices "
                "(%zu bytes)", vertex_section_size);
        SDL_free(submeshes);
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }
    SDL_memcpy(vertices, p, vertex_section_size);
    p += vertex_section_size;

    /* ── Copy index data ──────────────────────────────────────────── */
    uint32_t *indices = (uint32_t *)SDL_malloc(index_section_size);
    if (!indices) {
        SDL_Log("forge_pipeline_load_mesh: allocation failed for indices "
                "(%zu bytes)", index_section_size);
        SDL_free(vertices);
        SDL_free(submeshes);
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }
    SDL_memcpy(indices, p, index_section_size);

    for (size_t i = 0; i < total_indices; i++) {
        if (indices[i] >= vertex_count) {
            SDL_Log("forge_pipeline_load_mesh: '%s' index %zu out of bounds "
                    "(%u >= vertex_count %u)", path, i, indices[i], vertex_count);
            SDL_free(indices);
            SDL_free(vertices);
            SDL_free(submeshes);
            SDL_free(lods);
            SDL_free(file_data);
            return false;
        }
    }

    /* ── Populate output struct ───────────────────────────────────── */
    mesh->vertices      = vertices;
    mesh->indices       = indices;
    mesh->vertex_count  = vertex_count;
    mesh->vertex_stride = vertex_stride;
    mesh->lods          = lods;
    mesh->lod_count     = lod_count;
    mesh->flags         = flags;
    mesh->submeshes     = submeshes;
    mesh->submesh_count = submesh_count;

    SDL_free(file_data);
    return true;
}

void forge_pipeline_free_mesh(ForgePipelineMesh *mesh)
{
    if (!mesh) {
        return;
    }
    SDL_free(mesh->vertices);
    SDL_free(mesh->indices);
    SDL_free(mesh->lods);
    SDL_free(mesh->submeshes);
    SDL_memset(mesh, 0, sizeof(*mesh));
}

bool forge_pipeline_has_tangents(const ForgePipelineMesh *mesh)
{
    if (!mesh) {
        return false;
    }
    return (mesh->flags & FORGE_PIPELINE_FLAG_TANGENTS) != 0;
}

uint32_t forge_pipeline_lod_index_count(const ForgePipelineMesh *mesh,
                                        uint32_t lod)
{
    if (!mesh || lod >= mesh->lod_count) {
        return 0;
    }
    return mesh->lods[lod].index_count;
}

const uint32_t *forge_pipeline_lod_indices(const ForgePipelineMesh *mesh,
                                           uint32_t lod)
{
    if (!mesh || lod >= mesh->lod_count || !mesh->indices) {
        return NULL;
    }
    /* index_offset is a byte offset into the index section.
     * Divide by sizeof(uint32_t) to get the element offset. */
    uint32_t element_offset = mesh->lods[lod].index_offset / sizeof(uint32_t);
    return mesh->indices + element_offset;
}

uint32_t forge_pipeline_submesh_count(const ForgePipelineMesh *mesh)
{
    if (!mesh) {
        return 0;
    }
    return mesh->submesh_count;
}

const ForgePipelineSubmesh *forge_pipeline_lod_submesh(
    const ForgePipelineMesh *mesh, uint32_t lod, uint32_t submesh_idx)
{
    if (!mesh || !mesh->submeshes ||
        lod >= mesh->lod_count || submesh_idx >= mesh->submesh_count) {
        return NULL;
    }
    return &mesh->submeshes[(size_t)lod * mesh->submesh_count + submesh_idx];
}

/* ── Helper: copy a cJSON string into a fixed buffer, or empty it ──────── */
static void forge_pipeline__copy_json_str(const cJSON *item,
                                           char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (cJSON_IsString(item) && item->valuestring &&
        item->valuestring[0] != '\0') {
        SDL_strlcpy(out, item->valuestring, out_size);
    } else {
        out[0] = '\0';
    }
}

bool forge_pipeline_load_materials(const char *path,
                                    ForgePipelineMaterialSet *set)
{
    if (!path) {
        SDL_Log("forge_pipeline_load_materials: path is NULL");
        return false;
    }
    if (!set) {
        SDL_Log("forge_pipeline_load_materials: set is NULL");
        return false;
    }
    SDL_memset(set, 0, sizeof(*set));

    /* Load file */
    size_t file_size = 0;
    char *file_data = (char *)SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_materials: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* Parse JSON */
    cJSON *root = cJSON_ParseWithLength(file_data, file_size);
    SDL_free(file_data);
    if (!root) {
        SDL_Log("forge_pipeline_load_materials: failed to parse '%s'", path);
        return false;
    }

    /* Validate version */
    cJSON *j_version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!cJSON_IsNumber(j_version) ||
        j_version->valueint != FORGE_PIPELINE_FMAT_VERSION) {
        SDL_Log("forge_pipeline_load_materials: '%s' has unsupported version "
                "(expected %d)", path, FORGE_PIPELINE_FMAT_VERSION);
        cJSON_Delete(root);
        return false;
    }

    /* Materials array */
    cJSON *j_materials = cJSON_GetObjectItemCaseSensitive(root, "materials");
    if (!cJSON_IsArray(j_materials)) {
        SDL_Log("forge_pipeline_load_materials: '%s' missing materials array",
                path);
        cJSON_Delete(root);
        return false;
    }

    uint32_t count = (uint32_t)cJSON_GetArraySize(j_materials);
    if (count == 0) {
        /* Valid: meshes may have no materials (all submeshes use
         * material_index = -1).  Return an empty material set. */
        set->material_count = 0;
        set->materials      = NULL;
        cJSON_Delete(root);
        return true;
    }
    if (count > FORGE_PIPELINE_MAX_MATERIALS) {
        SDL_Log("forge_pipeline_load_materials: '%s' has %u materials (max %d)",
                path, count, FORGE_PIPELINE_MAX_MATERIALS);
        cJSON_Delete(root);
        return false;
    }

    /* Allocate */
    ForgePipelineMaterial *mats = (ForgePipelineMaterial *)SDL_calloc(
        count, sizeof(ForgePipelineMaterial));
    if (!mats) {
        SDL_Log("forge_pipeline_load_materials: allocation failed");
        cJSON_Delete(root);
        return false;
    }

    /* Parse each material */
    uint32_t i = 0;
    cJSON *j_mat = NULL;
    cJSON_ArrayForEach(j_mat, j_materials) {
        if (i >= count) break; /* guard against iteration overshoot */
        ForgePipelineMaterial *m = &mats[i];

        /* Name */
        forge_pipeline__copy_json_str(
            cJSON_GetObjectItemCaseSensitive(j_mat, "name"),
            m->name, sizeof(m->name));

        /* Base color factor (default white) */
        cJSON *j_bcf = cJSON_GetObjectItemCaseSensitive(j_mat,
                                                         "base_color_factor");
        if (cJSON_IsArray(j_bcf) && cJSON_GetArraySize(j_bcf) >= 4) {
            for (int c = 0; c < 4; c++) {
                cJSON *elem = cJSON_GetArrayItem(j_bcf, c);
                m->base_color_factor[c] = cJSON_IsNumber(elem)
                    ? (float)elem->valuedouble : 1.0f;
            }
        } else {
            m->base_color_factor[0] = 1.0f;
            m->base_color_factor[1] = 1.0f;
            m->base_color_factor[2] = 1.0f;
            m->base_color_factor[3] = 1.0f;
        }

        /* Texture paths */
        forge_pipeline__copy_json_str(
            cJSON_GetObjectItemCaseSensitive(j_mat, "base_color_texture"),
            m->base_color_texture, sizeof(m->base_color_texture));
        forge_pipeline__copy_json_str(
            cJSON_GetObjectItemCaseSensitive(j_mat,
                                             "metallic_roughness_texture"),
            m->metallic_roughness_texture,
            sizeof(m->metallic_roughness_texture));
        forge_pipeline__copy_json_str(
            cJSON_GetObjectItemCaseSensitive(j_mat, "normal_texture"),
            m->normal_texture, sizeof(m->normal_texture));
        forge_pipeline__copy_json_str(
            cJSON_GetObjectItemCaseSensitive(j_mat, "occlusion_texture"),
            m->occlusion_texture, sizeof(m->occlusion_texture));
        forge_pipeline__copy_json_str(
            cJSON_GetObjectItemCaseSensitive(j_mat, "emissive_texture"),
            m->emissive_texture, sizeof(m->emissive_texture));

        /* Scalar factors */
        cJSON *j_val;
        j_val = cJSON_GetObjectItemCaseSensitive(j_mat, "metallic_factor");
        m->metallic_factor = cJSON_IsNumber(j_val)
                                 ? (float)j_val->valuedouble : 1.0f;

        j_val = cJSON_GetObjectItemCaseSensitive(j_mat, "roughness_factor");
        m->roughness_factor = cJSON_IsNumber(j_val)
                                  ? (float)j_val->valuedouble : 1.0f;

        j_val = cJSON_GetObjectItemCaseSensitive(j_mat, "normal_scale");
        m->normal_scale = cJSON_IsNumber(j_val)
                              ? (float)j_val->valuedouble : 1.0f;

        j_val = cJSON_GetObjectItemCaseSensitive(j_mat, "occlusion_strength");
        m->occlusion_strength = cJSON_IsNumber(j_val)
                                    ? (float)j_val->valuedouble : 1.0f;

        /* Emissive factor (default black) */
        cJSON *j_ef = cJSON_GetObjectItemCaseSensitive(j_mat,
                                                        "emissive_factor");
        if (cJSON_IsArray(j_ef) && cJSON_GetArraySize(j_ef) >= 3) {
            for (int c = 0; c < 3; c++) {
                cJSON *elem = cJSON_GetArrayItem(j_ef, c);
                m->emissive_factor[c] = cJSON_IsNumber(elem)
                    ? (float)elem->valuedouble : 0.0f;
            }
        } else {
            m->emissive_factor[0] = 0.0f;
            m->emissive_factor[1] = 0.0f;
            m->emissive_factor[2] = 0.0f;
        }

        /* Alpha mode */
        cJSON *j_am = cJSON_GetObjectItemCaseSensitive(j_mat, "alpha_mode");
        if (cJSON_IsString(j_am)) {
            if (SDL_strcmp(j_am->valuestring, "MASK") == 0) {
                m->alpha_mode = FORGE_PIPELINE_ALPHA_MASK;
            } else if (SDL_strcmp(j_am->valuestring, "BLEND") == 0) {
                m->alpha_mode = FORGE_PIPELINE_ALPHA_BLEND;
            } else {
                m->alpha_mode = FORGE_PIPELINE_ALPHA_OPAQUE;
            }
        } else {
            m->alpha_mode = FORGE_PIPELINE_ALPHA_OPAQUE;
        }

        j_val = cJSON_GetObjectItemCaseSensitive(j_mat, "alpha_cutoff");
        m->alpha_cutoff = cJSON_IsNumber(j_val)
                              ? (float)j_val->valuedouble : 0.5f;

        j_val = cJSON_GetObjectItemCaseSensitive(j_mat, "double_sided");
        m->double_sided = cJSON_IsBool(j_val) ? cJSON_IsTrue(j_val) : false;

        i++;
    }

    set->materials      = mats;
    set->material_count = count;

    cJSON_Delete(root);
    return true;
}

void forge_pipeline_free_materials(ForgePipelineMaterialSet *set)
{
    if (!set) {
        return;
    }
    SDL_free(set->materials);
    SDL_memset(set, 0, sizeof(*set));
}

/* ── Helper: build the .meta.json sidecar path from the image path ────
 *
 * Given "/some/dir/texture.png", produces "/some/dir/texture.meta.json".
 * The caller must SDL_free the returned string. Returns NULL on failure. */
static char *forge_pipeline__meta_json_path(const char *image_path)
{
    /* Find the last dot to strip the extension. */
    const char *dot = SDL_strrchr(image_path, '.');
    size_t stem_len;
    if (dot) {
        stem_len = (size_t)(dot - image_path);
    } else {
        stem_len = SDL_strlen(image_path);
    }

    const char *suffix = ".meta.json";
    size_t suffix_len = SDL_strlen(suffix);
    size_t total_len = stem_len + suffix_len + 1;

    char *result = (char *)SDL_malloc(total_len);
    if (!result) {
        return NULL;
    }
    SDL_memcpy(result, image_path, stem_len);
    SDL_memcpy(result + stem_len, suffix, suffix_len);
    result[stem_len + suffix_len] = '\0';
    return result;
}

/* ── Helper: build a mip-level file path ──────────────────────────────
 *
 * For mip 0 returns a copy of the base path.
 * For mip N>0, given "/dir/tex.png" produces "/dir/tex_mipN.png".
 * The caller must SDL_free the returned string. */
static char *forge_pipeline__mip_file_path(const char *base_path,
                                           uint32_t mip_level)
{
    if (mip_level == 0) {
        size_t len = SDL_strlen(base_path) + 1;
        char *copy = (char *)SDL_malloc(len);
        if (copy) {
            SDL_memcpy(copy, base_path, len);
        }
        return copy;
    }

    /* Split at last dot: "/dir/tex" + ".png" */
    const char *dot = SDL_strrchr(base_path, '.');
    size_t stem_len;
    const char *ext;
    if (dot) {
        stem_len = (size_t)(dot - base_path);
        ext = dot; /* includes the dot */
    } else {
        stem_len = SDL_strlen(base_path);
        ext = "";
    }

    /* "_mip" + up to 2 digits + ext + null */
    size_t ext_len = SDL_strlen(ext);
    size_t total = stem_len + 4 + 2 + ext_len + 1;
    char *result = (char *)SDL_malloc(total);
    if (!result) {
        return NULL;
    }

    SDL_memcpy(result, base_path, stem_len);
    int written = SDL_snprintf(result + stem_len, total - stem_len,
                               "_mip%u%s", mip_level, ext);
    (void)written;
    return result;
}

/* ── Helper: extract the directory portion of a path ──────────────────
 *
 * Given "/some/dir/file.png", returns "/some/dir/" (with trailing slash).
 * Given "file.png", returns "" (empty string).
 * The caller must SDL_free the returned string. */
static char *forge_pipeline__dir_of(const char *path)
{
    const char *last_slash = SDL_strrchr(path, '/');
#ifdef _WIN32
    const char *last_bslash = SDL_strrchr(path, '\\');
    if (last_bslash && (!last_slash || last_bslash > last_slash)) {
        last_slash = last_bslash;
    }
#endif
    size_t dir_len;
    if (last_slash) {
        dir_len = (size_t)(last_slash - path) + 1; /* include the slash */
    } else {
        dir_len = 0;
    }
    char *dir = (char *)SDL_malloc(dir_len + 1);
    if (dir) {
        if (dir_len > 0) {
            SDL_memcpy(dir, path, dir_len);
        }
        dir[dir_len] = '\0';
    }
    return dir;
}

bool forge_pipeline_load_texture(const char *path,
                                 ForgePipelineTexture *tex)
{
    /* ── Validate inputs ──────────────────────────────────────────────── */
    if (!path) {
        SDL_Log("forge_pipeline_load_texture: path is NULL");
        return false;
    }
    if (!tex) {
        SDL_Log("forge_pipeline_load_texture: tex is NULL");
        return false;
    }
    SDL_memset(tex, 0, sizeof(*tex));

    /* ── Build and load the .meta.json sidecar ────────────────────────
     * The meta file contains dimensions, format, and mip level info
     * produced by the pipeline's texture plugin. */
    char *meta_path = forge_pipeline__meta_json_path(path);
    if (!meta_path) {
        SDL_Log("forge_pipeline_load_texture: failed to build meta path");
        return false;
    }

    size_t meta_size = 0;
    char *meta_data = (char *)SDL_LoadFile(meta_path, &meta_size);
    if (!meta_data) {
        SDL_Log("forge_pipeline_load_texture: failed to load '%s': %s",
                meta_path, SDL_GetError());
        SDL_free(meta_path);
        return false;
    }
    SDL_free(meta_path);

    /* ── Parse JSON ───────────────────────────────────────────────────
     * Texture plugin format (pipeline/plugins/texture.py):
     * {
     *   "source": "texture.png",
     *   "output": "texture.png",
     *   "output_width": 1024, "output_height": 1024,
     *   "mip_levels": [
     *     { "level": 0, "width": 1024, "height": 1024 },
     *     { "level": 1, "width": 512,  "height": 512  },
     *     ...
     *   ],
     *   "settings": { ... }
     * }
     *
     * Also accepts a forward-compatible format:
     * { "width": 1024, "height": 1024, "mips": [...] }
     */
    cJSON *root = cJSON_ParseWithLength(meta_data, meta_size);
    SDL_free(meta_data);
    if (!root) {
        SDL_Log("forge_pipeline_load_texture: failed to parse meta JSON "
                "for '%s'", path);
        return false;
    }

    /* Width and height of the base (mip 0) image.
     * The texture plugin uses "output_width"/"output_height" for the
     * processed dimensions.  We accept both that format and a simpler
     * "width"/"height" form for forward compatibility. */
    cJSON *j_width  = cJSON_GetObjectItemCaseSensitive(root, "output_width");
    cJSON *j_height = cJSON_GetObjectItemCaseSensitive(root, "output_height");
    if (!cJSON_IsNumber(j_width)) {
        j_width = cJSON_GetObjectItemCaseSensitive(root, "width");
    }
    if (!cJSON_IsNumber(j_height)) {
        j_height = cJSON_GetObjectItemCaseSensitive(root, "height");
    }
    if (!cJSON_IsNumber(j_width) || !cJSON_IsNumber(j_height) ||
        j_width->valuedouble <= 0.0 || j_height->valuedouble <= 0.0) {
        SDL_Log("forge_pipeline_load_texture: meta JSON missing or invalid "
                "width/height for '%s'", path);
        cJSON_Delete(root);
        return false;
    }
    tex->width  = (uint32_t)j_width->valueint;
    tex->height = (uint32_t)j_height->valueint;

    /* Format — default to RGBA8 if not specified */
    cJSON *j_format = cJSON_GetObjectItemCaseSensitive(root, "format");
    if (cJSON_IsString(j_format) &&
        SDL_strcmp(j_format->valuestring, "rgb8") == 0) {
        tex->format = FORGE_PIPELINE_TEX_RGB8;
    } else {
        tex->format = FORGE_PIPELINE_TEX_RGBA8;
    }

    /* ── Parse mip array ──────────────────────────────────────────────
     * The texture plugin uses "mip_levels" (array of {level, width, height}).
     * We also accept a "mips" array (with {file, width, height}) for
     * forward compatibility.  If neither is present, treat the base file
     * as a single mip level (mip 0). */
    cJSON *j_mips = cJSON_GetObjectItemCaseSensitive(root, "mip_levels");
    if (!cJSON_IsArray(j_mips)) {
        j_mips = cJSON_GetObjectItemCaseSensitive(root, "mips");
    }
    uint32_t mip_count = 0;

    if (cJSON_IsArray(j_mips)) {
        mip_count = (uint32_t)cJSON_GetArraySize(j_mips);
    }

    if (mip_count == 0) {
        /* No mip array — load the single base file as mip 0. */
        mip_count = 1;
    }

    if (mip_count > FORGE_PIPELINE_MAX_MIP_LEVELS) {
        SDL_Log("forge_pipeline_load_texture: '%s' has %u mip levels "
                "(max %d)", path, mip_count, FORGE_PIPELINE_MAX_MIP_LEVELS);
        cJSON_Delete(root);
        return false;
    }

    /* ── Allocate mip array ───────────────────────────────────────── */
    tex->mips = (ForgePipelineMipLevel *)SDL_calloc(
        mip_count, sizeof(ForgePipelineMipLevel));
    if (!tex->mips) {
        SDL_Log("forge_pipeline_load_texture: allocation failed for mips");
        cJSON_Delete(root);
        return false;
    }
    tex->mip_count = mip_count;

    /* ── Get the directory of the meta file for resolving relative paths ─ */
    char *base_dir = forge_pipeline__dir_of(path);
    if (!base_dir) {
        SDL_Log("forge_pipeline_load_texture: allocation failed for base_dir");
        cJSON_Delete(root);
        forge_pipeline_free_texture(tex);
        return false;
    }

    /* ── Load each mip level ──────────────────────────────────────── */
    bool ok = true;
    for (uint32_t i = 0; i < mip_count && ok; i++) {
        char *mip_path = NULL;
        uint32_t mip_w = tex->width >> i;
        uint32_t mip_h = tex->height >> i;
        if (mip_w == 0) mip_w = 1;
        if (mip_h == 0) mip_h = 1;

        if (cJSON_IsArray(j_mips)) {
            /* Use the file path and dimensions from the JSON entry. */
            cJSON *entry = cJSON_GetArrayItem(j_mips, (int)i);
            cJSON *j_file = cJSON_GetObjectItemCaseSensitive(entry, "file");
            cJSON *j_mw   = cJSON_GetObjectItemCaseSensitive(entry, "width");
            cJSON *j_mh   = cJSON_GetObjectItemCaseSensitive(entry, "height");

            if (cJSON_IsString(j_file)) {
                /* Build full path: base_dir + mip filename */
                size_t dir_len  = SDL_strlen(base_dir);
                size_t file_len = SDL_strlen(j_file->valuestring);
                mip_path = (char *)SDL_malloc(dir_len + file_len + 1);
                if (mip_path) {
                    SDL_memcpy(mip_path, base_dir, dir_len);
                    SDL_memcpy(mip_path + dir_len,
                               j_file->valuestring, file_len + 1);
                }
            }

            if (cJSON_IsNumber(j_mw) && j_mw->valuedouble > 0.0)
                mip_w = (uint32_t)j_mw->valueint;
            if (cJSON_IsNumber(j_mh) && j_mh->valuedouble > 0.0)
                mip_h = (uint32_t)j_mh->valueint;
        }

        /* Fallback: generate mip path from the base image path. */
        if (!mip_path) {
            mip_path = forge_pipeline__mip_file_path(path, i);
        }

        if (!mip_path) {
            SDL_Log("forge_pipeline_load_texture: failed to build path "
                    "for mip %u of '%s'", i, path);
            ok = false;
            break;
        }

        /* Load the raw file bytes for this mip level. */
        size_t data_size = 0;
        void *data = SDL_LoadFile(mip_path, &data_size);
        if (!data) {
            SDL_Log("forge_pipeline_load_texture: failed to load mip %u "
                    "from '%s': %s", i, mip_path, SDL_GetError());
            SDL_free(mip_path);
            ok = false;
            break;
        }

        if (data_size > UINT32_MAX) {
            SDL_Log("forge_pipeline_load_texture: mip %u of '%s' exceeds "
                    "4 GB size limit", i, path);
            SDL_free(data);
            SDL_free(mip_path);
            ok = false;
            break;
        }

        tex->mips[i].data   = data;
        tex->mips[i].width  = mip_w;
        tex->mips[i].height = mip_h;
        tex->mips[i].size   = (uint32_t)data_size;

        SDL_free(mip_path);
    }

    SDL_free(base_dir);
    cJSON_Delete(root);

    if (!ok) {
        forge_pipeline_free_texture(tex);
        return false;
    }

    return true;
}

void forge_pipeline_free_texture(ForgePipelineTexture *tex)
{
    if (!tex) {
        return;
    }
    if (tex->mips) {
        for (uint32_t i = 0; i < tex->mip_count; i++) {
            SDL_free(tex->mips[i].data);
        }
        SDL_free(tex->mips);
    }
    SDL_memset(tex, 0, sizeof(*tex));
}

#endif /* FORGE_PIPELINE_IMPLEMENTATION */

#endif /* FORGE_PIPELINE_H */
