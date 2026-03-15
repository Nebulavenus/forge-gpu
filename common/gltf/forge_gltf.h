/*
 * forge_gltf.h — Header-only glTF 2.0 parser for forge-gpu
 *
 * Parses a .gltf JSON file + binary buffers into CPU-side data structures
 * (vertices, indices, materials, nodes, transforms).  The caller is
 * responsible for uploading data to the GPU and loading textures.
 *
 * This keeps GPU concerns out of the parser, making it testable and
 * reusable.  The GPU lesson (Lesson 09) shows how to use these data
 * structures with SDL_GPU.
 *
 * Dependencies:
 *   - SDL3       (for file I/O, logging, memory allocation)
 *   - cJSON      (for JSON parsing — third_party/cJSON/)
 *   - forge_math (for vec2, vec3, mat4, quat)
 *
 * Usage:
 *   #include "gltf/forge_gltf.h"
 *
 *   ForgeArena arena = forge_arena_create(0);
 *   ForgeGltfScene scene;
 *   if (forge_gltf_load("model.gltf", &scene, &arena)) {
 *       // Access scene.nodes, scene.meshes, scene.primitives, etc.
 *       // Upload to GPU, render, etc.
 *   }
 *   forge_arena_destroy(&arena);  // frees all scene memory at once
 *
 * See: lessons/gpu/09-scene-loading/ for a full usage example
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_GLTF_H
#define FORGE_GLTF_H

#include <SDL3/SDL.h>
#include <limits.h>   /* INT_MAX */
#include "cJSON.h"
#include "math/forge_math.h"
#include "arena/forge_arena.h"

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Safe multiplication for allocation sizes.  Returns true if a * b fits
 * in size_t without overflow, and stores the product in *out.  Returns
 * false on overflow (product is undefined).  Used before every
 * forge_arena_alloc() call where the count comes from untrusted JSON. */
static bool forge_gltf__safe_mul(size_t a, size_t b, size_t *out)
{
    if (b != 0 && a > SIZE_MAX / b) return false;
    *out = a * b;
    return true;
}

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Maximum sizes for scene arrays.  Generous limits that cover typical
 * models (CesiumMilkTruck: 6 nodes; VirtualCity: 234 nodes, 167 materials). */
#define FORGE_GLTF_MAX_NODES      512
#define FORGE_GLTF_MAX_MESHES     256
#define FORGE_GLTF_MAX_PRIMITIVES 1024
#define FORGE_GLTF_MAX_MATERIALS  256
#define FORGE_GLTF_MAX_IMAGES     128
#define FORGE_GLTF_MAX_BUFFERS    16
#define FORGE_GLTF_MAX_SKINS      8
#define FORGE_GLTF_MAX_JOINTS     128
#define FORGE_GLTF_MAX_ANIMATIONS     16
#define FORGE_GLTF_MAX_ANIM_CHANNELS  128
#define FORGE_GLTF_MAX_ANIM_SAMPLERS  128
#define FORGE_GLTF_JOINTS_PER_VERT 4
#define FORGE_GLTF_MAX_MORPH_TARGETS       8
#define FORGE_GLTF_MORPH_DELTA_COMPONENTS  3  /* VEC3 per delta */

/* glTF 2.0 spec default for alphaCutoff when alphaMode is MASK. */
#define FORGE_GLTF_DEFAULT_ALPHA_CUTOFF 0.5f

/* Approximate alpha for KHR_materials_transmission surfaces.
 * Full transmission requires refraction and screen-space techniques;
 * we approximate it as standard alpha blending at this opacity. */
#define FORGE_GLTF_TRANSMISSION_ALPHA 0.5f

/* glTF component type constants (from the spec). */
#define FORGE_GLTF_BYTE           5120
#define FORGE_GLTF_UNSIGNED_BYTE  5121
#define FORGE_GLTF_SHORT          5122
#define FORGE_GLTF_UNSIGNED_SHORT 5123
#define FORGE_GLTF_UNSIGNED_INT   5125
#define FORGE_GLTF_FLOAT          5126

/* glTF tangent vectors are VEC4: xyz = direction, w = handedness */
#define FORGE_GLTF_TANGENT_COMPONENTS 4

/* Maximum path length for file references. */
#define FORGE_GLTF_PATH_SIZE 512

/* Maximum children per node (VirtualCity root has 131). */
#define FORGE_GLTF_MAX_CHILDREN 256

/* Maximum name length. */
#define FORGE_GLTF_NAME_SIZE 64

/* ── Vertex layout ────────────────────────────────────────────────────────── */
/* Interleaved vertex: position (float3) + normal (float3) + uv (float2).
 * Same layout as ForgeObjVertex, so the GPU pipeline is compatible. */

typedef struct ForgeGltfVertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
} ForgeGltfVertex;

/* ── Morph target (blend shape deltas) ───────────────────────────────────── */
/* Each morph target stores per-vertex displacement deltas for position,
 * normal, and/or tangent attributes.  The base mesh vertices are displaced
 * by: base + weight * delta for each active target.  Delta arrays are
 * arena-allocated and have vertex_count elements (matching the primitive). */

typedef struct ForgeGltfMorphTarget {
    float *position_deltas;  /* vertex_count × 3 floats, NULL if absent */
    float *normal_deltas;    /* vertex_count × 3 floats, NULL if absent */
    float *tangent_deltas;   /* vertex_count × 3 floats, NULL if absent */
} ForgeGltfMorphTarget;

/* ── Primitive (one draw call) ────────────────────────────────────────────── */
/* A primitive is a set of vertices + indices sharing one material.
 * A mesh may contain multiple primitives (one per material). */

typedef struct ForgeGltfPrimitive {
    ForgeGltfVertex *vertices;
    Uint32           vertex_count;
    void            *indices;       /* uint16_t or uint32_t array */
    Uint32           index_count;
    Uint32           index_stride;  /* 2 = uint16, 4 = uint32 */
    int              material_index; /* -1 = no material assigned */
    bool             has_uvs;       /* true if TEXCOORD_0 was present */
    vec4            *tangents;      /* NULL if no TANGENT attribute */
    bool             has_tangents;  /* true if TANGENT (VEC4) was present */
    Uint16          *joint_indices; /* 4 uint16 per vertex (JOINTS_0) — NULL if absent */
    float           *weights;       /* 4 float  per vertex (WEIGHTS_0) — NULL if absent */
    bool             has_skin_data; /* true if both JOINTS_0 and WEIGHTS_0 present */
    ForgeGltfMorphTarget *morph_targets;   /* arena-allocated, morph_target_count elements */
    int                   morph_target_count;
} ForgeGltfPrimitive;

/* ── Mesh ─────────────────────────────────────────────────────────────────── */
/* A mesh is a named collection of primitives. */

typedef struct ForgeGltfMesh {
    int  first_primitive;  /* index into scene.primitives[] */
    int  primitive_count;
    char name[FORGE_GLTF_NAME_SIZE];
    float *default_weights;    /* arena-allocated, default_weight_count elements (from mesh.weights) */
    int    default_weight_count;
} ForgeGltfMesh;

/* ── Alpha mode ───────────────────────────────────────────────────────────── */
/* Maps directly to glTF 2.0 alphaMode.  OPAQUE is the default.
 * When KHR_materials_transmission is present and no explicit alphaMode
 * is set, the parser promotes the material to BLEND as an approximation. */

typedef enum ForgeGltfAlphaMode {
    FORGE_GLTF_ALPHA_OPAQUE = 0,   /* fully opaque (default)              */
    FORGE_GLTF_ALPHA_MASK   = 1,   /* binary cutout via alphaCutoff       */
    FORGE_GLTF_ALPHA_BLEND  = 2    /* smooth transparency, needs sorting  */
} ForgeGltfAlphaMode;

/* ── Material ─────────────────────────────────────────────────────────────── */
/* PBR metallic-roughness material (glTF 2.0 core specification).
 * We store file paths (not GPU textures) so the caller can load
 * textures using whatever method they prefer. */

typedef struct ForgeGltfMaterial {
    float base_color[4];                       /* RGBA, default (1,1,1,1) */
    char  texture_path[FORGE_GLTF_PATH_SIZE];  /* empty = no texture */
    bool  has_texture;                         /* true if base color texture set */
    char  name[FORGE_GLTF_NAME_SIZE];          /* material name from glTF */
    ForgeGltfAlphaMode alpha_mode;             /* OPAQUE, MASK, or BLEND  */
    float              alpha_cutoff;           /* MASK threshold (def 0.5)*/
    bool               double_sided;           /* render both faces?      */
    char  normal_map_path[FORGE_GLTF_PATH_SIZE]; /* empty = no normal map */
    bool  has_normal_map;                      /* true if normalTexture set */

    /* Normal texture scale — multiplier for the sampled normal XY
     * components.  Default 1.0 per glTF spec.  Values < 1.0 flatten
     * the normal map effect, values > 1.0 exaggerate it. */
    float normal_scale;

    /* PBR metallic-roughness factors.  metallicFactor defaults to 1.0
     * and roughnessFactor defaults to 1.0 per the glTF spec.  These
     * are multiplied with the corresponding texture values if present. */
    float metallic_factor;
    float roughness_factor;
    char  metallic_roughness_path[FORGE_GLTF_PATH_SIZE]; /* empty = none */
    bool  has_metallic_roughness;              /* true if MR texture set */

    /* Occlusion texture — baked ambient occlusion stored in the R channel.
     * Strength (default 1.0) scales the occlusion effect: 0.0 = no
     * occlusion, 1.0 = full occlusion from the texture. */
    char  occlusion_path[FORGE_GLTF_PATH_SIZE];
    bool  has_occlusion;
    float occlusion_strength;

    /* Emissive — light emitted by the surface.  emissive_factor is an
     * RGB multiplier (default 0,0,0 = no emission).  When a texture is
     * present, the final emission is texture RGB × emissive_factor. */
    float emissive_factor[3];
    char  emissive_path[FORGE_GLTF_PATH_SIZE];
    bool  has_emissive;
} ForgeGltfMaterial;

/* ── Node ─────────────────────────────────────────────────────────────────── */
/* A node in the scene hierarchy with TRS transform. */

typedef struct ForgeGltfNode {
    int  mesh_index;      /* -1 = transform-only node (no geometry) */
    int  parent;          /* -1 = root */
    int  *children;       /* arena-allocated, child_count elements */
    int  child_count;
    mat4 local_transform; /* computed from TRS or raw matrix */
    mat4 world_transform; /* accumulated from root (set by compute_world_transforms) */
    vec3 translation;     /* decomposed TRS — for animation (default 0,0,0) */
    quat rotation;        /* decomposed TRS — for animation (default identity) */
    vec3 scale_xyz;       /* decomposed TRS — for animation (default 1,1,1) */
    bool has_trs;         /* true if node uses TRS (not a raw matrix) */
    int  skin_index;      /* index into scene.skins[], -1 = no skin */
    char name[FORGE_GLTF_NAME_SIZE];
} ForgeGltfNode;

/* ── Skin (skeletal hierarchy for vertex skinning) ────────────────────────── */
/* A skin maps a set of joint nodes to their inverse bind matrices.
 * At runtime, the joint matrix for vertex skinning is:
 *   jointMatrix[i] = node[joints[i]].worldTransform * inverseBindMatrices[i]
 * The inverse bind matrix transforms a vertex from model space into the
 * joint's local coordinate system at the bind pose. */

typedef struct ForgeGltfSkin {
    char name[FORGE_GLTF_NAME_SIZE];
    int  *joints;                                 /* arena-allocated, joint_count elements */
    int  joint_count;
    int  skeleton;                                /* root joint node, -1 if unset */
    mat4 *inverse_bind_matrices;                  /* arena-allocated, joint_count elements */
} ForgeGltfSkin;

/* ── Animation ────────────────────────────────────────────────────────────── */
/* glTF stores animations as a set of channels, each binding a sampler to a
 * node property (translation, rotation, or scale).  A sampler holds the
 * keyframe timestamps and output values with an interpolation mode.
 *
 * Data pointers reference the loaded binary buffers — no copying needed.
 * They remain valid for the lifetime of the ForgeGltfScene. */

/* Animation channel target path — TRS components or morph weights. */
typedef enum ForgeGltfAnimPath {
    FORGE_GLTF_ANIM_TRANSLATION = 0,
    FORGE_GLTF_ANIM_ROTATION    = 1,
    FORGE_GLTF_ANIM_SCALE       = 2,
    FORGE_GLTF_ANIM_MORPH_WEIGHTS = 3
} ForgeGltfAnimPath;

/* Component counts for animation target paths. */
#define FORGE_GLTF_ANIM_VEC3_COMPONENTS 3  /* translation and scale */
#define FORGE_GLTF_ANIM_QUAT_COMPONENTS 4  /* rotation */

/* Sampler interpolation mode.  CUBICSPLINE is not supported — the parser
 * logs a warning and skips the sampler entirely. */
