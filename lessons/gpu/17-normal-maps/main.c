/*
 * Lesson 17 — Normal Maps
 *
 * Teach tangent-space normal mapping: adding surface detail (bumps,
 * grooves, patterns) without extra geometry.  A normal map stores
 * per-texel surface directions in tangent space; the TBN matrix
 * transforms them to world space for lighting.
 *
 * This lesson uses the Khronos NormalTangentMirrorTest model, which
 * provides pre-computed tangent vectors (VEC4 with handedness) and
 * is specifically designed to test correct tangent-space handling.
 * The model places real geometry on the left and normal-mapped quads
 * on the right — when normal mapping works correctly, both columns
 * produce identical reflections.
 *
 * The lesson demonstrates:
 *   - Tangent-space normal mapping and the TBN matrix
 *   - Eric Lengyel's method for computing tangent/bitangent vectors
 *   - Using supplied tangent vectors from glTF (VEC4 with handedness)
 *   - Gram-Schmidt re-orthogonalization of the TBN basis
 *   - Sampling and decoding normal maps in the fragment shader
 *   - Comparing flat, per-vertex, and normal-mapped shading (1/2/3 keys)
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain      (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline              (Lesson 02)
 *   - Push uniforms for per-primitive color and MVP           (Lesson 03)
 *   - Texture + sampler binding                               (Lesson 04)
 *   - Depth buffer, window resize                             (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time         (Lesson 07)
 *   - glTF loading with multi-material rendering              (Lesson 09)
 *   - Blinn-Phong lighting with normal transformation         (Lesson 10)
 *   - Procedural grid floor                                   (Lesson 12)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   1 / 2 / 3          — switch shading: flat / per-vertex / normal-mapped
 *   Escape             — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include "math/forge_math.h"
#include "gltf/forge_gltf.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

/* ── Frame capture (compile-time option) ─────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────── */

#include "shaders/scene_vert_dxil.h"
#include "shaders/scene_vert_spirv.h"
#include "shaders/scene_frag_dxil.h"
#include "shaders/scene_frag_spirv.h"
#include "shaders/grid_vert_dxil.h"
#include "shaders/grid_vert_spirv.h"
#include "shaders/grid_frag_dxil.h"
#include "shaders/grid_frag_spirv.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_TITLE "Forge GPU - 17 Normal Maps"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

/* Dark blue background — matches the grid scenes in Lessons 12–16. */
#define CLEAR_R 0.0099f
#define CLEAR_G 0.0099f
#define CLEAR_B 0.0267f
#define CLEAR_A 1.0f

/* Depth buffer */
#define DEPTH_CLEAR 1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Texture constants */
#define BYTES_PER_PIXEL  4
#define WHITE_TEX_DIM    1
#define MAX_LOD          1000.0f

/* Path to the glTF model (relative to executable). */
#define GLTF_PATH "assets/NormalTangentMirrorTest.gltf"
#define PATH_BUFFER_SIZE 512
#define DEGENERATE_UV_EPSILON 1e-8f

/* The model's geometry ranges from y=-1.2 to y=1.05.  Shift upward
 * so the bottom rests on the grid floor (y = 0). */
#define SCENE_Y_OFFSET 1.2f

/* ── Camera parameters ───────────────────────────────────────────────── */

/* Position the camera to see the front of the model.
 * The model spans roughly x=[-1.4, 1.4], y=[-1.2, 1.05], z=[-0.01, 0.08].
 * After the Y offset it sits at y=[0, 2.25]. */
#define CAM_START_X    0.0f
#define CAM_START_Y    1.5f
#define CAM_START_Z    3.5f
#define CAM_START_YAW  0.0f
#define CAM_START_PITCH 0.0f

#define MOVE_SPEED       5.0f
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG    89.0f

#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

#define MS_TO_SEC     1000.0f
#define MAX_DELTA_TIME 0.1f

/* ── Grid floor parameters ───────────────────────────────────────────── */

#define GRID_HALF_SIZE   50.0f
#define GRID_NUM_VERTS    4
#define GRID_NUM_INDICES  6
#define GRID_VERTEX_PITCH 12   /* 3 floats * 4 bytes */

/* Blue grid lines on a dark background (linear sRGB, same as Lesson 12) */
#define GRID_LINE_R       0.068f
#define GRID_LINE_G       0.534f
#define GRID_LINE_B       0.932f
#define GRID_LINE_A       1.0f

#define GRID_BG_R         0.014f
#define GRID_BG_G         0.014f
#define GRID_BG_B         0.045f
#define GRID_BG_A         1.0f

#define GRID_SPACING      1.0f   /* world units between grid lines   */
#define GRID_LINE_WIDTH   0.02f  /* line thickness in grid-space      */
#define GRID_FADE_DIST    40.0f  /* distance at which grid fades out  */

/* Grid shader resource counts (no samplers, 1 uniform each) */
#define GRID_VS_NUM_SAMPLERS         0
#define GRID_VS_NUM_STORAGE_TEXTURES 0
#define GRID_VS_NUM_STORAGE_BUFFERS  0
#define GRID_VS_NUM_UNIFORM_BUFFERS  1

#define GRID_FS_NUM_SAMPLERS         0
#define GRID_FS_NUM_STORAGE_TEXTURES 0
#define GRID_FS_NUM_STORAGE_BUFFERS  0
#define GRID_FS_NUM_UNIFORM_BUFFERS  1

/* ── Shader resource counts ──────────────────────────────────────────── */

/* Scene vertex shader: 0 samplers, 0 storage, 1 uniform (MVP + model) */
#define VS_NUM_SAMPLERS         0
#define VS_NUM_STORAGE_TEXTURES 0
#define VS_NUM_STORAGE_BUFFERS  0
#define VS_NUM_UNIFORM_BUFFERS  1

/* Scene fragment shader: 2 samplers (diffuse + normal), 0 storage, 1 uniform */
#define FS_NUM_SAMPLERS         2
#define FS_NUM_STORAGE_TEXTURES 0
#define FS_NUM_STORAGE_BUFFERS  0
#define FS_NUM_UNIFORM_BUFFERS  1

