/*
 * GPU Lesson 33 — Vertex Pulling
 *
 * Demonstrates programmable vertex fetch using storage buffers instead of the
 * traditional fixed-function vertex input assembler.
 *
 * In conventional rendering, the pipeline declares vertex attributes (position,
 * normal, UV) and the GPU's input assembler reads them from vertex buffers
 * before the vertex shader runs.  Vertex pulling removes this coupling:
 * the pipeline declares NO vertex input, and the vertex shader manually reads
 * vertex data from a StructuredBuffer using SV_VertexID.
 *
 * This technique offers several advantages:
 *   - Vertex format changes don't require pipeline rebuilds
 *   - Multiple vertex formats can share a single pipeline
 *   - Compute shaders can write directly into the same storage buffer
 *   - Compressed or packed vertex formats become straightforward to decode
 *
 * Scene layout:
 *   - CesiumMilkTruck loaded via glTF, rendered with vertex pulling
 *   - 8 textured boxes (BoxTextured) scattered around the truck, also pulled
 *   - Procedural anti-aliased grid floor (traditional vertex input)
 *   - Blinn-Phong lighting with directional shadow map
 *
 * Architecture — 2 render passes per frame:
 *   1. Shadow pass   — depth-only, vertex-pulled mesh into shadow map
 *   2. Scene pass    — vertex-pulled meshes with lighting + shadows, then grid
 *
 * Controls:
 *   WASD / Space / LShift — Move camera
 *   Mouse                  — Look around
 *   Escape                 — Release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */
#define SDL_MAIN_USE_CALLBACKS 1

#include "gltf/forge_gltf.h"
#include "math/forge_math.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecodes ──────────────────────────────────────── */

#include "shaders/compiled/pulled_vert_spirv.h"
#include "shaders/compiled/pulled_vert_dxil.h"
#include "shaders/compiled/pulled_vert_msl.h"
#include "shaders/compiled/pulled_frag_spirv.h"
#include "shaders/compiled/pulled_frag_dxil.h"
#include "shaders/compiled/pulled_frag_msl.h"

#include "shaders/compiled/shadow_pulled_vert_spirv.h"
#include "shaders/compiled/shadow_pulled_vert_dxil.h"
#include "shaders/compiled/shadow_pulled_vert_msl.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"
#include "shaders/compiled/shadow_frag_msl.h"

#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_vert_msl.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"
#include "shaders/compiled/grid_frag_msl.h"

/* ── Constants ──────────────────────────────────────────────────────── */

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Camera. */
#define FOV_DEG            60
#define NEAR_PLANE         0.1f
#define FAR_PLANE          100.0f
#define CAM_SPEED          3.0f
#define MOUSE_SENS         0.003f
#define PITCH_CLAMP        1.5f

/* Camera initial position — slightly elevated, looking at the truck. */
#define CAM_START_X         4.0f
#define CAM_START_Y         2.5f
#define CAM_START_Z        -4.0f
#define CAM_START_YAW_DEG   145.0f
#define CAM_START_PITCH_DEG -15.0f

/* Directional light — from above-right. */
#define LIGHT_DIR_X     -0.4f
#define LIGHT_DIR_Y     -0.8f
#define LIGHT_DIR_Z     -0.4f
#define LIGHT_INTENSITY  0.9f
#define LIGHT_COLOR_R    1.0f
#define LIGHT_COLOR_G    0.95f
#define LIGHT_COLOR_B    0.85f

/* Scene material defaults. */
#define MATERIAL_AMBIENT      0.25f
#define MATERIAL_SHININESS    32.0f
#define MATERIAL_SPECULAR_STR 0.4f

/* Shadow map. */
#define SHADOW_MAP_SIZE   2048
#define SHADOW_DEPTH_FMT  SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_ORTHO_SIZE 8.0f
#define SHADOW_NEAR       0.1f
#define SHADOW_FAR        30.0f
#define LIGHT_DISTANCE    12.0f

/* Grid floor. */
#define GRID_HALF_SIZE    20.0f
#define GRID_Y            0.0f
#define GRID_SPACING      1.0f
#define GRID_LINE_WIDTH   0.02f
#define GRID_FADE_DIST    30.0f
#define GRID_AMBIENT      0.3f
#define GRID_INDEX_COUNT  6
#define GRID_LINE_GRAY    0.4f
#define GRID_BG_GRAY      0.25f

/* Texture. */
#define BYTES_PER_PIXEL   4
#define MAX_ANISOTROPY    8
#define MAX_LOD_UNLIMITED 1000.0f

/* Clear color — light grey-blue. */
#define CLEAR_R 0.6f
#define CLEAR_G 0.7f
#define CLEAR_B 0.8f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* Model asset paths (relative to executable). */
#define TRUCK_MODEL_PATH "assets/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH   "assets/BoxTextured/BoxTextured.gltf"

/* Light target (world-space point the shadow camera looks at). */
#define LIGHT_TARGET_Y    1.0f

/* Light direction degeneracy threshold. */
#define PARALLEL_THRESHOLD 0.99f

/* Path buffer. */
#define PATH_BUFFER_SIZE 512

/* Box layout — 8 boxes scattered around the truck. */
#define BOX_COUNT 8

/* ── Vertex layout for pulled meshes ─────────────────────────────────
 * This struct matches the PulledVertex in the HLSL StructuredBuffer.
 * Unlike traditional rendering where the GPU input assembler reads this
 * layout, here the vertex shader reads it manually from a storage buffer. */

typedef struct PulledVertex {
    vec3 position;   /* 12 bytes — world-space (or model-space) position */
    vec3 normal;     /* 12 bytes — surface normal for lighting           */
    vec2 uv;         /*  8 bytes — texture coordinates                   */
} PulledVertex;      /* 32 bytes total — clean alignment, no padding     */