typedef enum ForgeGltfInterpolation {
    FORGE_GLTF_INTERP_LINEAR = 0,  /* lerp for vec3, slerp for quat */
    FORGE_GLTF_INTERP_STEP   = 1   /* hold previous value until next keyframe */
} ForgeGltfInterpolation;

/* An animation sampler: keyframe timestamps paired with output values.
 * Pointers reference data inside scene->buffers[] (not owned). */
typedef struct ForgeGltfAnimSampler {
    const float           *timestamps;      /* keyframe_count floats              */
    const float           *values;          /* keyframe_count × value_components  */
    int                    keyframe_count;
    int                    value_components; /* 3 vec3, 4 quat, or N morph weights/keyframe */
    ForgeGltfInterpolation interpolation;
} ForgeGltfAnimSampler;

/* An animation channel: binds a sampler to a node property. */
typedef struct ForgeGltfAnimChannel {
    int               target_node;   /* index into scene->nodes[] */
    ForgeGltfAnimPath target_path;   /* TRS component or morph weights */
    int               sampler_index; /* index into parent animation's samplers[] */
} ForgeGltfAnimChannel;

/* A named animation clip containing samplers and channels. */
typedef struct ForgeGltfAnimation {
    char                  name[FORGE_GLTF_NAME_SIZE];
    float                 duration;  /* max timestamp across all samplers */
    ForgeGltfAnimSampler  *samplers; /* arena-allocated, sampler_count elements */
    int                   sampler_count;
    ForgeGltfAnimChannel  *channels; /* arena-allocated, channel_count elements */
    int                   channel_count;
} ForgeGltfAnimation;

/* ── Binary buffer ────────────────────────────────────────────────────────── */
/* A loaded .bin file referenced by the glTF. */

typedef struct ForgeGltfBuffer {
    Uint8  *data;
    Uint32  size;
} ForgeGltfBuffer;

/* ── Scene (top-level result) ─────────────────────────────────────────────── */
/* Everything parsed from a .gltf file.  All arrays are arena-allocated
 * via the ForgeArena passed to forge_gltf_load().  The arena owns all
 * memory — destroy the arena to free everything at once.
 *
 * Before arenas: ForgeGltfScene was ~1.5 MB of fixed-size arrays
 * (512 nodes × 256 children each = too large for the stack, hard limits
 * on scene complexity).  Now it is ~100 bytes of pointers — safe on the
 * stack, and scenes of any size work as long as the arena can grow. */

typedef struct ForgeGltfScene {
    ForgeGltfNode      *nodes;       /* arena-allocated, node_count elements */
    int                node_count;

    ForgeGltfMesh      *meshes;      /* arena-allocated, mesh_count elements */
    int                mesh_count;

    ForgeGltfPrimitive *primitives;  /* arena-allocated, primitive_count elements */
    int                primitive_count;

    ForgeGltfMaterial  *materials;   /* arena-allocated, material_count elements */
    int                material_count;

    ForgeGltfBuffer    buffers[FORGE_GLTF_MAX_BUFFERS]; /* fixed — max 16, tiny */
    int                buffer_count;

    int                *root_nodes;  /* arena-allocated, root_node_count elements */
    int                root_node_count;

    ForgeGltfSkin      *skins;       /* arena-allocated, skin_count elements */
    int                skin_count;

    ForgeGltfAnimation *animations;  /* arena-allocated, animation_count elements */
    int                animation_count;
} ForgeGltfScene;

/* ── API ──────────────────────────────────────────────────────────────────── */

/* Load a .gltf file and all referenced .bin buffers.
 * All memory is allocated from the provided arena.  On success, returns
 * true and fills *scene.  To free, destroy the arena — no separate free
 * call is needed.  forge_gltf_free() is retained as a no-op for
 * transition convenience.
 * On failure, returns false; the arena may contain partial allocations
 * (destroy it to clean up). */
static bool forge_gltf_load(const char *gltf_path, ForgeGltfScene *scene,
                            ForgeArena *arena);

/* Legacy free — now a no-op.  All memory is owned by the arena passed
 * to forge_gltf_load().  Destroy the arena to release everything. */
static void forge_gltf_free(ForgeGltfScene *scene);

/* Recursively compute world_transform for all nodes in the hierarchy.
 * Called automatically by forge_gltf_load(), but exposed in case you
 * need to recompute after modifying local transforms.
 * Returns false if the depth limit is reached (possible cycle). */
static bool forge_gltf_compute_world_transforms(ForgeGltfScene *scene,
                                                 int node_idx,
                                                 const mat4 *parent_world);

/* ══════════════════════════════════════════════════════════════════════════
 * Implementation (header-only — all functions are static)
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── File I/O helpers ────────────────────────────────────────────────────── */

static char *read_text(const char *path)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io) {
        SDL_Log("forge_gltf: failed to open '%s': %s", path, SDL_GetError());
        return NULL;
    }

    Sint64 size = SDL_GetIOSize(io);
    if (size < 0) {
        SDL_Log("forge_gltf: failed to get size of '%s': %s",
                path, SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }

    char *buf = (char *)SDL_calloc(1, (size_t)size + 1);
    if (!buf) {
        SDL_Log("forge_gltf: alloc failed for '%s' (%lld bytes)",
                path, (long long)size);
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }

    if (SDL_ReadIO(io, buf, (size_t)size) != (size_t)size) {
        SDL_Log("forge_gltf: read failed for '%s': %s", path, SDL_GetError());
        SDL_free(buf);
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }

    if (!SDL_CloseIO(io)) {
        SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                path, SDL_GetError());
        SDL_free(buf);
        return NULL;
    }
    return buf;
}

static Uint8 *read_binary(const char *path, Uint32 *out_size,
                          ForgeArena *arena)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io) {
        SDL_Log("forge_gltf: failed to open '%s': %s", path, SDL_GetError());
        return NULL;
    }

    Sint64 size = SDL_GetIOSize(io);
    if (size < 0) {
        SDL_Log("forge_gltf: failed to get size of '%s': %s",
                path, SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }
    if (size > UINT32_MAX) {
        SDL_Log("forge_gltf: file '%s' too large (%lld bytes)",
                path, (long long)size);
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }

    Uint8 *buf = (Uint8 *)forge_arena_alloc(arena, (size_t)size);
    if (!buf) {
        SDL_Log("forge_gltf: arena alloc failed for '%s' (%lld bytes)",
                path, (long long)size);
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }

    if (SDL_ReadIO(io, buf, (size_t)size) != (size_t)size) {
        SDL_Log("forge_gltf: read failed for '%s': %s", path, SDL_GetError());
        if (!SDL_CloseIO(io)) {
            SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                    path, SDL_GetError());
        }
        return NULL;
    }

    if (!SDL_CloseIO(io)) {
        SDL_Log("forge_gltf: SDL_CloseIO failed for '%s': %s",
                path, SDL_GetError());
        return NULL;
    }
    *out_size = (Uint32)size;
    return buf;
}

/* ── Path helpers ────────────────────────────────────────────────────────── */

static void build_path(char *out, size_t out_size,
                                    const char *base_dir, const char *relative)
{
    SDL_snprintf(out, out_size, "%s%s", base_dir, relative);
}

/* Resolve a glTF texture reference to a file path.
 * Follows the chain: textureInfo.index → textures[i].source → images[j].uri.
 * Returns true and fills out_path if the full chain resolves, false otherwise. */
static bool forge_gltf__resolve_texture(const cJSON *tex_info,
                                         const cJSON *textures_arr,
                                         const cJSON *images_arr,
                                         const char *base_dir,
                                         char *out_path, size_t out_size)
{
    if (!tex_info || !out_path || out_size == 0)
        return false;
    if (!cJSON_IsArray(textures_arr))
        return false;
    const cJSON *idx = cJSON_GetObjectItemCaseSensitive(tex_info, "index");
    if (!cJSON_IsNumber(idx) || idx->valueint < 0)
        return false;
    const cJSON *tex_obj = cJSON_GetArrayItem(textures_arr, idx->valueint);
    if (!tex_obj)
        return false;
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(tex_obj, "source");
    if (!cJSON_IsNumber(source) || source->valueint < 0)
        return false;
    if (!cJSON_IsArray(images_arr))
        return false;
    const cJSON *img = cJSON_GetArrayItem(images_arr, source->valueint);
    if (!img)
        return false;
    const cJSON *uri = cJSON_GetObjectItemCaseSensitive(img, "uri");
    if (!cJSON_IsString(uri))
        return false;
    build_path(out_path, out_size, base_dir, uri->valuestring);
    return true;
}

static void get_base_dir(char *base_dir, size_t base_dir_size,
                                      const char *gltf_path)
{
    SDL_strlcpy(base_dir, gltf_path, base_dir_size);
    char *last_sep = NULL;
    for (char *p = base_dir; *p; p++) {
        if (*p == '/' || *p == '\\') last_sep = p;
    }
    if (last_sep) {
        last_sep[1] = '\0';
    } else {
        base_dir[0] = '\0';
    }
}

/* ── cJSON helpers ───────────────────────────────────────────────────────── */

static void copy_name(char *dst, size_t dst_size,
                                   const cJSON *obj)
{
    dst[0] = '\0';
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(obj, "name");
    if (cJSON_IsString(name) && name->valuestring) {
        SDL_strlcpy(dst, name->valuestring, dst_size);
    }
}

/* ── Accessor helpers ─────────────────────────────────────────────────────── */

/* Return the byte size of one component, or 0 if the type is invalid.
 * glTF 2.0 allows six component types (5120–5126, skipping 5124). */
static int component_size(int component_type)
{
    switch (component_type) {
    case FORGE_GLTF_BYTE:           return 1;
    case FORGE_GLTF_UNSIGNED_BYTE:  return 1;
    case FORGE_GLTF_SHORT:          return 2;
    case FORGE_GLTF_UNSIGNED_SHORT: return 2;
    case FORGE_GLTF_UNSIGNED_INT:   return 4;
    case FORGE_GLTF_FLOAT:          return 4;
    default: return 0;
    }
}

/* Return the number of scalar components for an accessor type string.
 * E.g. "VEC3" → 3, "SCALAR" → 1.  Returns 0 for unknown types. */
static int type_component_count(const char *type)
{
    if (SDL_strcmp(type, "SCALAR") == 0) return 1;
    if (SDL_strcmp(type, "VEC2") == 0)   return 2;
    if (SDL_strcmp(type, "VEC3") == 0)   return 3;
    if (SDL_strcmp(type, "VEC4") == 0)   return 4;
    if (SDL_strcmp(type, "MAT2") == 0)   return 4;
    if (SDL_strcmp(type, "MAT3") == 0)   return 9;
    if (SDL_strcmp(type, "MAT4") == 0)   return 16;
    return 0;
}

/* ── Accessor data access ────────────────────────────────────────────────── */
/* Follow the glTF accessor → bufferView → buffer chain to find raw data.
 * Validates componentType, bufferView.byteLength, and accessor bounds
 * per the glTF 2.0 specification before returning a pointer.
 * Returns the number of components (1 for SCALAR, 2 for VEC2, 3 for VEC3,
 * etc.) via *out_num_components.  Callers must validate this against the
 * glTF spec requirements for each attribute (e.g. POSITION requires VEC3,
 * indices require SCALAR). */