/* ── Blinn-Phong lighting parameters ─────────────────────────────────── */

/* Directional light from upper-right-front, same style as Lessons 10–16. */
#define LIGHT_DIR_X       0.3f
#define LIGHT_DIR_Y       0.8f
#define LIGHT_DIR_Z       0.5f

#define AMBIENT_INTENSITY  0.15f
#define SPECULAR_STRENGTH  0.4f
#define SHININESS          32.0f

/* ── Normal mode constants ───────────────────────────────────────────── */

#define NORMAL_MODE_FLAT    0.0f
#define NORMAL_MODE_VERTEX  1.0f
#define NORMAL_MODE_MAPPED  2.0f

/* ── Vertex layout for normal-mapped geometry ────────────────────────── */
/* Extends the base ForgeGltfVertex with a vec4 tangent.  The tangent's
 * xyz stores the tangent direction; w stores the handedness (+1 or -1),
 * which encodes whether the UV space is mirrored. */

typedef struct SceneVertex {
    vec3 position;    /* TEXCOORD0 — object-space position        */
    vec3 normal;      /* TEXCOORD1 — object-space normal          */
    vec2 uv;          /* TEXCOORD2 — texture coordinates          */
    vec4 tangent;     /* TEXCOORD3 — tangent (xyz) + sign (w)     */
} SceneVertex;

/* ── Uniform structures (match HLSL cbuffers) ────────────────────────── */

typedef struct VertUniforms {
    mat4 mvp;                   /* 64 bytes */
    mat4 model;                 /* 64 bytes — total: 128 bytes */
} VertUniforms;

typedef struct FragUniforms {
    float base_color[4];        /* 16 bytes — RGBA multiplier              */
    float light_dir[4];         /* 16 bytes — world-space light direction  */
    float eye_pos[4];           /* 16 bytes — world-space camera position  */
    float has_texture;          /*  4 bytes — 1.0 = sample, 0.0 = skip    */
    float has_normal_map;       /*  4 bytes — 1.0 = sample normal map     */
    float shininess;            /*  4 bytes — specular exponent            */
    float ambient;              /*  4 bytes — ambient intensity [0..1]     */
    float specular_str;         /*  4 bytes — specular intensity [0..1]    */
    float normal_mode;          /*  4 bytes — 0/1/2 shading mode          */
    float _pad0;
    float _pad1;                /* total: 80 bytes                         */
} FragUniforms;

/* ── Grid fragment uniforms (match grid.frag.hlsl cbuffer) ────────────── */

typedef struct GridFragUniforms {
    float line_color[4];    /* 16 bytes */
    float bg_color[4];      /* 16 bytes */
    float light_dir[4];     /* 16 bytes — world-space light direction  */
    float eye_pos[4];       /* 16 bytes — world-space camera position  */
    float grid_spacing;     /*  4 bytes */
    float line_width;       /*  4 bytes */
    float fade_distance;    /*  4 bytes */
    float ambient;          /*  4 bytes — ambient intensity [0..1]     */
    float shininess;        /*  4 bytes — specular exponent            */
    float specular_str;     /*  4 bytes — specular intensity [0..1]    */
    float _pad0;            /*  4 bytes */
    float _pad1;            /*  4 bytes — total: 96 bytes */
} GridFragUniforms;

/* ── GPU-side per-primitive data ─────────────────────────────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    Uint32         index_count;
    int            material_index;
    SDL_GPUIndexElementSize index_type;
    bool           has_uvs;
} GpuPrimitive;

typedef struct GpuMaterial {
    float          base_color[4];
    SDL_GPUTexture *diffuse_texture;   /* NULL = use white placeholder  */
    bool           has_texture;
    SDL_GPUTexture *normal_texture;    /* NULL = no normal map          */
    bool           has_normal_map;
    bool           double_sided;
} GpuMaterial;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Scene pipeline (normal-mapped Blinn-Phong) */
    SDL_GPUGraphicsPipeline *scene_pipeline;

    /* Grid floor (procedural anti-aliased grid from Lesson 12) */
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUBuffer           *grid_vertex_buffer;
    SDL_GPUBuffer           *grid_index_buffer;

    /* Texture sampler (trilinear + repeat) */
    SDL_GPUSampler *sampler;

    /* Placeholder 1×1 white texture for untextured materials. */
    SDL_GPUTexture *white_texture;

    /* Flat normal map placeholder: 1×1 texture encoding (0.5, 0.5, 1.0)
     * which decodes to the tangent-space normal (0, 0, 1) — pointing
     * straight outward, producing no perturbation. */
    SDL_GPUTexture *flat_normal_texture;

    /* Loaded scene data (CPU side) */
    ForgeGltfScene scene;

    /* Uploaded GPU buffers (one per primitive) */
    GpuPrimitive   gpu_primitives[FORGE_GLTF_MAX_PRIMITIVES];
    int            gpu_primitive_count;

    /* Uploaded GPU materials */
    GpuMaterial    gpu_materials[FORGE_GLTF_MAX_MATERIALS];
    int            gpu_material_count;

    /* Loaded textures (for cleanup) */
    SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
    int            loaded_texture_count;

    /* Depth buffer (recreated on resize) */
    SDL_GPUTexture *depth_texture;
    Uint32          depth_width;
    Uint32          depth_height;

    /* Camera state */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Normal mode: 0 = flat, 1 = per-vertex, 2 = normal-mapped */
    float normal_mode;

    /* Timing */
    Uint64 last_ticks;
    bool   mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;

/* ══════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ══════════════════════════════════════════════════════════════════════ */

/* ── Create a shader from embedded bytecode ──────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code, size_t dxil_size,
    int num_samplers, int num_storage_textures,
    int num_storage_buffers, int num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage = stage;
    info.num_samplers = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers = num_storage_buffers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv_code;
        info.code_size = spirv_size;
        info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil_code;
        info.code_size = dxil_size;
        info.entrypoint = "main";
    } else {
        SDL_Log("No supported shader format");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("SDL_CreateGPUShader failed: %s", SDL_GetError());
    }
    return shader;
}

/* ── Upload raw data to a GPU buffer ─────────────────────────────────── */