/* Grid floor vertex — position only, uses traditional vertex input. */
typedef struct GridVertex {
    vec3 position;  /* world-space corner of the grid quad */
} GridVertex;

/* ── Uniform structures ─────────────────────────────────────────────── */

/* Scene vertex uniforms — slot 0. */
typedef struct SceneVertUniforms {
    mat4 mvp;      /* model-view-projection matrix                    */
    mat4 model;    /* model (world) matrix — for world-space normals  */
    mat4 light_vp; /* light VP — for shadow map projection            */
} SceneVertUniforms;

/* Shadow pass vertex uniforms — slot 0. */
typedef struct ShadowVertUniforms {
    mat4 light_vp; /* light view-projection matrix */
} ShadowVertUniforms;

/* Scene fragment uniforms. */
typedef struct SceneFragUniforms {
    float base_color[4];    /* material RGBA color                          */
    float eye_pos[3];       /* camera world-space position (for specular)   */
    float has_texture;      /* >0.5 = sample diffuse texture                */
    float ambient;          /* ambient light intensity [0..1]               */
    float shininess;        /* specular exponent (higher = tighter)         */
    float specular_str;     /* specular intensity multiplier [0..1]         */
    float _pad0;            /* 16-byte alignment padding                    */
    float light_dir[4];     /* directional light direction (xyz, w unused)  */
    float light_color[3];   /* directional light RGB color                  */
    float light_intensity;  /* directional light brightness multiplier      */
} SceneFragUniforms;

/* Grid vertex uniforms — VP + light VP for shadow sampling. */
typedef struct GridVertUniforms {
    mat4 vp;       /* camera view-projection matrix          */
    mat4 light_vp; /* light view-projection for shadow UVs   */
} GridVertUniforms;

/* Grid fragment uniforms. */
typedef struct GridFragUniforms {
    float line_color[4];    /* grid line RGBA color                        */
    float bg_color[4];      /* background surface RGBA color               */
    float light_dir[3];     /* world-space directional light direction     */
    float light_intensity;  /* light brightness multiplier                 */
    float eye_pos[3];       /* world-space camera position (for fade)      */
    float grid_spacing;     /* world units between grid lines (e.g. 1.0)  */
    float line_width;       /* line thickness in grid-space [0..0.5]      */
    float fade_distance;    /* distance where grid dissolves (meters)     */
    float ambient;          /* ambient light intensity [0..1]             */
    float _pad;             /* 16-byte alignment padding                  */
} GridFragUniforms;

/* ── GPU-side model data ────────────────────────────────────────────── */

/* A GPU primitive using vertex pulling — storage buffer instead of vertex buffer.
 * The key difference from traditional GpuPrimitive: vertex data lives in a
 * storage buffer (GRAPHICS_STORAGE_READ) rather than a vertex buffer (VERTEX). */
typedef struct PulledPrimitive {
    SDL_GPUBuffer *storage_buffer;         /* vertex data as storage buffer   */
    SDL_GPUBuffer *index_buffer;           /* index data (16 or 32 bit)       */
    Uint32 vertex_count;                   /* number of vertices              */
    Uint32 index_count;                    /* number of indices to draw       */
    int material_index;                    /* index into materials[], -1=none */
    SDL_GPUIndexElementSize index_type;    /* 16BIT or 32BIT                  */
    bool has_uvs;                          /* true if primitive has UV coords */
} PulledPrimitive;

typedef struct GpuMaterial {
    float base_color[4];                   /* material RGBA color             */
    SDL_GPUTexture *texture;               /* diffuse texture (NULL=white)    */
    bool has_texture;                      /* true if texture was loaded      */
} GpuMaterial;

/* A scene object — one model instance with its own transform. */
typedef struct SceneObject {
    PulledPrimitive *primitives;   /* array of pulled primitives         */
    int primitive_count;           /* number of primitives in this model */
    GpuMaterial *materials;        /* materials for this model           */
    int material_count;            /* number of materials                */
    mat4 model_matrix;             /* world transform for this instance  */
} SceneObject;

/* ── Application state ──────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;              /* main application window           */
    SDL_GPUDevice *device;              /* GPU device handle                 */

    /* Pipelines.
     * The pulled pipeline has NO vertex input state — the vertex shader
     * reads data from a storage buffer instead of vertex attributes.
     * This is the core of vertex pulling: decoupling format from pipeline. */
    SDL_GPUGraphicsPipeline *pulled_pipeline;   /* vertex-pulled Blinn-Phong */
    SDL_GPUGraphicsPipeline *shadow_pipeline;   /* vertex-pulled shadow depth */
    SDL_GPUGraphicsPipeline *grid_pipeline;     /* traditional grid floor     */

    /* Render targets. */
    SDL_GPUTexture *shadow_depth;   /* D32 depth (SHADOW_MAP_SIZE^2)       */
    SDL_GPUTexture *main_depth;     /* D32 depth (window-sized)            */

    /* Samplers. */
    SDL_GPUSampler *sampler;        /* trilinear + aniso (diffuse)         */
    SDL_GPUSampler *nearest_clamp;  /* nearest + clamp (shadow)            */

    /* Scene objects. */
    SDL_GPUTexture *white_texture;  /* 1x1 white fallback texture          */

    /* Truck model (CesiumMilkTruck). */
    ForgeGltfScene truck_scene;     /* parsed glTF data                    */
    ForgeArena     truck_arena;     /* arena backing truck glTF allocs     */
    SceneObject    truck;           /* GPU data + world transform          */

    /* Box model (BoxTextured) — 1 loaded model, instanced as BOX_COUNT. */
    ForgeGltfScene box_scene;       /* parsed glTF data                    */
    ForgeArena     box_arena;       /* arena backing box glTF allocs       */
    SceneObject    boxes[BOX_COUNT]; /* each box has its own transform     */

    /* Grid floor geometry. */
    SDL_GPUBuffer *grid_vb;         /* grid vertex buffer (4 corners)      */
    SDL_GPUBuffer *grid_ib;         /* grid index buffer (2 triangles)     */

    /* Light. */
    mat4 light_vp;                  /* light view-projection for shadows   */

    /* Swapchain format. */
    SDL_GPUTextureFormat swapchain_format; /* queried after sRGB setup     */

    /* Camera. */
    vec3  cam_position;             /* world-space camera position         */
    float cam_yaw;                  /* horizontal rotation (radians)       */
    float cam_pitch;                /* vertical rotation (radians, ±1.5)   */

    /* Timing and input. */
    Uint64 last_ticks;              /* previous frame timestamp (ticks)    */
    bool   mouse_captured;          /* true when mouse is in relative mode */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;           /* screenshot infrastructure           */