static const void *forge_gltf__get_accessor(
    const cJSON *root, const ForgeGltfScene *scene,
    int accessor_idx, int *out_count, int *out_component_type,
    int *out_num_components)
{
    const cJSON *accessors = cJSON_GetObjectItemCaseSensitive(root, "accessors");
    const cJSON *views = cJSON_GetObjectItemCaseSensitive(root, "bufferViews");
    if (!accessors || !views) return NULL;

    const cJSON *acc = cJSON_GetArrayItem(accessors, accessor_idx);
    if (!acc) return NULL;

    const cJSON *bv_idx = cJSON_GetObjectItemCaseSensitive(acc, "bufferView");
    const cJSON *comp = cJSON_GetObjectItemCaseSensitive(acc, "componentType");
    const cJSON *cnt = cJSON_GetObjectItemCaseSensitive(acc, "count");
    const cJSON *type_str = cJSON_GetObjectItemCaseSensitive(acc, "type");
    if (!bv_idx || !comp || !cnt || !cJSON_IsString(type_str) ||
        !cJSON_IsNumber(comp) || !cJSON_IsNumber(bv_idx) || !cJSON_IsNumber(cnt))
        return NULL;

    /* Validate componentType is one of the six values allowed by the spec. */
    int comp_size = component_size(comp->valueint);
    if (comp_size == 0) {
        SDL_Log("forge_gltf: accessor %d has invalid componentType %d",
                accessor_idx, comp->valueint);
        return NULL;
    }

    /* Determine element size from accessor type (SCALAR, VEC2, VEC3, etc.). */
    int num_components = type_component_count(type_str->valuestring);
    if (num_components == 0) {
        SDL_Log("forge_gltf: accessor %d has unknown type '%s'",
                accessor_idx, type_str->valuestring);
        return NULL;
    }

    int acc_offset = 0;
    const cJSON *acc_off = cJSON_GetObjectItemCaseSensitive(acc, "byteOffset");
    if (cJSON_IsNumber(acc_off)) {
        if (acc_off->valueint < 0) {
            SDL_Log("forge_gltf: accessor %d has negative byteOffset %d",
                    accessor_idx, acc_off->valueint);
            return NULL;
        }
        acc_offset = acc_off->valueint;
    }

    /* Bounds-check the bufferView index before accessing the array. */
    int view_count = cJSON_GetArraySize(views);
    if (bv_idx->valueint < 0 || bv_idx->valueint >= view_count) return NULL;

    const cJSON *view = cJSON_GetArrayItem(views, bv_idx->valueint);
    if (!view) return NULL;

    const cJSON *buf_idx = cJSON_GetObjectItemCaseSensitive(view, "buffer");
    const cJSON *bv_off_json = cJSON_GetObjectItemCaseSensitive(view, "byteOffset");
    const cJSON *bv_len_json = cJSON_GetObjectItemCaseSensitive(view, "byteLength");
    if (!buf_idx || !cJSON_IsNumber(buf_idx)) return NULL;

    int bi = buf_idx->valueint;
    if (bi < 0 || bi >= scene->buffer_count) return NULL;

    Uint32 bv_offset = 0;
    if (cJSON_IsNumber(bv_off_json)) {
        if (bv_off_json->valueint < 0) {
            SDL_Log("forge_gltf: bufferView %d has negative byteOffset %d",
                    bv_idx->valueint, bv_off_json->valueint);
            return NULL;
        }
        bv_offset = (Uint32)bv_off_json->valueint;
    }

    /* bufferView.byteLength is required by the spec — reject if missing. */
    if (!cJSON_IsNumber(bv_len_json) || bv_len_json->valueint <= 0) {
        SDL_Log("forge_gltf: bufferView %d missing or invalid byteLength",
                bv_idx->valueint);
        return NULL;
    }
    Uint32 bv_byte_length = (Uint32)bv_len_json->valueint;

    /* Ensure the bufferView itself fits within the binary buffer.
     * Use subtraction form to avoid Uint32 overflow in the addition. */
    if (bv_byte_length > scene->buffers[bi].size
        || bv_offset > scene->buffers[bi].size - bv_byte_length) {
        SDL_Log("forge_gltf: bufferView %d exceeds buffer %d bounds "
                "(offset %u + length %u > %u)",
                bv_idx->valueint, bi,
                bv_offset, bv_byte_length, scene->buffers[bi].size);
        return NULL;
    }

    /* Validate the accessor's data range fits within the bufferView.
     * Per glTF spec: byteOffset + (count-1)*stride + elementSize <= byteLength */
    int element_size = num_components * comp_size;
    int byte_stride = element_size; /* tightly packed by default */
    const cJSON *bv_stride_json = cJSON_GetObjectItemCaseSensitive(
        view, "byteStride");
    if (cJSON_IsNumber(bv_stride_json)) {
        if (bv_stride_json->valueint < 0) {
            SDL_Log("forge_gltf: accessor %d has negative byteStride %d",
                    accessor_idx, bv_stride_json->valueint);
            return NULL;
        }
        if (bv_stride_json->valueint > 0) {
            byte_stride = bv_stride_json->valueint;
        }
    }

    /* Reject interleaved accessors — consumers assume tightly packed data.
     * Per glTF spec byteStride==0 means tightly packed; we also accept
     * byteStride==element_size (which is equivalent).  Any other stride
     * means the accessor interleaves with other attributes and cannot be
     * read as a contiguous array. */
    if (byte_stride != element_size) {
        SDL_Log("forge_gltf: accessor %d has interleaved byteStride %d "
                "(expected %d for tightly packed data)",
                accessor_idx, byte_stride, element_size);
        return NULL;
    }

    int count = cnt->valueint;
    if (count < 0) {
        SDL_Log("forge_gltf: accessor %d has negative count %d",
                accessor_idx, count);
        return NULL;
    }
    if (count > 0) {
        /* Compute required = acc_offset + (count-1)*byte_stride + element_size.
         * Use size_t arithmetic to avoid Uint32 overflow on large counts. */
        size_t stride_span;
        if (!forge_gltf__safe_mul((size_t)(count - 1), (size_t)byte_stride,
                                  &stride_span)) {
            SDL_Log("forge_gltf: accessor %d stride span overflow",
                    accessor_idx);
            return NULL;
        }
        size_t required = (size_t)acc_offset + stride_span + (size_t)element_size;
        /* Check for addition overflow (carry past SIZE_MAX). */
        if (required < stride_span || required > (size_t)bv_byte_length) {
            SDL_Log("forge_gltf: accessor %d exceeds bufferView %d bounds "
                    "(need %zu bytes, view has %u)",
                    accessor_idx, bv_idx->valueint, required, bv_byte_length);
            return NULL;
        }
    }

    if (out_count) *out_count = count;
    if (out_component_type) *out_component_type = comp->valueint;
    if (out_num_components) *out_num_components = num_components;

    /* Validate alignment.  The glTF spec requires accessor byte offsets
     * to be aligned to the component size (e.g. 4 bytes for floats).
     * A malformed file violating this would cause undefined behavior on
     * strict-alignment architectures (ARM, MIPS).  Reject rather than
     * risk a bus error. */
    Uint32 total_offset = bv_offset + (Uint32)acc_offset;
    if (comp_size > 1) {
        if ((total_offset % (Uint32)comp_size) != 0) {
            SDL_Log("forge_gltf: accessor %d data at offset %u is not aligned "
                    "to %d-byte boundary",
                    accessor_idx, total_offset, comp_size);
            return NULL;
        }
        if (count > 1 && (byte_stride % comp_size) != 0) {
            SDL_Log("forge_gltf: accessor %d byteStride %d is not aligned "
                    "to %d-byte components",
                    accessor_idx, byte_stride, comp_size);
            return NULL;
        }
    }

    return scene->buffers[bi].data + total_offset;
}

/* ── Parse binary buffers ────────────────────────────────────────────────── */

static bool forge_gltf__parse_buffers(const cJSON *root, const char *base_dir,
                                       ForgeGltfScene *scene,
                                       ForgeArena *arena)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "buffers");
    if (!cJSON_IsArray(arr)) {
        SDL_Log("forge_gltf: no 'buffers' array");
        return false;
    }

    int count = cJSON_GetArraySize(arr);
    if (count > FORGE_GLTF_MAX_BUFFERS) {
        SDL_Log("forge_gltf: too many buffers (%d, max %d)",
                count, FORGE_GLTF_MAX_BUFFERS);
        return false;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *buf_obj = cJSON_GetArrayItem(arr, i);
        const cJSON *uri = cJSON_GetObjectItemCaseSensitive(buf_obj, "uri");
        if (!cJSON_IsString(uri)) {
            SDL_Log("forge_gltf: buffer %d missing 'uri'", i);
            return false;
        }

        char path[FORGE_GLTF_PATH_SIZE];
        build_path(path, sizeof(path), base_dir,
                                uri->valuestring);

        Uint32 file_size = 0;
        scene->buffers[i].data = read_binary(path, &file_size, arena);
        if (!scene->buffers[i].data) return false;
        scene->buffers[i].size = file_size;
    }
    scene->buffer_count = count;
    return true;
}

/* ── Parse materials ─────────────────────────────────────────────────────── */

static bool forge_gltf__parse_materials(const cJSON *root,
                                         const char *base_dir,
                                         ForgeGltfScene *scene,
                                         ForgeArena *arena)
{
    const cJSON *mats = cJSON_GetObjectItemCaseSensitive(root, "materials");
    if (!cJSON_IsArray(mats)) {
        scene->material_count = 0;
        return true;
    }

    const cJSON *images_arr = cJSON_GetObjectItemCaseSensitive(root, "images");
    const cJSON *textures_arr = cJSON_GetObjectItemCaseSensitive(root, "textures");

    int count = cJSON_GetArraySize(mats);
    if (count < 0) {
        SDL_Log("forge_gltf: invalid material count");
        return false;
    }
    if (count == 0) {
        scene->material_count = 0;
        return true;
    }

    size_t mat_bytes;
    if (!forge_gltf__safe_mul((size_t)count, sizeof(ForgeGltfMaterial), &mat_bytes)) {
        SDL_Log("forge_gltf: material allocation size overflow");
        return false;
    }
    scene->materials = (ForgeGltfMaterial *)forge_arena_alloc(arena, mat_bytes);
    if (!scene->materials) {
        SDL_Log("forge_gltf: arena alloc failed for %d materials", count);
        return false;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *mat = cJSON_GetArrayItem(mats, i);
        ForgeGltfMaterial *m = &scene->materials[i];

        /* Defaults per glTF 2.0 spec: opaque white, fully metallic,
         * fully rough, no textures, single-sided, no emission. */
        m->base_color[0] = 1.0f;
        m->base_color[1] = 1.0f;
        m->base_color[2] = 1.0f;
        m->base_color[3] = 1.0f;
        m->texture_path[0] = '\0';
        m->has_texture = false;
        m->alpha_mode = FORGE_GLTF_ALPHA_OPAQUE;
        m->alpha_cutoff = FORGE_GLTF_DEFAULT_ALPHA_CUTOFF;
        m->double_sided = false;
        m->normal_map_path[0] = '\0';
        m->has_normal_map = false;
        m->normal_scale = 1.0f;
        m->metallic_factor = 1.0f;
        m->roughness_factor = 1.0f;
        m->metallic_roughness_path[0] = '\0';
        m->has_metallic_roughness = false;
        m->occlusion_path[0] = '\0';
        m->has_occlusion = false;
        m->occlusion_strength = 1.0f;
        m->emissive_factor[0] = 0.0f;
        m->emissive_factor[1] = 0.0f;
        m->emissive_factor[2] = 0.0f;
        m->emissive_path[0] = '\0';
        m->has_emissive = false;
        copy_name(m->name, sizeof(m->name), mat);

        /* ── Alpha mode (glTF 2.0 core) ─────────────────────────────── */
        const cJSON *am = cJSON_GetObjectItemCaseSensitive(mat, "alphaMode");
        if (cJSON_IsString(am)) {
            if (SDL_strcmp(am->valuestring, "MASK") == 0)
                m->alpha_mode = FORGE_GLTF_ALPHA_MASK;
            else if (SDL_strcmp(am->valuestring, "BLEND") == 0)
                m->alpha_mode = FORGE_GLTF_ALPHA_BLEND;
        }

        /* ── Alpha cutoff (only meaningful for MASK, default 0.5) ──── */
        const cJSON *ac = cJSON_GetObjectItemCaseSensitive(mat, "alphaCutoff");
        if (cJSON_IsNumber(ac)) {
            m->alpha_cutoff = (float)ac->valuedouble;
            if (m->alpha_cutoff < 0.0f) m->alpha_cutoff = 0.0f;
            if (m->alpha_cutoff > 1.0f) m->alpha_cutoff = 1.0f;
        }

        /* ── Double-sided flag ───────────────────────────────────────── */
        const cJSON *ds = cJSON_GetObjectItemCaseSensitive(mat, "doubleSided");
        if (cJSON_IsBool(ds))
            m->double_sided = cJSON_IsTrue(ds);

        /* ── pbrMetallicRoughness (optional per glTF spec) ─────────── */
        const cJSON *pbr = cJSON_GetObjectItemCaseSensitive(
            mat, "pbrMetallicRoughness");
        if (pbr) {
            /* Base color factor. */
            const cJSON *factor = cJSON_GetObjectItemCaseSensitive(
                pbr, "baseColorFactor");
            if (cJSON_IsArray(factor) && cJSON_GetArraySize(factor) == 4) {
                for (int fi = 0; fi < 4; fi++) {
                    const cJSON *elem = cJSON_GetArrayItem(factor, fi);
                    if (!cJSON_IsNumber(elem)) continue;
                    m->base_color[fi] = (float)elem->valuedouble;
                    if (m->base_color[fi] < 0.0f) m->base_color[fi] = 0.0f;
                    if (m->base_color[fi] > 1.0f) m->base_color[fi] = 1.0f;
                }
            }

            /* Metallic factor (default 1.0, clamped to [0, 1]). */
            const cJSON *mf = cJSON_GetObjectItemCaseSensitive(
                pbr, "metallicFactor");
            if (cJSON_IsNumber(mf)) {
                m->metallic_factor = (float)mf->valuedouble;
                if (m->metallic_factor < 0.0f) m->metallic_factor = 0.0f;
                if (m->metallic_factor > 1.0f) m->metallic_factor = 1.0f;
            }

            /* Roughness factor (default 1.0, clamped to [0, 1]). */
            const cJSON *rf = cJSON_GetObjectItemCaseSensitive(
                pbr, "roughnessFactor");
            if (cJSON_IsNumber(rf)) {
                m->roughness_factor = (float)rf->valuedouble;
                if (m->roughness_factor < 0.0f) m->roughness_factor = 0.0f;
                if (m->roughness_factor > 1.0f) m->roughness_factor = 1.0f;
            }

            /* Base color texture. */
            m->has_texture = forge_gltf__resolve_texture(
                cJSON_GetObjectItemCaseSensitive(pbr, "baseColorTexture"),
                textures_arr, images_arr, base_dir,
                m->texture_path, sizeof(m->texture_path));

            /* Metallic-roughness texture (G=roughness, B=metallic). */
            m->has_metallic_roughness = forge_gltf__resolve_texture(
                cJSON_GetObjectItemCaseSensitive(
                    pbr, "metallicRoughnessTexture"),
                textures_arr, images_arr, base_dir,
                m->metallic_roughness_path,
                sizeof(m->metallic_roughness_path));
        }

        /* ── Approximate KHR_materials_transmission as alpha blend ──── */
        /* Transmission is a form of transparency where light passes
         * through the surface.  We approximate it as standard alpha
         * blending since full transmission requires refraction and
         * screen-space techniques beyond this parser's scope.
         *
         * This runs AFTER base color parsing so the override of
         * base_color[3] is not clobbered by baseColorFactor. */
        {
            const cJSON *exts = cJSON_GetObjectItemCaseSensitive(
                mat, "extensions");
            if (exts && cJSON_GetObjectItemCaseSensitive(
                    exts, "KHR_materials_transmission")) {
                if (m->alpha_mode == FORGE_GLTF_ALPHA_OPAQUE) {
                    m->alpha_mode = FORGE_GLTF_ALPHA_BLEND;
                    m->base_color[3] = FORGE_GLTF_TRANSMISSION_ALPHA;
                }
            }
        }

        /* ── Normal texture + scale ──────────────────────────────────── */
        /* glTF stores normalTexture at the material level (not inside
         * pbrMetallicRoughness).  The normal map stores tangent-space
         * normals that add surface detail without extra geometry. */
        {
            const cJSON *norm_tex_info = cJSON_GetObjectItemCaseSensitive(
                mat, "normalTexture");
            m->has_normal_map = forge_gltf__resolve_texture(
                norm_tex_info, textures_arr, images_arr, base_dir,
                m->normal_map_path, sizeof(m->normal_map_path));
            if (norm_tex_info) {
                const cJSON *ns = cJSON_GetObjectItemCaseSensitive(
                    norm_tex_info, "scale");
                if (cJSON_IsNumber(ns))
                    m->normal_scale = (float)ns->valuedouble;
            }
        }

        /* ── Occlusion texture + strength ────────────────────────────── */
        /* Ambient occlusion stored in the R channel.  The strength
         * scalar (default 1.0) controls the occlusion intensity. */
        {
            const cJSON *occ_tex_info = cJSON_GetObjectItemCaseSensitive(
                mat, "occlusionTexture");
            m->has_occlusion = forge_gltf__resolve_texture(
                occ_tex_info, textures_arr, images_arr, base_dir,
                m->occlusion_path, sizeof(m->occlusion_path));
            if (occ_tex_info) {
                const cJSON *os = cJSON_GetObjectItemCaseSensitive(
                    occ_tex_info, "strength");
                if (cJSON_IsNumber(os)) {
                    m->occlusion_strength = (float)os->valuedouble;
                    if (m->occlusion_strength < 0.0f)
                        m->occlusion_strength = 0.0f;
                    if (m->occlusion_strength > 1.0f)
                        m->occlusion_strength = 1.0f;
                }
            }
        }

        /* ── Emissive texture + factor ───────────────────────────────── */
        /* emissiveFactor is an RGB multiplier (default 0,0,0).  When
         * an emissive texture is present, final emission is the texture
         * RGB multiplied by emissiveFactor. */
        {
            const cJSON *em_tex_info = cJSON_GetObjectItemCaseSensitive(
                mat, "emissiveTexture");
            m->has_emissive = forge_gltf__resolve_texture(
                em_tex_info, textures_arr, images_arr, base_dir,
                m->emissive_path, sizeof(m->emissive_path));

            const cJSON *ef = cJSON_GetObjectItemCaseSensitive(
                mat, "emissiveFactor");
            if (cJSON_IsArray(ef) && cJSON_GetArraySize(ef) == 3) {
                for (int ei = 0; ei < 3; ei++) {
                    const cJSON *elem = cJSON_GetArrayItem(ef, ei);
                    if (cJSON_IsNumber(elem)) {
                        m->emissive_factor[ei] = (float)elem->valuedouble;
                        if (m->emissive_factor[ei] < 0.0f)
                            m->emissive_factor[ei] = 0.0f;
                    }
                }
                if (m->emissive_factor[0] > 0.0f ||
                    m->emissive_factor[1] > 0.0f ||
                    m->emissive_factor[2] > 0.0f) {
                    m->has_emissive = true;
                }
            }
        }
    }
    scene->material_count = count;
    return true;
}

