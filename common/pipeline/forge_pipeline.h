/* This is production-quality library code, not a lesson demo. See CLAUDE.md "Library quality standards". */
/*
 * forge_pipeline.h — Asset pipeline runtime library for forge-gpu
 *
 * Loads processed assets produced by the forge-gpu asset pipeline:
 *   - .fmesh v2 files (optimised meshes with submeshes, LODs, tangents)
 *   - .fmat files (PBR material sidecars — JSON)
 *   - .fscene files (node hierarchy with transforms and mesh references)
 *   - .fanim files (animation clips with samplers and channels)
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
 *   - string.h     (not needed — uses SDL_memcpy, SDL_memcmp, SDL_memset)
 *
 * See tools/mesh/main.c for the .fmesh writer.
 * See lessons/assets/ for walkthroughs of each pipeline stage.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_PIPELINE_H
#define FORGE_PIPELINE_H

#include <SDL3/SDL.h>
#include <float.h>
#include <stdint.h>
/* string.h no longer needed — all mem ops use SDL equivalents */

/* cJSON is used for parsing texture .meta.json sidecars. */
#include "cJSON.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

/* .fscene binary format identifiers */
#define FORGE_PIPELINE_FSCENE_MAGIC      "FSCN"
#define FORGE_PIPELINE_FSCENE_VERSION    1
#define FORGE_PIPELINE_FSCENE_HEADER_SIZE 24
#define FORGE_PIPELINE_FSCENE_NODE_SIZE  192
#define FORGE_PIPELINE_FSCENE_MAGIC_SIZE 4

/* Upper bounds for .fscene validation */
#define FORGE_PIPELINE_MAX_NODES      4096
#define FORGE_PIPELINE_MAX_ROOTS      256
#define FORGE_PIPELINE_MAX_SCENE_MESHES 1024

/* Visit states for cycle detection in world transform propagation */
#define FORGE_PIPELINE_VISIT_UNVISITED    0
#define FORGE_PIPELINE_VISIT_IN_PROGRESS  1
#define FORGE_PIPELINE_VISIT_DONE         2

/* .fmesh binary format identifiers */
#define FORGE_PIPELINE_FMESH_MAGIC   "FMSH"
#define FORGE_PIPELINE_FMESH_VERSION      2
#define FORGE_PIPELINE_FMESH_VERSION_SKIN 3

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

/* .fanim binary format identifiers */
#define FORGE_PIPELINE_FANIM_MAGIC       "FANM"
#define FORGE_PIPELINE_FANIM_MAGIC_SIZE  4
#define FORGE_PIPELINE_FANIM_VERSION     1
#define FORGE_PIPELINE_FANIM_HEADER_SIZE 12

/* .fanim clip header: 64B name + 4B duration + 4B sampler_count +
 * 4B channel_count */
#define FORGE_PIPELINE_CLIP_HEADER_SIZE    76

/* .fanim sampler header: 3 × u32 (keyframe_count, value_components,
 * interpolation) */
#define FORGE_PIPELINE_SAMPLER_HEADER_SIZE 12

/* Upper bounds for .fanim validation */
#define FORGE_PIPELINE_MAX_ANIM_CLIPS    256
#define FORGE_PIPELINE_MAX_ANIM_SAMPLERS 512
#define FORGE_PIPELINE_MAX_ANIM_CHANNELS 512
#define FORGE_PIPELINE_MAX_KEYFRAMES     65536

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

/* ── Compressed texture types ──────────────────────────────────────────── */

/* .ftex file format constants — pre-transcoded GPU-ready textures.
 * Produced at build time by forge_texture_tool; loaded at runtime with
 * forge_pipeline_load_ftex().  No transcoding library needed at runtime. */
#define FORGE_PIPELINE_FTEX_MAGIC            0x58455446  /* "FTEX" little-endian */
#define FORGE_PIPELINE_FTEX_VERSION          1
#define FORGE_PIPELINE_FTEX_HEADER_SIZE      32  /* 8 × uint32 fields */
#define FORGE_PIPELINE_FTEX_MIP_ENTRY_SIZE   16  /* 4 × uint32 per mip level */
#define FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS   32  /* max mip levels in .ftex (supports up to 4G×4G) */

/* GPU block-compressed format stored in .ftex files.
 * These map directly to SDL_GPU_TEXTUREFORMAT_* values. */
typedef enum ForgePipelineCompressedFormat {
    FORGE_PIPELINE_COMPRESSED_BC7_SRGB  = 1, /* BC7 in sRGB color space (base color, emissive) */
    FORGE_PIPELINE_COMPRESSED_BC7_UNORM = 2, /* BC7 in linear space (metallic-roughness, occlusion) */
    FORGE_PIPELINE_COMPRESSED_BC5_UNORM = 3  /* BC5 two-channel (normal maps: RG only) */
} ForgePipelineCompressedFormat;

/* One mip level of a compressed texture — GPU-ready block data. */
typedef struct ForgePipelineCompressedMip {
    void    *data;       /* GPU-ready compressed blocks (points into parent _file_buf) */
    uint32_t data_size;  /* byte count of compressed data */
    uint32_t width;      /* mip level width in pixels */
    uint32_t height;     /* mip level height in pixels */
} ForgePipelineCompressedMip;

/* A transcoded compressed texture ready for GPU upload. */
typedef struct ForgePipelineCompressedTexture {
    ForgePipelineCompressedMip   *mips;       /* mip chain array (mip_count entries) */
    uint32_t                      mip_count;  /* number of mip levels */
    uint32_t                      width;      /* base (mip 0) width */
    uint32_t                      height;     /* base (mip 0) height */
    ForgePipelineCompressedFormat format;     /* target GPU format */
    void                         *_file_buf;  /* internal: backing buffer for mip data pointers */
} ForgePipelineCompressedTexture;

/* Compression info parsed from a .meta.json sidecar. */
typedef struct ForgePipelineCompressionInfo {
    bool has_compression;  /* true if a "compression" block was found */
    char codec[32];        /* e.g. "uastc", "etc1s", "astc" */
    char container[32];    /* e.g. "ktx2", ".astc" */
    char compressed_file[FORGE_PIPELINE_MAT_PATH_SIZE]; /* relative path to compressed file */
    float ratio;           /* compression ratio (uncompressed / compressed) */
} ForgePipelineCompressionInfo;

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

/* ── Scene types ───────────────────────────────────────────────────────── */

/* A single node in the scene hierarchy.
 * Each node has an optional mesh reference, parent-child relationships,
 * TRS decomposition (for animation), and both local and world transforms.
 * World transforms are computed at load time by walking the hierarchy. */
typedef struct ForgePipelineSceneNode {
    char     name[64];            /* node name (null-terminated) */
    int32_t  parent;              /* parent node index, -1 = root */
    int32_t  mesh_index;          /* glTF mesh index, -1 = no mesh */
    int32_t  skin_index;          /* glTF skin index, -1 = no skin */
    uint32_t first_child;         /* index into scene.children[] */
    uint32_t child_count;         /* number of children */
    uint32_t has_trs;             /* 1 = TRS valid, 0 = raw matrix only */
    float    translation[3];      /* T component (default 0,0,0) */
    float    rotation[4];         /* R as quaternion xyzw (default 0,0,0,1) */
    float    scale[3];            /* S component (default 1,1,1) */
    float    local_transform[16]; /* column-major 4x4 local matrix */
    float    world_transform[16]; /* column-major 4x4 world matrix (computed) */
} ForgePipelineSceneNode;

/* Maps a glTF mesh index to a range of submeshes in the .fmesh file.
 * The renderer uses this to find which draw calls belong to a given node. */
typedef struct ForgePipelineSceneMesh {
    uint32_t first_submesh;  /* first submesh index in the .fmesh */
    uint32_t submesh_count;  /* number of submeshes (primitives) */
} ForgePipelineSceneMesh;

/* A loaded .fscene file — the glTF node hierarchy for a model.
 * Nodes reference meshes by index into the mesh table; the mesh table
 * maps each glTF mesh to a range of submeshes in the .fmesh file.
 * World transforms are computed at load time from the node tree. */
typedef struct ForgePipelineScene {
    ForgePipelineSceneNode *nodes;    /* node_count entries */
    uint32_t                node_count;  /* number of nodes in the hierarchy */
    ForgePipelineSceneMesh *meshes;   /* mesh_count entries */
    uint32_t                mesh_count;  /* number of glTF meshes */
    uint32_t               *roots;    /* root_count root node indices */
    uint32_t                root_count;  /* number of root nodes (no parent) */
    uint32_t               *children; /* flat children array referenced by nodes */
    uint32_t                child_count; /* total entries in children array */
} ForgePipelineScene;

/* ── Animation types ───────────────────────────────────────────────────── */

/* Interpolation mode for animation samplers. */
typedef enum ForgePipelineAnimInterp {
    FORGE_PIPELINE_INTERP_LINEAR = 0,
    FORGE_PIPELINE_INTERP_STEP   = 1
} ForgePipelineAnimInterp;

/* Target property for animation channels. */
typedef enum ForgePipelineAnimPath {
    FORGE_PIPELINE_ANIM_TRANSLATION = 0,
    FORGE_PIPELINE_ANIM_ROTATION    = 1,
    FORGE_PIPELINE_ANIM_SCALE       = 2
} ForgePipelineAnimPath;

/* A single animation sampler: keyframe timestamps and output values.
 * Translation/scale have 3 components, rotation has 4 (quaternion). */
typedef struct ForgePipelineAnimSampler {
    float   *timestamps;        /* heap-allocated, keyframe_count floats */
    float   *values;            /* keyframe_count * value_components floats */
    uint32_t keyframe_count;    /* number of keyframes in this sampler */
    uint32_t value_components;  /* 3 (translation/scale) or 4 (rotation) */
    ForgePipelineAnimInterp interpolation; /* LINEAR or STEP */
} ForgePipelineAnimSampler;

/* A single animation channel: links a sampler to a node property. */
typedef struct ForgePipelineAnimChannel {
    int32_t               target_node;   /* scene node index (-1 if unset) */
    ForgePipelineAnimPath target_path;   /* translation, rotation, or scale */
    uint32_t              sampler_index; /* index into the clip's sampler array */
} ForgePipelineAnimChannel;

/* One animation clip with its samplers and channels. */
typedef struct ForgePipelineAnimation {
    char     name[64];                    /* clip name (null-terminated) */
    float    duration;                    /* max timestamp across all samplers (seconds) */
    ForgePipelineAnimSampler *samplers;   /* heap-allocated sampler array */
    uint32_t                  sampler_count; /* number of samplers */
    ForgePipelineAnimChannel *channels;   /* heap-allocated channel array */
    uint32_t                  channel_count; /* number of channels */
} ForgePipelineAnimation;

/* A loaded .fanim file containing one or more animation clips. */
typedef struct ForgePipelineAnimFile {
    ForgePipelineAnimation *clips;      /* heap-allocated clip array */
    uint32_t                clip_count; /* number of clips in the file */
} ForgePipelineAnimFile;

/* ── Animation manifest types (.fanims) ───────────────────────────────── */

/* .fanims manifest version */
#define FORGE_PIPELINE_FANIMS_VERSION     1

/* Upper bounds for .fanims manifest validation */
#define FORGE_PIPELINE_MAX_ANIM_SET_CLIPS 256
#define FORGE_PIPELINE_MAX_CLIP_TAGS      16
#define FORGE_PIPELINE_MAX_TAG_LEN        32

/* Information about one animation clip from a .fanims manifest. */
typedef struct ForgePipelineAnimClipInfo {
    char     name[64];        /* clip name (manifest key) */
    char     file[256];       /* relative .fanim path */
    float    duration;        /* clip duration in seconds */
    bool     loop;            /* true if the clip should loop */
    char     tags[FORGE_PIPELINE_MAX_CLIP_TAGS][FORGE_PIPELINE_MAX_TAG_LEN];
                              /* user-defined tags (e.g. "locomotion", "idle") */
    uint32_t tag_count;       /* number of valid tags */
} ForgePipelineAnimClipInfo;

/* A loaded .fanims manifest — metadata for a set of per-clip .fanim files. */
typedef struct ForgePipelineAnimSet {
    char     model[64];       /* model name from manifest */
    ForgePipelineAnimClipInfo *clips; /* heap-allocated clip info array */
    uint32_t                   clip_count; /* number of clips in the manifest */
    char     base_dir[512];   /* directory containing the manifest (for path resolution) */
} ForgePipelineAnimSet;

/* ── Skin types (.fskin) ──────────────────────────────────────────────── */

