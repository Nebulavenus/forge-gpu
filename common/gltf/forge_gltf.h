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
 *   ForgeGltfScene scene;
 *   if (forge_gltf_load("model.gltf", &scene)) {
 *       // Access scene.nodes, scene.meshes, scene.primitives, etc.
 *       // Upload to GPU, render, etc.
 *       forge_gltf_free(&scene);
 *   }
 *
 * See: lessons/gpu/09-scene-loading/ for a full usage example
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_GLTF_H
#define FORGE_GLTF_H

#include <SDL3/SDL.h>
#include "cJSON.h"
#include "math/forge_math.h"

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
#define FORGE_GLTF_JOINTS_PER_VERT 4

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
} ForgeGltfPrimitive;

/* ── Mesh ─────────────────────────────────────────────────────────────────── */
/* A mesh is a named collection of primitives. */

typedef struct ForgeGltfMesh {
    int  first_primitive;  /* index into scene.primitives[] */
    int  primitive_count;
    char name[FORGE_GLTF_NAME_SIZE];
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
    int  children[FORGE_GLTF_MAX_CHILDREN];
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
    int  joints[FORGE_GLTF_MAX_JOINTS];          /* node indices */
    int  joint_count;
    int  skeleton;                                /* root joint node, -1 if unset */
    mat4 inverse_bind_matrices[FORGE_GLTF_MAX_JOINTS];
} ForgeGltfSkin;

/* ── Binary buffer ────────────────────────────────────────────────────────── */
/* A loaded .bin file referenced by the glTF. */

typedef struct ForgeGltfBuffer {
    Uint8  *data;
    Uint32  size;
} ForgeGltfBuffer;

/* ── Scene (top-level result) ─────────────────────────────────────────────── */
/* Everything parsed from a .gltf file.  All arrays are allocated with
 * SDL_calloc and freed by forge_gltf_free(). */

typedef struct ForgeGltfScene {
    ForgeGltfNode      nodes[FORGE_GLTF_MAX_NODES];
    int                node_count;

    ForgeGltfMesh      meshes[FORGE_GLTF_MAX_MESHES];
    int                mesh_count;

    ForgeGltfPrimitive primitives[FORGE_GLTF_MAX_PRIMITIVES];
    int                primitive_count;

    ForgeGltfMaterial  materials[FORGE_GLTF_MAX_MATERIALS];
    int                material_count;

    ForgeGltfBuffer    buffers[FORGE_GLTF_MAX_BUFFERS];
    int                buffer_count;

    int                root_nodes[FORGE_GLTF_MAX_NODES];
    int                root_node_count;

    ForgeGltfSkin      skins[FORGE_GLTF_MAX_SKINS];
    int                skin_count;
} ForgeGltfScene;

/* ── API ──────────────────────────────────────────────────────────────────── */

/* Load a .gltf file and all referenced .bin buffers.
 * On success, returns true and fills *scene.  Caller must call
 * forge_gltf_free() when done.
 * On failure, returns false and scene is in an indeterminate state. */
static bool forge_gltf_load(const char *gltf_path, ForgeGltfScene *scene);

/* Free all memory allocated by forge_gltf_load().
 * Safe to call on a zeroed scene (does nothing). */
static void forge_gltf_free(ForgeGltfScene *scene);

/* Recursively compute world_transform for all nodes in the hierarchy.
 * Called automatically by forge_gltf_load(), but exposed in case you
 * need to recompute after modifying local transforms. */