/* ── Parse meshes ────────────────────────────────────────────────────────── */

static bool forge_gltf__parse_meshes(const cJSON *root, ForgeGltfScene *scene,
                                      ForgeArena *arena)
{
    const cJSON *meshes = cJSON_GetObjectItemCaseSensitive(root, "meshes");
    if (!cJSON_IsArray(meshes)) {
        SDL_Log("forge_gltf: no 'meshes' array");
        return false;
    }

    int mesh_count = cJSON_GetArraySize(meshes);
    if (mesh_count < 0) {
        SDL_Log("forge_gltf: invalid mesh count");
        return false;
    }
    if (mesh_count == 0) {
        scene->mesh_count = 0;
        return true;
    }

    size_t mesh_bytes;
    if (!forge_gltf__safe_mul((size_t)mesh_count, sizeof(ForgeGltfMesh), &mesh_bytes)) {
        SDL_Log("forge_gltf: mesh allocation size overflow");
        return false;
    }
    scene->meshes = (ForgeGltfMesh *)forge_arena_alloc(arena, mesh_bytes);
    if (!scene->meshes) {
        SDL_Log("forge_gltf: arena alloc failed for %d meshes", mesh_count);
        return false;
    }

    /* Count total primitives to pre-allocate the primitives array.
     * Guard against overflow: each cJSON_GetArraySize can return up to
     * INT_MAX, and the sum of all primitive counts must fit in int. */
    int total_prims = 0;
    for (int mi = 0; mi < mesh_count; mi++) {
        const cJSON *mesh = cJSON_GetArrayItem(meshes, mi);
        const cJSON *prims = cJSON_GetObjectItemCaseSensitive(mesh, "primitives");
        if (cJSON_IsArray(prims)) {
            int prim_count = cJSON_GetArraySize(prims);
            if (prim_count < 0 || prim_count > INT_MAX - total_prims) {
                SDL_Log("forge_gltf: primitive count overflow");
                return false;
            }
            total_prims += prim_count;
        }
    }
    size_t prim_bytes;
    if (!forge_gltf__safe_mul((size_t)total_prims, sizeof(ForgeGltfPrimitive), &prim_bytes)) {
        SDL_Log("forge_gltf: primitive allocation size overflow");
        return false;
    }
    scene->primitives = (ForgeGltfPrimitive *)forge_arena_alloc(arena, prim_bytes);
    if (!scene->primitives && total_prims > 0) {
        SDL_Log("forge_gltf: arena alloc failed for %d primitives", total_prims);
        return false;
    }

    for (int mi = 0; mi < mesh_count; mi++) {
        const cJSON *mesh = cJSON_GetArrayItem(meshes, mi);
        const cJSON *prims = cJSON_GetObjectItemCaseSensitive(mesh, "primitives");
        if (!cJSON_IsArray(prims)) continue;

        ForgeGltfMesh *gm = &scene->meshes[mi];
        gm->first_primitive = scene->primitive_count;
        gm->primitive_count = 0;
        gm->default_weights = NULL;
        gm->default_weight_count = 0;
        copy_name(gm->name, sizeof(gm->name), mesh);

        /* Default morph target weights (mesh.weights[]) */
        const cJSON *weights_arr = cJSON_GetObjectItemCaseSensitive(
            mesh, "weights");
        int weight_count = cJSON_GetArraySize(weights_arr);
        if (weight_count > FORGE_GLTF_MAX_MORPH_TARGETS) {
            SDL_Log("forge_gltf: mesh %d has %d default morph weights "
                    "(max %d)", mi, weight_count,
                    FORGE_GLTF_MAX_MORPH_TARGETS);
            return false;
        }
        if (weight_count > 0) {
            size_t w_alloc;
            if (!forge_gltf__safe_mul(weight_count, sizeof(float), &w_alloc)) {
                SDL_Log("forge_gltf: weights alloc overflow");
                return false;
            }
            gm->default_weights = (float *)forge_arena_alloc(arena, w_alloc);
            if (!gm->default_weights) {
                SDL_Log("forge_gltf: arena alloc failed for default weights");
                return false;
            }
            gm->default_weight_count = weight_count;
            for (int wi = 0; wi < weight_count; wi++) {
                const cJSON *w = cJSON_GetArrayItem(weights_arr, wi);
                gm->default_weights[wi] = cJSON_IsNumber(w)
                    ? (float)w->valuedouble : 0.0f;
            }
        }

        int prim_count = cJSON_GetArraySize(prims);
        for (int pi = 0; pi < prim_count; pi++) {
            const cJSON *prim = cJSON_GetArrayItem(prims, pi);
            const cJSON *attrs = cJSON_GetObjectItemCaseSensitive(
                prim, "attributes");
            if (!attrs) continue;

            ForgeGltfPrimitive *gp =
                &scene->primitives[scene->primitive_count];
            SDL_memset(gp, 0, sizeof(*gp));

            /* Read vertex attributes. */
            const cJSON *pos_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "POSITION");
            if (!pos_acc) continue;

            int vert_count = 0;
            int comp_type = 0;
            int pos_num = 0;
            const float *positions = (const float *)forge_gltf__get_accessor(
                root, scene, pos_acc->valueint, &vert_count, &comp_type,
                &pos_num);
            /* glTF requires VEC3 for POSITION */
            if (!positions || comp_type != FORGE_GLTF_FLOAT
                  || pos_num != 3) {
                SDL_Log("forge_gltf: mesh %d primitive %d: "
                        "POSITION accessor %d failed validation",
                        mi, pi, pos_acc->valueint);
                return false;
            }

            const float *normals = NULL;
            const cJSON *norm_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "NORMAL");
            if (norm_acc) {
                int norm_count = 0;
                int norm_comp = 0;
                int norm_num = 0;
                const float *n = (const float *)forge_gltf__get_accessor(
                    root, scene, norm_acc->valueint, &norm_count, &norm_comp,
                    &norm_num);
                /* glTF requires VEC3 for NORMAL */
                if (n && norm_count == vert_count
                      && norm_comp == FORGE_GLTF_FLOAT
                      && norm_num == 3) {
                    normals = n;
                }
            }

            const float *uvs = NULL;
            const cJSON *uv_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "TEXCOORD_0");
            if (uv_acc) {
                int uv_count = 0;
                int uv_comp = 0;
                int uv_num = 0;
                const float *u = (const float *)forge_gltf__get_accessor(
                    root, scene, uv_acc->valueint, &uv_count, &uv_comp,
                    &uv_num);
                /* glTF requires VEC2 for TEXCOORD_0 */
                if (u && uv_count == vert_count
                      && uv_comp == FORGE_GLTF_FLOAT
                      && uv_num == 2) {
                    uvs = u;
                }
            }

            gp->has_uvs = (uvs != NULL);

            /* Read tangent data (VEC4: xyz = direction, w = handedness).
             * Tangent vectors are needed for normal mapping — they define
             * the local surface coordinate system together with the normal
             * and bitangent.  Stored in a separate array to avoid changing
             * the base ForgeGltfVertex layout. */
            const float *tangent_data = NULL;
            const cJSON *tangent_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "TANGENT");
            if (tangent_acc) {
                int tang_count = 0;
                int tang_comp = 0;
                int tang_num = 0;
                const float *t = (const float *)forge_gltf__get_accessor(
                    root, scene, tangent_acc->valueint,
                    &tang_count, &tang_comp, &tang_num);
                if (t && tang_count == vert_count
                      && tang_comp == FORGE_GLTF_FLOAT
                      && tang_num == FORGE_GLTF_TANGENT_COMPONENTS) {
                    tangent_data = t;
                }
            }

            /* Interleave into ForgeGltfVertex array. */
            size_t vert_bytes;
            if (!forge_gltf__safe_mul((size_t)vert_count, sizeof(ForgeGltfVertex), &vert_bytes)) {
                SDL_Log("forge_gltf: vertex allocation size overflow");
                return false;
            }
            gp->vertices = (ForgeGltfVertex *)forge_arena_alloc(arena, vert_bytes);
            if (!gp->vertices) {
                SDL_Log("forge_gltf: arena alloc failed for vertices");
                return false;
            }
            gp->vertex_count = (Uint32)vert_count;

            for (int v = 0; v < vert_count; v++) {
                gp->vertices[v].position.x = positions[v * 3 + 0];
                gp->vertices[v].position.y = positions[v * 3 + 1];
                gp->vertices[v].position.z = positions[v * 3 + 2];

                if (normals) {
                    gp->vertices[v].normal.x = normals[v * 3 + 0];
                    gp->vertices[v].normal.y = normals[v * 3 + 1];
                    gp->vertices[v].normal.z = normals[v * 3 + 2];
                }

                if (uvs) {
                    gp->vertices[v].uv.x = uvs[v * 2 + 0];
                    gp->vertices[v].uv.y = uvs[v * 2 + 1];
                }
            }

            /* Copy tangent data into a separate VEC4 array.  Stored
             * separately from ForgeGltfVertex so that lessons which don't
             * need tangents can use the same base vertex layout. */
            if (tangent_data) {
                size_t tang_bytes;
                /* TANGENT attribute was validated — allocation failure is fatal
                 * (OOM or overflow), not graceful degradation. */
                if (!forge_gltf__safe_mul((size_t)vert_count, sizeof(vec4), &tang_bytes)) {
                    SDL_Log("forge_gltf: tangent byte count overflow "
                            "(vert_count=%d)", vert_count);
                    return false;
                }
                gp->tangents = (vec4 *)forge_arena_alloc(arena, tang_bytes);
                if (!gp->tangents) {
                    SDL_Log("forge_gltf: failed to allocate tangent array "
                            "(%zu bytes)", tang_bytes);
                    return false;
                }
                gp->has_tangents = true;
                {
                    int tv;
                    for (tv = 0; tv < vert_count; tv++) {
                        gp->tangents[tv].x = tangent_data[tv * FORGE_GLTF_TANGENT_COMPONENTS + 0];
                        gp->tangents[tv].y = tangent_data[tv * FORGE_GLTF_TANGENT_COMPONENTS + 1];
                        gp->tangents[tv].z = tangent_data[tv * FORGE_GLTF_TANGENT_COMPONENTS + 2];
                        gp->tangents[tv].w = tangent_data[tv * FORGE_GLTF_TANGENT_COMPONENTS + 3];
                    }
                }
            }

            /* Read JOINTS_0 + WEIGHTS_0 for vertex skinning.
             * JOINTS_0 is VEC4 of UNSIGNED_SHORT (4 joint indices per vertex).
             * WEIGHTS_0 is VEC4 of FLOAT (4 blend weights per vertex).
             * Both must be present for skin data to be valid. */
            {
                const cJSON *joints_acc = cJSON_GetObjectItemCaseSensitive(
                    attrs, "JOINTS_0");
                const cJSON *weights_acc = cJSON_GetObjectItemCaseSensitive(
                    attrs, "WEIGHTS_0");

                if (joints_acc && weights_acc) {
                    int j_count = 0, j_comp = 0, j_num = 0;
                    const void *j_data = forge_gltf__get_accessor(
                        root, scene, joints_acc->valueint,
                        &j_count, &j_comp, &j_num);

                    int w_count = 0, w_comp = 0, w_num = 0;
                    const float *w_data = (const float *)forge_gltf__get_accessor(
                        root, scene, weights_acc->valueint,
                        &w_count, &w_comp, &w_num);

                    /* glTF 2.0 allows JOINTS_0 as UNSIGNED_BYTE or
                     * UNSIGNED_SHORT, and WEIGHTS_0 as FLOAT (or
                     * UNSIGNED_BYTE/SHORT normalized, not yet supported). */
                    bool j_valid = (j_comp == FORGE_GLTF_UNSIGNED_SHORT
                                 || j_comp == FORGE_GLTF_UNSIGNED_BYTE);
                    bool w_valid = (w_comp == FORGE_GLTF_FLOAT);

                    if (j_data && w_data
                        && j_count == vert_count && w_count == vert_count
                        && j_valid
                        && j_num == FORGE_GLTF_JOINTS_PER_VERT
                        && w_valid
                        && w_num == FORGE_GLTF_JOINTS_PER_VERT) {

                        /* Allocate joint indices (always stored as Uint16). */
                        size_t ji_elems, ji_bytes;
                        if (!forge_gltf__safe_mul((size_t)vert_count, FORGE_GLTF_JOINTS_PER_VERT, &ji_elems)
                            || !forge_gltf__safe_mul(ji_elems, sizeof(Uint16), &ji_bytes)) {
                            SDL_Log("forge_gltf: mesh %d primitive %d: "
                                    "joint index size overflow", mi, pi);
                            return false;
                        }
                        gp->joint_indices = (Uint16 *)forge_arena_alloc(
                            arena, ji_bytes);

                        /* Allocate weights (FLOAT × 4). */
                        size_t wt_elems, wt_bytes;
                        if (!forge_gltf__safe_mul((size_t)vert_count, FORGE_GLTF_JOINTS_PER_VERT, &wt_elems)
                            || !forge_gltf__safe_mul(wt_elems, sizeof(float), &wt_bytes)) {
                            SDL_Log("forge_gltf: mesh %d primitive %d: "
                                    "weight size overflow", mi, pi);
                            return false;
                        }
                        gp->weights = (float *)forge_arena_alloc(
                            arena, wt_bytes);

                        if (gp->joint_indices && gp->weights) {
                            if (j_comp == FORGE_GLTF_UNSIGNED_SHORT) {
                                SDL_memcpy(gp->joint_indices, j_data, ji_bytes);
                            } else {
                                /* Widen UNSIGNED_BYTE → Uint16. */
                                const Uint8 *src = (const Uint8 *)j_data;
                                size_t total = (size_t)vert_count * FORGE_GLTF_JOINTS_PER_VERT;
                                for (size_t k = 0; k < total; k++) {
                                    gp->joint_indices[k] = (Uint16)src[k];
                                }
                            }
                            SDL_memcpy(gp->weights, w_data, wt_bytes);
                            gp->has_skin_data = true;
                        } else {
                            /* Validated skin data that fails allocation is a
                             * hard error — silently dropping it would produce
                             * an unskinned mesh with corrupted animation. */
                            SDL_Log("forge_gltf: mesh %d primitive %d: "
                                    "skin data allocation failed",
                                    mi, pi);
                            return false;
                        }
                    } else if (j_data && w_data && !j_valid) {
                        SDL_Log("forge_gltf: unsupported JOINTS_0 type %d",
                                j_comp);
                    } else if (j_data && w_data && !w_valid) {
                        SDL_Log("forge_gltf: unsupported WEIGHTS_0 type %d",
                                w_comp);
                    }
                }
            }

            /* ── Morph targets (blend shapes) ─────────────────────────── */
            const cJSON *targets_arr = cJSON_GetObjectItemCaseSensitive(
                prim, "targets");
            int target_count = cJSON_GetArraySize(targets_arr);
            if (target_count > FORGE_GLTF_MAX_MORPH_TARGETS) {
                SDL_Log("forge_gltf: primitive %d has %d morph targets "
                        "(max %d)", pi, target_count,
                        FORGE_GLTF_MAX_MORPH_TARGETS);
                return false;
            }
            if (target_count > 0) {
                if (gm->default_weight_count > 0 &&
                    gm->default_weight_count != target_count) {
                    SDL_Log("forge_gltf: mesh %d default_weight_count %d "
                            "does not match primitive %d "
                            "morph_target_count %d",
                            mi, gm->default_weight_count, pi,
                            target_count);
                    return false;
                }
                size_t mt_alloc;
                if (!forge_gltf__safe_mul(target_count,
                        sizeof(ForgeGltfMorphTarget), &mt_alloc)) {
                    SDL_Log("forge_gltf: morph target alloc overflow");
                    return false;
                }
                gp->morph_targets = (ForgeGltfMorphTarget *)
                    forge_arena_alloc(arena, mt_alloc);
                if (!gp->morph_targets) {
                    SDL_Log("forge_gltf: arena alloc failed for morph targets");
                    return false;
                }
                SDL_memset(gp->morph_targets, 0, mt_alloc);
                gp->morph_target_count = target_count;

                for (int ti = 0; ti < target_count; ti++) {
                    const cJSON *tgt = cJSON_GetArrayItem(targets_arr, ti);
                    ForgeGltfMorphTarget *mt = &gp->morph_targets[ti];

                    /* POSITION deltas (required for every morph target) */
                    const cJSON *mt_pos_acc = cJSON_GetObjectItemCaseSensitive(
                        tgt, "POSITION");
                    if (!cJSON_IsNumber(mt_pos_acc)) {
                        SDL_Log("forge_gltf: morph target %d missing "
                                "POSITION accessor", ti);
                        return false;
                    }
                    {
                        int mt_count = 0, mt_comp = 0, mt_num = 0;
                        const float *mt_data = (const float *)
                            forge_gltf__get_accessor(root, scene,
                                mt_pos_acc->valueint,
                                &mt_count, &mt_comp, &mt_num);
                        if (!mt_data
                            || mt_comp != FORGE_GLTF_FLOAT
                            || mt_num != FORGE_GLTF_MORPH_DELTA_COMPONENTS
                            || mt_count != (int)gp->vertex_count) {
                            SDL_Log("forge_gltf: morph target %d has "
                                    "invalid POSITION deltas (count=%d, "
                                    "comp_type=%d, num_components=%d, "
                                    "vertex_count=%u)",
                                    ti, mt_count, mt_comp, mt_num,
                                    gp->vertex_count);
                            return false;
                        }
                        size_t floats_needed, d_bytes;
                        if (!forge_gltf__safe_mul((size_t)mt_count,
                                FORGE_GLTF_MORPH_DELTA_COMPONENTS,
                                &floats_needed) ||
                            !forge_gltf__safe_mul(floats_needed,
                                sizeof(float), &d_bytes)) {
                            SDL_Log("forge_gltf: morph target %d "
                                    "position delta size overflow", ti);
                            return false;
                        }
                        mt->position_deltas = (float *)
                            forge_arena_alloc(arena, d_bytes);
                        if (!mt->position_deltas) {
                            SDL_Log("forge_gltf: failed to allocate "
                                    "morph target %d position deltas",
                                    ti);
                            return false;
                        }
                        SDL_memcpy(mt->position_deltas, mt_data, d_bytes);
                    }

                    /* NORMAL deltas (optional — fail if present but invalid) */
                    const cJSON *mt_norm_acc = cJSON_GetObjectItemCaseSensitive(
                        tgt, "NORMAL");
                    if (mt_norm_acc && !cJSON_IsNumber(mt_norm_acc)) {
                        SDL_Log("forge_gltf: morph target %d has non-numeric "
                                "NORMAL accessor", ti);
                        return false;
                    }
                    if (cJSON_IsNumber(mt_norm_acc)) {
                        int mt_count = 0, mt_comp = 0, mt_num = 0;
                        const float *mt_data = (const float *)
                            forge_gltf__get_accessor(root, scene,
                                mt_norm_acc->valueint,
                                &mt_count, &mt_comp, &mt_num);
                        if (!mt_data
                            || mt_comp != FORGE_GLTF_FLOAT
                            || mt_num != FORGE_GLTF_MORPH_DELTA_COMPONENTS
                            || mt_count != (int)gp->vertex_count) {
                            SDL_Log("forge_gltf: morph target %d has "
                                    "invalid NORMAL deltas (count=%d, "
                                    "comp_type=%d, num_components=%d, "
                                    "vertex_count=%u)",
                                    ti, mt_count, mt_comp, mt_num,
                                    gp->vertex_count);
                            return false;
                        }
                        size_t floats_needed, d_bytes;
                        if (!forge_gltf__safe_mul((size_t)mt_count,
                                FORGE_GLTF_MORPH_DELTA_COMPONENTS,
                                &floats_needed) ||
                            !forge_gltf__safe_mul(floats_needed,
                                sizeof(float), &d_bytes)) {
                            SDL_Log("forge_gltf: morph target %d "
                                    "normal delta size overflow", ti);
                            return false;
                        }
                        mt->normal_deltas = (float *)
                            forge_arena_alloc(arena, d_bytes);
                        if (!mt->normal_deltas) {
                            SDL_Log("forge_gltf: failed to allocate "
                                    "morph target %d normal deltas",
                                    ti);
                            return false;
                        }
                        SDL_memcpy(mt->normal_deltas, mt_data, d_bytes);
                    }

                    /* TANGENT deltas (optional, VEC3 — no handedness delta) */
                    const cJSON *mt_tan_acc = cJSON_GetObjectItemCaseSensitive(
                        tgt, "TANGENT");
                    if (mt_tan_acc && !cJSON_IsNumber(mt_tan_acc)) {
                        SDL_Log("forge_gltf: morph target %d has non-numeric "
                                "TANGENT accessor", ti);
                        return false;
                    }
                    if (cJSON_IsNumber(mt_tan_acc)) {
                        int mt_count = 0, mt_comp = 0, mt_num = 0;
                        const float *mt_data = (const float *)
                            forge_gltf__get_accessor(root, scene,
                                mt_tan_acc->valueint,
                                &mt_count, &mt_comp, &mt_num);
                        if (!mt_data
                            || mt_comp != FORGE_GLTF_FLOAT
                            || mt_num != FORGE_GLTF_MORPH_DELTA_COMPONENTS
                            || mt_count != (int)gp->vertex_count) {
                            SDL_Log("forge_gltf: morph target %d has "
                                    "invalid TANGENT deltas (count=%d, "
                                    "comp_type=%d, num_components=%d, "
                                    "vertex_count=%u)",
                                    ti, mt_count, mt_comp, mt_num,
                                    gp->vertex_count);
                            return false;
                        }
                        size_t floats_needed, d_bytes;
                        if (!forge_gltf__safe_mul((size_t)mt_count,
                                FORGE_GLTF_MORPH_DELTA_COMPONENTS,
                                &floats_needed) ||
                            !forge_gltf__safe_mul(floats_needed,
                                sizeof(float), &d_bytes)) {
                            SDL_Log("forge_gltf: morph target %d "
                                    "tangent delta size overflow", ti);
                            return false;
                        }
                        mt->tangent_deltas = (float *)
                            forge_arena_alloc(arena, d_bytes);
                        if (!mt->tangent_deltas) {
                            SDL_Log("forge_gltf: failed to allocate "
                                    "morph target %d tangent deltas",
                                    ti);
                            return false;
                        }
                        SDL_memcpy(mt->tangent_deltas, mt_data, d_bytes);
                    }
                }
            }

            /* Read index data. */
            const cJSON *idx_acc = cJSON_GetObjectItemCaseSensitive(
                prim, "indices");
            if (idx_acc && cJSON_IsNumber(idx_acc)) {
                int idx_count = 0;
                int idx_comp = 0;
                int idx_num = 0;
                const void *idx_data = forge_gltf__get_accessor(
                    root, scene, idx_acc->valueint, &idx_count, &idx_comp,
                    &idx_num);

                /* glTF requires SCALAR for indices */
                if (!idx_data || idx_num != 1) {
                    SDL_Log("forge_gltf: mesh %d primitive %d: "
                            "index accessor %d failed validation",
                            mi, pi, idx_acc->valueint);
                    return false;
                }
                if (idx_count > 0) {
                    Uint32 elem_size = 0;
                    bool widen_bytes = false;
                    if (idx_comp == FORGE_GLTF_UNSIGNED_BYTE) {
                        /* glTF allows UNSIGNED_BYTE indices — widen to
                         * Uint16 so downstream code only handles 2/4. */
                        elem_size = 2;
                        widen_bytes = true;
                    } else if (idx_comp == FORGE_GLTF_UNSIGNED_SHORT) {
                        elem_size = 2;
                    } else if (idx_comp == FORGE_GLTF_UNSIGNED_INT) {
                        elem_size = 4;
                    } else {
                        SDL_Log("forge_gltf: unsupported index component "
                                "type %d for mesh %d primitive %d",
                                idx_comp, mi, pi);
                        return false;
                    }

                    /* Guard against overflow: idx_count * elem_size
                     * must fit in Uint32.  elem_size is 2 or 4. */
                    if ((Uint32)idx_count > UINT32_MAX / elem_size) {
                        SDL_Log("forge_gltf: index buffer size overflow");
                        return false;
                    }
                    Uint32 total = (Uint32)idx_count * elem_size;
                    gp->indices = forge_arena_alloc(arena, total);
                    if (!gp->indices) {
                        SDL_Log("forge_gltf: arena alloc failed for index buffer");
                        return false;
                    }
                    if (widen_bytes) {
                        /* Widen UNSIGNED_BYTE → Uint16 (same pattern
                         * as joint index widening above). */
                        const Uint8 *src = (const Uint8 *)idx_data;
                        Uint16 *dst = (Uint16 *)gp->indices;
                        for (Uint32 k = 0; k < (Uint32)idx_count; k++) {
                            dst[k] = (Uint16)src[k];
                            if (dst[k] >= gp->vertex_count) {
                                SDL_Log("forge_gltf: mesh %d primitive %d: "
                                        "index[%u]=%u >= vertex_count %u",
                                        mi, pi, k,
                                        (unsigned)dst[k], gp->vertex_count);
                                return false;
                            }
                        }
                    } else {
                        SDL_memcpy(gp->indices, idx_data, total);
                        /* Validate all indices are within vertex range. */
                        if (idx_comp == FORGE_GLTF_UNSIGNED_SHORT) {
                            const Uint16 *idx16 =
                                (const Uint16 *)gp->indices;
                            for (Uint32 k = 0; k < (Uint32)idx_count; k++) {
                                if (idx16[k] >= gp->vertex_count) {
                                    SDL_Log("forge_gltf: mesh %d primitive "
                                            "%d: index[%u]=%u >= "
                                            "vertex_count %u",
                                            mi, pi, k,
                                            (unsigned)idx16[k],
                                            gp->vertex_count);
                                    return false;
                                }
                            }
                        } else { /* FORGE_GLTF_UNSIGNED_INT */
                            const Uint32 *idx32 =
                                (const Uint32 *)gp->indices;
                            for (Uint32 k = 0; k < (Uint32)idx_count; k++) {
                                if (idx32[k] >= gp->vertex_count) {
                                    SDL_Log("forge_gltf: mesh %d primitive "
                                            "%d: index[%u]=%u >= "
                                            "vertex_count %u",
                                            mi, pi, k,
                                            (unsigned)idx32[k],
                                            gp->vertex_count);
                                    return false;
                                }
                            }
                        }
                    }
                    gp->index_count = (Uint32)idx_count;
                    gp->index_stride = elem_size;
                }
            }

            /* Material reference. */
            const cJSON *mat_idx = cJSON_GetObjectItemCaseSensitive(
                prim, "material");
            if (cJSON_IsNumber(mat_idx) && mat_idx->valueint >= 0
                && mat_idx->valueint < scene->material_count) {
                gp->material_index = mat_idx->valueint;
            } else {
                gp->material_index = -1; /* no material or out of range */
            }

            scene->primitive_count++;
            gm->primitive_count++;
        }
    }
    scene->mesh_count = mesh_count;
    return true;
}