/* .fskin binary format identifiers */
#define FORGE_PIPELINE_FSKIN_MAGIC       "FSKN"
#define FORGE_PIPELINE_FSKIN_MAGIC_SIZE  4
#define FORGE_PIPELINE_FSKIN_VERSION     1
#define FORGE_PIPELINE_FSKIN_HEADER_SIZE 12

/* Upper bounds for .fskin validation */
#define FORGE_PIPELINE_MAX_SKINS         64
#define FORGE_PIPELINE_MAX_SKIN_JOINTS   256

/* A single skin: joint hierarchy and inverse bind matrices. */
typedef struct ForgePipelineSkin {
    char     name[64];              /* skin name (null-terminated) */
    int32_t *joints;                /* joint_count node indices */
    float   *inverse_bind_matrices; /* joint_count * 16 floats (column-major) */
    uint32_t joint_count;           /* number of joints in this skin */
    int32_t  skeleton;              /* root joint node, -1 if unset */
} ForgePipelineSkin;

/* A loaded .fskin file containing one or more skins. */
typedef struct ForgePipelineSkinSet {
    ForgePipelineSkin *skins;       /* heap-allocated skin array */
    uint32_t           skin_count;  /* number of skins in the file */
} ForgePipelineSkinSet;

/* ── Skinned vertex types ─────────────────────────────────────────────── */

/* .fmesh v3 flag for skinned vertex data */
#define FORGE_PIPELINE_FLAG_SKINNED (1u << 1)

/* Skinned vertex strides:
 * Stride 56: position(12) + normal(12) + uv(8) + joints(8) + weights(16)
 * Stride 72: position(12) + normal(12) + uv(8) + tangent(16) + joints(8) + weights(16) */
#define FORGE_PIPELINE_VERTEX_STRIDE_SKIN     56
#define FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN 72

/* Skinned vertex without tangent data (stride 56). */
typedef struct ForgePipelineVertexSkin {
    float    position[3];   /* 12B */
    float    normal[3];     /* 12B */
    float    uv[2];         /*  8B */
    uint16_t joints[4];     /*  8B — bone indices */
    float    weights[4];    /* 16B — blend weights */
} ForgePipelineVertexSkin;  /* 56B total */

/* Skinned vertex with tangent data (stride 72). */
typedef struct ForgePipelineVertexSkinTan {
    float    position[3];   /* 12B */
    float    normal[3];     /* 12B */
    float    uv[2];         /*  8B */
    float    tangent[4];    /* 16B — xyz = tangent direction, w = bitangent sign */
    uint16_t joints[4];     /*  8B — bone indices */
    float    weights[4];    /* 16B — blend weights */
} ForgePipelineVertexSkinTan; /* 72B total */

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

/*
 * forge_pipeline_detect_sidecar — Check .meta.json for compression info.
 *
 * Given the path to an image file (e.g. "texture.jpg"), looks for the
 * corresponding .meta.json sidecar and parses the "compression" block.
 * Populates `info` with codec, container, compressed file path, etc.
 *
 * Returns true if the sidecar was found and parsed (even if no compression
 * block is present — check info->has_compression).  Returns false if the
 * sidecar doesn't exist or can't be parsed.
 */
bool forge_pipeline_detect_sidecar(const char *image_path,
                                    ForgePipelineCompressionInfo *info);

/*
 * forge_pipeline_load_ftex — Load a pre-transcoded .ftex texture.
 *
 * Reads a .ftex file produced at build time by forge_texture_tool.
 * The file contains GPU-ready BC7/BC5 block data with a full mip chain —
 * no transcoding library is needed at runtime.
 *
 * The result contains one mip level per stored mip, each with a pointer
 * into a single heap-allocated buffer.  The caller owns the memory and
 * must call forge_pipeline_free_compressed_texture() when done.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_ftex(const char *ftex_path,
                               ForgePipelineCompressedTexture *tex);

/*
 * forge_pipeline_free_compressed_texture — Release compressed texture memory.
 *
 * Safe to call on a zeroed struct (no-op).
 */
void forge_pipeline_free_compressed_texture(ForgePipelineCompressedTexture *tex);

/*
 * forge_pipeline_load_scene — Load a .fscene binary file.
 *
 * Reads the file at `path`, validates the header, and populates `scene`
 * with allocated node, mesh, root, and children data.  World transforms
 * are computed by walking the hierarchy (parent's world * child's local).
 *
 * The caller owns the memory and must call forge_pipeline_free_scene()
 * when done.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_scene(const char *path, ForgePipelineScene *scene);

/*
 * forge_pipeline_free_scene — Release all memory owned by a scene.
 *
 * Safe to call on a zeroed or already-freed scene (no-op if scene is NULL).
 */
void forge_pipeline_free_scene(ForgePipelineScene *scene);

/*
 * forge_pipeline_scene_get_mesh — Look up submesh range for a mesh index.
 *
 * Returns a pointer to the mesh entry at `mesh_index`, or NULL if the
 * index is out of range.  Use the returned first_submesh and submesh_count
 * to index into the .fmesh submesh table.
 */
const ForgePipelineSceneMesh *forge_pipeline_scene_get_mesh(
    const ForgePipelineScene *scene, uint32_t mesh_index);

/*
 * forge_pipeline_load_animation — Load a .fanim binary file.
 *
 * Reads the file at `path`, validates the header, and populates `file`
 * with allocated clip, sampler, and channel data.  Per-sampler timestamp
 * and value arrays are individually heap-allocated.  The caller owns the
 * memory and must call forge_pipeline_free_animation() when done.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_animation(const char *path,
                                    ForgePipelineAnimFile *file);

/*
 * forge_pipeline_free_animation — Release all memory owned by an anim file.
 *
 * Frees per-sampler arrays, per-clip sampler/channel arrays, the clips
 * array, and zeroes the struct.  Safe to call on a zeroed, NULL, or
 * already-freed file.
 */
void forge_pipeline_free_animation(ForgePipelineAnimFile *file);

/*
 * forge_pipeline_load_anim_set — Load a .fanims JSON manifest.
 *
 * Reads the file at `path`, parses the manifest, and populates `set`
 * with allocated clip info data.  The base_dir is set to the directory
 * containing the manifest file for resolving relative .fanim paths.
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_anim_set(const char *path,
                                   ForgePipelineAnimSet *set);

/*
 * forge_pipeline_free_anim_set — Release all memory owned by an anim set.
 *
 * Safe to call on a zeroed, NULL, or already-freed set.
 */
void forge_pipeline_free_anim_set(ForgePipelineAnimSet *set);

/*
 * forge_pipeline_find_clip — Find a clip by name in an animation set.
 *
 * Returns a pointer to the clip info, or NULL if not found.
 * The returned pointer is valid until the set is freed.
 */
const ForgePipelineAnimClipInfo *forge_pipeline_find_clip(
    const ForgePipelineAnimSet *set, const char *name);

/*
 * forge_pipeline_load_clip — Load a clip by name from an animation set.
 *
 * Convenience: finds the clip in the manifest, constructs the full path
 * from base_dir + file, and calls forge_pipeline_load_animation().
 *
 * Returns true on success, false if the clip is not found or loading fails.
 */
bool forge_pipeline_load_clip(const ForgePipelineAnimSet *set,
                               const char *name,
                               ForgePipelineAnimFile *file);

/*
 * forge_pipeline_load_skins — Load a .fskin binary file.
 *
 * Reads the file at `path`, validates the header, and populates `skins`
 * with allocated skin data (joints and inverse bind matrices).
 * The caller owns the memory and must call forge_pipeline_free_skins().
 *
 * Returns true on success, false on any error (logged via SDL_Log).
 */
bool forge_pipeline_load_skins(const char *path,
                                ForgePipelineSkinSet *skins);

/*
 * forge_pipeline_free_skins — Release all memory owned by a skin set.
 *
 * Safe to call on a zeroed, NULL, or already-freed set.
 */
void forge_pipeline_free_skins(ForgePipelineSkinSet *skins);

/*
 * forge_pipeline_has_skin_data — Check if the mesh has skinned vertices.
 */
bool forge_pipeline_has_skin_data(const ForgePipelineMesh *mesh);

/* ── Animation evaluation ─────────────────────────────────────────────── */

/* These functions require forge_math.h (vec3, quat, mat4).  They are only
 * declared when forge_math.h has already been included.  forge_scene.h
 * includes forge_math.h before this header, so they are always available
 * when using the scene renderer. */
#ifdef FORGE_MATH_H

/* Small epsilon for timestamp comparisons in pipeline animation. */
#define FORGE_PIPELINE_ANIM_EPSILON 1e-7f

/*
 * forge_pipeline_anim_apply — Evaluate animation at time t, update node TRS.
 *
 * For each channel in the animation, samples the keyframe data at time t
 * using binary search + interpolation (vec3_lerp for translation/scale,
 * quat_slerp for rotation).  Updates the target node's translation,
 * rotation, scale, and recomputes its local_transform = T × R × S.
 *
 * Only nodes targeted by animation channels have their local_transform
 * rebuilt.  Non-animated nodes keep their existing local_transform, so
 * callers must ensure local_transform is consistent with TRS before the
 * first call (the scene loader sets this up automatically).
 *
 * If loop is true, wraps t to [0, duration).  Otherwise clamps.
 * After calling this, call forge_pipeline_scene_compute_world_transforms()
 * to propagate changes through the hierarchy.
 */
void forge_pipeline_anim_apply(
    const ForgePipelineAnimation *anim,
    ForgePipelineSceneNode *nodes, uint32_t node_count,
    float t, bool loop);

/*
 * forge_pipeline_scene_compute_world_transforms — Rebuild world transforms.
 *
 * Walks the scene hierarchy from root nodes downward, computing
 * world_transform = parent_world × local_transform for each node.
 * Root nodes use the identity as their parent transform.
 *
 * `children` / `child_count` are the flat child index array from
 * ForgePipelineScene (scene.children, scene.child_count).  Each node's
 * first_child + child_count index into this array.  Pass NULL / 0 when
 * no child array is available — nodes with child_count > 0 will not
 * recurse (only root-level nodes get their world transforms set).
 *
 * Must be called after forge_pipeline_anim_apply() to propagate
 * updated local transforms through the hierarchy.
 */
void forge_pipeline_scene_compute_world_transforms(
    ForgePipelineSceneNode *nodes, uint32_t node_count,
    const uint32_t *root_nodes, uint32_t root_count,
    const uint32_t *children, uint32_t child_count);

/*
 * forge_pipeline_compute_joint_matrices — Compute skinning joint matrices.
 *
 * For each joint in the skin, computes:
 *   joint_matrix[i] = inv(mesh_world) × joint_world × IBM[i]
 *
 * where mesh_world is the world transform of the node that owns the mesh,
 * joint_world is the world transform of the joint node, and IBM is the
 * inverse bind matrix from the skin data.
 *
 * Writes to out_matrices[] (up to max_joints entries).
 * Returns the number of joint matrices written.
 */
uint32_t forge_pipeline_compute_joint_matrices(
    const ForgePipelineSkin *skin,
    const ForgePipelineSceneNode *nodes, uint32_t node_count,
    int mesh_node_index,
    mat4 *out_matrices, uint32_t max_joints);