static void forge_gltf_compute_world_transforms(ForgeGltfScene *scene,
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

static Uint8 *read_binary(const char *path, Uint32 *out_size)
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

    Uint8 *buf = (Uint8 *)SDL_malloc((size_t)size);
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
 * per the glTF 2.0 specification before returning a pointer. */

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
    if (!bv_idx || !comp || !cnt || !cJSON_IsString(type_str)) return NULL;

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
    if (cJSON_IsNumber(acc_off)) acc_offset = acc_off->valueint;

    /* Bounds-check the bufferView index before accessing the array. */
    int view_count = cJSON_GetArraySize(views);
    if (bv_idx->valueint < 0 || bv_idx->valueint >= view_count) return NULL;

    const cJSON *view = cJSON_GetArrayItem(views, bv_idx->valueint);
    if (!view) return NULL;

    const cJSON *buf_idx = cJSON_GetObjectItemCaseSensitive(view, "buffer");
    const cJSON *bv_off_json = cJSON_GetObjectItemCaseSensitive(view, "byteOffset");
    const cJSON *bv_len_json = cJSON_GetObjectItemCaseSensitive(view, "byteLength");
    if (!buf_idx) return NULL;

    int bi = buf_idx->valueint;
    if (bi < 0 || bi >= scene->buffer_count) return NULL;

    Uint32 bv_offset = 0;
    if (cJSON_IsNumber(bv_off_json)) bv_offset = (Uint32)bv_off_json->valueint;

    /* bufferView.byteLength is required by the spec — reject if missing. */
    if (!cJSON_IsNumber(bv_len_json) || bv_len_json->valueint <= 0) {
        SDL_Log("forge_gltf: bufferView %d missing or invalid byteLength",
                bv_idx->valueint);
        return NULL;
    }
    Uint32 bv_byte_length = (Uint32)bv_len_json->valueint;

    /* Ensure the bufferView itself fits within the binary buffer. */
    if (bv_offset + bv_byte_length > scene->buffers[bi].size) {
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
    if (cJSON_IsNumber(bv_stride_json) && bv_stride_json->valueint > 0) {
        byte_stride = bv_stride_json->valueint;
    }

    int count = cnt->valueint;
    if (count > 0) {
        Uint32 required = (Uint32)acc_offset
                        + (Uint32)(count - 1) * (Uint32)byte_stride
                        + (Uint32)element_size;
        if (required > bv_byte_length) {
            SDL_Log("forge_gltf: accessor %d exceeds bufferView %d bounds "
                    "(need %u bytes, view has %u)",
                    accessor_idx, bv_idx->valueint, required, bv_byte_length);
            return NULL;
        }
    }

    if (out_count) *out_count = count;
    if (out_component_type) *out_component_type = comp->valueint;
    if (out_num_components) *out_num_components = num_components;

    return scene->buffers[bi].data + bv_offset + (Uint32)acc_offset;
}

/* ── Parse binary buffers ────────────────────────────────────────────────── */

static bool forge_gltf__parse_buffers(const cJSON *root, const char *base_dir,
                                       ForgeGltfScene *scene)
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
        scene->buffers[i].data = read_binary(path, &file_size);
        if (!scene->buffers[i].data) return false;
        scene->buffers[i].size = file_size;
    }
    scene->buffer_count = count;
    return true;
}

/* ── Parse materials ─────────────────────────────────────────────────────── */