/* ── Parse nodes ─────────────────────────────────────────────────────────── */

static bool forge_gltf__parse_nodes(const cJSON *root, ForgeGltfScene *scene,
                                     ForgeArena *arena)
{
    const cJSON *nodes = cJSON_GetObjectItemCaseSensitive(root, "nodes");
    if (!cJSON_IsArray(nodes)) {
        SDL_Log("forge_gltf: no 'nodes' array");
        return false;
    }

    int count = cJSON_GetArraySize(nodes);
    if (count < 0) {
        SDL_Log("forge_gltf: invalid node count");
        return false;
    }
    if (count == 0) {
        scene->node_count = 0;
        return true;
    }

    size_t node_bytes;
    if (!forge_gltf__safe_mul((size_t)count, sizeof(ForgeGltfNode), &node_bytes)) {
        SDL_Log("forge_gltf: node allocation size overflow");
        return false;
    }
    scene->nodes = (ForgeGltfNode *)forge_arena_alloc(arena, node_bytes);
    if (!scene->nodes) {
        SDL_Log("forge_gltf: arena alloc failed for %d nodes", count);
        return false;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *node = cJSON_GetArrayItem(nodes, i);
        ForgeGltfNode *gn = &scene->nodes[i];

        gn->mesh_index = -1;
        gn->parent = -1;
        gn->child_count = 0;
        gn->local_transform = mat4_identity();
        gn->world_transform = mat4_identity();
        gn->translation = vec3_create(0.0f, 0.0f, 0.0f);
        gn->rotation    = quat_identity();
        gn->scale_xyz   = vec3_create(1.0f, 1.0f, 1.0f);
        gn->has_trs     = false;
        gn->skin_index  = -1;
        copy_name(gn->name, sizeof(gn->name), node);

        /* Mesh reference. */
        const cJSON *mesh_idx = cJSON_GetObjectItemCaseSensitive(node, "mesh");
        if (cJSON_IsNumber(mesh_idx) && mesh_idx->valueint >= 0
            && mesh_idx->valueint < scene->mesh_count) {
            gn->mesh_index = mesh_idx->valueint;
        }

        /* Skin reference — store raw index, validate after skins are parsed. */
        const cJSON *skin_idx = cJSON_GetObjectItemCaseSensitive(node, "skin");
        if (cJSON_IsNumber(skin_idx)) {
            gn->skin_index = skin_idx->valueint;
        }

        /* Children — arena-allocated to the actual count (no fixed limit). */
        const cJSON *children = cJSON_GetObjectItemCaseSensitive(
            node, "children");
        if (cJSON_IsArray(children)) {
            int cc = cJSON_GetArraySize(children);
            if (cc < 0) cc = 0;
            if (cc > 0) {
                size_t child_bytes;
                if (!forge_gltf__safe_mul((size_t)cc, sizeof(int), &child_bytes)) {
                    SDL_Log("forge_gltf: node %d children allocation size overflow", i);
                    return false;
                }
                gn->children = (int *)forge_arena_alloc(arena, child_bytes);
                if (!gn->children) {
                    SDL_Log("forge_gltf: arena alloc failed for node %d children", i);
                    return false;
                }
            }
            int valid = 0;
            for (int c = 0; c < cc; c++) {
                const cJSON *item = cJSON_GetArrayItem(children, c);
                if (cJSON_IsNumber(item)
                    && item->valueint >= 0
                    && item->valueint < count) {
                    gn->children[valid++] = item->valueint;
                }
            }
            gn->child_count = valid;
        }

        /* Compute local transform from TRS or matrix. */
        const cJSON *matrix = cJSON_GetObjectItemCaseSensitive(node, "matrix");
        if (cJSON_IsArray(matrix) && cJSON_GetArraySize(matrix) == 16) {
            for (int j = 0; j < 16; j++) {
                const cJSON *elem = cJSON_GetArrayItem(matrix, j);
                gn->local_transform.m[j] = elem ? (float)elem->valuedouble
                                                 : 0.0f;
            }
            /* has_trs stays false — raw matrix node */
        } else {
            /* TRS decomposition: local = T * R * S.
             * Also store individual T/R/S for animation support. */
            gn->has_trs = true;

            const cJSON *trans = cJSON_GetObjectItemCaseSensitive(
                node, "translation");
            if (cJSON_IsArray(trans) && cJSON_GetArraySize(trans) == 3) {
                const cJSON *t0 = cJSON_GetArrayItem(trans, 0);
                const cJSON *t1 = cJSON_GetArrayItem(trans, 1);
                const cJSON *t2 = cJSON_GetArrayItem(trans, 2);
                if (cJSON_IsNumber(t0) && cJSON_IsNumber(t1)
                    && cJSON_IsNumber(t2)) {
                    gn->translation = vec3_create(
                        (float)t0->valuedouble,
                        (float)t1->valuedouble,
                        (float)t2->valuedouble);
                }
            }

            const cJSON *rot = cJSON_GetObjectItemCaseSensitive(
                node, "rotation");
            if (cJSON_IsArray(rot) && cJSON_GetArraySize(rot) == 4) {
                /* glTF: [x, y, z, w] → our quat_create: (w, x, y, z) */
                const cJSON *rx = cJSON_GetArrayItem(rot, 0);
                const cJSON *ry = cJSON_GetArrayItem(rot, 1);
                const cJSON *rz = cJSON_GetArrayItem(rot, 2);
                const cJSON *rw = cJSON_GetArrayItem(rot, 3);
                if (cJSON_IsNumber(rx) && cJSON_IsNumber(ry)
                    && cJSON_IsNumber(rz) && cJSON_IsNumber(rw)) {
                    gn->rotation = quat_normalize(quat_create(
                        (float)rw->valuedouble,
                        (float)rx->valuedouble,
                        (float)ry->valuedouble,
                        (float)rz->valuedouble));
                }
            }

            const cJSON *scl = cJSON_GetObjectItemCaseSensitive(
                node, "scale");
            if (cJSON_IsArray(scl) && cJSON_GetArraySize(scl) == 3) {
                const cJSON *s0 = cJSON_GetArrayItem(scl, 0);
                const cJSON *s1 = cJSON_GetArrayItem(scl, 1);
                const cJSON *s2 = cJSON_GetArrayItem(scl, 2);
                if (cJSON_IsNumber(s0) && cJSON_IsNumber(s1)
                    && cJSON_IsNumber(s2)) {
                    gn->scale_xyz = vec3_create(
                        (float)s0->valuedouble,
                        (float)s1->valuedouble,
                        (float)s2->valuedouble);
                }
            }

            mat4 t_mat = mat4_translate(gn->translation);
            mat4 r_mat = quat_to_mat4(gn->rotation);
            mat4 s_mat = mat4_scale(gn->scale_xyz);
            gn->local_transform =
                mat4_multiply(t_mat, mat4_multiply(r_mat, s_mat));
        }
    }
    scene->node_count = count;

    /* Set parent references from child lists. */
    for (int i = 0; i < count; i++) {
        for (int c = 0; c < scene->nodes[i].child_count; c++) {
            int ci = scene->nodes[i].children[c];
            if (ci >= 0 && ci < count) {
                scene->nodes[ci].parent = i;
            }
        }
    }

    /* Identify root nodes from the default scene. */
    const cJSON *scenes_arr = cJSON_GetObjectItemCaseSensitive(root, "scenes");
    const cJSON *scene_idx = cJSON_GetObjectItemCaseSensitive(root, "scene");
    int default_scene = cJSON_IsNumber(scene_idx) ? scene_idx->valueint : 0;

    scene->root_node_count = 0;
    if (cJSON_IsArray(scenes_arr)) {
        const cJSON *sc = cJSON_GetArrayItem(scenes_arr, default_scene);
        if (!sc) {
            SDL_Log("forge_gltf: default scene index %d out of range",
                    default_scene);
            return false;
        }
        const cJSON *roots = cJSON_GetObjectItemCaseSensitive(sc, "nodes");
        if (cJSON_IsArray(roots)) {
            int rc = cJSON_GetArraySize(roots);
            if (rc < 0) rc = 0;
            if (rc > 0) {
                size_t root_bytes;
                if (!forge_gltf__safe_mul((size_t)rc, sizeof(int), &root_bytes)) {
                    SDL_Log("forge_gltf: root nodes allocation size overflow");
                    return false;
                }
                scene->root_nodes = (int *)forge_arena_alloc(arena, root_bytes);
                if (!scene->root_nodes) {
                    SDL_Log("forge_gltf: arena alloc failed for root nodes");
                    return false;
                }
            }
            int valid_roots = 0;
            for (int i = 0; i < rc; i++) {
                const cJSON *item = cJSON_GetArrayItem(roots, i);
                if (cJSON_IsNumber(item)
                    && item->valueint >= 0
                    && item->valueint < count) {
                    scene->root_nodes[valid_roots++] = item->valueint;
                } else if (item) {
                    SDL_Log("forge_gltf: default scene root index %d is invalid",
                            cJSON_IsNumber(item) ? item->valueint : -1);
                    return false;
                }
            }
            scene->root_node_count = valid_roots;
        }
    }

    return true;
}