#endif /* FORGE_MATH_H — animation evaluation requires forge_math.h */

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

    /* Version — v2 or v3 */
    uint32_t version = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (version != FORGE_PIPELINE_FMESH_VERSION &&
        version != FORGE_PIPELINE_FMESH_VERSION_SKIN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has unsupported version %u "
                "(expected %u or %u)", path, version,
                FORGE_PIPELINE_FMESH_VERSION,
                FORGE_PIPELINE_FMESH_VERSION_SKIN);
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
        vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_TAN &&
        vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_SKIN &&
        vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has invalid vertex stride %u",
                path, vertex_stride);
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

    /* Reject unknown flag bits — prevents misinterpreting corrupt or
     * newer-version files that use flags this loader doesn't understand. */
    {
        const uint32_t known_flags =
            FORGE_PIPELINE_FLAG_TANGENTS | FORGE_PIPELINE_FLAG_SKINNED;
        if ((flags & ~known_flags) != 0u) {
            SDL_Log("forge_pipeline_load_mesh: '%s' has unknown flags 0x%08x",
                    path, flags & ~known_flags);
            SDL_free(file_data);
            return false;
        }
    }

    /* ── Validate flag / stride consistency ───────────────────────── */
    bool has_tangents = (flags & FORGE_PIPELINE_FLAG_TANGENTS) != 0;
    bool has_skin = (flags & FORGE_PIPELINE_FLAG_SKINNED) != 0;

    /* Tangent flag must match tangent-capable strides */
    if (has_tangents && vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_TAN &&
        vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has TANGENTS flag but "
                "stride is %u", path, vertex_stride);
        SDL_free(file_data);
        return false;
    }
    if (!has_tangents && (vertex_stride == FORGE_PIPELINE_VERTEX_STRIDE_TAN ||
        vertex_stride == FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN)) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has tangent stride %u "
                "but FORGE_PIPELINE_FLAG_TANGENTS is not set",
                path, vertex_stride);
        SDL_free(file_data);
        return false;
    }

    /* Skin flag must match skinned strides */
    if (has_skin && vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_SKIN &&
        vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has SKINNED flag but "
                "stride is %u", path, vertex_stride);
        SDL_free(file_data);
        return false;
    }
    if (!has_skin && (vertex_stride == FORGE_PIPELINE_VERTEX_STRIDE_SKIN ||
        vertex_stride == FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN)) {
        SDL_Log("forge_pipeline_load_mesh: '%s' has skinned stride %u "
                "but FORGE_PIPELINE_FLAG_SKINNED is not set",
                path, vertex_stride);
        SDL_free(file_data);
        return false;
    }

    /* Skinned data requires v3 */
    if (has_skin && version < FORGE_PIPELINE_FMESH_VERSION_SKIN) {
        SDL_Log("forge_pipeline_load_mesh: '%s' v%u has SKINNED flag "
                "(requires v3)", path, version);
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

bool forge_pipeline_has_skin_data(const ForgePipelineMesh *mesh)
{
    if (!mesh) {
        return false;
    }
    return (mesh->flags & FORGE_PIPELINE_FLAG_SKINNED) != 0;
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

/* ── Path safety helper ────────────────────────────────────────────────── */

/* Validate that a path is a safe relative path: non-empty, no absolute
 * prefix (Unix root or Windows drive letter), no ".." traversal sequences.
 * Used to prevent directory escape when loading paths from untrusted data
 * such as .meta.json sidecars or animation set manifests. */
static bool forge_pipeline__is_safe_relative_path(const char *path)
{
    if (!path || path[0] == '\0') return false;
    /* Reject Unix/UNC absolute paths */
    if (path[0] == '/' || path[0] == '\\') return false;
    /* Reject Windows drive-letter paths (e.g. "C:\...") */
    size_t len = SDL_strlen(path);
    if (len > 1 && path[1] == ':') return false;
    /* Reject directory traversal: "../", "..\", standalone "..",
     * or trailing "/.." / "\.." (e.g. "subdir/..") */
    if (SDL_strstr(path, "../") || SDL_strstr(path, "..\\") ||
        SDL_strcmp(path, "..") == 0) {
        return false;
    }
    if (len >= 3 && path[len - 2] == '.' && path[len - 1] == '.' &&
        (path[len - 3] == '/' || path[len - 3] == '\\')) {
        return false;
    }
    return true;
}

/* ── Compressed texture support ─────────────────────────────────────────── */

/* Include the Basis Universal C wrapper when compressed texture support is
 * available.  The .ftex format is loaded directly — no Basis Universal
 * transcoder is needed at runtime. */

bool forge_pipeline_detect_sidecar(const char *image_path,
                                    ForgePipelineCompressionInfo *info)
{
    if (!image_path || !info) {
        SDL_Log("forge_pipeline_detect_sidecar: NULL argument");
        return false;
    }
    SDL_memset(info, 0, sizeof(*info));

    /* Build .meta.json path from the image path */
    char *meta_path = forge_pipeline__meta_json_path(image_path);
    if (!meta_path) {
        return false;
    }

    /* Load and parse the sidecar */
    size_t meta_size = 0;
    char *meta_data = (char *)SDL_LoadFile(meta_path, &meta_size);
    SDL_free(meta_path);
    if (!meta_data) {
        return false; /* sidecar doesn't exist — not an error */
    }

    cJSON *root = cJSON_ParseWithLength(meta_data, meta_size);
    SDL_free(meta_data);
    if (!root) {
        SDL_Log("forge_pipeline_detect_sidecar: failed to parse JSON for '%s'",
                image_path);
        return false;
    }

    /* Look for the "compression" block */
    cJSON *j_comp = cJSON_GetObjectItemCaseSensitive(root, "compression");
    if (!cJSON_IsObject(j_comp)) {
        /* Valid sidecar but no compression — that's fine */
        cJSON_Delete(root);
        return true;
    }

    /* Validate compressed_file before marking sidecar as compressed.
     * A missing or invalid path means the sidecar is unusable. */
    cJSON *j_file = cJSON_GetObjectItemCaseSensitive(j_comp, "compressed_file");
    if (!cJSON_IsString(j_file) ||
        !forge_pipeline__is_safe_relative_path(j_file->valuestring)) {
        SDL_Log("forge_pipeline_detect_sidecar: missing or unsafe "
                "compressed_file for '%s'", image_path);
        cJSON_Delete(root);
        return false;
    }
    {
        const char *cf = j_file->valuestring;
        size_t cf_len = SDL_strlen(cf);
        /* Reject overlong paths that would be silently truncated */
        if (cf_len >= sizeof(info->compressed_file)) {
            SDL_Log("forge_pipeline_detect_sidecar: compressed_file too long "
                    "(%u >= %u) for '%s'",
                    (unsigned)cf_len, (unsigned)sizeof(info->compressed_file),
                    image_path);
            cJSON_Delete(root);
            return false;
        }
        SDL_strlcpy(info->compressed_file, cf,
                     sizeof(info->compressed_file));
    }

    cJSON *j_codec = cJSON_GetObjectItemCaseSensitive(j_comp, "codec");
    if (cJSON_IsString(j_codec) && j_codec->valuestring) {
        SDL_strlcpy(info->codec, j_codec->valuestring, sizeof(info->codec));
    }

    cJSON *j_container = cJSON_GetObjectItemCaseSensitive(j_comp, "container");
    if (cJSON_IsString(j_container) && j_container->valuestring) {
        SDL_strlcpy(info->container, j_container->valuestring,
                     sizeof(info->container));
    }

    info->has_compression = true;

    cJSON *j_ratio = cJSON_GetObjectItemCaseSensitive(j_comp, "ratio");
    if (cJSON_IsNumber(j_ratio)) {
        info->ratio = (float)j_ratio->valuedouble;
    }

    cJSON_Delete(root);
    return true;
}

bool forge_pipeline_load_ftex(const char *ftex_path,
                               ForgePipelineCompressedTexture *tex)
{
    if (!ftex_path || !tex) {
        SDL_Log("forge_pipeline_load_ftex: NULL argument");
        return false;
    }
    SDL_memset(tex, 0, sizeof(*tex));

    /* Load the entire .ftex file into memory.
     * We keep this buffer alive — mip data pointers point into it. */
    size_t file_size = 0;
    uint8_t *file_data = (uint8_t *)SDL_LoadFile(ftex_path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_ftex: failed to load '%s': %s",
                ftex_path, SDL_GetError());
        return false;
    }

    /* Parse header */
    if (file_size < FORGE_PIPELINE_FTEX_HEADER_SIZE) {
        SDL_Log("forge_pipeline_load_ftex: '%s' too small for header", ftex_path);
        SDL_free(file_data);
        return false;
    }

    /* Read header fields (little-endian) */
    uint32_t magic      = (uint32_t)file_data[0]  | (uint32_t)file_data[1]  << 8
                        | (uint32_t)file_data[2]  << 16 | (uint32_t)file_data[3]  << 24;
    uint32_t version    = (uint32_t)file_data[4]  | (uint32_t)file_data[5]  << 8
                        | (uint32_t)file_data[6]  << 16 | (uint32_t)file_data[7]  << 24;
    uint32_t format     = (uint32_t)file_data[8]  | (uint32_t)file_data[9]  << 8
                        | (uint32_t)file_data[10] << 16 | (uint32_t)file_data[11] << 24;
    uint32_t width      = (uint32_t)file_data[12] | (uint32_t)file_data[13] << 8
                        | (uint32_t)file_data[14] << 16 | (uint32_t)file_data[15] << 24;
    uint32_t height     = (uint32_t)file_data[16] | (uint32_t)file_data[17] << 8
                        | (uint32_t)file_data[18] << 16 | (uint32_t)file_data[19] << 24;
    uint32_t mip_count  = (uint32_t)file_data[20] | (uint32_t)file_data[21] << 8
                        | (uint32_t)file_data[22] << 16 | (uint32_t)file_data[23] << 24;

    if (magic != FORGE_PIPELINE_FTEX_MAGIC) {
        SDL_Log("forge_pipeline_load_ftex: '%s' bad magic (0x%08X)",
                ftex_path, magic);
        SDL_free(file_data);
        return false;
    }
    if (version != FORGE_PIPELINE_FTEX_VERSION) {
        SDL_Log("forge_pipeline_load_ftex: '%s' unsupported version %u",
                ftex_path, version);
        SDL_free(file_data);
        return false;
    }
    if (format < FORGE_PIPELINE_COMPRESSED_BC7_SRGB ||
        format > FORGE_PIPELINE_COMPRESSED_BC5_UNORM) {
        SDL_Log("forge_pipeline_load_ftex: '%s' unknown format %u",
                ftex_path, format);
        SDL_free(file_data);
        return false;
    }
    if (mip_count == 0 || mip_count > FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS) {
        SDL_Log("forge_pipeline_load_ftex: '%s' invalid mip_count %u",
                ftex_path, mip_count);
        SDL_free(file_data);
        return false;
    }
    if (width == 0 || height == 0) {
        SDL_Log("forge_pipeline_load_ftex: '%s' invalid base dimensions %ux%u",
                ftex_path, width, height);
        SDL_free(file_data);
        return false;
    }

    /* Derive maximum legal mip count from base dimensions.
     * A texture can have at most log2(max(w,h)) + 1 mip levels. */
    uint32_t max_dim = (width > height) ? width : height;
    uint32_t max_mips = 1;
    for (uint32_t d = max_dim; d > 1; d >>= 1) max_mips++;
    if (mip_count > max_mips) {
        SDL_Log("forge_pipeline_load_ftex: '%s' mip_count %u exceeds "
                "max %u for %ux%u", ftex_path, mip_count, max_mips,
                width, height);
        SDL_free(file_data);
        return false;
    }

    /* Validate file size covers mip entries */
    size_t entries_end = FORGE_PIPELINE_FTEX_HEADER_SIZE + (size_t)mip_count * FORGE_PIPELINE_FTEX_MIP_ENTRY_SIZE;
    if (file_size < entries_end) {
        SDL_Log("forge_pipeline_load_ftex: '%s' truncated mip entries",
                ftex_path);
        SDL_free(file_data);
        return false;
    }

    /* Parse mip entries and set up pointers into the loaded buffer */
    tex->mips = (ForgePipelineCompressedMip *)SDL_calloc(
        mip_count, sizeof(ForgePipelineCompressedMip));
    if (!tex->mips) {
        SDL_Log("forge_pipeline_load_ftex: allocation failed");
        SDL_free(file_data);
        return false;
    }

    uint8_t *entry_ptr = file_data + FORGE_PIPELINE_FTEX_HEADER_SIZE;
    for (uint32_t i = 0; i < mip_count; i++) {
        uint32_t mip_offset = (uint32_t)entry_ptr[0]  | (uint32_t)entry_ptr[1]  << 8
                            | (uint32_t)entry_ptr[2]  << 16 | (uint32_t)entry_ptr[3]  << 24;
        uint32_t mip_size   = (uint32_t)entry_ptr[4]  | (uint32_t)entry_ptr[5]  << 8
                            | (uint32_t)entry_ptr[6]  << 16 | (uint32_t)entry_ptr[7]  << 24;
        uint32_t mip_w      = (uint32_t)entry_ptr[8]  | (uint32_t)entry_ptr[9]  << 8
                            | (uint32_t)entry_ptr[10] << 16 | (uint32_t)entry_ptr[11] << 24;
        uint32_t mip_h      = (uint32_t)entry_ptr[12] | (uint32_t)entry_ptr[13] << 8
                            | (uint32_t)entry_ptr[14] << 16 | (uint32_t)entry_ptr[15] << 24;
        entry_ptr += FORGE_PIPELINE_FTEX_MIP_ENTRY_SIZE;

        /* Derive expected dimensions for this mip level from the base size.
         * Standard mip chain: each level is half the previous, clamped to 1. */
        uint32_t expected_w = width >> i;
        uint32_t expected_h = height >> i;
        if (expected_w == 0) expected_w = 1;
        if (expected_h == 0) expected_h = 1;

        /* Validate mip metadata: dimensions must match the expected chain,
         * data must live past the header/entry table, and the payload must
         * be at least as large as the BC block data for these dimensions. */
        uint64_t mip_end = (uint64_t)mip_offset + (uint64_t)mip_size;
        uint64_t blocks_x = ((uint64_t)expected_w + 3u) / 4u;
        uint64_t blocks_y = ((uint64_t)expected_h + 3u) / 4u;
        /* Overflow-safe: check blocks_x * blocks_y, then * 16. */
        if (blocks_x > 0 && blocks_y > UINT64_MAX / blocks_x) {
            SDL_Log("forge_pipeline_load_ftex: '%s' mip %u dimensions overflow",
                    ftex_path, i);
            SDL_free(tex->mips);
            tex->mips = NULL;
            SDL_free(file_data);
            return false;
        }
        uint64_t block_count = blocks_x * blocks_y;
        const uint64_t bytes_per_block = 16u; /* BC5 and BC7 both use 16 bytes per 4x4 block */
        if (block_count > UINT64_MAX / bytes_per_block) {
            SDL_Log("forge_pipeline_load_ftex: '%s' mip %u BC size overflow",
                    ftex_path, i);
            SDL_free(tex->mips);
            tex->mips = NULL;
            SDL_free(file_data);
            return false;
        }
        uint64_t min_bc_size = block_count * bytes_per_block;
        if (mip_w != expected_w || mip_h != expected_h ||
            (uint64_t)mip_offset < entries_end ||
            mip_end > (uint64_t)file_size ||
            (uint64_t)mip_size < min_bc_size) {
            SDL_Log("forge_pipeline_load_ftex: '%s' mip %u invalid "
                    "(off=%u size=%u dim=%ux%u expected=%ux%u)",
                    ftex_path, i, mip_offset, mip_size, mip_w, mip_h,
                    expected_w, expected_h);
            SDL_free(tex->mips);
            tex->mips = NULL;
            SDL_free(file_data);
            return false;
        }

        tex->mips[i].data      = file_data + mip_offset;
        tex->mips[i].data_size = mip_size;
        tex->mips[i].width     = mip_w;
        tex->mips[i].height    = mip_h;
    }

    tex->width     = width;
    tex->height    = height;
    tex->mip_count = mip_count;
    tex->format    = (ForgePipelineCompressedFormat)format;

    /* Store the file buffer pointer so free() can release it.
     * Mip data pointers point into this buffer — it must outlive them. */
    tex->_file_buf = file_data;

    return true;
}

void forge_pipeline_free_compressed_texture(ForgePipelineCompressedTexture *tex)
{
    if (!tex) return;
    /* The mip data pointers all point into _file_buf — free it once. */
    SDL_free(tex->_file_buf);
    SDL_free(tex->mips);
    SDL_memset(tex, 0, sizeof(*tex));
}

/* ── Scene loader (.fscene) ─────────────────────────────────────────────── */

/* Read a little-endian int32 from raw bytes. */
static int32_t forge_pipeline__read_i32_le(const uint8_t *buf)
{
    uint32_t u = forge_pipeline__read_u32_le(buf);
    int32_t val;
    SDL_memcpy(&val, &u, sizeof(val));
    return val;
}

/* Multiply two column-major 4x4 matrices: out = a * b.
 * Standalone helper to avoid depending on forge_math.h. */
static void forge_pipeline__mat4_mul(float *out, const float *a, const float *b)
{
    int i, j, k;
    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            float sum = 0.0f;
            for (k = 0; k < 4; k++) {
                sum += a[k * 4 + i] * b[j * 4 + k];
            }
            out[j * 4 + i] = sum;
        }
    }
}

