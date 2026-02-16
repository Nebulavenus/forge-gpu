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

/* glTF component type constants (from the spec). */
#define FORGE_GLTF_BYTE           5120
#define FORGE_GLTF_UNSIGNED_BYTE  5121
#define FORGE_GLTF_SHORT          5122
#define FORGE_GLTF_UNSIGNED_SHORT 5123
#define FORGE_GLTF_UNSIGNED_INT   5125
#define FORGE_GLTF_FLOAT          5126

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
} ForgeGltfPrimitive;

/* ── Mesh ─────────────────────────────────────────────────────────────────── */
/* A mesh is a named collection of primitives. */

typedef struct ForgeGltfMesh {
    int  first_primitive;  /* index into scene.primitives[] */
    int  primitive_count;
    char name[FORGE_GLTF_NAME_SIZE];
} ForgeGltfMesh;

/* ── Material ─────────────────────────────────────────────────────────────── */
/* Basic PBR material: base color + optional texture path.
 * We store the file path (not a GPU texture) so the caller can load
 * textures using whatever method they prefer. */

typedef struct ForgeGltfMaterial {
    float base_color[4];                       /* RGBA, default (1,1,1,1) */
    char  texture_path[FORGE_GLTF_PATH_SIZE];  /* empty = no texture */
    bool  has_texture;
    char  name[FORGE_GLTF_NAME_SIZE];
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
    char name[FORGE_GLTF_NAME_SIZE];
} ForgeGltfNode;

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
    SDL_snprintf(out, (int)out_size, "%s%s", base_dir, relative);
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
    int accessor_idx, int *out_count, int *out_component_type)
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

        /* Defaults: opaque white, no texture. */
        m->base_color[0] = 1.0f;
        m->base_color[1] = 1.0f;
        m->base_color[2] = 1.0f;
        m->base_color[3] = 1.0f;
        m->texture_path[0] = '\0';
        m->has_texture = false;
        copy_name(m->name, sizeof(m->name), mat);

        const cJSON *pbr = cJSON_GetObjectItemCaseSensitive(
            mat, "pbrMetallicRoughness");
        if (!pbr) continue;

        /* Base color factor. */
        const cJSON *factor = cJSON_GetObjectItemCaseSensitive(
            pbr, "baseColorFactor");
        if (cJSON_IsArray(factor) && cJSON_GetArraySize(factor) == 4) {
            /* Defaults if individual elements are malformed. */
            const float defaults[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            for (int fi = 0; fi < 4; fi++) {
                const cJSON *elem = cJSON_GetArrayItem(factor, fi);
                m->base_color[fi] = elem ? (float)elem->valuedouble
                                         : defaults[fi];
            }
        }

        /* Base color texture (resolve to file path). */
        const cJSON *tex_info = cJSON_GetObjectItemCaseSensitive(
            pbr, "baseColorTexture");
        if (tex_info && cJSON_IsArray(textures_arr)) {
            const cJSON *idx = cJSON_GetObjectItemCaseSensitive(tex_info, "index");
            if (cJSON_IsNumber(idx)) {
                const cJSON *tex_obj = cJSON_GetArrayItem(
                    textures_arr, idx->valueint);
                if (tex_obj) {
                    const cJSON *source = cJSON_GetObjectItemCaseSensitive(
                        tex_obj, "source");
                    if (cJSON_IsNumber(source) && cJSON_IsArray(images_arr)) {
                        const cJSON *img = cJSON_GetArrayItem(
                            images_arr, source->valueint);
                        if (img) {
                            const cJSON *uri = cJSON_GetObjectItemCaseSensitive(
                                img, "uri");
                            if (cJSON_IsString(uri)) {
                                build_path(
                                    m->texture_path,
                                    sizeof(m->texture_path),
                                    base_dir, uri->valuestring);
                                m->has_texture = true;
                            }
                        }
                    }
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
                root, scene, pos_acc->valueint, &vert_count, &comp_type);
            if (!positions || comp_type != FORGE_GLTF_FLOAT) continue;

            const float *normals = NULL;
            const cJSON *norm_acc = cJSON_GetObjectItemCaseSensitive(
                attrs, "NORMAL");
            if (norm_acc) {
                int norm_count = 0;
                int norm_comp = 0;
                const float *n = (const float *)forge_gltf__get_accessor(
                    root, scene, norm_acc->valueint, &norm_count, &norm_comp);
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
                    root, scene, uv_acc->valueint, &uv_count, &uv_comp);
                if (u && uv_count == vert_count
                      && uv_comp == FORGE_GLTF_FLOAT) {
                    uvs = u;
                }
            }

            gp->has_uvs = (uvs != NULL);

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

            /* Read index data. */
            const cJSON *idx_acc = cJSON_GetObjectItemCaseSensitive(
                prim, "indices");
            if (idx_acc && cJSON_IsNumber(idx_acc)) {
                int idx_count = 0;
                int idx_comp = 0;
                const void *idx_data = forge_gltf__get_accessor(
                    root, scene, idx_acc->valueint, &idx_count, &idx_comp);

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
        copy_name(gn->name, sizeof(gn->name), node);

        /* Mesh reference. */
        const cJSON *mesh_idx = cJSON_GetObjectItemCaseSensitive(node, "mesh");
        if (cJSON_IsNumber(mesh_idx)) gn->mesh_index = mesh_idx->valueint;

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
        } else {
            /* TRS decomposition: local = T * R * S */
            mat4 T = mat4_identity();
            mat4 R = mat4_identity();
            mat4 S = mat4_identity();

            const cJSON *trans = cJSON_GetObjectItemCaseSensitive(
                node, "translation");
            if (cJSON_IsArray(trans) && cJSON_GetArraySize(trans) == 3) {
                const cJSON *t0 = cJSON_GetArrayItem(trans, 0);
                const cJSON *t1 = cJSON_GetArrayItem(trans, 1);
                const cJSON *t2 = cJSON_GetArrayItem(trans, 2);
                if (t0 && t1 && t2) {
                    T = mat4_translate(vec3_create(
                        (float)t0->valuedouble,
                        (float)t1->valuedouble,
                        (float)t2->valuedouble));
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
                if (rx && ry && rz && rw) {
                    quat q = quat_create(
                        (float)rw->valuedouble,
                        (float)rx->valuedouble,
                        (float)ry->valuedouble,
                        (float)rz->valuedouble);
                    R = quat_to_mat4(q);
                }
            }

            const cJSON *scl = cJSON_GetObjectItemCaseSensitive(
                node, "scale");
            if (cJSON_IsArray(scl) && cJSON_GetArraySize(scl) == 3) {
                const cJSON *s0 = cJSON_GetArrayItem(scl, 0);
                const cJSON *s1 = cJSON_GetArrayItem(scl, 1);
                const cJSON *s2 = cJSON_GetArrayItem(scl, 2);
                if (s0 && s1 && s2) {
                    S = mat4_scale(vec3_create(
                        (float)s0->valuedouble,
                        (float)s1->valuedouble,
                        (float)s2->valuedouble));
                }
            }

            gn->local_transform = mat4_multiply(T, mat4_multiply(R, S));
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
    }

    for (int i = 0; i < scene->buffer_count; i++) {
        SDL_free(scene->buffers[i].data);
    }

    SDL_memset(scene, 0, sizeof(*scene));
}

#endif /* FORGE_GLTF_H */