static bool forge_gltf__parse_materials(const cJSON *root,
                                         const char *base_dir,
                                         ForgeGltfScene *scene)
{
    const cJSON *mats = cJSON_GetObjectItemCaseSensitive(root, "materials");
    if (!cJSON_IsArray(mats)) {
        scene->material_count = 0;
        return true;
    }

    const cJSON *images_arr = cJSON_GetObjectItemCaseSensitive(root, "images");
    const cJSON *textures_arr = cJSON_GetObjectItemCaseSensitive(root, "textures");

    int count = cJSON_GetArraySize(mats);
    if (count > FORGE_GLTF_MAX_MATERIALS) {
        SDL_Log("forge_gltf: %d materials, capping at %d",
                count, FORGE_GLTF_MAX_MATERIALS);
        count = FORGE_GLTF_MAX_MATERIALS;
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

static bool forge_gltf__parse_meshes(const cJSON *root, ForgeGltfScene *scene)
{
    const cJSON *meshes = cJSON_GetObjectItemCaseSensitive(root, "meshes");
    if (!cJSON_IsArray(meshes)) {
        SDL_Log("forge_gltf: no 'meshes' array");
        return false;
    }

    int mesh_count = cJSON_GetArraySize(meshes);
    if (mesh_count > FORGE_GLTF_MAX_MESHES) mesh_count = FORGE_GLTF_MAX_MESHES;

    for (int mi = 0; mi < mesh_count; mi++) {
        const cJSON *mesh = cJSON_GetArrayItem(meshes, mi);
        const cJSON *prims = cJSON_GetObjectItemCaseSensitive(mesh, "primitives");
        if (!cJSON_IsArray(prims)) continue;

        ForgeGltfMesh *gm = &scene->meshes[mi];
        gm->first_primitive = scene->primitive_count;
        gm->primitive_count = 0;
        copy_name(gm->name, sizeof(gm->name), mesh);

        int prim_count = cJSON_GetArraySize(prims);
        for (int pi = 0; pi < prim_count; pi++) {
            if (scene->primitive_count >= FORGE_GLTF_MAX_PRIMITIVES) break;

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
            const float *positions = (const float *)forge_gltf__get_accessor(
                root, scene, pos_acc->valueint, &vert_count, &comp_type,
                NULL);
            if (!positions || comp_type != FORGE_GLTF_FLOAT) continue;

            const float *normals = NULL;
            const cJSON *norm_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "NORMAL");
            if (norm_acc) {
                int norm_count = 0;
                int norm_comp = 0;
                const float *n = (const float *)forge_gltf__get_accessor(
                    root, scene, norm_acc->valueint, &norm_count, &norm_comp,
                    NULL);
                if (n && norm_count == vert_count
                      && norm_comp == FORGE_GLTF_FLOAT) {
                    normals = n;
                }
            }

            const float *uvs = NULL;
            const cJSON *uv_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "TEXCOORD_0");
            if (uv_acc) {
                int uv_count = 0;
                int uv_comp = 0;
                const float *u = (const float *)forge_gltf__get_accessor(
                    root, scene, uv_acc->valueint, &uv_count, &uv_comp,
                    NULL);
                if (u && uv_count == vert_count
                      && uv_comp == FORGE_GLTF_FLOAT) {
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
            gp->vertices = (ForgeGltfVertex *)SDL_calloc(
                (size_t)vert_count, sizeof(ForgeGltfVertex));
            if (!gp->vertices) continue;
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
                gp->tangents = (vec4 *)SDL_calloc(
                    (size_t)vert_count, sizeof(vec4));
                if (gp->tangents) {
                    gp->has_tangents = true;
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
                        size_t ji_bytes = (size_t)vert_count
                                        * FORGE_GLTF_JOINTS_PER_VERT
                                        * sizeof(Uint16);
                        gp->joint_indices = (Uint16 *)SDL_malloc(ji_bytes);

                        /* Allocate weights (FLOAT × 4). */
                        size_t wt_bytes = (size_t)vert_count
                                        * FORGE_GLTF_JOINTS_PER_VERT
                                        * sizeof(float);
                        gp->weights = (float *)SDL_malloc(wt_bytes);

                        if (gp->joint_indices && gp->weights) {
                            if (j_comp == FORGE_GLTF_UNSIGNED_SHORT) {
                                SDL_memcpy(gp->joint_indices, j_data, ji_bytes);
                            } else {
                                /* Widen UNSIGNED_BYTE → Uint16. */
                                const Uint8 *src = (const Uint8 *)j_data;
                                int total = vert_count * FORGE_GLTF_JOINTS_PER_VERT;
                                for (int k = 0; k < total; k++) {
                                    gp->joint_indices[k] = (Uint16)src[k];
                                }
                            }
                            SDL_memcpy(gp->weights, w_data, wt_bytes);
                            gp->has_skin_data = true;
                        } else {
                            SDL_free(gp->joint_indices);
                            SDL_free(gp->weights);
                            gp->joint_indices = NULL;
                            gp->weights = NULL;
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

            /* Read index data. */
            const cJSON *idx_acc = cJSON_GetObjectItemCaseSensitive(
                prim, "indices");
            if (idx_acc && cJSON_IsNumber(idx_acc)) {
                int idx_count = 0;
                int idx_comp = 0;
                const void *idx_data = forge_gltf__get_accessor(
                    root, scene, idx_acc->valueint, &idx_count, &idx_comp,
                    NULL);

                if (idx_data && idx_count > 0) {
                    Uint32 elem_size = 0;
                    if (idx_comp == FORGE_GLTF_UNSIGNED_SHORT) {
                        elem_size = 2;
                    } else if (idx_comp == FORGE_GLTF_UNSIGNED_INT) {
                        elem_size = 4;
                    } else {
                        SDL_Log("forge_gltf: unsupported index type %d",
                                idx_comp);
                        SDL_free(gp->vertices);
                        gp->vertices = NULL;
                        continue;
                    }

                    Uint32 total = (Uint32)idx_count * elem_size;
                    gp->indices = SDL_malloc(total);
                    if (gp->indices) {
                        SDL_memcpy(gp->indices, idx_data, total);
                        gp->index_count = (Uint32)idx_count;
                        gp->index_stride = elem_size;
                    }
                }
            }

            /* Material reference. */
            const cJSON *mat_idx = cJSON_GetObjectItemCaseSensitive(
                prim, "material");
            gp->material_index = cJSON_IsNumber(mat_idx)
                                 ? mat_idx->valueint : -1;

            scene->primitive_count++;
            gm->primitive_count++;
        }
    }
    scene->mesh_count = mesh_count;
    return true;
}

/* ── Parse nodes ─────────────────────────────────────────────────────────── */

static bool forge_gltf__parse_nodes(const cJSON *root, ForgeGltfScene *scene)
{
    const cJSON *nodes = cJSON_GetObjectItemCaseSensitive(root, "nodes");
    if (!cJSON_IsArray(nodes)) {
        SDL_Log("forge_gltf: no 'nodes' array");
        return false;
    }

    int count = cJSON_GetArraySize(nodes);
    if (count > FORGE_GLTF_MAX_NODES) count = FORGE_GLTF_MAX_NODES;

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
        if (cJSON_IsNumber(mesh_idx)) gn->mesh_index = mesh_idx->valueint;

        /* Skin reference — store raw index, validate after skins are parsed. */
        const cJSON *skin_idx = cJSON_GetObjectItemCaseSensitive(node, "skin");
        if (cJSON_IsNumber(skin_idx)) {
            gn->skin_index = skin_idx->valueint;
        }

        /* Children. */
        const cJSON *children = cJSON_GetObjectItemCaseSensitive(
            node, "children");
        if (cJSON_IsArray(children)) {
            int cc = cJSON_GetArraySize(children);
            if (cc > FORGE_GLTF_MAX_CHILDREN) {
                SDL_Log("forge_gltf: node %d has %d children, capping at %d",
                        i, cc, FORGE_GLTF_MAX_CHILDREN);
                cc = FORGE_GLTF_MAX_CHILDREN;
            }
            int valid = 0;
            for (int c = 0; c < cc; c++) {
                const cJSON *item = cJSON_GetArrayItem(children, c);
                if (item) {
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
        const cJSON *roots = cJSON_GetObjectItemCaseSensitive(sc, "nodes");
        if (cJSON_IsArray(roots)) {
            int rc = cJSON_GetArraySize(roots);
            int valid_roots = 0;
            for (int i = 0; i < rc && i < FORGE_GLTF_MAX_NODES; i++) {
                const cJSON *item = cJSON_GetArrayItem(roots, i);
                if (item) {
                    scene->root_nodes[valid_roots++] = item->valueint;
                }
            }
            scene->root_node_count = valid_roots;
        }
    }

    return true;
}

/* ── Compute world transforms ────────────────────────────────────────────── */

static void forge_gltf_compute_world_transforms(ForgeGltfScene *scene,
                                                 int node_idx,
                                                 const mat4 *parent_world)
{
    if (node_idx < 0 || node_idx >= scene->node_count) return;

    ForgeGltfNode *node = &scene->nodes[node_idx];
    node->world_transform = mat4_multiply(*parent_world, node->local_transform);

    for (int i = 0; i < node->child_count; i++) {
        forge_gltf_compute_world_transforms(
            scene, node->children[i], &node->world_transform);
    }
}

/* ── Parse skins ─────────────────────────────────────────────────────────── */

static bool forge_gltf__parse_skins(const cJSON *root, ForgeGltfScene *scene)
{
    const cJSON *skins = cJSON_GetObjectItemCaseSensitive(root, "skins");
    if (!cJSON_IsArray(skins)) {
        scene->skin_count = 0;
        return true; /* skins are optional */
    }

    int count = cJSON_GetArraySize(skins);
    if (count > FORGE_GLTF_MAX_SKINS) {
        SDL_Log("forge_gltf: %d skins, capping at %d",
                count, FORGE_GLTF_MAX_SKINS);
        count = FORGE_GLTF_MAX_SKINS;
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
            if (jc > FORGE_GLTF_MAX_JOINTS) {
                SDL_Log("forge_gltf: skin %d has %d joints, capping at %d",
                        i, jc, FORGE_GLTF_MAX_JOINTS);
                jc = FORGE_GLTF_MAX_JOINTS;
            }
            for (int j = 0; j < jc; j++) {
                const cJSON *item = cJSON_GetArrayItem(joints_arr, j);
                int ji = item ? item->valueint : -1;
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

/* ── Main load function ──────────────────────────────────────────────────── */

static bool forge_gltf_load(const char *gltf_path, ForgeGltfScene *scene)
{
    SDL_memset(scene, 0, sizeof(*scene));

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

    bool ok = forge_gltf__parse_buffers(root, base_dir, scene);
    if (ok) ok = forge_gltf__parse_materials(root, base_dir, scene);
    if (ok) ok = forge_gltf__parse_meshes(root, scene);
    if (ok) ok = forge_gltf__parse_nodes(root, scene);
    if (ok) ok = forge_gltf__parse_skins(root, scene);

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
        forge_gltf_free(scene);
        return false;
    }

    /* Compute world transforms from hierarchy. */
    mat4 identity = mat4_identity();
    for (int i = 0; i < scene->root_node_count; i++) {
        forge_gltf_compute_world_transforms(
            scene, scene->root_nodes[i], &identity);
    }

    return true;
}

/* ── Free ────────────────────────────────────────────────────────────────── */

static void forge_gltf_free(ForgeGltfScene *scene)
{
    if (!scene) return;

    for (int i = 0; i < scene->primitive_count; i++) {
        SDL_free(scene->primitives[i].vertices);
        SDL_free(scene->primitives[i].indices);
        SDL_free(scene->primitives[i].tangents);
        SDL_free(scene->primitives[i].joint_indices);
        SDL_free(scene->primitives[i].weights);
    }

    for (int i = 0; i < scene->buffer_count; i++) {
        SDL_free(scene->buffers[i].data);
    }

    SDL_memset(scene, 0, sizeof(*scene));
}

#endif /* FORGE_GLTF_H */