/* Set a column-major 4x4 matrix to identity. */
static void forge_pipeline__mat4_identity(float *m)
{
    SDL_memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* Recursively compute world transforms by walking the node tree.
 * Tracks visited nodes via *visited (caller-allocated, node_count bools).
 * Validates that each node's stored parent matches the traversal parent.
 * Returns false if a cycle, duplicate visit, parent mismatch, or invalid
 * index is detected.  Depth is bounded by node_count — since visited[]
 * prevents revisiting any node, recursion can never exceed the total
 * number of nodes in the scene. */
static bool forge_pipeline__compute_world_transforms(
    ForgePipelineScene *scene, uint32_t node_idx, int32_t expected_parent,
    const float *parent_world, uint32_t depth, bool *visited)
{
    if (node_idx >= scene->node_count) return true; /* skip invalid */
    if (depth >= scene->node_count) {
        SDL_Log("forge_pipeline_load_scene: hierarchy depth %u exceeds node "
                "count %u (possible cycle at node %u)",
                depth, scene->node_count, node_idx);
        return false;
    }
    if (visited[node_idx]) {
        SDL_Log("forge_pipeline_load_scene: node %u visited more than once "
                "(shared child or cycle)", node_idx);
        return false;
    }
    visited[node_idx] = true;

    ForgePipelineSceneNode *node = &scene->nodes[node_idx];

    /* Verify node's stored parent matches the actual traversal parent */
    if (node->parent != expected_parent) {
        SDL_Log("forge_pipeline_load_scene: node %u has parent %d, "
                "expected %d (graph inconsistency)",
                node_idx, node->parent, expected_parent);
        return false;
    }

    forge_pipeline__mat4_mul(node->world_transform,
                             parent_world, node->local_transform);

    uint32_t ci;
    for (ci = 0; ci < node->child_count; ci++) {
        uint32_t arr_idx = node->first_child + ci;
        if (arr_idx >= scene->child_count) return true; /* bounds check */
        uint32_t child_idx = scene->children[arr_idx];
        if (!forge_pipeline__compute_world_transforms(
                scene, child_idx, (int32_t)node_idx,
                node->world_transform, depth + 1, visited)) {
            return false;
        }
    }
    return true;
}

bool forge_pipeline_load_scene(const char *path, ForgePipelineScene *scene)
{
    if (!path || !scene) return false;
    SDL_memset(scene, 0, sizeof(*scene));

    /* Load entire file into memory */
    size_t file_size = 0;
    void *file_data = SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_scene: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    const uint8_t *data = (const uint8_t *)file_data;

    /* ── Validate header ─────────────────────────────────────────────────── */
    if (file_size < FORGE_PIPELINE_FSCENE_HEADER_SIZE) {
        SDL_Log("forge_pipeline_load_scene: file too small (%zu bytes)", file_size);
        SDL_free(file_data);
        return false;
    }

    if (SDL_memcmp(data, FORGE_PIPELINE_FSCENE_MAGIC,
                   FORGE_PIPELINE_FSCENE_MAGIC_SIZE) != 0) {
        SDL_Log("forge_pipeline_load_scene: bad magic in '%s'", path);
        SDL_free(file_data);
        return false;
    }

    uint32_t version    = forge_pipeline__read_u32_le(data + 4);
    uint32_t node_count = forge_pipeline__read_u32_le(data + 8);
    uint32_t mesh_count = forge_pipeline__read_u32_le(data + 12);
    uint32_t root_count = forge_pipeline__read_u32_le(data + 16);

    if (version != FORGE_PIPELINE_FSCENE_VERSION) {
        SDL_Log("forge_pipeline_load_scene: unsupported version %u in '%s'",
                version, path);
        SDL_free(file_data);
        return false;
    }

    if (node_count > FORGE_PIPELINE_MAX_NODES) {
        SDL_Log("forge_pipeline_load_scene: node_count %u exceeds max %u",
                node_count, FORGE_PIPELINE_MAX_NODES);
        SDL_free(file_data);
        return false;
    }
    if (mesh_count > FORGE_PIPELINE_MAX_SCENE_MESHES) {
        SDL_Log("forge_pipeline_load_scene: mesh_count %u exceeds max %u",
                mesh_count, FORGE_PIPELINE_MAX_SCENE_MESHES);
        SDL_free(file_data);
        return false;
    }
    if (root_count > FORGE_PIPELINE_MAX_ROOTS) {
        SDL_Log("forge_pipeline_load_scene: root_count %u exceeds max %u",
                root_count, FORGE_PIPELINE_MAX_ROOTS);
        SDL_free(file_data);
        return false;
    }

    /* ── Compute expected file size (with overflow protection) ────────── */
    size_t offset = FORGE_PIPELINE_FSCENE_HEADER_SIZE;
    size_t roots_size = (size_t)root_count * 4;
    size_t mesh_table_size = (size_t)mesh_count * 8;
    size_t node_table_size = (size_t)node_count * FORGE_PIPELINE_FSCENE_NODE_SIZE;

    /* Check for overflow: node_count * 192 could overflow on 32-bit */
    if (node_count > 0 &&
        node_table_size / FORGE_PIPELINE_FSCENE_NODE_SIZE != node_count) {
        SDL_Log("forge_pipeline_load_scene: node_count %u causes size overflow",
                node_count);
        SDL_free(file_data);
        return false;
    }

    size_t min_size = offset + roots_size + mesh_table_size + node_table_size;
    /* Check each addition for overflow */
    if (min_size < offset || min_size < roots_size) {
        SDL_Log("forge_pipeline_load_scene: section sizes overflow");
        SDL_free(file_data);
        return false;
    }

    if (file_size < min_size) {
        SDL_Log("forge_pipeline_load_scene: file too small for %u nodes, "
                "%u meshes, %u roots (%zu < %zu)",
                node_count, mesh_count, root_count, file_size, min_size);
        SDL_free(file_data);
        return false;
    }

    /* ── Read root indices ───────────────────────────────────────────────── */
    uint32_t *roots = NULL;
    if (root_count > 0) {
        roots = (uint32_t *)SDL_malloc(root_count * sizeof(uint32_t));
        if (!roots) {
            SDL_Log("forge_pipeline_load_scene: allocation failed for roots");
            SDL_free(file_data);
            return false;
        }
        uint32_t ri;
        for (ri = 0; ri < root_count; ri++) {
            roots[ri] = forge_pipeline__read_u32_le(data + offset);
            offset += 4;
            /* Validate root index is within node array bounds */
            if (roots[ri] >= node_count) {
                SDL_Log("forge_pipeline_load_scene: root[%u] = %u "
                        "out of range (node_count = %u)",
                        ri, roots[ri], node_count);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }
        }
    }

    /* ── Read mesh table ─────────────────────────────────────────────────── */
    ForgePipelineSceneMesh *meshes = NULL;
    if (mesh_count > 0) {
        meshes = (ForgePipelineSceneMesh *)SDL_malloc(
            mesh_count * sizeof(ForgePipelineSceneMesh));
        if (!meshes) {
            SDL_Log("forge_pipeline_load_scene: allocation failed for meshes");
            SDL_free(roots);
            SDL_free(file_data);
            return false;
        }
        uint32_t mi;
        for (mi = 0; mi < mesh_count; mi++) {
            meshes[mi].first_submesh = forge_pipeline__read_u32_le(data + offset);
            meshes[mi].submesh_count = forge_pipeline__read_u32_le(data + offset + 4);
            offset += 8;

            /* Validate submesh range fits within the format limit */
            uint32_t end = meshes[mi].first_submesh + meshes[mi].submesh_count;
            if (end < meshes[mi].first_submesh ||
                end > FORGE_PIPELINE_MAX_SUBMESHES) {
                SDL_Log("forge_pipeline_load_scene: mesh %u submesh range "
                        "[%u, %u) exceeds max %u",
                        mi, meshes[mi].first_submesh, end,
                        (uint32_t)FORGE_PIPELINE_MAX_SUBMESHES);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }
        }
    }

    /* ── Read node table ─────────────────────────────────────────────────── */
    ForgePipelineSceneNode *nodes = NULL;
    uint32_t total_children = 0;

    if (node_count > 0) {
        nodes = (ForgePipelineSceneNode *)SDL_calloc(
            node_count, sizeof(ForgePipelineSceneNode));
        if (!nodes) {
            SDL_Log("forge_pipeline_load_scene: allocation failed for nodes");
            SDL_free(meshes);
            SDL_free(roots);
            SDL_free(file_data);
            return false;
        }

        uint32_t ni;
        for (ni = 0; ni < node_count; ni++) {
            const uint8_t *p = data + offset;
            ForgePipelineSceneNode *node = &nodes[ni];

            /* Name: 64 bytes */
            SDL_memcpy(node->name, p, 64);
            node->name[63] = '\0';  /* ensure null termination */
            p += 64;

            /* Parent, mesh, skin */
            node->parent     = forge_pipeline__read_i32_le(p);      p += 4;
            node->mesh_index = forge_pipeline__read_i32_le(p);      p += 4;
            node->skin_index = forge_pipeline__read_i32_le(p);      p += 4;

            /* Validate parent: must be -1 (root) or a valid node index */
            if (node->parent < -1 ||
                (node->parent >= 0 &&
                 (uint32_t)node->parent >= node_count)) {
                SDL_Log("forge_pipeline_load_scene: node %u parent %d "
                        "out of range (node_count = %u)",
                        ni, node->parent, node_count);
                SDL_free(nodes);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }

            /* Validate mesh_index: must be -1 (no mesh) or [0, mesh_count) */
            if (node->mesh_index < -1 ||
                (node->mesh_index >= 0 &&
                 (uint32_t)node->mesh_index >= mesh_count)) {
                SDL_Log("forge_pipeline_load_scene: node %u mesh_index %d "
                        "out of range (mesh_count = %u)",
                        ni, node->mesh_index, mesh_count);
                SDL_free(nodes);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }

            /* Validate skin_index: must be -1 (no skin) or non-negative */
            if (node->skin_index < -1) {
                SDL_Log("forge_pipeline_load_scene: node %u skin_index %d "
                        "is invalid (-1 means no skin)",
                        ni, node->skin_index);
                SDL_free(nodes);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }

            /* Children */
            node->first_child = forge_pipeline__read_u32_le(p);     p += 4;
            node->child_count = forge_pipeline__read_u32_le(p);     p += 4;

            /* TRS flag: must be 0 or 1 */
            {
                uint32_t has_trs_raw = forge_pipeline__read_u32_le(p);
                p += 4;
                if (has_trs_raw > 1) {
                    SDL_Log("forge_pipeline_load_scene: node %u has_trs=%u "
                            "(expected 0 or 1)", ni, has_trs_raw);
                    SDL_free(nodes);
                    SDL_free(meshes);
                    SDL_free(roots);
                    SDL_free(file_data);
                    return false;
                }
                node->has_trs = has_trs_raw;
            }

            int fi;
            for (fi = 0; fi < 3; fi++) {
                node->translation[fi] = forge_pipeline__read_f32_le(p);
                p += 4;
            }
            for (fi = 0; fi < 4; fi++) {
                node->rotation[fi] = forge_pipeline__read_f32_le(p);
                p += 4;
            }
            for (fi = 0; fi < 3; fi++) {
                node->scale[fi] = forge_pipeline__read_f32_le(p);
                p += 4;
            }

            /* Local transform */
            for (fi = 0; fi < 16; fi++) {
                node->local_transform[fi] = forge_pipeline__read_f32_le(p);
                p += 4;
            }

            /* Accumulate total children with overflow check */
            uint32_t prev_total = total_children;
            total_children += node->child_count;
            if (total_children < prev_total) {
                SDL_Log("forge_pipeline_load_scene: total_children overflow "
                        "at node %u", ni);
                SDL_free(nodes);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }

            offset += FORGE_PIPELINE_FSCENE_NODE_SIZE;
        }

        /* A valid forest with N nodes and R roots has at most N-R edges.
         * Reject impossible totals early — before allocating children[]. */
        {
            uint32_t max_edges = node_count > root_count
                               ? node_count - root_count : 0;
            if (total_children > max_edges) {
                SDL_Log("forge_pipeline_load_scene: total_children %u exceeds "
                        "maximum edges for forest (%u nodes, %u roots)",
                        total_children, node_count, root_count);
                SDL_free(nodes);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }
        }

        /* Validate first_child + child_count doesn't exceed total children
         * for each node. */
        for (ni = 0; ni < node_count; ni++) {
            ForgePipelineSceneNode *node = &nodes[ni];
            if (node->child_count > 0) {
                uint32_t end = node->first_child + node->child_count;
                /* Overflow or past end of children array */
                if (end < node->first_child || end > total_children) {
                    SDL_Log("forge_pipeline_load_scene: node %u children "
                            "range [%u, %u) exceeds total_children %u",
                            ni, node->first_child, end, total_children);
                    SDL_free(nodes);
                    SDL_free(meshes);
                    SDL_free(roots);
                    SDL_free(file_data);
                    return false;
                }
            }
        }
    }

    /* ── Read children array ─────────────────────────────────────────────── */
    /* Use 64-bit math to prevent overflow on 32-bit hosts where
     * (size_t)total_children * 4 could wrap before the bounds check. */
    uint64_t children_bytes = (uint64_t)total_children * 4;
    uint64_t children_end   = (uint64_t)offset + children_bytes;
    if (children_end > (uint64_t)file_size) {
        SDL_Log("forge_pipeline_load_scene: file too small for children array "
                "(%zu < %" SDL_PRIu64 ")", file_size, children_end);
        SDL_free(nodes);
        SDL_free(meshes);
        SDL_free(roots);
        SDL_free(file_data);
        return false;
    }

    uint32_t *children = NULL;
    if (total_children > 0) {
        children = (uint32_t *)SDL_malloc(total_children * sizeof(uint32_t));
        if (!children) {
            SDL_Log("forge_pipeline_load_scene: allocation failed for children");
            SDL_free(nodes);
            SDL_free(meshes);
            SDL_free(roots);
            SDL_free(file_data);
            return false;
        }
        uint32_t ci;
        for (ci = 0; ci < total_children; ci++) {
            children[ci] = forge_pipeline__read_u32_le(data + offset);
            offset += 4;
            /* Validate child index is within node array bounds */
            if (children[ci] >= node_count) {
                SDL_Log("forge_pipeline_load_scene: children[%u] = %u "
                        "out of range (node_count = %u)",
                        ci, children[ci], node_count);
                SDL_free(children);
                SDL_free(nodes);
                SDL_free(meshes);
                SDL_free(roots);
                SDL_free(file_data);
                return false;
            }
        }
    }

    /* Done with the raw file data */
    SDL_free(file_data);

    /* ── Populate scene struct ───────────────────────────────────────────── */
    scene->nodes       = nodes;
    scene->node_count  = node_count;
    scene->meshes      = meshes;
    scene->mesh_count  = mesh_count;
    scene->roots       = roots;
    scene->root_count  = root_count;
    scene->children    = children;
    scene->child_count = total_children;

    /* ── Compute world transforms ────────────────────────────────────────── */
    float identity[16];
    forge_pipeline__mat4_identity(identity);

    /* Track visited nodes to detect cycles and unreachable nodes.
     * SDL_calloc(0, ...) may return NULL on some platforms (C standard says
     * the result is implementation-defined), so skip allocation for empty
     * scenes. */
    bool *visited = NULL;
    if (node_count > 0) {
        visited = (bool *)SDL_calloc(node_count, sizeof(bool));
        if (!visited) {
            SDL_Log("forge_pipeline_load_scene: allocation failed for visited[]");
            forge_pipeline_free_scene(scene);
            return false;
        }
    }

    uint32_t ri;
    for (ri = 0; ri < root_count; ri++) {
        if (!forge_pipeline__compute_world_transforms(
                scene, roots[ri], -1, identity, 0, visited)) {
            /* Cycle or duplicate child detected — free and fail */
            SDL_free(visited);
            forge_pipeline_free_scene(scene);
            return false;
        }
    }

    /* Verify every node was reached exactly once */
    {
        uint32_t vi;
        for (vi = 0; vi < node_count; vi++) {
            if (!visited[vi]) {
                SDL_Log("forge_pipeline_load_scene: node %u unreachable "
                        "from roots[]", vi);
                SDL_free(visited);
                forge_pipeline_free_scene(scene);
                return false;
            }
        }
    }
    SDL_free(visited);

    return true;
}

void forge_pipeline_free_scene(ForgePipelineScene *scene)
{
    if (!scene) return;
    SDL_free(scene->nodes);
    SDL_free(scene->meshes);
    SDL_free(scene->roots);
    SDL_free(scene->children);
    SDL_memset(scene, 0, sizeof(*scene));
}

const ForgePipelineSceneMesh *forge_pipeline_scene_get_mesh(
    const ForgePipelineScene *scene, uint32_t mesh_index)
{
    if (!scene || mesh_index >= scene->mesh_count) return NULL;
    return &scene->meshes[mesh_index];
}

/* ── Animation loader (.fanim) ──────────────────────────────────────────── */

/* Free a single clip's internal arrays (samplers, channels). */
static void forge_pipeline__free_clip(ForgePipelineAnimation *clip)
{
    if (!clip) return;
    if (clip->samplers) {
        uint32_t si;
        for (si = 0; si < clip->sampler_count; si++) {
            SDL_free(clip->samplers[si].timestamps);
            SDL_free(clip->samplers[si].values);
        }
        SDL_free(clip->samplers);
    }
    SDL_free(clip->channels);
}

bool forge_pipeline_load_animation(const char *path,
                                    ForgePipelineAnimFile *file)
{
    /* ── Validate inputs ──────────────────────────────────────────────── */
    if (!path) {
        SDL_Log("forge_pipeline_load_animation: path is NULL");
        return false;
    }
    if (!file) {
        SDL_Log("forge_pipeline_load_animation: file is NULL");
        return false;
    }
    SDL_memset(file, 0, sizeof(*file));

    /* ── Load entire file ─────────────────────────────────────────────── */
    size_t file_size = 0;
    uint8_t *file_data = (uint8_t *)SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_animation: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* ── Validate header ──────────────────────────────────────────────── */
    if (file_size < FORGE_PIPELINE_FANIM_HEADER_SIZE) {
        SDL_Log("forge_pipeline_load_animation: file '%s' too small for "
                "header (%zu bytes, need %d)", path, file_size,
                FORGE_PIPELINE_FANIM_HEADER_SIZE);
        SDL_free(file_data);
        return false;
    }

    const uint8_t *p = file_data;

    if (SDL_memcmp(p, FORGE_PIPELINE_FANIM_MAGIC,
                   FORGE_PIPELINE_FANIM_MAGIC_SIZE) != 0) {
        SDL_Log("forge_pipeline_load_animation: '%s' is not a .fanim file "
                "(bad magic)", path);
        SDL_free(file_data);
        return false;
    }
    p += FORGE_PIPELINE_FANIM_MAGIC_SIZE;

    uint32_t version = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (version == 0 || version != FORGE_PIPELINE_FANIM_VERSION) {
        SDL_Log("forge_pipeline_load_animation: '%s' has unsupported version "
                "%u (expected %u)", path, version,
                FORGE_PIPELINE_FANIM_VERSION);
        SDL_free(file_data);
        return false;
    }

    uint32_t clip_count = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (clip_count > FORGE_PIPELINE_MAX_ANIM_CLIPS) {
        SDL_Log("forge_pipeline_load_animation: '%s' has %u clips (max %d)",
                path, clip_count, FORGE_PIPELINE_MAX_ANIM_CLIPS);
        SDL_free(file_data);
        return false;
    }

    /* Empty file (0 clips) is valid */
    if (clip_count == 0) {
        SDL_free(file_data);
        return true;
    }

    /* ── Allocate clips array ─────────────────────────────────────────── */
    ForgePipelineAnimation *clips = (ForgePipelineAnimation *)SDL_calloc(
        clip_count, sizeof(ForgePipelineAnimation));
    if (!clips) {
        SDL_Log("forge_pipeline_load_animation: allocation failed for clips");
        SDL_free(file_data);
        return false;
    }

    const uint8_t *end = file_data + file_size;

    /* ── Read each clip ───────────────────────────────────────────────── */
    uint32_t ci;
    for (ci = 0; ci < clip_count; ci++) {
        ForgePipelineAnimation *clip = &clips[ci];

        /* Clip header: 64B name + 4B duration + 4B sampler_count +
         * 4B channel_count */
        if (p + FORGE_PIPELINE_CLIP_HEADER_SIZE > end) {
            SDL_Log("forge_pipeline_load_animation: '%s' truncated at "
                    "clip %u header", path, ci);
            goto fail;
        }

        SDL_memcpy(clip->name, p, 64);
        clip->name[63] = '\0';
        p += 64;

        clip->duration = forge_pipeline__read_f32_le(p);
        p += sizeof(float);

        clip->sampler_count = forge_pipeline__read_u32_le(p);
        p += sizeof(uint32_t);
        clip->channel_count = forge_pipeline__read_u32_le(p);
        p += sizeof(uint32_t);

        if (clip->sampler_count > FORGE_PIPELINE_MAX_ANIM_SAMPLERS) {
            SDL_Log("forge_pipeline_load_animation: '%s' clip %u has %u "
                    "samplers (max %d)", path, ci, clip->sampler_count,
                    FORGE_PIPELINE_MAX_ANIM_SAMPLERS);
            goto fail;
        }
        if (clip->channel_count > FORGE_PIPELINE_MAX_ANIM_CHANNELS) {
            SDL_Log("forge_pipeline_load_animation: '%s' clip %u has %u "
                    "channels (max %d)", path, ci, clip->channel_count,
                    FORGE_PIPELINE_MAX_ANIM_CHANNELS);
            goto fail;
        }

        /* ── Allocate and read samplers ───────────────────────────────── */
        if (clip->sampler_count > 0) {
            clip->samplers = (ForgePipelineAnimSampler *)SDL_calloc(
                clip->sampler_count, sizeof(ForgePipelineAnimSampler));
            if (!clip->samplers) {
                SDL_Log("forge_pipeline_load_animation: allocation failed "
                        "for clip %u samplers", ci);
                goto fail;
            }
        }

        uint32_t si;
        for (si = 0; si < clip->sampler_count; si++) {
            ForgePipelineAnimSampler *samp = &clip->samplers[si];

            /* Sampler header: 3 × u32 */
            if (p + FORGE_PIPELINE_SAMPLER_HEADER_SIZE > end) {
                SDL_Log("forge_pipeline_load_animation: '%s' truncated at "
                        "clip %u sampler %u header", path, ci, si);
                goto fail;
            }

            samp->keyframe_count  = forge_pipeline__read_u32_le(p);
            p += sizeof(uint32_t);
            samp->value_components = forge_pipeline__read_u32_le(p);
            p += sizeof(uint32_t);
            {
                uint32_t interp_raw = forge_pipeline__read_u32_le(p);
                p += sizeof(uint32_t);
                if (interp_raw != FORGE_PIPELINE_INTERP_LINEAR &&
                    interp_raw != FORGE_PIPELINE_INTERP_STEP) {
                    SDL_Log("forge_pipeline_load_animation: '%s' clip %u "
                            "sampler %u has invalid interpolation %u",
                            path, ci, si, interp_raw);
                    goto fail;
                }
                samp->interpolation = (ForgePipelineAnimInterp)interp_raw;
            }

            if (samp->keyframe_count > FORGE_PIPELINE_MAX_KEYFRAMES) {
                SDL_Log("forge_pipeline_load_animation: '%s' clip %u "
                        "sampler %u has %u keyframes (max %d)",
                        path, ci, si, samp->keyframe_count,
                        FORGE_PIPELINE_MAX_KEYFRAMES);
                goto fail;
            }

            if (samp->value_components != 3 && samp->value_components != 4) {
                SDL_Log("forge_pipeline_load_animation: '%s' clip %u "
                        "sampler %u has invalid value_components %u "
                        "(expected 3 or 4)", path, ci, si,
                        samp->value_components);
                goto fail;
            }

            /* Timestamps: keyframe_count floats */
            size_t ts_bytes = (size_t)samp->keyframe_count * sizeof(float);
            if (p + ts_bytes > end) {
                SDL_Log("forge_pipeline_load_animation: '%s' truncated at "
                        "clip %u sampler %u timestamps", path, ci, si);
                goto fail;
            }

            if (samp->keyframe_count > 0) {
                samp->timestamps = (float *)SDL_malloc(ts_bytes);
                if (!samp->timestamps) {
                    SDL_Log("forge_pipeline_load_animation: allocation "
                            "failed for timestamps");
                    goto fail;
                }
                uint32_t ki;
                for (ki = 0; ki < samp->keyframe_count; ki++) {
                    samp->timestamps[ki] = forge_pipeline__read_f32_le(p);
                    p += sizeof(float);
                }
            }

            /* Values: keyframe_count * value_components floats */
            size_t val_count = (size_t)samp->keyframe_count
                             * samp->value_components;
            size_t val_bytes = val_count * sizeof(float);
            if (p + val_bytes > end) {
                SDL_Log("forge_pipeline_load_animation: '%s' truncated at "
                        "clip %u sampler %u values", path, ci, si);
                goto fail;
            }

            if (val_count > 0) {
                samp->values = (float *)SDL_malloc(val_bytes);
                if (!samp->values) {
                    SDL_Log("forge_pipeline_load_animation: allocation "
                            "failed for values");
                    goto fail;
                }
                size_t vi;
                for (vi = 0; vi < val_count; vi++) {
                    samp->values[vi] = forge_pipeline__read_f32_le(p);
                    p += sizeof(float);
                }
            }
        }

        /* ── Allocate and read channels ───────────────────────────────── */
        if (clip->channel_count > 0) {
            clip->channels = (ForgePipelineAnimChannel *)SDL_calloc(
                clip->channel_count, sizeof(ForgePipelineAnimChannel));
            if (!clip->channels) {
                SDL_Log("forge_pipeline_load_animation: allocation failed "
                        "for clip %u channels", ci);
                goto fail;
            }
        }

        uint32_t chi;
        for (chi = 0; chi < clip->channel_count; chi++) {
            /* Channel: i32 + u32 + u32 = 12 bytes */
            if (p + 12 > end) {
                SDL_Log("forge_pipeline_load_animation: '%s' truncated at "
                        "clip %u channel %u", path, ci, chi);
                goto fail;
            }

            clip->channels[chi].target_node =
                forge_pipeline__read_i32_le(p);
            p += sizeof(int32_t);
            {
                uint32_t path_raw = forge_pipeline__read_u32_le(p);
                p += sizeof(uint32_t);
                if (path_raw != FORGE_PIPELINE_ANIM_TRANSLATION &&
                    path_raw != FORGE_PIPELINE_ANIM_ROTATION &&
                    path_raw != FORGE_PIPELINE_ANIM_SCALE) {
                    SDL_Log("forge_pipeline_load_animation: '%s' clip %u "
                            "channel %u has invalid target_path %u",
                            path, ci, chi, path_raw);
                    goto fail;
                }
                clip->channels[chi].target_path =
                    (ForgePipelineAnimPath)path_raw;
            }
            clip->channels[chi].sampler_index =
                forge_pipeline__read_u32_le(p);
            p += sizeof(uint32_t);

            /* Validate sampler_index is within this clip's sampler array */
            if (clip->channels[chi].sampler_index >= clip->sampler_count) {
                SDL_Log("forge_pipeline_load_animation: '%s' clip %u "
                        "channel %u sampler_index %u out of range "
                        "(sampler_count %u)", path, ci, chi,
                        clip->channels[chi].sampler_index,
                        clip->sampler_count);
                goto fail;
            }

            /* Validate sampler value_components matches target_path:
             * rotation requires 4 (quaternion), translation/scale require 3 */
            {
                ForgePipelineAnimChannel *ch = &clip->channels[chi];
                const ForgePipelineAnimSampler *samp =
                    &clip->samplers[ch->sampler_index];
                uint32_t expected = (ch->target_path ==
                    FORGE_PIPELINE_ANIM_ROTATION) ? 4u : 3u;
                if (samp->value_components != expected) {
                    SDL_Log("forge_pipeline_load_animation: '%s' clip %u "
                            "channel %u target_path %u requires %u "
                            "components, but sampler %u has %u",
                            path, ci, chi, (uint32_t)ch->target_path,
                            expected, ch->sampler_index,
                            samp->value_components);
                    goto fail;
                }
            }
        }
    }

    /* ── Success ──────────────────────────────────────────────────────── */
    file->clips      = clips;
    file->clip_count  = clip_count;

    SDL_free(file_data);
    return true;

fail:
    /* Clean up all clips allocated so far (ci is the failing clip) */
    {
        uint32_t fi;
        for (fi = 0; fi <= ci && fi < clip_count; fi++) {
            forge_pipeline__free_clip(&clips[fi]);
        }
    }
    SDL_free(clips);
    SDL_free(file_data);
    return false;
}

void forge_pipeline_free_animation(ForgePipelineAnimFile *file)
{
    if (!file) return;
    if (file->clips) {
        uint32_t ci;
        for (ci = 0; ci < file->clip_count; ci++) {
            forge_pipeline__free_clip(&file->clips[ci]);
        }
        SDL_free(file->clips);
    }
    SDL_memset(file, 0, sizeof(*file));
}

/* ── Skin (.fskin) loader ─────────────────────────────────────────────── */

/* Helper: free one skin's arrays */
static void forge_pipeline__free_skin(ForgePipelineSkin *skin)
{
    if (skin->joints) {
        SDL_free(skin->joints);
        skin->joints = NULL;
    }
    if (skin->inverse_bind_matrices) {
        SDL_free(skin->inverse_bind_matrices);
        skin->inverse_bind_matrices = NULL;
    }
}

bool forge_pipeline_load_skins(const char *path,
                                ForgePipelineSkinSet *skins)
{
    if (!path) {
        SDL_Log("forge_pipeline_load_skins: path is NULL");
        return false;
    }
    if (!skins) {
        SDL_Log("forge_pipeline_load_skins: skins is NULL");
        return false;
    }
    SDL_memset(skins, 0, sizeof(*skins));

    /* Load file */
    size_t file_size = 0;
    uint8_t *file_data = (uint8_t *)SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_skins: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* Validate header size */
    if (file_size < FORGE_PIPELINE_FSKIN_HEADER_SIZE) {
        SDL_Log("forge_pipeline_load_skins: '%s' too small for header "
                "(%zu bytes, need %d)", path, file_size,
                FORGE_PIPELINE_FSKIN_HEADER_SIZE);
        SDL_free(file_data);
        return false;
    }

    const uint8_t *p = file_data;
    const uint8_t *end = file_data + file_size;

    /* Magic */
    if (SDL_memcmp(p, FORGE_PIPELINE_FSKIN_MAGIC,
                   FORGE_PIPELINE_FSKIN_MAGIC_SIZE) != 0) {
        SDL_Log("forge_pipeline_load_skins: '%s' bad magic", path);
        SDL_free(file_data);
        return false;
    }
    p += FORGE_PIPELINE_FSKIN_MAGIC_SIZE;

    /* Version */
    uint32_t version = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (version != FORGE_PIPELINE_FSKIN_VERSION) {
        SDL_Log("forge_pipeline_load_skins: '%s' unsupported version %u "
                "(expected %u)", path, version, FORGE_PIPELINE_FSKIN_VERSION);
        SDL_free(file_data);
        return false;
    }

    /* Skin count */
    uint32_t skin_count = forge_pipeline__read_u32_le(p);
    p += sizeof(uint32_t);
    if (skin_count > FORGE_PIPELINE_MAX_SKINS) {
        SDL_Log("forge_pipeline_load_skins: '%s' has %u skins (max %d)",
                path, skin_count, FORGE_PIPELINE_MAX_SKINS);
        SDL_free(file_data);
        return false;
    }

    if (skin_count == 0) {
        skins->skin_count = 0;
        skins->skins = NULL;
        SDL_free(file_data);
        return true;
    }

    /* Allocate skins */
    ForgePipelineSkin *skin_array = (ForgePipelineSkin *)SDL_calloc(
        skin_count, sizeof(ForgePipelineSkin));
    if (!skin_array) {
        SDL_Log("forge_pipeline_load_skins: allocation failed");
        SDL_free(file_data);
        return false;
    }

    /* Parse each skin */
    uint32_t si = 0;
    for (si = 0; si < skin_count; si++) {
        ForgePipelineSkin *skin = &skin_array[si];

        /* Name: 64 bytes */
        if (p + sizeof(skin->name) > end) {
            SDL_Log("forge_pipeline_load_skins: '%s' truncated at skin %u name",
                    path, si);
            goto fail;
        }
        SDL_memcpy(skin->name, p, sizeof(skin->name));
        skin->name[sizeof(skin->name) - 1] = '\0';
        p += sizeof(skin->name);

        /* Joint count */
        if (p + sizeof(uint32_t) > end) {
            SDL_Log("forge_pipeline_load_skins: '%s' truncated at skin %u "
                    "joint_count", path, si);
            goto fail;
        }
        skin->joint_count = forge_pipeline__read_u32_le(p);
        p += sizeof(uint32_t);

        if (skin->joint_count > FORGE_PIPELINE_MAX_SKIN_JOINTS) {
            SDL_Log("forge_pipeline_load_skins: '%s' skin %u has %u joints "
                    "(max %d)", path, si, skin->joint_count,
                    FORGE_PIPELINE_MAX_SKIN_JOINTS);
            goto fail;
        }

        /* Skeleton root */
        if (p + sizeof(int32_t) > end) {
            SDL_Log("forge_pipeline_load_skins: '%s' truncated at skin %u "
                    "skeleton", path, si);
            goto fail;
        }
        skin->skeleton = forge_pipeline__read_i32_le(p);
        p += sizeof(int32_t);

        /* Joints array: joint_count × i32 */
        if (skin->joint_count > 0) {
            size_t joints_bytes = (size_t)skin->joint_count * sizeof(int32_t);
            if (p + joints_bytes > end) {
                SDL_Log("forge_pipeline_load_skins: '%s' truncated at skin %u "
                        "joints", path, si);
                goto fail;
            }
            skin->joints = (int32_t *)SDL_malloc(joints_bytes);
            if (!skin->joints) {
                SDL_Log("forge_pipeline_load_skins: joint allocation failed");
                goto fail;
            }
            uint32_t ji;
            for (ji = 0; ji < skin->joint_count; ji++) {
                skin->joints[ji] = forge_pipeline__read_i32_le(p);
                p += sizeof(int32_t);
            }
        }

        /* Inverse bind matrices: joint_count × 16 floats */
        if (skin->joint_count > 0) {
            size_t ibm_floats = (size_t)skin->joint_count * 16;
            size_t ibm_bytes = ibm_floats * sizeof(float);
            if (p + ibm_bytes > end) {
                SDL_Log("forge_pipeline_load_skins: '%s' truncated at skin %u "
                        "inverse bind matrices", path, si);
                goto fail;
            }
            skin->inverse_bind_matrices = (float *)SDL_malloc(ibm_bytes);
            if (!skin->inverse_bind_matrices) {
                SDL_Log("forge_pipeline_load_skins: IBM allocation failed");
                goto fail;
            }
            size_t fi;
            for (fi = 0; fi < ibm_floats; fi++) {
                skin->inverse_bind_matrices[fi] = forge_pipeline__read_f32_le(p);
                p += sizeof(float);
            }
        }
    }

    skins->skins = skin_array;
    skins->skin_count = skin_count;
    SDL_free(file_data);
    return true;

fail:
    {
        uint32_t fi;
        for (fi = 0; fi <= si && fi < skin_count; fi++) {
            forge_pipeline__free_skin(&skin_array[fi]);
        }
    }
    SDL_free(skin_array);
    SDL_free(file_data);
    return false;
}

void forge_pipeline_free_skins(ForgePipelineSkinSet *skins)
{
    if (!skins) return;
    if (skins->skins) {
        uint32_t i;
        for (i = 0; i < skins->skin_count; i++) {
            forge_pipeline__free_skin(&skins->skins[i]);
        }
        SDL_free(skins->skins);
    }
    SDL_memset(skins, 0, sizeof(*skins));
}

/* ── Animation manifest (.fanims) loader ──────────────────────────────── */

bool forge_pipeline_load_anim_set(const char *path,
                                   ForgePipelineAnimSet *set)
{
    if (!path) {
        SDL_Log("forge_pipeline_load_anim_set: path is NULL");
        return false;
    }
    if (!set) {
        SDL_Log("forge_pipeline_load_anim_set: set is NULL");
        return false;
    }
    SDL_memset(set, 0, sizeof(*set));

    /* Load file */
    size_t file_size = 0;
    char *file_data = (char *)SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_pipeline_load_anim_set: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* Parse JSON */
    cJSON *root = cJSON_ParseWithLength(file_data, file_size);
    SDL_free(file_data);
    if (!root) {
        SDL_Log("forge_pipeline_load_anim_set: failed to parse '%s'", path);
        return false;
    }

    /* Validate version */
    cJSON *j_version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!cJSON_IsNumber(j_version) ||
        j_version->valueint != FORGE_PIPELINE_FANIMS_VERSION) {
        SDL_Log("forge_pipeline_load_anim_set: '%s' has unsupported version "
                "(expected %d)", path, FORGE_PIPELINE_FANIMS_VERSION);
        cJSON_Delete(root);
        return false;
    }

    /* Model name (optional) */
    cJSON *j_model = cJSON_GetObjectItemCaseSensitive(root, "model");
    if (cJSON_IsString(j_model) && j_model->valuestring) {
        SDL_strlcpy(set->model, j_model->valuestring, sizeof(set->model));
    }

    /* Clips object */
    cJSON *j_clips = cJSON_GetObjectItemCaseSensitive(root, "clips");
    if (!j_clips || !cJSON_IsObject(j_clips)) {
        SDL_Log("forge_pipeline_load_anim_set: '%s' missing 'clips' object",
                path);
        cJSON_Delete(root);
        return false;
    }

    uint32_t count = 0;
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, j_clips) { count++; }
    }

    if (count == 0) {
        /* Valid: model may have no animations. */
        set->clip_count = 0;
        set->clips      = NULL;
        /* Compute base_dir */
        SDL_strlcpy(set->base_dir, path, sizeof(set->base_dir));
        {
            char *sep = SDL_strrchr(set->base_dir, '/');
            char *sep2 = SDL_strrchr(set->base_dir, '\\');
            if (sep2 && (!sep || sep2 > sep)) sep = sep2;
            if (sep) *(sep + 1) = '\0';
            else set->base_dir[0] = '\0';
        }
        cJSON_Delete(root);
        return true;
    }

    if (count > FORGE_PIPELINE_MAX_ANIM_SET_CLIPS) {
        SDL_Log("forge_pipeline_load_anim_set: '%s' has %u clips (max %d)",
                path, count, FORGE_PIPELINE_MAX_ANIM_SET_CLIPS);
        cJSON_Delete(root);
        return false;
    }

    /* Allocate clips */
    ForgePipelineAnimClipInfo *clips = (ForgePipelineAnimClipInfo *)SDL_calloc(
        count, sizeof(ForgePipelineAnimClipInfo));
    if (!clips) {
        SDL_Log("forge_pipeline_load_anim_set: allocation failed");
        cJSON_Delete(root);
        return false;
    }

    /* Parse each clip */
    uint32_t ci = 0;
    cJSON *j_clip = NULL;
    cJSON_ArrayForEach(j_clip, j_clips) {
        if (ci >= count) break;
        ForgePipelineAnimClipInfo *info = &clips[ci];

        /* Name is the key — reject empty or overlong keys */
        if (!j_clip->string || j_clip->string[0] == '\0' ||
            SDL_strlen(j_clip->string) >= sizeof(info->name)) {
            SDL_Log("forge_pipeline_load_anim_set: '%s' has an invalid "
                    "clip key", path);
            SDL_free(clips);
            cJSON_Delete(root);
            return false;
        }
        SDL_strlcpy(info->name, j_clip->string, sizeof(info->name));

        /* File (required — reject empty or overlong paths) */
        cJSON *j_file = cJSON_GetObjectItemCaseSensitive(j_clip, "file");
        if (!cJSON_IsString(j_file) || !j_file->valuestring ||
            j_file->valuestring[0] == '\0' ||
            SDL_strlen(j_file->valuestring) >= sizeof(info->file)) {
            SDL_Log("forge_pipeline_load_anim_set: '%s' clip '%s' missing "
                    "a valid 'file' string", path, info->name);
            SDL_free(clips);
            cJSON_Delete(root);
            return false;
        }
        SDL_strlcpy(info->file, j_file->valuestring, sizeof(info->file));

        /* Duration (optional, default 0) */
        cJSON *j_dur = cJSON_GetObjectItemCaseSensitive(j_clip, "duration");
        if (cJSON_IsNumber(j_dur)) {
            info->duration = (float)j_dur->valuedouble;
        }

        /* Loop flag (optional, default false) */
        cJSON *j_loop = cJSON_GetObjectItemCaseSensitive(j_clip, "loop");
        if (cJSON_IsBool(j_loop)) {
            info->loop = cJSON_IsTrue(j_loop) ? true : false;
        }

        /* Tags array (optional) */
        cJSON *j_tags = cJSON_GetObjectItemCaseSensitive(j_clip, "tags");
        if (cJSON_IsArray(j_tags)) {
            int tag_total = cJSON_GetArraySize(j_tags);
            if (tag_total > FORGE_PIPELINE_MAX_CLIP_TAGS) {
                SDL_Log("forge_pipeline_load_anim_set: '%s' clip '%s' has "
                        "%d tags (max %d)", path, info->name,
                        tag_total, FORGE_PIPELINE_MAX_CLIP_TAGS);
                SDL_free(clips);
                cJSON_Delete(root);
                return false;
            }
            uint32_t accepted = 0;
            int ti;
            for (ti = 0; ti < tag_total; ti++) {
                cJSON *j_tag = cJSON_GetArrayItem(j_tags, ti);
                if (!cJSON_IsString(j_tag) || !j_tag->valuestring ||
                    j_tag->valuestring[0] == '\0' ||
                    SDL_strlen(j_tag->valuestring) >= FORGE_PIPELINE_MAX_TAG_LEN) {
                    SDL_Log("forge_pipeline_load_anim_set: '%s' clip '%s' "
                            "has an invalid tag at index %d",
                            path, info->name, ti);
                    SDL_free(clips);
                    cJSON_Delete(root);
                    return false;
                }
                SDL_strlcpy(info->tags[accepted], j_tag->valuestring,
                            FORGE_PIPELINE_MAX_TAG_LEN);
                accepted++;
            }
            info->tag_count = accepted;
        }

        ci++;
    }

    /* Compute base_dir from the manifest path */
    SDL_strlcpy(set->base_dir, path, sizeof(set->base_dir));
    {
        char *sep = SDL_strrchr(set->base_dir, '/');
        char *sep2 = SDL_strrchr(set->base_dir, '\\');
        if (sep2 && (!sep || sep2 > sep)) sep = sep2;
        if (sep) *(sep + 1) = '\0';
        else set->base_dir[0] = '\0';
    }

    set->clips      = clips;
    set->clip_count  = ci;

    cJSON_Delete(root);
    return true;
}