static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    const void *data, Uint32 size)
{
    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!transfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUBufferCreateInfo bci;
    SDL_zero(bci);
    bci.usage = usage;
    bci.size = size;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &bci);
    if (!buffer) {
        SDL_Log("SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = transfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size = size;

    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return buffer;
}

/* ── Load an image file to a GPU texture with mipmaps ────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        /* Try SDL_LoadSurface for PNG support. */
        surface = SDL_LoadSurface(path);
    }
    if (!surface) {
        SDL_Log("Failed to load texture %s: %s", path, SDL_GetError());
        return NULL;
    }

    /* Convert to RGBA8 for consistent GPU upload. */
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!rgba) {
        SDL_Log("Failed to convert surface: %s", SDL_GetError());
        return NULL;
    }

    Uint32 w = (Uint32)rgba->w;
    Uint32 h = (Uint32)rgba->h;

    /* Calculate mip count. */
    Uint32 max_dim = w > h ? w : h;
    Uint32 mip_count = 1;
    while (max_dim > 1) { max_dim >>= 1; mip_count++; }

    SDL_GPUTextureCreateInfo tci;
    SDL_zero(tci);
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.width = w;
    tci.height = h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = mip_count;
    /* SAMPLER for fragment shader access, COLOR_TARGET is required by
     * SDL_GenerateMipmapsForGPUTexture to blit between mip levels. */
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
              | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tci);
    if (!texture) {
        SDL_Log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        SDL_DestroySurface(rgba);
        return NULL;
    }

    /* Upload mip 0. */
    Uint32 data_size = w * h * BYTES_PER_PIXEL;

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = data_size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!transfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(rgba);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(rgba);
        return NULL;
    }
    SDL_memcpy(mapped, rgba->pixels, data_size);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(rgba);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = transfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = texture;
    dst.w = w;
    dst.h = h;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    /* Generate remaining mip levels. */
    SDL_GenerateMipmapsForGPUTexture(cmd, texture);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer (texture) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    return texture;
}

/* ── Create a 1×1 placeholder texture with a given RGBA color ────────── */

