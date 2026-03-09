/*
 * forge_pipeline.h — Asset pipeline runtime library for forge-gpu
 *
 * Loads processed assets produced by the forge-gpu asset pipeline:
 *   - .fmesh files (optimised meshes with LOD levels and optional tangents)
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
#define FORGE_PIPELINE_FMESH_VERSION 1

/* .fmesh header layout sizes (bytes) */
#define FORGE_PIPELINE_HEADER_SIZE    32
#define FORGE_PIPELINE_LOD_ENTRY_SIZE 12

/* Vertex stride values — determines whether tangent data is present.
 * Stride 32: position(12) + normal(12) + uv(8)
 * Stride 48: position(12) + normal(12) + uv(8) + tangent(16) */
#define FORGE_PIPELINE_VERTEX_STRIDE_NO_TAN 32
#define FORGE_PIPELINE_VERTEX_STRIDE_TAN    48

/* Flags stored in the .fmesh header */
#define FORGE_PIPELINE_FLAG_TANGENTS (1u << 0)

/* .fmesh magic identifier size */
#define FORGE_PIPELINE_FMESH_MAGIC_SIZE 4

/* Size of the reserved padding at the end of the .fmesh header (bytes) */
#define FORGE_PIPELINE_HEADER_RESERVED 8

/* Upper bounds for validation */
#define FORGE_PIPELINE_MAX_LODS       8
#define FORGE_PIPELINE_MAX_MIP_LEVELS 16

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

/* One LOD entry from the .fmesh file. */
typedef struct ForgePipelineLod {
    uint32_t index_count;  /* number of indices in this LOD level */
    uint32_t index_offset; /* byte offset into the concatenated index section */
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
typedef struct ForgePipelineMesh {
    void             *vertices;     /* vertex array (cast to Vertex or VertexTan) */
    uint32_t         *indices;      /* all LOD indices concatenated (uint32) */
    uint32_t          vertex_count; /* number of vertices in the array */
    uint32_t          vertex_stride;/* bytes per vertex (32 or 48) */
    ForgePipelineLod *lods;         /* LOD table (lod_count entries) */
    uint32_t          lod_count;    /* number of LOD levels (1..MAX_LODS) */
    uint32_t          flags;        /* bit field (see FORGE_PIPELINE_FLAG_*) */
} ForgePipelineMesh;

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

    /* Version */
    uint32_t version = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (version != FORGE_PIPELINE_FMESH_VERSION) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has unsupported version %u "
                "(expected %d)", path, version, FORGE_PIPELINE_FMESH_VERSION);
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

    /* Skip reserved padding at end of 32-byte header */
    p += FORGE_PIPELINE_HEADER_RESERVED;

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

    /* ── Validate file size against expected data ─────────────────── */
    size_t lod_section_size = (size_t)lod_count * FORGE_PIPELINE_LOD_ENTRY_SIZE;
    size_t vertex_section_size = (size_t)vertex_count * vertex_stride;

    /* We need the LOD entries to compute total indices, so first check
     * that the file is large enough to contain the LOD table. */
    size_t min_size_for_lods = FORGE_PIPELINE_HEADER_SIZE + lod_section_size;
    if (file_size < min_size_for_lods) {
        SDL_Log("forge_pipeline_load_mesh: '%s' too small for LOD table "
                "(%zu bytes, need %zu)", path, file_size, min_size_for_lods);
        SDL_free(file_data);
        return false;
    }

    /* ── Read LOD entries ─────────────────────────────────────────── */
    ForgePipelineLod *lods = (ForgePipelineLod *)SDL_calloc(
        lod_count, sizeof(ForgePipelineLod));
    if (!lods) {
        SDL_Log("forge_pipeline_load_mesh: allocation failed for LOD entries");
        SDL_free(file_data);
        return false;
    }

    size_t total_indices = 0;
    size_t expected_index_offset = 0;
    for (uint32_t i = 0; i < lod_count; i++) {
        lods[i].index_count  = forge_pipeline__read_u32_le(p);
        p += sizeof(uint32_t);
        lods[i].index_offset = forge_pipeline__read_u32_le(p);
        p += sizeof(uint32_t);
        lods[i].target_error = forge_pipeline__read_f32_le(p);
        p += sizeof(uint32_t); /* float stored as uint32 bits */

        /* Validate that index_offset is aligned and contiguous */
        if ((lods[i].index_offset % sizeof(uint32_t)) != 0 ||
            (size_t)lods[i].index_offset != expected_index_offset) {
            SDL_Log("forge_pipeline_load_mesh: '%s' has invalid index_offset "
                    "for LOD %u (got %u, expected %zu)",
                    path, i, lods[i].index_offset, expected_index_offset);
            SDL_free(lods);
            SDL_free(file_data);
            return false;
        }
        expected_index_offset += (size_t)lods[i].index_count * sizeof(uint32_t);
        total_indices += lods[i].index_count;
    }

    /* ── Validate full file size ──────────────────────────────────── */
    size_t index_section_size = total_indices * sizeof(uint32_t);
    size_t expected_size = FORGE_PIPELINE_HEADER_SIZE + lod_section_size
                         + vertex_section_size + index_section_size;
    if (file_size < expected_size) {
        SDL_Log("forge_pipeline_load_mesh: '%s' is truncated "
                "(%zu bytes, expected %zu)", path, file_size, expected_size);
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }

    /* ── Copy vertex data ─────────────────────────────────────────── */
    void *vertices = SDL_malloc(vertex_section_size);
    if (!vertices) {
        SDL_Log("forge_pipeline_load_mesh: allocation failed for vertices "
                "(%zu bytes)", vertex_section_size);
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
        SDL_free(lods);
        SDL_free(file_data);
        return false;
    }
    SDL_memcpy(indices, p, index_section_size);

    /* ── Populate output struct ───────────────────────────────────── */
    mesh->vertices     = vertices;
    mesh->indices      = indices;
    mesh->vertex_count = vertex_count;
    mesh->vertex_stride = vertex_stride;
    mesh->lods         = lods;
    mesh->lod_count    = lod_count;
    mesh->flags        = flags;

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