void forge_pipeline_free_anim_set(ForgePipelineAnimSet *set)
{
    if (!set) return;
    if (set->clips) {
        SDL_free(set->clips);
    }
    SDL_memset(set, 0, sizeof(*set));
}

const ForgePipelineAnimClipInfo *forge_pipeline_find_clip(
    const ForgePipelineAnimSet *set, const char *name)
{
    if (!set || !name) return NULL;
    uint32_t i;
    for (i = 0; i < set->clip_count; i++) {
        if (SDL_strcmp(set->clips[i].name, name) == 0) {
            return &set->clips[i];
        }
    }
    return NULL;
}

bool forge_pipeline_load_clip(const ForgePipelineAnimSet *set,
                               const char *name,
                               ForgePipelineAnimFile *file)
{
    if (file) {
        SDL_memset(file, 0, sizeof(*file));
    }
    if (!set || !name || !file) {
        SDL_Log("forge_pipeline_load_clip: NULL argument");
        return false;
    }

    const ForgePipelineAnimClipInfo *info = forge_pipeline_find_clip(set, name);
    if (!info) {
        SDL_Log("forge_pipeline_load_clip: clip '%s' not found", name);
        return false;
    }

    /* Reject unsafe paths: absolute or with directory traversal sequences */
    if (!forge_pipeline__is_safe_relative_path(info->file)) {
        SDL_Log("forge_pipeline_load_clip: unsafe clip path '%s'", info->file);
        return false;
    }

    /* Build full path: base_dir(512) + file(256) */
    char full_path[768];
    if (set->base_dir[0]) {
        SDL_snprintf(full_path, sizeof(full_path), "%s%s",
                     set->base_dir, info->file);
    } else {
        SDL_strlcpy(full_path, info->file, sizeof(full_path));
    }

    return forge_pipeline_load_animation(full_path, file);
}