#endif
} app_state;

/* ── Helper: create shader from embedded bytecode ───────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
    const char *msl_code, unsigned int msl_size,
    Uint32 num_samplers,
    Uint32 num_storage_buffers,
    Uint32 num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage               = stage;
    info.entrypoint          = "main";
    info.num_samplers        = num_samplers;
    info.num_storage_buffers = num_storage_buffers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL) && msl_code && msl_size > 0) {
        info.format     = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
        info.code       = (const unsigned char *)msl_code;
        info.code_size  = msl_size;
    } else {
        SDL_Log("No supported shader format available");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("Failed to create shader: %s", SDL_GetError());
    }
    return shader;
}

/* ── Helper: upload data to a GPU buffer ────────────────────────────── */

static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage,
    const void *data, Uint32 size)
{
    SDL_GPUBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = usage;
    buf_info.size  = size;

    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
    if (!buffer) {
        SDL_Log("Failed to create GPU buffer: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = size;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer for upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass for upload: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size   = size;

    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit upload command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return buffer;
}

/* ── Helper: load texture from file ─────────────────────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert surface: %s", SDL_GetError());
        return NULL;
    }

    Uint32 w = (Uint32)converted->w;
    Uint32 h = (Uint32)converted->h;
    Uint32 max_dim = w > h ? w : h;
    Uint32 mip_levels = (Uint32)(forge_log2f((float)max_dim)) + 1;

    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.width               = w;
    tex_info.height              = h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = mip_levels;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                   SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    Uint32 row_bytes   = w * BYTES_PER_PIXEL;
    Uint32 total_bytes = w * h * BYTES_PER_PIXEL;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        SDL_DestroySurface(converted);
        return NULL;
    }

    {
        const Uint8 *row_src = (const Uint8 *)converted->pixels;
        Uint8 *row_dst = (Uint8 *)mapped;
        for (Uint32 row = 0; row < h; row++) {
            SDL_memcpy(row_dst + row * row_bytes,
                       row_src + row * converted->pitch, row_bytes);
        }
    }
    SDL_UnmapGPUTransferBuffer(device, xfer);
    SDL_DestroySurface(converted);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass for texture upload: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w       = w;
    dst.h       = h;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_GenerateMipmapsForGPUTexture(cmd, tex);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Helper: 1x1 white placeholder texture ──────────────────────────── */

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device)
{
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.width               = 1;
    tex_info.height              = 1;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = 1;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create white texture: %s", SDL_GetError());
        return NULL;
    }

    Uint8 white[4] = { 255, 255, 255, 255 };

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = sizeof(white);

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create white texture xfer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map white texture xfer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(mapped, white, sizeof(white));
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for white texture: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass for white texture: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w       = 1;
    dst.h       = 1;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit white texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Upload model as vertex-pulled primitives ───────────────────────── */

/* The key function that distinguishes this lesson: vertex data is uploaded
 * to a STORAGE buffer (GRAPHICS_STORAGE_READ) instead of a VERTEX buffer.
 * The vertex shader will read from this buffer using SV_VertexID.
 *
 * Traditional approach:  upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, ...)
 * Vertex pulling:        upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, ...)
 *
 * That one flag change is the entire CPU-side difference. */

static bool upload_pulled_model(SDL_GPUDevice *device,
                                const ForgeGltfScene *scene,
                                SceneObject *obj)
{
    obj->primitive_count = scene->primitive_count;
    obj->primitives = (PulledPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(PulledPrimitive));
    if (!obj->primitives) {
        SDL_Log("Failed to allocate pulled primitives");
        return false;
    }

    /* First pass: upload all primitive GPU data (storage + index buffers). */
    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *src = &scene->primitives[i];
        PulledPrimitive *dst = &obj->primitives[i];

        dst->material_index  = src->material_index;
        dst->index_count     = src->index_count;
        dst->vertex_count    = src->vertex_count;
        dst->has_uvs         = src->has_uvs;

        /* Build PulledVertex array from glTF vertex data. */
        if (src->vertices && src->vertex_count > 0) {
            PulledVertex *verts = (PulledVertex *)SDL_calloc(
                (size_t)src->vertex_count, sizeof(PulledVertex));
            if (!verts) {
                SDL_Log("Failed to allocate pulled vertices");
                return false;
            }

            for (Uint32 v = 0; v < src->vertex_count; v++) {
                verts[v].position = src->vertices[v].position;
                verts[v].normal   = src->vertices[v].normal;
                verts[v].uv       = src->vertices[v].uv;
            }

            /* Upload as GRAPHICS_STORAGE_READ — this is the vertex pulling
             * difference.  The buffer will be bound with
             * SDL_BindGPUVertexStorageBuffers instead of SDL_BindGPUVertexBuffers. */
            Uint32 vb_size = src->vertex_count * (Uint32)sizeof(PulledVertex);
            dst->storage_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                verts, vb_size);
            SDL_free(verts);

            if (!dst->storage_buffer) return false;
        }

        /* Upload index buffer (unchanged from traditional rendering). */
        if (src->indices && src->index_count > 0) {
            if (src->index_stride != 2 && src->index_stride != 4) {
                SDL_Log("Unsupported index stride %u", (unsigned)src->index_stride);
                return false;
            }
            Uint32 ib_size = src->index_count * src->index_stride;
            dst->index_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_INDEX, src->indices, ib_size);
            if (!dst->index_buffer) return false;
            dst->index_type = (src->index_stride == 2)
                ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                : SDL_GPU_INDEXELEMENTSIZE_32BIT;
        }
    }

    /* Load materials and textures. */
    obj->material_count = scene->material_count;
    obj->materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!obj->materials) {
        SDL_Log("Failed to allocate GPU materials");
        return false;
    }

    for (int i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &obj->materials[i];

        dst->base_color[0] = src->base_color[0];
        dst->base_color[1] = src->base_color[1];
        dst->base_color[2] = src->base_color[2];
        dst->base_color[3] = src->base_color[3];
        dst->has_texture   = src->has_texture;
        dst->texture       = NULL;

        if (src->has_texture && src->texture_path[0] != '\0') {
            dst->texture = load_texture(device, src->texture_path);
            if (!dst->texture) {
                dst->has_texture = false;
            }
        }
    }

    return true;
}