/* ── Compute world transforms ────────────────────────────────────────────── */

/* Maximum hierarchy depth to prevent stack overflow from circular references.
 * glTF scenes rarely exceed 64 levels; 256 is generous. */
#define FORGE_GLTF_MAX_DEPTH 256

static bool forge_gltf__compute_world_transforms_impl(ForgeGltfScene *scene,
                                                       int node_idx,
                                                       const mat4 *parent_world,
                                                       int depth)
{
    if (node_idx < 0 || node_idx >= scene->node_count) return true;
    if (depth >= FORGE_GLTF_MAX_DEPTH) {
        SDL_Log("forge_gltf: hierarchy depth limit (%d) reached at node %d "
                "(possible cycle)", FORGE_GLTF_MAX_DEPTH, node_idx);
        return false;
    }

    ForgeGltfNode *node = &scene->nodes[node_idx];
    node->world_transform = mat4_multiply(*parent_world, node->local_transform);

    for (int i = 0; i < node->child_count; i++) {
        if (!forge_gltf__compute_world_transforms_impl(
                scene, node->children[i], &node->world_transform, depth + 1)) {
            return false;
        }
    }
    return true;
}

static bool forge_gltf_compute_world_transforms(ForgeGltfScene *scene,
                                                 int node_idx,
                                                 const mat4 *parent_world)
{
    if (!scene) {
        SDL_Log("forge_gltf: compute_world_transforms: scene is NULL");
        return false;
    }
    if (!parent_world) {
        SDL_Log("forge_gltf: compute_world_transforms: parent_world is NULL");
        return false;
    }
    if (node_idx < 0 || node_idx >= scene->node_count) {
        SDL_Log("forge_gltf: compute_world_transforms: node_idx %d out of "
                "range (0..%d)", node_idx, scene->node_count - 1);
        return false;
    }
    return forge_gltf__compute_world_transforms_impl(scene, node_idx,
                                                     parent_world, 0);
}