#ifdef FORGE_MATH_H /* animation evaluation requires forge_math.h */

/* ══════════════════════════════════════════════════════════════════════════
 * Animation evaluation
 * ══════════════════════════════════════════════════════════════════════════ */

/* Binary search for keyframe interval: find lo such that
 * timestamps[lo] <= t < timestamps[lo+1].  O(log n). */
static int forge_pipeline__anim_find_keyframe(
    const float *timestamps, uint32_t count, float t)
{
    if (count < 2) return 0; /* Need at least two keyframes for interpolation */
    int lo = 0;
    int hi = (int)count - 1;
    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        if (timestamps[mid] <= t) lo = mid;
        else                      hi = mid;
    }
    return lo;
}

/* Evaluate a vec3 sampler (translation or scale) at time t. */
static vec3 forge_pipeline__anim_eval_vec3(
    const ForgePipelineAnimSampler *sampler, float t)
{
    const float *ts   = sampler->timestamps;
    const float *vals = sampler->values;
    uint32_t count    = sampler->keyframe_count;

    if (count == 0) return vec3_create(0.0f, 0.0f, 0.0f);
    if (t <= ts[0])         return vec3_create(vals[0], vals[1], vals[2]);
    if (t >= ts[count - 1]) {
        const float *v = vals + (count - 1) * 3;
        return vec3_create(v[0], v[1], v[2]);
    }

    int lo = forge_pipeline__anim_find_keyframe(ts, count, t);

    /* STEP interpolation */
    if (sampler->interpolation == FORGE_PIPELINE_INTERP_STEP) {
        const float *v = vals + lo * 3;
        return vec3_create(v[0], v[1], v[2]);
    }

    /* LINEAR interpolation */
    float t0   = ts[lo];
    float t1   = ts[lo + 1];
    float span = t1 - t0;
    float alpha = (span > FORGE_PIPELINE_ANIM_EPSILON)
                ? (t - t0) / span : 0.0f;

    const float *a = vals + lo * 3;
    const float *b = vals + (lo + 1) * 3;
    return vec3_lerp(vec3_create(a[0], a[1], a[2]),
                     vec3_create(b[0], b[1], b[2]), alpha);
}