/* ── Draw all primitives of a scene object (vertex-pulled) ──────────── */

/* Draw a single pulled primitive — binds the storage buffer, index buffer,
 * and issues the draw call.  Extracted so both the node-walking and simple
 * (single-node) paths can share it. */

static void draw_pulled_primitive(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const app_state *state,
    const SceneObject *obj,
    const PulledPrimitive *prim,
    const mat4 *world,
    const mat4 *cam_vp,
    bool shadow_pass)
{
    if (!prim->storage_buffer || !prim->index_buffer) return;

    /* Push uniforms for this draw. */
    if (shadow_pass) {
        ShadowVertUniforms shadow_u;
        shadow_u.light_vp = mat4_multiply(state->light_vp, *world);
        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_u, sizeof(shadow_u));
    } else {
        SceneVertUniforms scene_vu;
        scene_vu.mvp      = mat4_multiply(*cam_vp, *world);
        scene_vu.model    = *world;
        scene_vu.light_vp = mat4_multiply(state->light_vp, *world);
        SDL_PushGPUVertexUniformData(cmd, 0, &scene_vu, sizeof(scene_vu));

        /* Fragment uniforms: material + lighting. */
        SDL_GPUTexture *tex = state->white_texture;
        SceneFragUniforms frag_u;
        SDL_zero(frag_u);

        if (prim->material_index >= 0 &&
            prim->material_index < obj->material_count) {
            const GpuMaterial *mat = &obj->materials[prim->material_index];
            frag_u.base_color[0] = mat->base_color[0];
            frag_u.base_color[1] = mat->base_color[1];
            frag_u.base_color[2] = mat->base_color[2];
            frag_u.base_color[3] = mat->base_color[3];
            const bool use_tex = mat->has_texture && prim->has_uvs;
            frag_u.has_texture = use_tex ? 1.0f : 0.0f;
            if (use_tex && mat->texture) tex = mat->texture;
        } else {
            frag_u.base_color[0] = 1.0f;
            frag_u.base_color[1] = 1.0f;
            frag_u.base_color[2] = 1.0f;
            frag_u.base_color[3] = 1.0f;
            frag_u.has_texture   = 0.0f;
        }

        frag_u.eye_pos[0]       = state->cam_position.x;
        frag_u.eye_pos[1]       = state->cam_position.y;
        frag_u.eye_pos[2]       = state->cam_position.z;
        frag_u.ambient          = MATERIAL_AMBIENT;
        frag_u.shininess        = MATERIAL_SHININESS;
        frag_u.specular_str     = MATERIAL_SPECULAR_STR;
        frag_u.light_dir[0]     = LIGHT_DIR_X;
        frag_u.light_dir[1]     = LIGHT_DIR_Y;
        frag_u.light_dir[2]     = LIGHT_DIR_Z;
        frag_u.light_dir[3]     = 0.0f;
        frag_u.light_color[0]   = LIGHT_COLOR_R;
        frag_u.light_color[1]   = LIGHT_COLOR_G;
        frag_u.light_color[2]   = LIGHT_COLOR_B;
        frag_u.light_intensity  = LIGHT_INTENSITY;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        /* Bind diffuse (slot 0) and shadow (slot 1) textures. */
        SDL_GPUTextureSamplerBinding tex_binds[2];
        tex_binds[0] = (SDL_GPUTextureSamplerBinding){
            .texture = tex, .sampler = state->sampler };
        tex_binds[1] = (SDL_GPUTextureSamplerBinding){
            .texture = state->shadow_depth,
            .sampler = state->nearest_clamp };
        SDL_BindGPUFragmentSamplers(pass, 0, tex_binds, 2);
    }

    /* VERTEX PULLING: bind the storage buffer instead of a vertex buffer.
     * SDL_BindGPUVertexStorageBuffers maps to register(t[n], space0) in HLSL.
     * The vertex shader reads from this buffer using SV_VertexID. */
    SDL_GPUBuffer *storage_bufs[1] = { prim->storage_buffer };
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_bufs, 1);

    /* Index buffer binding is unchanged — vertex pulling only affects
     * how vertex attributes are fetched, not how indices work. */
    SDL_GPUBufferBinding ib = { prim->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, prim->index_type);
    SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
}