static SDL_GPUTexture *create_1x1_texture(SDL_GPUDevice *device,
                                           Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_GPUTextureCreateInfo tci;
    SDL_zero(tci);
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.width = 1;
    tci.height = 1;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tci);
    if (!tex) {
        SDL_Log("SDL_CreateGPUTexture (1x1) failed: %s", SDL_GetError());
        return NULL;
    }

    Uint8 pixels[4] = { r, g, b, a };

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(pixels);
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tb) {
        SDL_Log("SDL_CreateGPUTransferBuffer (1x1) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *p = SDL_MapGPUTransferBuffer(device, tb, false);
    if (!p) {
        SDL_Log("SDL_MapGPUTransferBuffer (1x1) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(p, pixels, sizeof(pixels));
    SDL_UnmapGPUTransferBuffer(device, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer (1x1) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = tb;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = 1;
    dst.h = 1;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer (1x1 tex): %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, tb);
    return tex;
}

/* ══════════════════════════════════════════════════════════════════════
 * Tangent Computation — Eric Lengyel's Method
 *
 * For models that don't supply tangent vectors (no TANGENT attribute),
 * we compute them from the mesh geometry.  This is Eric Lengyel's
 * method from "Foundations of Game Engine Development, Volume 2":
 *
 * For each triangle with positions P0, P1, P2 and UVs (u0,v0), (u1,v1),
 * (u2,v2):
 *   - Edge vectors: e1 = P1 - P0,  e2 = P2 - P0
 *   - UV deltas:    (du1, dv1) = UV1 - UV0,  (du2, dv2) = UV2 - UV0
 *   - Determinant:  det = du1 * dv2 - du2 * dv1
 *   - Tangent:   T = (1/det) * (dv2 * e1 - dv1 * e2)
 *   - Bitangent: B = (1/det) * (du1 * e2 - du2 * e1)
 *
 * The per-triangle tangents are accumulated per-vertex (averaged across
 * all triangles sharing each vertex), then orthogonalized against the
 * vertex normal using Gram-Schmidt.  Handedness is computed from the
 * cross product to handle mirrored UVs correctly.
 *
 * The NormalTangentMirrorTest model supplies pre-computed tangents, so
 * this function is used as a fallback for models without them.
 * ══════════════════════════════════════════════════════════════════════ */

static void compute_tangents_lengyel(
    const ForgeGltfVertex *vertices, Uint32 vert_count,
    const void *indices, Uint32 index_count, Uint32 index_stride,
    vec4 *out_tangents)
{
    Uint32 v;

    /* Temporary arrays: accumulated tangent (tan1) and bitangent (tan2)
     * directions per vertex, averaged across all sharing triangles. */
    vec3 *tan1 = (vec3 *)SDL_calloc(vert_count, sizeof(vec3));
    vec3 *tan2 = (vec3 *)SDL_calloc(vert_count, sizeof(vec3));
    if (!tan1 || !tan2) {
        SDL_free(tan1);
        SDL_free(tan2);
        /* Fall back to default tangent (1,0,0) with +1 handedness */
        for (v = 0; v < vert_count; v++)
            out_tangents[v] = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    /* ── Step 1: Accumulate per-triangle tangent/bitangent ─────────── */
    {
        Uint32 i;
        for (i = 0; i < index_count; i += 3) {
            Uint32 i0, i1, i2;
            if (index_stride == 2) {
                const Uint16 *idx = (const Uint16 *)indices;
                i0 = idx[i]; i1 = idx[i+1]; i2 = idx[i+2];
            } else {
                const Uint32 *idx = (const Uint32 *)indices;
                i0 = idx[i]; i1 = idx[i+1]; i2 = idx[i+2];
            }

            /* Triangle edge vectors in object space */
            vec3 e1 = vec3_sub(vertices[i1].position, vertices[i0].position);
            vec3 e2 = vec3_sub(vertices[i2].position, vertices[i0].position);

            /* UV coordinate deltas */
            float du1 = vertices[i1].uv.x - vertices[i0].uv.x;
            float dv1 = vertices[i1].uv.y - vertices[i0].uv.y;
            float du2 = vertices[i2].uv.x - vertices[i0].uv.x;
            float dv2 = vertices[i2].uv.y - vertices[i0].uv.y;

            /* Determinant of the UV edge matrix.  If zero, the triangle
             * has degenerate UVs (zero-area in texture space) — skip it. */
            float det = du1 * dv2 - du2 * dv1;
            if (SDL_fabsf(det) < DEGENERATE_UV_EPSILON) continue;
            float inv_det = 1.0f / det;

            /* Solve for the tangent and bitangent directions:
             * [T]   [dv2  -dv1] [e1]
             * [B] = [-du2  du1] [e2] * (1 / det) */
            vec3 t = vec3_scale(
                vec3_sub(vec3_scale(e1, dv2), vec3_scale(e2, dv1)),
                inv_det);
            vec3 b = vec3_scale(
                vec3_sub(vec3_scale(e2, du1), vec3_scale(e1, du2)),
                inv_det);

            /* Accumulate to all three vertices of this triangle */
            tan1[i0] = vec3_add(tan1[i0], t);
            tan1[i1] = vec3_add(tan1[i1], t);
            tan1[i2] = vec3_add(tan1[i2], t);
            tan2[i0] = vec3_add(tan2[i0], b);
            tan2[i1] = vec3_add(tan2[i1], b);
            tan2[i2] = vec3_add(tan2[i2], b);
        }
    }

    /* ── Step 2: Orthogonalize and compute handedness per vertex ───── */
    for (v = 0; v < vert_count; v++) {
        vec3 n = vertices[v].normal;
        vec3 t = tan1[v];

        /* Gram-Schmidt: project out the normal component from the
         * tangent, then normalize.  This ensures T ⊥ N. */
        vec3 ortho_t = vec3_normalize(
            vec3_sub(t, vec3_scale(n, vec3_dot(n, t))));

        /* Handedness: the sign of dot(cross(N, T), B) tells us whether
         * the UV space is right-handed (+1) or left-handed (-1, mirrored).
         * Storing this in tangent.w lets the shader reconstruct the
         * correct bitangent direction. */
        float hand = (vec3_dot(vec3_cross(n, t), tan2[v]) < 0.0f)
                     ? -1.0f : 1.0f;

        out_tangents[v] = vec4_create(ortho_t.x, ortho_t.y,
                                       ortho_t.z, hand);
    }

    SDL_free(tan1);
    SDL_free(tan2);
}

/* ── Upload the parsed glTF scene to GPU buffers and textures ────────── */

static bool upload_scene_to_gpu(SDL_GPUDevice *device, app_state *state) {
    ForgeGltfScene *scene = &state->scene;
    int i;

    /* ── Upload vertex and index buffers per primitive ──────────────── */
    for (i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *prim = &scene->primitives[i];
        GpuPrimitive *gpu = &state->gpu_primitives[i];

        gpu->material_index = prim->material_index;
        gpu->has_uvs = prim->has_uvs;
        gpu->index_count = (Uint32)prim->index_count;
        gpu->index_type = (prim->index_stride == 4)
            ? SDL_GPU_INDEXELEMENTSIZE_32BIT
            : SDL_GPU_INDEXELEMENTSIZE_16BIT;

        /* ── Interleave into SceneVertex with tangent ──────────────── */
        /* The base parser stores positions/normals/UVs in ForgeGltfVertex
         * and tangents in a separate array.  We merge them into our
         * extended SceneVertex layout for GPU upload. */
        Uint32 vert_count = prim->vertex_count;
        SceneVertex *verts = (SceneVertex *)SDL_calloc(
            vert_count, sizeof(SceneVertex));
        if (!verts) {
            SDL_Log("Failed to allocate SceneVertex array for prim %d", i);
            state->gpu_primitive_count = i;
            return false;
        }

        {
            Uint32 v;
            for (v = 0; v < vert_count; v++) {
                verts[v].position = prim->vertices[v].position;
                verts[v].normal   = prim->vertices[v].normal;
                verts[v].uv       = prim->vertices[v].uv;

                if (prim->has_tangents && prim->tangents) {
                    /* Use the glTF-supplied tangent vectors directly.
                     * The model was authored with these tangents, and
                     * using them ensures correct normal mapping for
                     * mirrored UVs. */
                    verts[v].tangent = prim->tangents[v];
                } else {
                    /* Placeholder — will be overwritten below by
                     * Lengyel's method if indices are available. */
                    verts[v].tangent = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
                }
            }
        }

        /* If the model doesn't supply tangents, compute them using
         * Eric Lengyel's method from triangle edges and UV deltas. */
        if (!prim->has_tangents && prim->indices && prim->index_count > 0) {
            vec4 *computed = (vec4 *)SDL_calloc(vert_count, sizeof(vec4));
            if (computed) {
                compute_tangents_lengyel(
                    prim->vertices, vert_count,
                    prim->indices, prim->index_count,
                    prim->index_stride, computed);
                {
                    Uint32 v;
                    for (v = 0; v < vert_count; v++)
                        verts[v].tangent = computed[v];
                }
                SDL_free(computed);
            }
        }

        /* Vertex buffer */
        Uint32 vb_size = (Uint32)(vert_count * sizeof(SceneVertex));
        gpu->vertex_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_VERTEX,
            verts, vb_size);
        SDL_free(verts);
        if (!gpu->vertex_buffer) {
            SDL_Log("Failed to upload vertex buffer for primitive %d", i);
            state->gpu_primitive_count = i;
            return false;
        }

        /* Index buffer */
        Uint32 ib_size = (Uint32)(prim->index_count * prim->index_stride);
        gpu->index_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_INDEX,
            prim->indices, ib_size);
        if (!gpu->index_buffer) {
            SDL_Log("Failed to upload index buffer for primitive %d", i);
            SDL_ReleaseGPUBuffer(device, gpu->vertex_buffer);
            gpu->vertex_buffer = NULL;
            state->gpu_primitive_count = i;
            return false;
        }

        state->gpu_primitive_count = i + 1;
    }

    /* ── Load textures with deduplication ───────────────────────────── */
    /* Multiple materials can reference the same image file.  Cache by
     * path so we only load each texture once. */
    const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
    SDL_GPUTexture *loaded_tex[FORGE_GLTF_MAX_IMAGES];
    int loaded_count = 0;

    for (i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &state->gpu_materials[i];

        dst->base_color[0] = src->base_color[0];
        dst->base_color[1] = src->base_color[1];
        dst->base_color[2] = src->base_color[2];
        dst->base_color[3] = src->base_color[3];
        dst->has_texture = src->has_texture;
        dst->diffuse_texture = NULL;
        dst->has_normal_map = src->has_normal_map;
        dst->normal_texture = NULL;
        dst->double_sided = src->double_sided;

        /* Load diffuse (base color) texture */
        if (src->has_texture && src->texture_path[0] != '\0') {
            bool found = false;
            int j;
            for (j = 0; j < loaded_count; j++) {
                if (SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
                    dst->diffuse_texture = loaded_tex[j];
                    found = true;
                    break;
                }
            }
            if (!found && loaded_count < FORGE_GLTF_MAX_IMAGES) {
                dst->diffuse_texture = load_texture(device, src->texture_path);
                if (dst->diffuse_texture) {
                    loaded_paths[loaded_count] = src->texture_path;
                    loaded_tex[loaded_count] = dst->diffuse_texture;
                    state->loaded_textures[state->loaded_texture_count++] =
                        dst->diffuse_texture;
                    loaded_count++;
                } else {
                    SDL_Log("Diffuse texture load failed: %s", src->texture_path);
                    dst->has_texture = false;
                }
            }
        }

        /* Load normal map texture */
        if (src->has_normal_map && src->normal_map_path[0] != '\0') {
            bool found = false;
            int j;
            for (j = 0; j < loaded_count; j++) {
                if (SDL_strcmp(loaded_paths[j], src->normal_map_path) == 0) {
                    dst->normal_texture = loaded_tex[j];
                    found = true;
                    break;
                }
            }
            if (!found && loaded_count < FORGE_GLTF_MAX_IMAGES) {
                dst->normal_texture = load_texture(device,
                                                    src->normal_map_path);
                if (dst->normal_texture) {
                    loaded_paths[loaded_count] = src->normal_map_path;
                    loaded_tex[loaded_count] = dst->normal_texture;
                    state->loaded_textures[state->loaded_texture_count++] =
                        dst->normal_texture;
                    loaded_count++;
                } else {
                    SDL_Log("Normal map load failed: %s",
                            src->normal_map_path);
                    dst->has_normal_map = false;
                }
            }
        }
    }
    state->gpu_material_count = scene->material_count;

    return true;
}

/* ── Draw one primitive ──────────────────────────────────────────────── */

static void draw_primitive(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const app_state *state,
    const mat4 *vp,
    int node_index,
    int prim_index)
{
    const ForgeGltfNode *node = &state->scene.nodes[node_index];
    const GpuPrimitive *prim = &state->gpu_primitives[prim_index];

    /* Vertex uniforms: MVP + model matrix for world-space lighting. */
    VertUniforms vu;
    vu.model = node->world_transform;
    vu.mvp = mat4_multiply(*vp, vu.model);
    SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

    /* Fragment uniforms: base color + lighting + normal map parameters. */
    FragUniforms fu;
    SDL_GPUTexture *diffuse_tex = state->white_texture;
    SDL_GPUTexture *normal_tex = state->flat_normal_texture;

    if (prim->material_index >= 0 &&
        prim->material_index < state->gpu_material_count) {
        const GpuMaterial *mat = &state->gpu_materials[prim->material_index];
        fu.base_color[0] = mat->base_color[0];
        fu.base_color[1] = mat->base_color[1];
        fu.base_color[2] = mat->base_color[2];
        fu.base_color[3] = mat->base_color[3];
        fu.has_texture = (mat->has_texture && mat->diffuse_texture)
                         ? 1.0f : 0.0f;
        fu.has_normal_map = (mat->has_normal_map && mat->normal_texture)
                            ? 1.0f : 0.0f;
        if (mat->diffuse_texture) diffuse_tex = mat->diffuse_texture;
        if (mat->normal_texture)  normal_tex = mat->normal_texture;
    } else {
        fu.base_color[0] = 1.0f;
        fu.base_color[1] = 1.0f;
        fu.base_color[2] = 1.0f;
        fu.base_color[3] = 1.0f;
        fu.has_texture = 0.0f;
        fu.has_normal_map = 0.0f;
    }

    /* Blinn-Phong lighting uniforms */
    {
        vec3 light = vec3_normalize(
            vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
        fu.light_dir[0] = light.x;
        fu.light_dir[1] = light.y;
        fu.light_dir[2] = light.z;
        fu.light_dir[3] = 0.0f;
    }
    fu.eye_pos[0] = state->cam_position.x;
    fu.eye_pos[1] = state->cam_position.y;
    fu.eye_pos[2] = state->cam_position.z;
    fu.eye_pos[3] = 0.0f;
    fu.shininess = SHININESS;
    fu.ambient = AMBIENT_INTENSITY;
    fu.specular_str = SPECULAR_STRENGTH;
    fu.normal_mode = state->normal_mode;
    fu._pad0 = 0.0f;
    fu._pad1 = 0.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

    /* Bind diffuse texture + sampler (slot 0). */
    SDL_GPUTextureSamplerBinding tsb0;
    SDL_zero(tsb0);
    tsb0.texture = diffuse_tex;
    tsb0.sampler = state->sampler;

    /* Bind normal map + sampler (slot 1). */
    SDL_GPUTextureSamplerBinding tsb1;
    SDL_zero(tsb1);
    tsb1.texture = normal_tex;
    tsb1.sampler = state->sampler;

    SDL_GPUTextureSamplerBinding bindings[2] = { tsb0, tsb1 };
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

    /* Bind vertex + index buffers. */
    SDL_GPUBufferBinding vbb;
    SDL_zero(vbb);
    vbb.buffer = prim->vertex_buffer;
    SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);

    SDL_GPUBufferBinding ibb;
    SDL_zero(ibb);
    ibb.buffer = prim->index_buffer;
    SDL_BindGPUIndexBuffer(pass, &ibb, prim->index_type);

    SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
}

/* ══════════════════════════════════════════════════════════════════════
 * SDL Application Callbacks
 * ══════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppInit ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    /* ── 1. Initialise SDL ──────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ───────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true,  /* debug mode */
        NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 3. Create window ───────────────────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Claim the window for GPU rendering ──────────────────────── */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 5. Set up sRGB swapchain ───────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 6. Allocate application state ──────────────────────────────── */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window = window;
    state->device = device;
    state->normal_mode = NORMAL_MODE_MAPPED;  /* default: normal mapping on */

    /* ── 7. Load the glTF model ─────────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            SDL_free(state);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
        char gltf_path[PATH_BUFFER_SIZE];
        SDL_snprintf(gltf_path, sizeof(gltf_path), "%s%s", base, GLTF_PATH);

        if (!forge_gltf_load(gltf_path, &state->scene)) {
            SDL_Log("Failed to load glTF: %s", gltf_path);
            SDL_free(state);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }

        SDL_Log("Loaded glTF: %d nodes, %d meshes, %d primitives, "
                "%d materials",
                state->scene.node_count, state->scene.mesh_count,
                state->scene.primitive_count, state->scene.material_count);

        /* Report tangent availability. */
        {
            int pi;
            for (pi = 0; pi < state->scene.primitive_count; pi++) {
                if (state->scene.primitives[pi].has_tangents) {
                    SDL_Log("  Primitive %d: supplied tangent vectors (VEC4)", pi);
                } else {
                    SDL_Log("  Primitive %d: no tangents — computing via "
                            "Lengyel's method", pi);
                }
            }
        }

        /* Raise the scene above the grid floor. */
        {
            mat4 lift = mat4_translate(
                vec3_create(0.0f, SCENE_Y_OFFSET, 0.0f));
            int ni;
            for (ni = 0; ni < state->scene.node_count; ni++) {
                state->scene.nodes[ni].world_transform = mat4_multiply(
                    lift, state->scene.nodes[ni].world_transform);
            }
        }
    }

    /* ── 8. Create shaders ──────────────────────────────────────────── */
    SDL_GPUShader *scene_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil),
        VS_NUM_SAMPLERS, VS_NUM_STORAGE_TEXTURES,
        VS_NUM_STORAGE_BUFFERS, VS_NUM_UNIFORM_BUFFERS);
    if (!scene_vs) goto fail;

    SDL_GPUShader *scene_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_STORAGE_TEXTURES,
        FS_NUM_STORAGE_BUFFERS, FS_NUM_UNIFORM_BUFFERS);
    if (!scene_fs) { SDL_ReleaseGPUShader(device, scene_vs); goto fail; }

    /* ── 9. Define vertex layout for SceneVertex ───────────────────── */
    /* SceneVertex has 4 attributes: position, normal, uv, tangent.
     * The tangent (vec4) is the new addition for normal mapping. */
    SDL_GPUVertexBufferDescription vb_desc;
    SDL_zero(vb_desc);
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(SceneVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute attrs[4];
    SDL_zero(attrs);

    /* Location 0: position (float3) */
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = offsetof(SceneVertex, position);

    /* Location 1: normal (float3) */
    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = offsetof(SceneVertex, normal);

    /* Location 2: uv (float2) */
    attrs[2].location = 2;
    attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset = offsetof(SceneVertex, uv);

    /* Location 3: tangent (float4) — xyz direction + w handedness */
    attrs[3].location = 3;
    attrs[3].buffer_slot = 0;
    attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attrs[3].offset = offsetof(SceneVertex, tangent);

    /* ── 10. Create scene pipeline ──────────────────────────────────── */
    /* Depth-tested, back-face culling OFF (model is double-sided),
     * no blending needed — this model is fully opaque. */
    {
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = scene_vs;
        pipe.fragment_shader = scene_fs;
        pipe.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = attrs;
        pipe.vertex_input_state.num_vertex_attributes = 4;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->scene_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->scene_pipeline) {
            SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
            SDL_ReleaseGPUShader(device, scene_fs);
            SDL_ReleaseGPUShader(device, scene_vs);
            goto fail;
        }
    }

    /* Shaders are baked into pipelines — safe to release now. */
    SDL_ReleaseGPUShader(device, scene_fs);
    SDL_ReleaseGPUShader(device, scene_vs);

    /* ── 11. Create GRID pipeline ──────────────────────────────────── */
    {
        SDL_GPUShader *grid_vs = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_vert_spirv, sizeof(grid_vert_spirv),
            grid_vert_dxil, sizeof(grid_vert_dxil),
            GRID_VS_NUM_SAMPLERS, GRID_VS_NUM_STORAGE_TEXTURES,
            GRID_VS_NUM_STORAGE_BUFFERS, GRID_VS_NUM_UNIFORM_BUFFERS);
        if (!grid_vs) goto fail;

        SDL_GPUShader *grid_fs = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_frag_spirv, sizeof(grid_frag_spirv),
            grid_frag_dxil, sizeof(grid_frag_dxil),
            GRID_FS_NUM_SAMPLERS, GRID_FS_NUM_STORAGE_TEXTURES,
            GRID_FS_NUM_STORAGE_BUFFERS, GRID_FS_NUM_UNIFORM_BUFFERS);
        if (!grid_fs) {
            SDL_ReleaseGPUShader(device, grid_vs);
            goto fail;
        }

        SDL_GPUVertexBufferDescription grid_vb_desc;
        SDL_zero(grid_vb_desc);
        grid_vb_desc.slot = 0;
        grid_vb_desc.pitch = GRID_VERTEX_PITCH;
        grid_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        grid_vb_desc.instance_step_rate = 0;

        SDL_GPUVertexAttribute grid_attr;
        SDL_zero(grid_attr);
        grid_attr.location = 0;
        grid_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        grid_attr.offset = 0;

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = grid_vs;
        pipe.fragment_shader = grid_fs;
        pipe.vertex_input_state.vertex_buffer_descriptions = &grid_vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = &grid_attr;
        pipe.vertex_input_state.num_vertex_attributes = 1;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->grid_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        SDL_ReleaseGPUShader(device, grid_fs);
        SDL_ReleaseGPUShader(device, grid_vs);
        if (!state->grid_pipeline) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto fail;
        }
    }

    /* ── 12. Upload grid geometry ──────────────────────────────────── */
    {
        float grid_verts[GRID_NUM_VERTS * 3] = {
            -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
            -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
        };
        Uint16 grid_indices[GRID_NUM_INDICES] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vertex_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_VERTEX,
            grid_verts, sizeof(grid_verts));
        if (!state->grid_vertex_buffer) goto fail;

        state->grid_index_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_INDEX,
            grid_indices, sizeof(grid_indices));
        if (!state->grid_index_buffer) goto fail;
    }

    /* ── 13. Create sampler ─────────────────────────────────────────── */
    {
        SDL_GPUSamplerCreateInfo sci;
        SDL_zero(sci);
        sci.min_filter = SDL_GPU_FILTER_LINEAR;
        sci.mag_filter = SDL_GPU_FILTER_LINEAR;
        sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sci.max_lod = MAX_LOD;

        state->sampler = SDL_CreateGPUSampler(device, &sci);
        if (!state->sampler) {
            SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
            goto fail;
        }
    }

    /* ── 14. Create placeholder textures ─────────────────────────────── */

    /* White 1×1 texture for materials without a diffuse texture. */
    state->white_texture = create_1x1_texture(device, 255, 255, 255, 255);
    if (!state->white_texture) {
        SDL_Log("Failed to create white placeholder texture");
        goto fail;
    }

    /* Flat normal map: (128, 128, 255) encodes tangent-space normal (0,0,1)
     * — pointing straight outward, producing no surface perturbation.
     * Used for materials that don't have a normal map assigned. */
    state->flat_normal_texture = create_1x1_texture(device, 128, 128, 255, 255);
    if (!state->flat_normal_texture) {
        SDL_Log("Failed to create flat normal map placeholder");
        goto fail;
    }

    /* ── 15. Upload scene to GPU ────────────────────────────────────── */
    if (!upload_scene_to_gpu(device, state)) {
        SDL_Log("Failed to upload scene to GPU");
        goto fail;
    }

    /* ── 16. Create depth texture ───────────────────────────────────── */
    {
        int win_w, win_h;
        if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
            SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
            goto fail;
        }

        SDL_GPUTextureCreateInfo dci;
        SDL_zero(dci);
        dci.type = SDL_GPU_TEXTURETYPE_2D;
        dci.format = DEPTH_FORMAT;
        dci.width = (Uint32)win_w;
        dci.height = (Uint32)win_h;
        dci.layer_count_or_depth = 1;
        dci.num_levels = 1;
        dci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->depth_texture = SDL_CreateGPUTexture(device, &dci);
        if (!state->depth_texture) {
            SDL_Log("SDL_CreateGPUTexture (depth) failed: %s",
                    SDL_GetError());
            goto fail;
        }
        state->depth_width = (Uint32)win_w;
        state->depth_height = (Uint32)win_h;
    }

    /* ── 17. Camera initial state ───────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_START_YAW * (FORGE_PI / 180.0f);
    state->cam_pitch = CAM_START_PITCH * (FORGE_PI / 180.0f);
    state->last_ticks = SDL_GetPerformanceCounter();
    state->mouse_captured = false;

    /* ── 18. Capture mouse ──────────────────────────────────────────── */
    if (SDL_SetWindowRelativeMouseMode(window, true)) {
        state->mouse_captured = true;
    }

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            goto fail;
        }
    }