/* Evaluate a quaternion sampler (rotation) at time t.
 * Pipeline stores quaternions as [x,y,z,w]; forge_math uses quat(w,x,y,z). */
static quat forge_pipeline__anim_eval_quat(
    const ForgePipelineAnimSampler *sampler, float t)
{
    const float *ts   = sampler->timestamps;
    const float *vals = sampler->values;
    uint32_t count    = sampler->keyframe_count;

    if (count == 0) return quat_create(1.0f, 0.0f, 0.0f, 0.0f);
    if (t <= ts[0])
        return quat_create(vals[3], vals[0], vals[1], vals[2]);
    if (t >= ts[count - 1]) {
        const float *v = vals + (count - 1) * 4;
        return quat_create(v[3], v[0], v[1], v[2]);
    }

    int lo = forge_pipeline__anim_find_keyframe(ts, count, t);

    const float *a = vals + lo * 4;
    quat qa = quat_create(a[3], a[0], a[1], a[2]);

    if (sampler->interpolation == FORGE_PIPELINE_INTERP_STEP)
        return qa;

    float t0   = ts[lo];
    float t1   = ts[lo + 1];
    float span = t1 - t0;
    float alpha = (span > FORGE_PIPELINE_ANIM_EPSILON)
                ? (t - t0) / span : 0.0f;

    const float *b = vals + (lo + 1) * 4;
    quat qb = quat_create(b[3], b[0], b[1], b[2]);
    return quat_slerp(qa, qb, alpha);
}