/* Draw all primitives of a scene object by walking the glTF node hierarchy.
 *
 * This is the correct approach for models like CesiumMilkTruck where multiple
 * nodes (cab, body, front wheels, rear wheels) reference different meshes —
 * or even the same mesh with different transforms.  Each node's world_transform
 * is composed with the object's model_matrix to produce the final transform.
 *
 * Compare with traditional rendering (lesson 09) where we would call
 * SDL_BindGPUVertexBuffers — here we call SDL_BindGPUVertexStorageBuffers
 * instead, but the node iteration pattern is identical. */

static void draw_pulled_object(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const app_state *state,
    const SceneObject *obj,
    const ForgeGltfScene *scene,
    const mat4 *cam_vp,
    bool shadow_pass)
{
    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        /* Each node has its own world transform — compose with the object's
         * placement matrix so instances (like the boxes) can be repositioned. */
        mat4 world = mat4_multiply(obj->model_matrix, node->world_transform);

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int idx = mesh->first_primitive + pi;
            if (idx < 0 || idx >= obj->primitive_count) continue;
            draw_pulled_primitive(pass, cmd, state, obj,
                                  &obj->primitives[idx], &world,
                                  cam_vp, shadow_pass);
        }
    }
}

/* ── Build box transforms — scattered around the truck ──────────────── */

static void build_box_transforms(app_state *state)
{
    /* Box positions: arranged in a rough circle around the origin.
     * The BoxTextured model spans -1..+1 on each axis (2-unit cube),
     * so the Y translation must be 0.5 * scale to sit on the ground. */
    const struct { float x, z, scale, angle; } box_layout[BOX_COUNT] = {
        {  3.0f,  2.0f, 0.8f,  25.0f },
        { -3.0f,  1.5f, 1.0f, -15.0f },
        {  2.0f, -3.0f, 0.6f,  45.0f },
        { -2.5f, -2.5f, 0.9f, -30.0f },
        {  4.5f, -1.0f, 0.7f,  60.0f },
        { -4.0f,  0.0f, 1.1f,  10.0f },
        {  1.0f,  4.0f, 0.5f, -45.0f },
        { -1.0f, -4.5f, 0.8f,  35.0f },
    };

    for (int i = 0; i < BOX_COUNT; i++) {
        float ground_y = box_layout[i].scale * 0.5f;
        mat4 T = mat4_translate(vec3_create(
            box_layout[i].x, ground_y, box_layout[i].z));
        mat4 R = mat4_rotate_y(box_layout[i].angle * FORGE_DEG2RAD);
        mat4 S = mat4_scale(vec3_create(
            box_layout[i].scale, box_layout[i].scale, box_layout[i].scale));
        state->boxes[i].model_matrix = mat4_multiply(T, mat4_multiply(R, S));
    }
}