/* ── Parse skins ─────────────────────────────────────────────────────────── */

static bool forge_gltf__parse_skins(const cJSON *root, ForgeGltfScene *scene,
                                     ForgeArena *arena)
{
    const cJSON *skins = cJSON_GetObjectItemCaseSensitive(root, "skins");
    if (!cJSON_IsArray(skins)) {
        scene->skin_count = 0;
        return true; /* skins are optional */
    }

    int count = cJSON_GetArraySize(skins);
    if (count < 0) {
        SDL_Log("forge_gltf: invalid skin count");
        return false;
    }
    if (count == 0) {
        scene->skin_count = 0;
        return true;
    }

    size_t skin_bytes;
    if (!forge_gltf__safe_mul((size_t)count, sizeof(ForgeGltfSkin), &skin_bytes)) {
        SDL_Log("forge_gltf: skin allocation size overflow");
        return false;
    }
    scene->skins = (ForgeGltfSkin *)forge_arena_alloc(arena, skin_bytes);
    if (!scene->skins) {
        SDL_Log("forge_gltf: arena alloc failed for %d skins", count);
        return false;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *skin_obj = cJSON_GetArrayItem(skins, i);
        ForgeGltfSkin *skin = &scene->skins[i];

        SDL_memset(skin, 0, sizeof(*skin));
        skin->skeleton = -1;
        copy_name(skin->name, sizeof(skin->name), skin_obj);

        /* Parse skeleton root node reference. */
        const cJSON *skel = cJSON_GetObjectItemCaseSensitive(
            skin_obj, "skeleton");
        if (cJSON_IsNumber(skel)) {
            int sk = skel->valueint;
            if (sk >= 0 && sk < scene->node_count) {
                skin->skeleton = sk;
            } else {
                SDL_Log("forge_gltf: skin %d skeleton index %d out of range",
                        i, sk);
            }
        }

        /* Parse joint node indices. */
        const cJSON *joints_arr = cJSON_GetObjectItemCaseSensitive(
            skin_obj, "joints");
        if (cJSON_IsArray(joints_arr)) {
            int jc = cJSON_GetArraySize(joints_arr);
            if (jc < 0) jc = 0;
            if (jc > 0) {
                size_t j_bytes, ibm_bytes;
                if (!forge_gltf__safe_mul((size_t)jc, sizeof(int), &j_bytes)
                    || !forge_gltf__safe_mul((size_t)jc, sizeof(mat4), &ibm_bytes)) {
                    SDL_Log("forge_gltf: skin %d joint allocation overflow", i);
                    return false;
                }
                skin->joints = (int *)forge_arena_alloc(arena, j_bytes);
                skin->inverse_bind_matrices = (mat4 *)forge_arena_alloc(arena, ibm_bytes);
            }
            if ((!skin->joints || !skin->inverse_bind_matrices) && jc > 0) {
                SDL_Log("forge_gltf: arena alloc failed for skin %d joints", i);
                return false;
            }
            for (int j = 0; j < jc; j++) {
                const cJSON *item = cJSON_GetArrayItem(joints_arr, j);
                int ji = cJSON_IsNumber(item) ? item->valueint : -1;
                if (ji < 0 || ji >= scene->node_count) {
                    SDL_Log("forge_gltf: skin %d joint %d index %d out of range",
                            i, j, ji);
                    ji = -1;
                }
                skin->joints[j] = ji;
            }
            skin->joint_count = jc;
        }

        /* Parse inverse bind matrices from the accessor.
         * These are MAT4 float data stored in the binary buffer.
         * We copy them into the skin struct so they survive JSON cleanup. */
        const cJSON *ibm_acc = cJSON_GetObjectItemCaseSensitive(
            skin_obj, "inverseBindMatrices");
        if (cJSON_IsNumber(ibm_acc)) {
            int ibm_count = 0;
            int ibm_comp = 0;
            int ibm_num = 0;
            const float *ibm_data = (const float *)forge_gltf__get_accessor(
                root, scene, ibm_acc->valueint,
                &ibm_count, &ibm_comp, &ibm_num);

            if (ibm_data && ibm_comp == FORGE_GLTF_FLOAT && ibm_num == 16) {
                int copy_count = ibm_count < skin->joint_count
                               ? ibm_count : skin->joint_count;
                for (int j = 0; j < copy_count; j++) {
                    /* glTF stores matrices in column-major order, which
                     * matches our mat4.m[16] layout directly. */
                    SDL_memcpy(skin->inverse_bind_matrices[j].m,
                               ibm_data + j * 16,
                               16 * sizeof(float));
                }
                /* Fill remaining joints with identity if accessor is short. */
                for (int j = copy_count; j < skin->joint_count; j++) {
                    skin->inverse_bind_matrices[j] = mat4_identity();
                }
            } else {
                SDL_Log("forge_gltf: skin %d has invalid IBM accessor, "
                        "using identity matrices", i);
                for (int j = 0; j < skin->joint_count; j++) {
                    skin->inverse_bind_matrices[j] = mat4_identity();
                }
            }
        } else {
            /* Per glTF spec, if inverseBindMatrices is absent, use identity. */
            for (int j = 0; j < skin->joint_count; j++) {
                skin->inverse_bind_matrices[j] = mat4_identity();
            }
        }

        SDL_Log("forge_gltf: skin %d '%s': %d joints, skeleton=%d",
                i, skin->name, skin->joint_count, skin->skeleton);
    }
    scene->skin_count = count;
    return true;
}

/* ── Parse animations ────────────────────────────────────────────────────── */