/* Rebuild a node's local_transform from its TRS components.
 * local_transform = T × R × S (column-major, stored as float[16]). */
static void forge_pipeline__rebuild_local(ForgePipelineSceneNode *node)
{
    mat4 t_mat = mat4_translate(vec3_create(
        node->translation[0], node->translation[1], node->translation[2]));
    quat rot = quat_create(
        node->rotation[3], node->rotation[0],
        node->rotation[1], node->rotation[2]);
    mat4 r_mat = quat_to_mat4(rot);
    mat4 s_mat = mat4_scale(vec3_create(
        node->scale[0], node->scale[1], node->scale[2]));
    mat4 result = mat4_multiply(t_mat, mat4_multiply(r_mat, s_mat));
    SDL_memcpy(node->local_transform, &result, sizeof(node->local_transform));
}

void forge_pipeline_anim_apply(
    const ForgePipelineAnimation *anim,
    ForgePipelineSceneNode *nodes, uint32_t node_count,
    float t, bool loop)
{
    if (!anim || !nodes || node_count == 0 || anim->channel_count == 0)
        return;

    if (node_count > FORGE_PIPELINE_MAX_NODES) {
        SDL_Log("forge_pipeline_anim_apply: node_count %u exceeds max %u — "
                "animation not applied",
                node_count, FORGE_PIPELINE_MAX_NODES);
        return;
    }

    /* Reject non-finite time to prevent NaN propagation through
     * SDL_fmodf, alpha math, and downstream transforms. */
    if (t != t || t > FLT_MAX || t < -FLT_MAX) {
        SDL_Log("forge_pipeline_anim_apply: non-finite time input; using 0");
        t = 0.0f;
    }

    /* Wrap or clamp time */
    if (anim->duration > FORGE_PIPELINE_ANIM_EPSILON) {
        if (loop) {
            t = SDL_fmodf(t, anim->duration);
            if (t < 0.0f) t += anim->duration;
        } else {
            if (t < 0.0f) t = 0.0f;
            if (t > anim->duration) t = anim->duration;
        }
    } else {
        t = 0.0f;
    }

    /* Track which nodes were modified to avoid redundant rebuilds.
     * Fixed-size (4096 bytes) because MSVC C99 does not support VLAs. */
    uint8_t modified[FORGE_PIPELINE_MAX_NODES];
    SDL_memset(modified, 0, node_count * sizeof(uint8_t));

    for (uint32_t ci = 0; ci < anim->channel_count; ci++) {
        const ForgePipelineAnimChannel *ch = &anim->channels[ci];

        if (ch->target_node < 0 || (uint32_t)ch->target_node >= node_count)
            continue;
        if (ch->sampler_index >= anim->sampler_count) continue;

        const ForgePipelineAnimSampler *samp =
            &anim->samplers[ch->sampler_index];
        if (!samp->timestamps || !samp->values || samp->keyframe_count == 0)
            continue;

        ForgePipelineSceneNode *node = &nodes[ch->target_node];

        switch (ch->target_path) {
        case FORGE_PIPELINE_ANIM_TRANSLATION: {
            vec3 v = forge_pipeline__anim_eval_vec3(samp, t);
            node->translation[0] = v.x;
            node->translation[1] = v.y;
            node->translation[2] = v.z;
            break;
        }
        case FORGE_PIPELINE_ANIM_ROTATION: {
            quat q = forge_pipeline__anim_eval_quat(samp, t);
            /* Store back as xyzw (pipeline format) */
            node->rotation[0] = q.x;
            node->rotation[1] = q.y;
            node->rotation[2] = q.z;
            node->rotation[3] = q.w;
            break;
        }
        case FORGE_PIPELINE_ANIM_SCALE: {
            vec3 v = forge_pipeline__anim_eval_vec3(samp, t);
            node->scale[0] = v.x;
            node->scale[1] = v.y;
            node->scale[2] = v.z;
            break;
        }
        default:
            /* Morph target weights not yet supported — skip silently */
            continue; /* skip modified[] flag below */
        }

        modified[ch->target_node] = 1;
    }

    /* Rebuild local_transform for modified nodes */
    for (uint32_t i = 0; i < node_count; i++) {
        if (modified[i])
            forge_pipeline__rebuild_local(&nodes[i]);
    }
}

/* ── World transform computation ─────────────────────────────────────── */

/* Recursive helper: propagate parent_world × local → world for a subtree.
 * Uses the indexed child array for O(child_count) traversal instead of
 * scanning all nodes. Cycle detection via visit_state prevents unbounded
 * recursion on malformed input: 0 = unvisited, 1 = in-progress, 2 = done. */
static void forge_pipeline__propagate_world(
    ForgePipelineSceneNode *nodes, uint32_t node_count,
    const uint32_t *children, uint32_t child_array_count,
    uint32_t node_idx, const mat4 *parent_world, uint8_t *visit_state)
{
    if (node_idx >= node_count) {
        SDL_Log("forge_pipeline: world transform traversal: node index %u "
                "out of range (node_count=%u)",
                (unsigned)node_idx, (unsigned)node_count);
        return;
    }
    if (visit_state[node_idx] == FORGE_PIPELINE_VISIT_IN_PROGRESS) {
        SDL_Log("forge_pipeline: cycle detected at node %u — "
                "skipping to prevent infinite recursion",
                (unsigned)node_idx);
        return;
    }
    if (visit_state[node_idx] == FORGE_PIPELINE_VISIT_DONE) return;
    visit_state[node_idx] = FORGE_PIPELINE_VISIT_IN_PROGRESS;

    ForgePipelineSceneNode *node = &nodes[node_idx];
    mat4 local;
    SDL_memcpy(&local, node->local_transform, sizeof(local));
    mat4 world = mat4_multiply(*parent_world, local);
    SDL_memcpy(node->world_transform, &world, sizeof(world));

    /* Recurse into children using the indexed child array (O(child_count),
     * matching forge_pipeline__compute_world_transforms). */
    if (children && node->child_count > 0) {
        for (uint32_t ci = 0; ci < node->child_count; ci++) {
            uint32_t arr_idx = node->first_child + ci;
            if (arr_idx >= child_array_count) {
                SDL_Log("forge_pipeline: world transform traversal: "
                        "child array index %u out of range "
                        "(child_array_count=%u) for node %u",
                        (unsigned)arr_idx, (unsigned)child_array_count,
                        (unsigned)node_idx);
                break;
            }
            uint32_t child_idx = children[arr_idx];
            forge_pipeline__propagate_world(
                nodes, node_count, children, child_array_count,
                child_idx, &world, visit_state);
        }
    }

    visit_state[node_idx] = FORGE_PIPELINE_VISIT_DONE;
}

void forge_pipeline_scene_compute_world_transforms(
    ForgePipelineSceneNode *nodes, uint32_t node_count,
    const uint32_t *root_nodes, uint32_t root_count,
    const uint32_t *children, uint32_t child_count)
{
    if (!nodes || node_count == 0) return;

    if (node_count > FORGE_PIPELINE_MAX_NODES) {
        SDL_Log("forge_pipeline: compute_world_transforms: node_count %u "
                "exceeds FORGE_PIPELINE_MAX_NODES (%u) — skipping",
                node_count, FORGE_PIPELINE_MAX_NODES);
        return;
    }

    /* Stack-allocated visit state (4096 bytes max) for cycle detection.
     * MSVC C99 does not support VLAs, so we use the fixed upper bound. */
    uint8_t visit_state[FORGE_PIPELINE_MAX_NODES];
    SDL_memset(visit_state, FORGE_PIPELINE_VISIT_UNVISITED,
               node_count * sizeof(uint8_t));

    mat4 identity = mat4_identity();

    if (root_nodes && root_count > 0) {
        for (uint32_t i = 0; i < root_count; i++) {
            forge_pipeline__propagate_world(
                nodes, node_count, children, child_count,
                root_nodes[i], &identity, visit_state);
        }
    } else {
        /* No explicit roots — process nodes with parent == -1 */
        for (uint32_t i = 0; i < node_count; i++) {
            if (nodes[i].parent < 0) {
                forge_pipeline__propagate_world(
                    nodes, node_count, children, child_count,
                    i, &identity, visit_state);
            }
        }
    }
}

/* ── Joint matrix computation ────────────────────────────────────────── */

uint32_t forge_pipeline_compute_joint_matrices(
    const ForgePipelineSkin *skin,
    const ForgePipelineSceneNode *nodes, uint32_t node_count,
    int mesh_node_index,
    mat4 *out_matrices, uint32_t max_joints)
{
    if (!skin || !nodes || !out_matrices || max_joints == 0)
        return 0;
    if (skin->joint_count == 0 || !skin->joints ||
        !skin->inverse_bind_matrices)
        return 0;

    /* Get mesh node world transform and compute its inverse */
    mat4 mesh_world = mat4_identity();
    if (mesh_node_index >= 0 && (uint32_t)mesh_node_index < node_count) {
        SDL_memcpy(&mesh_world,
                   nodes[mesh_node_index].world_transform, sizeof(mesh_world));
    }
    mat4 inv_mesh_world = mat4_inverse(mesh_world);

    uint32_t count = skin->joint_count;
    if (count > max_joints) count = max_joints;

    for (uint32_t i = 0; i < count; i++) {
        int32_t joint_idx = skin->joints[i];

        /* Skip invalid joint indices — identity prevents deformation */
        if (joint_idx < 0 || (uint32_t)joint_idx >= node_count) {
            out_matrices[i] = mat4_identity();
            continue;
        }

        mat4 joint_world;
        SDL_memcpy(&joint_world, nodes[joint_idx].world_transform, sizeof(joint_world));

        /* Inverse bind matrix from the skin data */
        mat4 ibm;
        SDL_memcpy(&ibm, &skin->inverse_bind_matrices[i * 16], sizeof(ibm));

        /* joint_matrix = inv(mesh_world) × joint_world × IBM */
        out_matrices[i] = mat4_multiply(
            inv_mesh_world, mat4_multiply(joint_world, ibm));
    }

    return count;
}

#endif /* FORGE_MATH_H — animation evaluation implementation */

#endif /* FORGE_PIPELINE_IMPLEMENTATION */

#endif /* FORGE_PIPELINE_H */