/* ── SDL_AppInit ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 33 \xe2\x80\x94 Vertex Pulling",
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    state->window           = window;
    state->device           = device;
    state->swapchain_format = swapchain_format;

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            goto init_fail;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    /* ── White placeholder texture ──────────────────────────────── */
    state->white_texture = create_white_texture(device);
    if (!state->white_texture) goto init_fail;

    /* ── Samplers ───────────────────────────────────────────────── */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter        = SDL_GPU_FILTER_LINEAR;
        si.mag_filter        = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        si.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.max_anisotropy    = MAX_ANISOTROPY;
        si.enable_anisotropy = true;
        si.max_lod           = MAX_LOD_UNLIMITED;
        state->sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->sampler) {
            SDL_Log("Failed to create sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_NEAREST;
        si.mag_filter     = SDL_GPU_FILTER_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->nearest_clamp = SDL_CreateGPUSampler(device, &si);
        if (!state->nearest_clamp) {
            SDL_Log("Failed to create nearest_clamp sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Load CesiumMilkTruck model ────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            goto init_fail;
        }
        char path[PATH_BUFFER_SIZE];
        SDL_snprintf(path, sizeof(path), "%s%s", base, TRUCK_MODEL_PATH);

        state->truck_arena = forge_arena_create(0);
        if (!state->truck_arena.first) {
            SDL_Log("Out of memory creating arena for CesiumMilkTruck");
            goto init_fail;
        }
        if (!forge_gltf_load(path, &state->truck_scene, &state->truck_arena)) {
            SDL_Log("Failed to load CesiumMilkTruck: %s", path);
            forge_arena_destroy(&state->truck_arena);
            goto init_fail;
        }

        if (!upload_pulled_model(device, &state->truck_scene, &state->truck)) {
            SDL_Log("Failed to upload truck model to GPU");
            goto init_fail;
        }

        /* The truck's model_matrix is identity — per-primitive node transforms
         * from the glTF hierarchy (cab, body, wheels) are applied in draw. */
        state->truck.model_matrix = mat4_identity();
    }

    /* ── Load BoxTextured model and create box instances ────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            goto init_fail;
        }
        char path[PATH_BUFFER_SIZE];
        SDL_snprintf(path, sizeof(path), "%s%s", base, BOX_MODEL_PATH);

        state->box_arena = forge_arena_create(0);
        if (!state->box_arena.first) {
            SDL_Log("Out of memory creating arena for BoxTextured");
            goto init_fail;
        }
        if (!forge_gltf_load(path, &state->box_scene, &state->box_arena)) {
            SDL_Log("Failed to load BoxTextured: %s", path);
            forge_arena_destroy(&state->box_arena);
            goto init_fail;
        }

        /* Upload box model once. */
        if (!upload_pulled_model(device, &state->box_scene, &state->boxes[0])) {
            SDL_Log("Failed to upload box model to GPU");
            goto init_fail;
        }

        /* Share GPU buffers and materials across all box instances.
         * Each instance only differs by its model_matrix.  The storage
         * buffers are read-only, so sharing is safe. */
        for (int i = 1; i < BOX_COUNT; i++) {
            state->boxes[i].primitives      = state->boxes[0].primitives;
            state->boxes[i].primitive_count  = state->boxes[0].primitive_count;
            state->boxes[i].materials        = state->boxes[0].materials;
            state->boxes[i].material_count   = state->boxes[0].material_count;
        }

        build_box_transforms(state);
    }

    /* ── Shadow depth texture ───────────────────────────────────── */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type                = SDL_GPU_TEXTURETYPE_2D;
        ti.format              = SHADOW_DEPTH_FMT;
        ti.width               = SHADOW_MAP_SIZE;
        ti.height              = SHADOW_MAP_SIZE;
        ti.layer_count_or_depth = 1;
        ti.num_levels          = 1;
        ti.usage               = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                                 SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->shadow_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->shadow_depth) {
            SDL_Log("Failed to create shadow depth texture: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Main depth texture (window-sized) ──────────────────────── */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type                = SDL_GPU_TEXTURETYPE_2D;
        ti.format              = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        ti.width               = WINDOW_WIDTH;
        ti.height              = WINDOW_HEIGHT;
        ti.layer_count_or_depth = 1;
        ti.num_levels          = 1;
        ti.usage               = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        state->main_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->main_depth) {
            SDL_Log("Failed to create main depth texture: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Vertex-pulled pipeline ─────────────────────────────────── */
    /* This pipeline has NO vertex input state.  Zero vertex buffers,
     * zero vertex attributes.  The vertex shader reads all data from
     * a storage buffer using SV_VertexID — that is vertex pulling. */
    {
        /* Vertex shader: 1 storage buffer (vertex data), 1 uniform buffer. */
        SDL_GPUShader *vert = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            pulled_vert_spirv, sizeof(pulled_vert_spirv),
            pulled_vert_dxil,  sizeof(pulled_vert_dxil),
            pulled_vert_msl,   pulled_vert_msl_size,
            0, 1, 1);   /* 0 samplers, 1 storage buffer, 1 uniform buffer */
        /* Fragment shader: 2 samplers (diffuse + shadow), 1 uniform buffer. */
        SDL_GPUShader *frag = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            pulled_frag_spirv, sizeof(pulled_frag_spirv),
            pulled_frag_dxil,  sizeof(pulled_frag_dxil),
            pulled_frag_msl,   pulled_frag_msl_size,
            2, 0, 1);   /* 2 samplers, 0 storage buffers, 1 uniform buffer */
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        /* VERTEX PULLING: empty vertex input state.
         * No vertex buffer descriptions, no vertex attributes.
         * The pipeline doesn't know or care about the vertex format.  */
        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        /* vis.num_vertex_buffers    = 0;  (already zero from SDL_zero) */
        /* vis.num_vertex_attributes = 0;  (already zero from SDL_zero) */

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state = vis;
        pi.target_info.num_color_targets = 1;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.has_depth_stencil_target = true;
        pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        state->pulled_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->pulled_pipeline) {
            SDL_Log("Failed to create pulled pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Shadow pipeline (vertex-pulled) ───────────────────────── */
    {
        SDL_GPUShader *vert = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            shadow_pulled_vert_spirv, sizeof(shadow_pulled_vert_spirv),
            shadow_pulled_vert_dxil,  sizeof(shadow_pulled_vert_dxil),
            shadow_pulled_vert_msl,   shadow_pulled_vert_msl_size,
            0, 1, 1);   /* 0 samplers, 1 storage buffer, 1 uniform buffer */
        SDL_GPUShader *frag = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_frag_spirv, sizeof(shadow_frag_spirv),
            shadow_frag_dxil,  sizeof(shadow_frag_dxil),
            shadow_frag_msl,   shadow_frag_msl_size,
            0, 0, 0);   /* no resources */
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        /* Empty vertex input — vertex pulling applies to shadows too. */
        SDL_GPUVertexInputState vis;
        SDL_zero(vis);

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state = vis;
        pi.target_info.has_depth_stencil_target = true;
        pi.target_info.depth_stencil_format = SHADOW_DEPTH_FMT;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->shadow_pipeline) {
            SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid pipeline (traditional vertex input) ──────────────── */
    /* The grid still uses conventional vertex attributes to provide contrast
     * with the vertex-pulled meshes.  This shows both approaches coexisting. */
    {
        SDL_GPUShader *vert = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_vert_spirv, sizeof(grid_vert_spirv),
            grid_vert_dxil,  sizeof(grid_vert_dxil),
            grid_vert_msl,   grid_vert_msl_size,
            0, 0, 1);
        SDL_GPUShader *frag = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_frag_spirv, sizeof(grid_frag_spirv),
            grid_frag_dxil,  sizeof(grid_frag_dxil),
            grid_frag_msl,   grid_frag_msl_size,
            1, 0, 1);  /* 1 sampler (shadow map), 0 storage buffers, 1 uniform buffer */
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexAttribute attr;
        SDL_zero(attr);
        attr.location = 0;
        attr.format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attr.offset   = 0;

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot       = 0;
        vbd.pitch      = sizeof(GridVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = &attr;
        vis.num_vertex_attributes      = 1;

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state = vis;
        pi.target_info.num_color_targets = 1;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.has_depth_stencil_target = true;
        pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->grid_pipeline) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid floor geometry ────────────────────────────────────── */
    {
        GridVertex verts[4] = {
            {{ -GRID_HALF_SIZE, GRID_Y, -GRID_HALF_SIZE }},
            {{  GRID_HALF_SIZE, GRID_Y, -GRID_HALF_SIZE }},
            {{  GRID_HALF_SIZE, GRID_Y,  GRID_HALF_SIZE }},
            {{ -GRID_HALF_SIZE, GRID_Y,  GRID_HALF_SIZE }}
        };
        Uint16 indices[6] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vb = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_VERTEX,
            verts, sizeof(verts));
        state->grid_ib = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_INDEX,
            indices, sizeof(indices));
        if (!state->grid_vb || !state->grid_ib) goto init_fail;
    }

    /* ── Compute light VP matrix ────────────────────────────────── */
    {
        vec3 light_dir = vec3_normalize(
            vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
        vec3 light_pos = vec3_scale(light_dir, -LIGHT_DISTANCE);
        vec3 target = vec3_create(0.0f, LIGHT_TARGET_Y, 0.0f);

        /* Choose an up vector that isn't parallel to the light direction. */
        vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
        if (SDL_fabsf(vec3_dot(light_dir, up)) > PARALLEL_THRESHOLD) {
            up = vec3_create(0.0f, 0.0f, 1.0f);
        }

        mat4 light_view = mat4_look_at(light_pos, target, up);
        mat4 light_proj = mat4_orthographic(
            -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
            -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
            SHADOW_NEAR, SHADOW_FAR);
        state->light_vp = mat4_multiply(light_proj, light_view);
    }

    /* ── Camera ─────────────────────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw   = CAM_START_YAW_DEG * FORGE_DEG2RAD;
    state->cam_pitch = CAM_START_PITCH_DEG * FORGE_DEG2RAD;
    state->last_ticks = SDL_GetPerformanceCounter();

    SDL_Log("Lesson 33 initialised: truck (%d prims), %d boxes (pulled)",
            state->truck.primitive_count, BOX_COUNT);

    return SDL_APP_CONTINUE;

init_fail:
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ───────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
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
                } else {
                    state->mouse_captured = false;
                }
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
                SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            } else {
                state->mouse_captured = true;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENS;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENS;
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
        }
        break;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Delta time ─────────────────────────────────────────────── */
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - state->last_ticks) /
               (float)SDL_GetPerformanceFrequency();
    if (dt > MAX_FRAME_DT) dt = MAX_FRAME_DT;
    state->last_ticks = now;

    /* ── Camera movement ────────────────────────────────────────── */
    {
        const bool *keys = SDL_GetKeyboardState(NULL);
        float speed = CAM_SPEED * dt;

        quat cam_orient = quat_from_euler(
            state->cam_yaw, state->cam_pitch, 0.0f);
        vec3 forward = quat_forward(cam_orient);
        vec3 right   = quat_right(cam_orient);

        if (keys[SDL_SCANCODE_W])
            state->cam_position = vec3_add(state->cam_position,
                                           vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_S])
            state->cam_position = vec3_add(state->cam_position,
                                           vec3_scale(forward, -speed));
        if (keys[SDL_SCANCODE_D])
            state->cam_position = vec3_add(state->cam_position,
                                           vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_A])
            state->cam_position = vec3_add(state->cam_position,
                                           vec3_scale(right, -speed));
        if (keys[SDL_SCANCODE_SPACE])
            state->cam_position.y += speed;
        if (keys[SDL_SCANCODE_LSHIFT])
            state->cam_position.y -= speed;
    }

    /* ── Build camera matrices ──────────────────────────────────── */
    quat cam_orient = quat_from_euler(
        state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    mat4 proj = mat4_perspective(
        (float)FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* ── Acquire swapchain ──────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_CONTINUE;
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

    /* ── Pass 1: Shadow map (vertex-pulled) ────────────────────── */
    {
        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture     = state->shadow_depth;
        depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op    = SDL_GPU_STOREOP_STORE;
        depth_target.clear_depth = 1.0f;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &depth_target);
        if (!pass) {
            SDL_Log("Failed to begin shadow render pass: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_CONTINUE;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->shadow_pipeline);

        /* Draw truck shadow (vertex-pulled). */
        draw_pulled_object(pass, cmd, state, &state->truck,
                           &state->truck_scene, NULL, true);

        /* Draw box shadows (vertex-pulled). */
        for (int i = 0; i < BOX_COUNT; i++) {
            draw_pulled_object(pass, cmd, state, &state->boxes[i],
                               &state->box_scene, NULL, true);
        }

        SDL_EndGPURenderPass(pass);
    }

    /* ── Pass 2: Scene (vertex-pulled meshes + traditional grid) ── */
    {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture    = swapchain_tex;
        color_target.load_op    = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op   = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture     = state->main_depth;
        depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op    = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.clear_depth = 1.0f;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, &depth_target);
        if (!pass) {
            SDL_Log("Failed to begin scene render pass: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_CONTINUE;
        }

        /* ── Draw vertex-pulled meshes ─────────────────────────── */
        SDL_BindGPUGraphicsPipeline(pass, state->pulled_pipeline);

        /* Draw truck (vertex-pulled). */
        draw_pulled_object(pass, cmd, state, &state->truck,
                           &state->truck_scene, &cam_vp, false);

        /* Draw boxes (vertex-pulled, shared storage buffers). */
        for (int i = 0; i < BOX_COUNT; i++) {
            draw_pulled_object(pass, cmd, state, &state->boxes[i],
                               &state->box_scene, &cam_vp, false);
        }

        /* ── Draw grid floor (traditional vertex input) ────────── */
        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

        GridVertUniforms grid_vu;
        grid_vu.vp       = cam_vp;
        grid_vu.light_vp = state->light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &grid_vu, sizeof(grid_vu));

        GridFragUniforms grid_fu;
        SDL_zero(grid_fu);
        grid_fu.line_color[0] = GRID_LINE_GRAY;
        grid_fu.line_color[1] = GRID_LINE_GRAY;
        grid_fu.line_color[2] = GRID_LINE_GRAY;
        grid_fu.line_color[3] = 1.0f;
        grid_fu.bg_color[0]   = GRID_BG_GRAY;
        grid_fu.bg_color[1]   = GRID_BG_GRAY;
        grid_fu.bg_color[2]   = GRID_BG_GRAY;
        grid_fu.bg_color[3]   = 1.0f;
        grid_fu.light_dir[0]  = LIGHT_DIR_X;
        grid_fu.light_dir[1]  = LIGHT_DIR_Y;
        grid_fu.light_dir[2]  = LIGHT_DIR_Z;
        grid_fu.light_intensity = LIGHT_INTENSITY;
        grid_fu.eye_pos[0]    = state->cam_position.x;
        grid_fu.eye_pos[1]    = state->cam_position.y;
        grid_fu.eye_pos[2]    = state->cam_position.z;
        grid_fu.grid_spacing  = GRID_SPACING;
        grid_fu.line_width    = GRID_LINE_WIDTH;
        grid_fu.fade_distance = GRID_FADE_DIST;
        grid_fu.ambient       = GRID_AMBIENT;
        SDL_PushGPUFragmentUniformData(cmd, 0, &grid_fu, sizeof(grid_fu));

        /* Bind shadow depth texture for grid shadow sampling. */
        SDL_GPUTextureSamplerBinding grid_tex = {
            .texture = state->shadow_depth,
            .sampler = state->nearest_clamp
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &grid_tex, 1);

        SDL_GPUBufferBinding grid_vb = { state->grid_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &grid_vb, 1);
        SDL_GPUBufferBinding grid_ib = { state->grid_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &grid_ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, GRID_INDEX_COUNT, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain_tex)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
        }
        if (forge_capture_should_quit(&state->capture)) {
            return SDL_APP_SUCCESS;
        }
    } else
#endif
    {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    if (!appstate) return;

    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    if (device) {
        if (!SDL_WaitForGPUIdle(device)) {
            SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }

        /* Release truck primitives (storage buffers + index buffers). */
        if (state->truck.primitives) {
            for (int i = 0; i < state->truck.primitive_count; i++) {
                if (state->truck.primitives[i].storage_buffer)
                    SDL_ReleaseGPUBuffer(device, state->truck.primitives[i].storage_buffer);
                if (state->truck.primitives[i].index_buffer)
                    SDL_ReleaseGPUBuffer(device, state->truck.primitives[i].index_buffer);
            }
            SDL_free(state->truck.primitives);
        }

        /* Release truck materials / textures. */
        if (state->truck.materials) {
            for (int i = 0; i < state->truck.material_count; i++) {
                if (!state->truck.materials[i].texture) continue;
                bool already_released = false;
                for (int j = 0; j < i; j++) {
                    if (state->truck.materials[j].texture ==
                        state->truck.materials[i].texture) {
                        already_released = true;
                        break;
                    }
                }
                if (!already_released)
                    SDL_ReleaseGPUTexture(device, state->truck.materials[i].texture);
            }
            SDL_free(state->truck.materials);
        }

        /* Release box primitives (only boxes[0] owns the GPU resources). */
        if (state->boxes[0].primitives) {
            for (int i = 0; i < state->boxes[0].primitive_count; i++) {
                if (state->boxes[0].primitives[i].storage_buffer)
                    SDL_ReleaseGPUBuffer(device,
                        state->boxes[0].primitives[i].storage_buffer);
                if (state->boxes[0].primitives[i].index_buffer)
                    SDL_ReleaseGPUBuffer(device,
                        state->boxes[0].primitives[i].index_buffer);
            }
            SDL_free(state->boxes[0].primitives);
        }

        /* Release box materials / textures (dedup like truck above). */
        if (state->boxes[0].materials) {
            for (int i = 0; i < state->boxes[0].material_count; i++) {
                if (!state->boxes[0].materials[i].texture) continue;
                bool already_released = false;
                for (int j = 0; j < i; j++) {
                    if (state->boxes[0].materials[j].texture ==
                        state->boxes[0].materials[i].texture) {
                        already_released = true;
                        break;
                    }
                }
                if (!already_released)
                    SDL_ReleaseGPUTexture(device,
                        state->boxes[0].materials[i].texture);
            }
            SDL_free(state->boxes[0].materials);
        }

        /* Release grid buffers. */
        if (state->grid_vb) SDL_ReleaseGPUBuffer(device, state->grid_vb);
        if (state->grid_ib) SDL_ReleaseGPUBuffer(device, state->grid_ib);

        /* Release textures. */
        if (state->white_texture) SDL_ReleaseGPUTexture(device, state->white_texture);
        if (state->shadow_depth)  SDL_ReleaseGPUTexture(device, state->shadow_depth);
        if (state->main_depth)    SDL_ReleaseGPUTexture(device, state->main_depth);

        /* Release samplers. */
        if (state->sampler)       SDL_ReleaseGPUSampler(device, state->sampler);
        if (state->nearest_clamp) SDL_ReleaseGPUSampler(device, state->nearest_clamp);

        /* Release pipelines. */
        if (state->pulled_pipeline) SDL_ReleaseGPUGraphicsPipeline(device, state->pulled_pipeline);
        if (state->shadow_pipeline) SDL_ReleaseGPUGraphicsPipeline(device, state->shadow_pipeline);
        if (state->grid_pipeline)   SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);

#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, device);
#endif

        forge_arena_destroy(&state->truck_arena);
        forge_arena_destroy(&state->box_arena);

        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(device);
    }

    SDL_free(state);
}