static bool forge_gltf__parse_animations(const cJSON *root,
                                          ForgeGltfScene *scene,
                                          ForgeArena *arena)
{
    const cJSON *anims = cJSON_GetObjectItemCaseSensitive(root, "animations");
    if (!cJSON_IsArray(anims)) {
        scene->animation_count = 0;
        return true; /* animations are optional */
    }

    int anim_count = cJSON_GetArraySize(anims);
    if (anim_count < 0) {
        SDL_Log("forge_gltf: invalid animation count");
        return false;
    }
    if (anim_count == 0) {
        scene->animation_count = 0;
        return true;
    }

    size_t anim_bytes;
    if (!forge_gltf__safe_mul((size_t)anim_count, sizeof(ForgeGltfAnimation), &anim_bytes)) {
        SDL_Log("forge_gltf: animation allocation size overflow");
        return false;
    }
    scene->animations = (ForgeGltfAnimation *)forge_arena_alloc(arena, anim_bytes);
    if (!scene->animations) {
        SDL_Log("forge_gltf: arena alloc failed for %d animations", anim_count);
        return false;
    }

    const cJSON *accessors = cJSON_GetObjectItemCaseSensitive(root, "accessors");
    const cJSON *views = cJSON_GetObjectItemCaseSensitive(root, "bufferViews");
    if (!cJSON_IsArray(accessors) || !cJSON_IsArray(views)) {
        SDL_Log("forge_gltf: animations present but no accessors/bufferViews");
        scene->animation_count = 0;
        return true; /* non-fatal — skip animations */
    }

    int stored_anims = 0;
    for (int ai = 0; ai < anim_count; ai++) {
        const cJSON *anim_obj = cJSON_GetArrayItem(anims, ai);
        ForgeGltfAnimation *anim = &scene->animations[stored_anims];
        SDL_memset(anim, 0, sizeof(*anim));

        copy_name(anim->name, sizeof(anim->name), anim_obj);
        if (anim->name[0] == '\0') {
            SDL_snprintf(anim->name, sizeof(anim->name), "Animation_%d", ai);
        }

        /* ── Parse samplers ────────────────────────────────────────── */
        const cJSON *samp_arr = cJSON_GetObjectItemCaseSensitive(
            anim_obj, "samplers");
        if (!cJSON_IsArray(samp_arr)) continue;

        int samp_count = cJSON_GetArraySize(samp_arr);
        if (samp_count < 0) samp_count = 0; /* defensive: not documented by cJSON */

        /* Allocate samplers array — may be larger than needed due to
         * skipped CUBICSPLINE samplers; sampler_count records actual. */
        if (samp_count > 0) {
            size_t samp_bytes;
            if (!forge_gltf__safe_mul((size_t)samp_count, sizeof(ForgeGltfAnimSampler), &samp_bytes)) {
                SDL_Log("forge_gltf: sampler allocation size overflow");
                return false;
            }
            anim->samplers = (ForgeGltfAnimSampler *)forge_arena_alloc(arena, samp_bytes);
            if (!anim->samplers) {
                SDL_Log("forge_gltf: arena alloc failed for animation samplers");
                return false;
            }
        }

        float max_time = 0.0f;
        int si_out = 0; /* write index for compacted samplers */

        /* Map JSON sampler index to compacted index (-1 if skipped).
         * Channels reference samplers by JSON index, so we need this
         * to remap channel.sampler_index after compaction. */
        int *sampler_remap = NULL;
        if (samp_count > 0) {
            size_t remap_bytes;
            if (!forge_gltf__safe_mul((size_t)samp_count, sizeof(int), &remap_bytes)) {
                SDL_Log("forge_gltf: sampler remap allocation size overflow");
                return false;
            }
            sampler_remap = (int *)forge_arena_alloc(arena, remap_bytes);
            if (!sampler_remap) {
                SDL_Log("forge_gltf: arena alloc failed for sampler remap");
                return false;
            }
        }
        for (int ri = 0; ri < samp_count; ri++) sampler_remap[ri] = -1;

        for (int si = 0; si < samp_count; si++) {
            const cJSON *samp_obj = cJSON_GetArrayItem(samp_arr, si);
            ForgeGltfAnimSampler temp_samp;
            SDL_memset(&temp_samp, 0, sizeof(temp_samp));

            /* Interpolation mode. */
            const cJSON *interp_json = cJSON_GetObjectItemCaseSensitive(
                samp_obj, "interpolation");
            if (cJSON_IsString(interp_json)) {
                if (SDL_strcmp(interp_json->valuestring, "STEP") == 0) {
                    temp_samp.interpolation = FORGE_GLTF_INTERP_STEP;
                } else if (SDL_strcmp(interp_json->valuestring,
                                     "CUBICSPLINE") == 0) {
                    /* CUBICSPLINE stores 3 values per keyframe (in-tangent,
                     * value, out-tangent).  Falling back to LINEAR with
                     * this layout would read in-tangents as values.
                     * Skip the sampler entirely. */
                    SDL_Log("forge_gltf: animation %d sampler %d uses "
                            "CUBICSPLINE (not supported), skipping",
                            ai, si);
                    continue;
                } else {
                    temp_samp.interpolation = FORGE_GLTF_INTERP_LINEAR;
                }
            }

            /* Resolve input accessor (timestamps). */
            const cJSON *input_json = cJSON_GetObjectItemCaseSensitive(
                samp_obj, "input");
            if (!cJSON_IsNumber(input_json)) continue;

            int in_count = 0;
            int in_comp = 0;
            int in_num = 0;
            const float *timestamps = (const float *)forge_gltf__get_accessor(
                root, scene, input_json->valueint, &in_count, &in_comp,
                &in_num);
            /* glTF requires SCALAR for animation input (timestamps) */
            if (!timestamps || in_comp != FORGE_GLTF_FLOAT
                  || in_count <= 0 || in_num != 1) {
                continue;
            }

            /* Resolve output accessor (values). */
            const cJSON *output_json = cJSON_GetObjectItemCaseSensitive(
                samp_obj, "output");
            if (!cJSON_IsNumber(output_json)) continue;

            int out_count = 0;
            int out_comp = 0;
            int out_num = 0;
            const float *values = (const float *)forge_gltf__get_accessor(
                root, scene, output_json->valueint,
                &out_count, &out_comp, &out_num);
            if (!values || out_comp != FORGE_GLTF_FLOAT
                || out_count < in_count
                || (out_count % in_count) != 0) {
                continue;
            }

            temp_samp.timestamps      = timestamps;
            temp_samp.values           = values;
            temp_samp.keyframe_count   = in_count;
            /* For TRS samplers out_count == in_count, so the multiplier
             * is 1 and value_components equals out_num (3 or 4).  For
             * morph-weight samplers the output accessor is SCALAR with
             * count = keyframe_count × target_count, giving the actual
             * number of weights per keyframe. */
            temp_samp.value_components = out_num * (out_count / in_count);

            /* Track max timestamp for clip duration. */
            float last_t = timestamps[in_count - 1];
            if (last_t > max_time) max_time = last_t;

            /* Store compacted sampler and record remap. */
            anim->samplers[si_out] = temp_samp;
            sampler_remap[si] = si_out;
            si_out++;
        }
        anim->sampler_count = si_out;
        anim->duration = max_time;

        /* ── Parse channels ────────────────────────────────────────── */
        const cJSON *chan_arr = cJSON_GetObjectItemCaseSensitive(
            anim_obj, "channels");
        if (!cJSON_IsArray(chan_arr)) continue;

        int chan_count = cJSON_GetArraySize(chan_arr);
        if (chan_count < 0) chan_count = 0;
        if (chan_count > 0) {
            size_t chan_bytes;
            if (!forge_gltf__safe_mul((size_t)chan_count, sizeof(ForgeGltfAnimChannel), &chan_bytes)) {
                SDL_Log("forge_gltf: channel allocation size overflow");
                return false;
            }
            anim->channels = (ForgeGltfAnimChannel *)forge_arena_alloc(
                arena, chan_bytes);
            if (!anim->channels) {
                SDL_Log("forge_gltf: arena alloc failed for animation channels");
                return false;
            }
        }

        int stored = 0;

        for (int ci = 0; ci < chan_count; ci++) {
            const cJSON *chan_obj = cJSON_GetArrayItem(chan_arr, ci);

            const cJSON *target = cJSON_GetObjectItemCaseSensitive(
                chan_obj, "target");
            const cJSON *samp_idx = cJSON_GetObjectItemCaseSensitive(
                chan_obj, "sampler");
            if (!target || !cJSON_IsNumber(samp_idx)) continue;

            const cJSON *node_json = cJSON_GetObjectItemCaseSensitive(
                target, "node");
            const cJSON *path_json = cJSON_GetObjectItemCaseSensitive(
                target, "path");
            if (!cJSON_IsNumber(node_json)
                || !cJSON_IsString(path_json)) continue;

            /* Map path string to enum. */
            ForgeGltfAnimPath anim_path;
            if (SDL_strcmp(path_json->valuestring, "translation") == 0) {
                anim_path = FORGE_GLTF_ANIM_TRANSLATION;
            } else if (SDL_strcmp(path_json->valuestring, "rotation") == 0) {
                anim_path = FORGE_GLTF_ANIM_ROTATION;
            } else if (SDL_strcmp(path_json->valuestring, "scale") == 0) {
                anim_path = FORGE_GLTF_ANIM_SCALE;
            } else if (SDL_strcmp(path_json->valuestring, "weights") == 0) {
                anim_path = FORGE_GLTF_ANIM_MORPH_WEIGHTS;
            } else {
                /* Skip unknown paths. */
                continue;
            }

            int raw_si = samp_idx->valueint;
            if (raw_si < 0 || raw_si >= samp_count) continue;

            /* Remap JSON sampler index to compacted index. */
            int si = sampler_remap[raw_si];
            if (si < 0) continue; /* sampler was skipped */

            /* Validate that the sampler has data and the component count
             * matches the target path.  A rotation sampler must have 4
             * components (VEC4); translation and scale must have 3 (VEC3).
             * A mismatch would cause out-of-bounds reads in evaluation. */
            const ForgeGltfAnimSampler *ref_samp = &anim->samplers[si];
            if (!ref_samp->timestamps || !ref_samp->values) continue;
            /* Morph weights have variable component count (= target_count).
             * Validate width against supported morph target limits. */
            if (anim_path == FORGE_GLTF_ANIM_MORPH_WEIGHTS) {
                if (ref_samp->value_components < 1
                    || ref_samp->value_components
                       > FORGE_GLTF_MAX_MORPH_TARGETS) {
                    SDL_Log("forge_gltf: animation %d channel %d: "
                            "sampler has %d morph weights "
                            "(valid range 1..%d)",
                            ai, ci, ref_samp->value_components,
                            FORGE_GLTF_MAX_MORPH_TARGETS);
                    continue;
                }
            } else {
                int expected_comp = (anim_path == FORGE_GLTF_ANIM_ROTATION)
                                  ? FORGE_GLTF_ANIM_QUAT_COMPONENTS
                                  : FORGE_GLTF_ANIM_VEC3_COMPONENTS;
                if (ref_samp->value_components != expected_comp) {
                    SDL_Log("forge_gltf: animation %d channel %d: sampler has "
                            "%d components, expected %d for '%s'",
                            ai, ci, ref_samp->value_components,
                            expected_comp, path_json->valuestring);
                    continue;
                }
            }

            int ni = node_json->valueint;
            if (ni < 0 || ni >= scene->node_count) {
                SDL_Log("forge_gltf: animation %d channel %d targets "
                        "node %d (out of range)", ai, ci, ni);
                continue;
            }

            ForgeGltfAnimChannel *ch = &anim->channels[stored];
            ch->target_node  = ni;
            ch->target_path  = anim_path;
            ch->sampler_index = si;
            stored++;
        }
        anim->channel_count = stored;

        /* Skip animations with no usable channels (e.g. all CUBICSPLINE
         * samplers or channels that failed validation).  Without this check,
         * animation_count would include empty placeholder clips. */
        if (anim->channel_count <= 0) {
            SDL_Log("forge_gltf: animation %d has no usable channels, "
                    "skipping", ai);
            continue;
        }

        SDL_Log("forge_gltf: animation %d '%s': %.3fs, %d samplers, "
                "%d channels",
                ai, anim->name, anim->duration,
                anim->sampler_count, anim->channel_count);
        stored_anims++;
    }
    scene->animation_count = stored_anims;
    return true;
}

/* Internal helper: zero the scene struct on failure paths within
 * forge_gltf_load().  Unlike the public forge_gltf_free() (which is a no-op),
 * this prevents dangling pointers when the arena is about to be destroyed. */
static void forge_gltf__clear_scene(ForgeGltfScene *scene)
{
    if (scene) {
        SDL_memset(scene, 0, sizeof(*scene));
    }
}

/* ── Main load function ──────────────────────────────────────────────────── */

static bool forge_gltf_load(const char *gltf_path, ForgeGltfScene *scene,
                            ForgeArena *arena)
{
    if (!gltf_path || !scene || !arena) {
        SDL_Log("forge_gltf: invalid arguments to forge_gltf_load");
        return false;
    }

    SDL_memset(scene, 0, sizeof(*scene));

    /* JSON text is temporary — use SDL_calloc, not the arena. */
    char *json_text = read_text(gltf_path);
    if (!json_text) return false;

    cJSON *root = cJSON_Parse(json_text);
    SDL_free(json_text);
    if (!root) {
        SDL_Log("forge_gltf: JSON parse error: %s", cJSON_GetErrorPtr());
        return false;
    }

    char base_dir[FORGE_GLTF_PATH_SIZE];
    get_base_dir(base_dir, sizeof(base_dir), gltf_path);

    bool ok = forge_gltf__parse_buffers(root, base_dir, scene, arena);
    if (ok) ok = forge_gltf__parse_materials(root, base_dir, scene, arena);
    if (ok) ok = forge_gltf__parse_meshes(root, scene, arena);
    if (ok) ok = forge_gltf__parse_nodes(root, scene, arena);
    if (ok) ok = forge_gltf__parse_skins(root, scene, arena);
    if (ok) ok = forge_gltf__parse_animations(root, scene, arena);

    /* Validate node skin references now that skin_count is known. */
    if (ok) {
        for (int i = 0; i < scene->node_count; i++) {
            int si = scene->nodes[i].skin_index;
            if (si >= scene->skin_count) {
                SDL_Log("forge_gltf: node %d skin index %d invalid "
                        "(skin_count=%d)", i, si, scene->skin_count);
                scene->nodes[i].skin_index = -1;
            }
        }
    }

    cJSON_Delete(root);

    if (!ok) {
        forge_gltf__clear_scene(scene);
        return false;
    }

    /* Compute world transforms from hierarchy. */
    mat4 identity = mat4_identity();
    for (int i = 0; i < scene->root_node_count; i++) {
        if (!forge_gltf_compute_world_transforms(
                scene, scene->root_nodes[i], &identity)) {
            forge_gltf__clear_scene(scene);
            return false;
        }
    }

    return true;
}

/* ── Free ────────────────────────────────────────────────────────────────── */

static void forge_gltf_free(ForgeGltfScene *scene)
{
    /* All memory is owned by the arena passed to forge_gltf_load().
     * Destroy the arena to release everything.  This function is retained
     * as a no-op so existing code that calls it continues to compile. */
    (void)scene;
}

#endif /* FORGE_GLTF_H */