#endif

    *appstate = state;
    SDL_Log("Initialization complete — normal mapping active (press 1/2/3 "
            "to toggle flat/smooth/normal-mapped)");
    return SDL_APP_CONTINUE;

fail:
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->scene_pipeline);
    if (state->sampler) SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->flat_normal_texture)
        SDL_ReleaseGPUTexture(device, state->flat_normal_texture);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    {
        int ci;
        for (ci = 0; ci < state->loaded_texture_count; ci++)
            SDL_ReleaseGPUTexture(device, state->loaded_textures[ci]);
        for (ci = 0; ci < state->gpu_primitive_count; ci++) {
            if (state->gpu_primitives[ci].vertex_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[ci].vertex_buffer);
            if (state->gpu_primitives[ci].index_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[ci].index_buffer);
        }
    }
    forge_gltf_free(&state->scene);
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            if (state->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                }
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        /* Normal mode toggle: 1 = flat, 2 = per-vertex, 3 = normal-mapped */
        if (event->key.key == SDLK_1) {
            state->normal_mode = NORMAL_MODE_FLAT;
            SDL_Log("Normal mode: FLAT (face normals from ddx/ddy)");
        }
        if (event->key.key == SDLK_2) {
            state->normal_mode = NORMAL_MODE_VERTEX;
            SDL_Log("Normal mode: PER-VERTEX (smooth interpolated normals)");
        }
        if (event->key.key == SDLK_3) {
            state->normal_mode = NORMAL_MODE_MAPPED;
            SDL_Log("Normal mode: NORMAL MAPPED (tangent-space perturbation)");
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            if (SDL_SetWindowRelativeMouseMode(state->window, true)) {
                state->mouse_captured = true;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            float max_pitch = MAX_PITCH_DEG * (FORGE_PI / 180.0f);
            if (state->cam_pitch > max_pitch) state->cam_pitch = max_pitch;
            if (state->cam_pitch < -max_pitch) state->cam_pitch = -max_pitch;
        }
        break;

    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate) {
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Delta time ──────────────────────────────────────────────────── */
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - state->last_ticks) /
               (float)SDL_GetPerformanceFrequency();
    state->last_ticks = now;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── Camera movement ─────────────────────────────────────────────── */
    {
        quat orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
        vec3 forward = quat_forward(orient);
        vec3 right   = quat_right(orient);
        vec3 up      = vec3_create(0.0f, 1.0f, 0.0f);

        const bool *keys = SDL_GetKeyboardState(NULL);
        float speed = MOVE_SPEED * dt;

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_SPACE])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(up, speed));
        if (keys[SDL_SCANCODE_LSHIFT])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(up, speed));
    }

    /* ── Acquire swapchain texture ───────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw_w, sw_h;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain_tex, &sw_w, &sw_h)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }
    if (!swapchain_tex) {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ── Resize depth buffer if needed ───────────────────────────────── */
    if (sw_w != state->depth_width || sw_h != state->depth_height) {
        if (state->depth_texture)
            SDL_ReleaseGPUTexture(device, state->depth_texture);

        SDL_GPUTextureCreateInfo dci;
        SDL_zero(dci);
        dci.type = SDL_GPU_TEXTURETYPE_2D;
        dci.format = DEPTH_FORMAT;
        dci.width = sw_w;
        dci.height = sw_h;
        dci.layer_count_or_depth = 1;
        dci.num_levels = 1;
        dci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->depth_texture = SDL_CreateGPUTexture(device, &dci);
        if (!state->depth_texture) {
            SDL_Log("SDL_CreateGPUTexture (depth resize) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }
        state->depth_width = sw_w;
        state->depth_height = sw_h;
    }

    /* ── Build camera matrices ───────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)sw_w / (float)sw_h;
    mat4 proj = mat4_perspective(
        FOV_DEG * (FORGE_PI / 180.0f), aspect, NEAR_PLANE, FAR_PLANE);
    mat4 vp = mat4_multiply(proj, view);

    /* ── Begin render pass ───────────────────────────────────────────── */
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = swapchain_tex;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color.r = CLEAR_R;
    color_target.clear_color.g = CLEAR_G;
    color_target.clear_color.b = CLEAR_B;
    color_target.clear_color.a = CLEAR_A;

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture = state->depth_texture;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.clear_depth = DEPTH_CLEAR;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
        cmd, &color_target, 1, &depth_target);
    if (!pass) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    SDL_SetGPUViewport(pass, &(SDL_GPUViewport){
        0, 0, (float)sw_w, (float)sw_h, 0.0f, 1.0f });
    SDL_SetGPUScissor(pass, &(SDL_Rect){ 0, 0, (int)sw_w, (int)sw_h });

    /* ── Render grid floor ───────────────────────────────────────────── */
    {
        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

        /* Vertex uniform: VP matrix (no model — grid is at origin) */
        SDL_PushGPUVertexUniformData(cmd, 0, &vp, sizeof(vp));

        /* Fragment uniform: grid appearance + lighting parameters */
        GridFragUniforms gfu;
        gfu.line_color[0] = GRID_LINE_R;
        gfu.line_color[1] = GRID_LINE_G;
        gfu.line_color[2] = GRID_LINE_B;
        gfu.line_color[3] = GRID_LINE_A;
        gfu.bg_color[0]   = GRID_BG_R;
        gfu.bg_color[1]   = GRID_BG_G;
        gfu.bg_color[2]   = GRID_BG_B;
        gfu.bg_color[3]   = GRID_BG_A;
        {
            vec3 light = vec3_normalize(
                vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
            gfu.light_dir[0] = light.x;
            gfu.light_dir[1] = light.y;
            gfu.light_dir[2] = light.z;
            gfu.light_dir[3] = 0.0f;
        }
        gfu.eye_pos[0]     = state->cam_position.x;
        gfu.eye_pos[1]     = state->cam_position.y;
        gfu.eye_pos[2]     = state->cam_position.z;
        gfu.eye_pos[3]     = 0.0f;
        gfu.grid_spacing   = GRID_SPACING;
        gfu.line_width     = GRID_LINE_WIDTH;
        gfu.fade_distance  = GRID_FADE_DIST;
        gfu.ambient        = AMBIENT_INTENSITY;
        gfu.shininess      = SHININESS;
        gfu.specular_str   = SPECULAR_STRENGTH;
        gfu._pad0          = 0.0f;
        gfu._pad1          = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

        SDL_GPUBufferBinding gvb;
        SDL_zero(gvb);
        gvb.buffer = state->grid_vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &gvb, 1);

        SDL_GPUBufferBinding gib;
        SDL_zero(gib);
        gib.buffer = state->grid_index_buffer;
        SDL_BindGPUIndexBuffer(pass, &gib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES, 1, 0, 0, 0);
    }

    /* ── Render scene ────────────────────────────────────────────────── */
    {
        SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);
        int ni;
        for (ni = 0; ni < state->scene.node_count; ni++) {
            const ForgeGltfNode *node = &state->scene.nodes[ni];
            if (node->mesh_index < 0) continue;

            const ForgeGltfMesh *mesh =
                &state->scene.meshes[node->mesh_index];
            int pi;
            for (pi = 0; pi < mesh->primitive_count; pi++) {
                int gi = mesh->first_primitive + pi;
                draw_primitive(pass, cmd, state, &vp, ni, gi);
            }
        }
    }

    /* ── End render pass ─────────────────────────────────────────────── */
    SDL_EndGPURenderPass(pass);

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE && swapchain_tex) {
        if (forge_capture_finish_frame(&state->capture, cmd, swapchain_tex)) {
            if (forge_capture_should_quit(&state->capture)) {
                return SDL_APP_SUCCESS;
            }
            return SDL_APP_CONTINUE;
        }
    }
#endif

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    app_state *state = (app_state *)appstate;
    (void)result;

    if (!state) return;

    SDL_GPUDevice *device = state->device;

    /* Wait for GPU to finish all pending work before releasing. */
    if (!SDL_WaitForGPUIdle(device)) {
        SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* Release in reverse order of creation. */
    {
        int i;
        for (i = 0; i < state->gpu_primitive_count; i++) {
            if (state->gpu_primitives[i].vertex_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[i].vertex_buffer);
            if (state->gpu_primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[i].index_buffer);
        }
        for (i = 0; i < state->loaded_texture_count; i++)
            SDL_ReleaseGPUTexture(device, state->loaded_textures[i]);
    }

    if (state->flat_normal_texture)
        SDL_ReleaseGPUTexture(device, state->flat_normal_texture);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->sampler)
        SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->scene_pipeline);

    forge_gltf_free(&state->scene);

    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
}
