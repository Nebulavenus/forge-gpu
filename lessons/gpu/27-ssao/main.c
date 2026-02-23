/*
 * GPU Lesson 27 — SSAO (Screen-Space Ambient Occlusion)
 *
 * Screen-space ambient occlusion estimates how much ambient light reaches
 * each pixel by sampling the depth buffer in a hemisphere around the
 * surface normal. The result darkens crevices, corners, and contact areas
 * where light is naturally blocked by nearby geometry.
 *
 * Architecture — 5 render passes per frame:
 *   1. Shadow pass    — directional light depth map (2048x2048)
 *   2. Geometry pass  — lit color + view normals + depth (MRT)
 *   3. SSAO pass      — hemisphere kernel sampling (fullscreen quad)
 *   4. Blur pass      — 4x4 box blur (smooths noise tile pattern)
 *   5. Composite pass — combines scene color with AO factor
 *
 * Controls:
 *   1                       — AO only (default for screenshot)
 *   2                       — Full render with AO applied
 *   3                       — Full render without AO (comparison)
 *   D                       — Toggle IGN dithering (ON by default)
 *   WASD / Space / LShift   — Move camera
 *   Mouse                   — Look around
 *   Escape                  — Release mouse / quit
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

#include "shaders/compiled/shadow_vert_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"

#include "shaders/compiled/scene_vert_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_frag_dxil.h"

#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"

#include "shaders/compiled/fullscreen_vert_spirv.h"
#include "shaders/compiled/fullscreen_vert_dxil.h"

#include "shaders/compiled/ssao_frag_spirv.h"
#include "shaders/compiled/ssao_frag_dxil.h"
#include "shaders/compiled/blur_frag_spirv.h"
#include "shaders/compiled/blur_frag_dxil.h"
#include "shaders/compiled/composite_frag_spirv.h"
#include "shaders/compiled/composite_frag_dxil.h"

/* ── Constants ──────────────────────────────────────────────────────── */

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Camera. */
#define FOV_DEG            60
#define NEAR_PLANE         0.1f
#define FAR_PLANE          100.0f
#define CAM_SPEED          5.0f
#define MOUSE_SENS         0.003f
#define PITCH_CLAMP        1.5f

/* Camera initial position — front-right, looking at the origin. */
#define CAM_START_X         2.0f
#define CAM_START_Y         1.5f
#define CAM_START_Z         3.5f
#define CAM_START_YAW_DEG   30.0f
#define CAM_START_PITCH_DEG -8.0f

/* Directional light — shines from behind the camera toward the scene. */
#define LIGHT_DIR_X     -0.5f
#define LIGHT_DIR_Y     -0.8f
#define LIGHT_DIR_Z     -0.5f
#define LIGHT_INTENSITY  0.8f
#define LIGHT_COLOR_R    1.0f
#define LIGHT_COLOR_G    0.95f
#define LIGHT_COLOR_B    0.9f

/* Scene material defaults. */
#define MATERIAL_AMBIENT      0.15f
#define MATERIAL_SHININESS    64.0f
#define MATERIAL_SPECULAR_STR 0.3f

/* Shadow map. */
#define SHADOW_MAP_SIZE   2048
#define SHADOW_DEPTH_FMT  SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Shadow orthographic projection bounds (fits the scene). */
#define SHADOW_ORTHO_SIZE 15.0f
#define SHADOW_NEAR       0.1f
#define SHADOW_FAR        50.0f
#define LIGHT_DISTANCE    20.0f

/* SSAO parameters. */
#define SSAO_KERNEL_SIZE   64
#define SSAO_RADIUS        0.5f
#define SSAO_BIAS          0.025f
#define NOISE_TEX_SIZE     4

/* Fullscreen quad (2 triangles, no vertex buffer). */
#define FULLSCREEN_QUAD_VERTS 6

/* Grid. */
#define GRID_HALF_SIZE     50.0f
#define GRID_INDEX_COUNT   6
#define GRID_SPACING       1.0f
#define GRID_LINE_WIDTH    0.02f
#define GRID_FADE_DISTANCE 40.0f

/* Grid colors (linear space). */
#define GRID_LINE_R 0.15f
#define GRID_LINE_G 0.55f
#define GRID_LINE_B 0.85f
#define GRID_BG_R   0.04f
#define GRID_BG_G   0.04f
#define GRID_BG_B   0.08f

/* Clear color — dark background. */
#define CLEAR_R 0.008f
#define CLEAR_G 0.008f
#define CLEAR_B 0.026f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* Model asset paths (relative to executable). */
#define TRUCK_MODEL_PATH "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH   "assets/models/BoxTextured/BoxTextured.gltf"

/* Box placement — crates scattered near the truck. */
#define BOX_COUNT 5

/* Texture sampler — trilinear filtering with anisotropy. */
#define MAX_ANISOTROPY 4
#define BYTES_PER_PIXEL 4

/* Display modes. */
#define MODE_AO_ONLY    0
#define MODE_WITH_AO    1
#define MODE_NO_AO      2

/* Light direction degeneracy — skip if light is nearly parallel to up. */
#define PARALLEL_THRESHOLD 0.99f

/* SSAO kernel generation. */
#define SSAO_DEFAULT_SEED   12345u
#define SSAO_EPSILON        0.0001f
#define SSAO_SCALE_START    0.1f
#define SSAO_SCALE_RANGE    0.9f
#define SSAO_SCALE_MIN      0.01f

/* Noise texture generation. */
#define NOISE_DEFAULT_SEED  67890u
#define NOISE_EPSILON       0.0001f

/* ── Uniform structures ─────────────────────────────────────────────── */

/* Scene vertex uniforms — pushed per draw call. */
typedef struct SceneVertUniforms {
    mat4 mvp;      /* model-view-projection matrix */
    mat4 model;    /* model (world) matrix         */
    mat4 view;     /* camera view matrix           */
    mat4 light_vp; /* light view-projection matrix */
} SceneVertUniforms;

/* Scene fragment uniforms. */
typedef struct SceneFragUniforms {
    float base_color[4];    /* material RGBA              */
    float eye_pos[3];       /* camera position            */
    float has_texture;      /* > 0.5 = sample diffuse_tex */
    float ambient;          /* ambient intensity          */
    float shininess;        /* specular exponent          */
    float specular_str;     /* specular strength          */
    float _pad0;
    float light_dir[4];     /* directional light dir      */
    float light_color[3];   /* directional light color    */
    float light_intensity;  /* directional light strength */
} SceneFragUniforms;

/* Shadow vertex uniforms. */
typedef struct ShadowVertUniforms {
    mat4 light_mvp; /* light VP * model — transforms to light clip space */
} ShadowVertUniforms;

/* Grid vertex uniforms. */
typedef struct GridVertUniforms {
    mat4 vp;         /* view-projection matrix */
    mat4 view;       /* view matrix            */
    mat4 light_vp;   /* light view-projection  */
} GridVertUniforms;

/* Grid fragment uniforms. */
typedef struct GridFragUniforms {
    float line_color[4];    /* grid line RGBA (linear space)         */
    float bg_color[4];      /* background RGBA (linear space)        */
    float eye_pos[3];       /* camera world-space position           */
    float grid_spacing;     /* distance between grid lines (world)   */
    float line_width;       /* half-width of each line (world units) */
    float fade_distance;    /* distance at which grid fades out      */
    float ambient;          /* ambient light intensity (0..1)        */
    float light_intensity;  /* directional light brightness          */
    float light_dir[4];     /* directional light direction (xyz)     */
    float light_color[3];   /* directional light RGB (linear)        */
    float _pad;
} GridFragUniforms;

/* SSAO fragment uniforms. */
typedef struct SSAOUniforms {
    float samples[SSAO_KERNEL_SIZE * 4]; /* hemisphere kernel (vec4 aligned) */
    mat4  projection;                     /* camera projection matrix        */
    mat4  inv_projection;                 /* inverse projection              */
    float noise_scale[2];                 /* screen_size / noise_size        */
    float radius;                         /* sample hemisphere radius        */
    float bias;                           /* self-occlusion bias             */
    int   use_ign_jitter;                 /* 1 = add IGN rotation jitter     */
    float _pad[3];
} SSAOUniforms;

/* Blur fragment uniforms. */
typedef struct BlurUniforms {
    float texel_size[2]; /* 1/width, 1/height of the SSAO texture */
    float _pad[2];
} BlurUniforms;

/* Composite fragment uniforms. */
typedef struct CompositeUniforms {
    int   display_mode; /* MODE_AO_ONLY / MODE_WITH_AO / MODE_NO_AO */
    int   use_dither;   /* 1 = apply IGN dithering to reduce banding */
    float _pad[2];
} CompositeUniforms;

/* ── GPU-side model types ───────────────────────────────────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;       /* per-vertex data (pos, normal, uv)    */
    SDL_GPUBuffer *index_buffer;        /* triangle index data                  */
    Uint32 index_count;                 /* number of indices to draw            */
    int material_index;                 /* index into ModelData.materials (-1=none) */
    SDL_GPUIndexElementSize index_type; /* 16-bit or 32-bit indices             */
    bool has_uvs;                       /* true if vertices have texture coords */
} GpuPrimitive;

typedef struct GpuMaterial {
    float base_color[4];     /* RGBA base color factor (linear space)  */
    SDL_GPUTexture *texture; /* diffuse texture (NULL if no texture)   */
    bool has_texture;        /* true if texture should be sampled      */
} GpuMaterial;

typedef struct ModelData {
    ForgeGltfScene scene;          /* parsed glTF data (CPU-side)           */
    GpuPrimitive  *primitives;     /* GPU buffers per primitive (heap)      */
    int            primitive_count; /* number of primitives in the model    */
    GpuMaterial   *materials;      /* material properties + textures (heap) */
    int            material_count;  /* number of materials in the model     */
} ModelData;

typedef struct BoxPlacement {
    vec3  position;   /* world-space center of the box   */
    float y_rotation; /* rotation around Y axis (radians) */
} BoxPlacement;

/* ── Application state ──────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window; /* application window handle    */
    SDL_GPUDevice *device; /* GPU device for all rendering */

    /* Pipelines. */
    SDL_GPUGraphicsPipeline *scene_pipeline;     /* Blinn-Phong + shadow MRT pass   */
    SDL_GPUGraphicsPipeline *grid_pipeline;      /* procedural grid MRT pass        */
    SDL_GPUGraphicsPipeline *shadow_pipeline;    /* depth-only shadow map pass      */
    SDL_GPUGraphicsPipeline *ssao_pipeline;      /* hemisphere kernel SSAO pass     */
    SDL_GPUGraphicsPipeline *blur_pipeline;      /* 4x4 box blur for raw AO        */
    SDL_GPUGraphicsPipeline *composite_pipeline; /* scene color * AO to swapchain  */

    /* Geometry pass render targets. */
    SDL_GPUTexture *scene_color;      /* R8G8B8A8_UNORM — lit color        */
    SDL_GPUTexture *view_normals;     /* R16G16B16A16_FLOAT — view normals */
    SDL_GPUTexture *scene_depth;      /* D32_FLOAT — depth buffer          */

    /* SSAO render targets. */
    SDL_GPUTexture *ssao_raw;         /* R8_UNORM — raw AO output    */
    SDL_GPUTexture *ssao_blurred;     /* R8_UNORM — blurred AO       */

    /* Shadow map. */
    SDL_GPUTexture *shadow_depth;     /* D32_FLOAT — directional shadow  */

    /* SSAO noise texture (4x4 random rotations). */
    SDL_GPUTexture *noise_texture;    /* R32G32B32A32_FLOAT — tiled rotation vectors */

    /* Samplers. */
    SDL_GPUSampler *sampler;          /* trilinear + anisotropy (textures) */
    SDL_GPUSampler *nearest_clamp;    /* nearest, clamp (G-buffer reads)   */
    SDL_GPUSampler *nearest_repeat;   /* nearest, repeat (noise texture)   */
    SDL_GPUSampler *linear_clamp;     /* linear, clamp (AO blur/composite) */

    /* Scene objects. */
    SDL_GPUTexture *white_texture;              /* 1x1 fallback for untextured prims */
    ModelData truck;                            /* CesiumMilkTruck glTF model        */
    ModelData box;                              /* BoxTextured glTF model            */
    BoxPlacement box_placements[BOX_COUNT];     /* world transforms for crate copies */

    /* Grid geometry. */
    SDL_GPUBuffer *grid_vertex_buffer; /* 4-vert XZ plane quad           */
    SDL_GPUBuffer *grid_index_buffer;  /* 6 indices (2 triangles)        */

    /* Light. */
    mat4 light_vp; /* directional light view-projection (orthographic) */

    /* SSAO kernel (generated at init). */
    float ssao_kernel[SSAO_KERNEL_SIZE * 4]; /* 64 hemisphere sample dirs (vec4) */

    /* Swapchain format. */
    SDL_GPUTextureFormat swapchain_format; /* queried after swapchain setup */

    /* Camera. */
    vec3  cam_position; /* world-space camera position                */
    float cam_yaw;      /* horizontal rotation (radians, 0 = +Z)     */
    float cam_pitch;    /* vertical rotation (radians, clamped ±1.5) */

    /* Display mode and settings. */
    int  display_mode;   /* MODE_AO_ONLY, MODE_WITH_AO, MODE_NO_AO */
    bool use_ign_jitter; /* IGN jitter for SSAO kernel rotation     */
    bool use_dither;     /* IGN dithering on composite output       */

    /* Timing and input. */
    Uint64 last_ticks;     /* perf counter from previous frame     */
    bool   mouse_captured; /* true while relative mouse mode is on */

#ifdef FORGE_CAPTURE
    ForgeCapture capture; /* screenshot / GIF capture state */
#endif
} app_state;

/* ── Helper: create shader from embedded bytecode ───────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage              = stage;
    info.entrypoint         = "main";
    info.num_samplers       = num_samplers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
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

/* ── Helper: upload buffer data ─────────────────────────────────────── */

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

/* ── Helper: free model GPU resources ───────────────────────────────── */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model)
{
    if (model->primitives) {
        for (int i = 0; i < model->primitive_count; i++) {
            if (model->primitives[i].vertex_buffer) {
                bool already_released = false;
                for (int j = 0; j < i; j++) {
                    if (model->primitives[j].vertex_buffer ==
                        model->primitives[i].vertex_buffer) {
                        already_released = true;
                        break;
                    }
                }
                if (!already_released)
                    SDL_ReleaseGPUBuffer(device, model->primitives[i].vertex_buffer);
            }
            if (model->primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device, model->primitives[i].index_buffer);
        }
        SDL_free(model->primitives);
        model->primitives = NULL;
    }

    if (model->materials) {
        for (int i = 0; i < model->material_count; i++) {
            if (!model->materials[i].texture) continue;
            bool already_released = false;
            for (int j = 0; j < i; j++) {
                if (model->materials[j].texture == model->materials[i].texture) {
                    already_released = true;
                    break;
                }
            }
            if (!already_released)
                SDL_ReleaseGPUTexture(device, model->materials[i].texture);
        }
        SDL_free(model->materials);
        model->materials = NULL;
    }

    forge_gltf_free(&model->scene);
}

/* ── Helper: upload glTF model to GPU ───────────────────────────────── */

static bool upload_model_to_gpu(SDL_GPUDevice *device, ModelData *model)
{
    ForgeGltfScene *scene = &model->scene;

    model->primitive_count = scene->primitive_count;
    model->primitives = (GpuPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(GpuPrimitive));
    if (!model->primitives) {
        SDL_Log("Failed to allocate GPU primitives");
        return false;
    }

    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *src = &scene->primitives[i];
        GpuPrimitive *dst = &model->primitives[i];

        dst->material_index = src->material_index;
        dst->index_count    = src->index_count;
        dst->has_uvs        = src->has_uvs;

        if (src->vertices && src->vertex_count > 0) {
            Uint32 vb_size = src->vertex_count * (Uint32)sizeof(ForgeGltfVertex);
            dst->vertex_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_VERTEX, src->vertices, vb_size);
            if (!dst->vertex_buffer) {
                free_model_gpu(device, model);
                return false;
            }
        }

        if (src->indices && src->index_count > 0) {
            Uint32 ib_size = src->index_count * src->index_stride;
            dst->index_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_INDEX, src->indices, ib_size);
            if (!dst->index_buffer) {
                free_model_gpu(device, model);
                return false;
            }
            dst->index_type = (src->index_stride == 2)
                ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                : SDL_GPU_INDEXELEMENTSIZE_32BIT;
        }
    }

    /* Load materials and textures. */
    model->material_count = scene->material_count;
    model->materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!model->materials) {
        SDL_Log("Failed to allocate GPU materials");
        free_model_gpu(device, model);
        return false;
    }

    {
        SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
        const char     *loaded_paths[FORGE_GLTF_MAX_IMAGES];
        int             loaded_count = 0;
        SDL_memset(loaded_textures, 0, sizeof(loaded_textures));
        SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

        for (int i = 0; i < scene->material_count; i++) {
            const ForgeGltfMaterial *src = &scene->materials[i];
            GpuMaterial *dst = &model->materials[i];

            dst->base_color[0] = src->base_color[0];
            dst->base_color[1] = src->base_color[1];
            dst->base_color[2] = src->base_color[2];
            dst->base_color[3] = src->base_color[3];
            dst->has_texture   = src->has_texture;
            dst->texture       = NULL;

            if (src->has_texture && src->texture_path[0] != '\0') {
                bool found = false;
                for (int j = 0; j < loaded_count; j++) {
                    if (loaded_paths[j] &&
                        SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
                        dst->texture = loaded_textures[j];
                        found = true;
                        break;
                    }
                }

                if (!found && loaded_count < FORGE_GLTF_MAX_IMAGES) {
                    dst->texture = load_texture(device, src->texture_path);
                    if (dst->texture) {
                        loaded_textures[loaded_count] = dst->texture;
                        loaded_paths[loaded_count]    = src->texture_path;
                        loaded_count++;
                    } else {
                        dst->has_texture = false;
                    }
                }
            }
        }
    }

    return true;
}

/* ── Helper: load + upload a glTF model ─────────────────────────────── */

static bool setup_model(SDL_GPUDevice *device, ModelData *model, const char *path)
{
    if (!forge_gltf_load(path, &model->scene)) {
        SDL_Log("Failed to load glTF: %s", path);
        return false;
    }
    return upload_model_to_gpu(device, model);
}

/* ── SSAO: generate hemisphere kernel ───────────────────────────────── */

static void generate_ssao_kernel(float *kernel)
{
    uint32_t seed = SSAO_DEFAULT_SEED;

    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        /* Generate random point in unit hemisphere (+Z up). */
        seed = forge_hash_pcg(seed);
        float x = forge_hash_to_sfloat(seed);
        seed = forge_hash_pcg(seed);
        float y = forge_hash_to_sfloat(seed);
        seed = forge_hash_pcg(seed);
        float z = forge_hash_to_float(seed); /* [0,1) — hemisphere only */

        /* Normalize to get a direction on the hemisphere surface. */
        float len = SDL_sqrtf(x * x + y * y + z * z);
        if (len < SSAO_EPSILON) len = 1.0f;
        x /= len;
        y /= len;
        z /= len;

        /* Scale with quadratic falloff — concentrate samples near surface.
         * scale = lerp(SSAO_SCALE_START, 1.0, (i/SSAO_KERNEL_SIZE)^2) */
        float t = (float)i / (float)SSAO_KERNEL_SIZE;
        float scale = SSAO_SCALE_START + SSAO_SCALE_RANGE * t * t;

        /* Apply random length within [0,1] * scale to distribute inside
         * the hemisphere volume, not just on the surface. */
        seed = forge_hash_pcg(seed);
        float r = forge_hash_to_float(seed);
        scale *= r;
        if (scale < SSAO_SCALE_MIN) scale = SSAO_SCALE_MIN;

        kernel[i * 4 + 0] = x * scale;
        kernel[i * 4 + 1] = y * scale;
        kernel[i * 4 + 2] = z * scale;
        kernel[i * 4 + 3] = 0.0f; /* padding */
    }
}

/* ── SSAO: create 4x4 noise texture ─────────────────────────────────── */

static SDL_GPUTexture *create_noise_texture(SDL_GPUDevice *device)
{
    /* Generate 4x4 random rotation vectors in XY plane. */
    float noise_data[NOISE_TEX_SIZE * NOISE_TEX_SIZE * 4];
    uint32_t seed = NOISE_DEFAULT_SEED;

    for (int i = 0; i < NOISE_TEX_SIZE * NOISE_TEX_SIZE; i++) {
        seed = forge_hash_pcg(seed);
        float x = forge_hash_to_sfloat(seed);
        seed = forge_hash_pcg(seed);
        float y = forge_hash_to_sfloat(seed);

        /* Normalize to unit length in XY. */
        float len = SDL_sqrtf(x * x + y * y);
        if (len < NOISE_EPSILON) { x = 1.0f; y = 0.0f; len = 1.0f; }
        x /= len;
        y /= len;

        noise_data[i * 4 + 0] = x;
        noise_data[i * 4 + 1] = y;
        noise_data[i * 4 + 2] = 0.0f;
        noise_data[i * 4 + 3] = 0.0f;
    }

    /* Create R32G32B32A32_FLOAT texture. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    tex_info.width               = NOISE_TEX_SIZE;
    tex_info.height              = NOISE_TEX_SIZE;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = 1;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create noise texture: %s", SDL_GetError());
        return NULL;
    }

    Uint32 total_bytes = NOISE_TEX_SIZE * NOISE_TEX_SIZE * 4 * sizeof(float);

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create noise transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map noise transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(mapped, noise_data, total_bytes);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for noise upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass for noise texture: %s", SDL_GetError());
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
    dst.w       = NOISE_TEX_SIZE;
    dst.h       = NOISE_TEX_SIZE;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit noise texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Helper: draw a model into the shadow map (depth-only) ─────────── */

static void draw_model_shadow(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const mat4 *placement,
    const mat4 *light_vp)
{
    const ForgeGltfScene *scene = &model->scene;

    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        mat4 model_mat = mat4_multiply(*placement, node->world_transform);
        ShadowVertUniforms vert_u;
        vert_u.light_mvp = mat4_multiply(*light_vp, model_mat);
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

            if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
                continue;

            SDL_GPUBufferBinding vb;
            SDL_zero(vb);
            vb.buffer = gpu_prim->vertex_buffer;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib;
            SDL_zero(ib);
            ib.buffer = gpu_prim->index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib, gpu_prim->index_type);

            SDL_DrawGPUIndexedPrimitives(pass, gpu_prim->index_count, 1, 0, 0, 0);
        }
    }
}

/* ── Helper: draw a model with the scene pipeline (MRT) ─────────────── */

static void draw_model_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    const mat4 *placement,
    const mat4 *cam_vp,
    const mat4 *view_mat)
{
    const ForgeGltfScene *scene = &model->scene;

    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        mat4 model_mat = mat4_multiply(*placement, node->world_transform);
        mat4 mvp       = mat4_multiply(*cam_vp, model_mat);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model_mat;
        vert_u.view     = *view_mat;
        vert_u.light_vp = state->light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

            if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
                continue;

            SDL_GPUTexture *tex = state->white_texture;

            SceneFragUniforms frag_u;
            SDL_zero(frag_u);

            if (gpu_prim->material_index >= 0 &&
                gpu_prim->material_index < model->material_count) {
                const GpuMaterial *mat = &model->materials[gpu_prim->material_index];
                frag_u.base_color[0] = mat->base_color[0];
                frag_u.base_color[1] = mat->base_color[1];
                frag_u.base_color[2] = mat->base_color[2];
                frag_u.base_color[3] = mat->base_color[3];
                frag_u.has_texture = mat->has_texture ? 1.0f : 0.0f;
                if (mat->texture)
                    tex = mat->texture;
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

            /* Bind 2 samplers: diffuse (slot 0), shadow depth (slot 1). */
            SDL_GPUTextureSamplerBinding tex_binds[2];
            tex_binds[0] = (SDL_GPUTextureSamplerBinding){
                .texture = tex, .sampler = state->sampler };
            tex_binds[1] = (SDL_GPUTextureSamplerBinding){
                .texture = state->shadow_depth,
                .sampler = state->nearest_clamp };
            SDL_BindGPUFragmentSamplers(pass, 0, tex_binds, 2);

            SDL_GPUBufferBinding vb;
            SDL_zero(vb);
            vb.buffer = gpu_prim->vertex_buffer;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib;
            SDL_zero(ib);
            ib.buffer = gpu_prim->index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib, gpu_prim->index_type);

            SDL_DrawGPUIndexedPrimitives(pass, gpu_prim->index_count, 1, 0, 0, 0);
        }
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
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 27 \xe2\x80\x94 SSAO", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

    /* Request SDR_LINEAR for correct gamma handling. */
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

    /* Allocate app state. */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window           = window;
    state->device           = device;
    state->swapchain_format = swapchain_format;
    state->display_mode     = MODE_AO_ONLY;
    state->use_ign_jitter   = true;
    state->use_dither       = true;

    /* Set appstate early so SDL_AppQuit can clean up on init failure. */
    *appstate = state;

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
        /* Trilinear + anisotropy for model textures. */
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

        state->sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->sampler) {
            SDL_Log("Failed to create sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }
    {
        /* Nearest, clamp — for G-buffer reads (normals, depth, shadow). */
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
    {
        /* Nearest, repeat — for noise texture (tiles across screen). */
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_NEAREST;
        si.mag_filter     = SDL_GPU_FILTER_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

        state->nearest_repeat = SDL_CreateGPUSampler(device, &si);
        if (!state->nearest_repeat) {
            SDL_Log("Failed to create nearest_repeat sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }
    {
        /* Linear, clamp — for AO blur and composite reads. */
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_LINEAR;
        si.mag_filter     = SDL_GPU_FILTER_LINEAR;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        state->linear_clamp = SDL_CreateGPUSampler(device, &si);
        if (!state->linear_clamp) {
            SDL_Log("Failed to create linear_clamp sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Load models ────────────────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            goto init_fail;
        }
        char path[512];

        SDL_snprintf(path, sizeof(path), "%s%s", base, TRUCK_MODEL_PATH);
        if (!setup_model(device, &state->truck, path))
            goto init_fail;

        SDL_snprintf(path, sizeof(path), "%s%s", base, BOX_MODEL_PATH);
        if (!setup_model(device, &state->box, path))
            goto init_fail;
    }

    /* ── Shadow pipeline (depth-only) ───────────────────────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            shadow_vert_spirv, sizeof(shadow_vert_spirv),
            shadow_vert_dxil, sizeof(shadow_vert_dxil), 0, 1);
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_frag_spirv, sizeof(shadow_frag_spirv),
            shadow_frag_dxil, sizeof(shadow_frag_dxil), 0, 0);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot       = 0;
        vb_desc.pitch      = sizeof(ForgeGltfVertex);
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[3];
        SDL_zero(attrs);
        attrs[0].location = 0;
        attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset   = offsetof(ForgeGltfVertex, position);
        attrs[1].location = 1;
        attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset   = offsetof(ForgeGltfVertex, normal);
        attrs[2].location = 2;
        attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset   = offsetof(ForgeGltfVertex, uv);

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 3;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        /* Front-face culling for shadow bias. */
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_FRONT;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        /* Depth-only: write closest fragments for shadow comparison. */
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        /* No color output — this pass only produces the shadow depth map. */
        pi.target_info.num_color_targets        = 0;
        pi.target_info.depth_stencil_format     = SHADOW_DEPTH_FMT;
        pi.target_info.has_depth_stencil_target = true;

        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->shadow_pipeline) {
            SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Scene pipeline (2 color targets: color + view normals) ── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            scene_vert_spirv, sizeof(scene_vert_spirv),
            scene_vert_dxil, sizeof(scene_vert_dxil), 0, 1);
        /* 2 samplers: diffuse (slot 0), shadow (slot 1). */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            scene_frag_spirv, sizeof(scene_frag_spirv),
            scene_frag_dxil, sizeof(scene_frag_dxil), 2, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot       = 0;
        vb_desc.pitch      = sizeof(ForgeGltfVertex);
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[3];
        SDL_zero(attrs);
        attrs[0].location = 0;
        attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset   = offsetof(ForgeGltfVertex, position);
        attrs[1].location = 1;
        attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset   = offsetof(ForgeGltfVertex, normal);
        attrs[2].location = 2;
        attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset   = offsetof(ForgeGltfVertex, uv);

        /* MRT: Target 0 = scene color, Target 1 = view normals. */
        SDL_GPUColorTargetDescription color_descs[2];
        SDL_zero(color_descs);
        /* LDR scene color — sufficient for non-HDR forward shading. */
        color_descs[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        /* Float16 preserves negative view-space normals without clamping. */
        color_descs[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 3;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        /* Back-face cull for solid closed meshes. */
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        /* Standard depth test so geometry occludes correctly. */
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions  = color_descs;
        pi.target_info.num_color_targets          = 2;
        /* 32-bit depth gives SSAO enough precision to reconstruct positions. */
        pi.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.target_info.has_depth_stencil_target   = true;

        state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->scene_pipeline) {
            SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid pipeline (2 color targets: color + view normals) ── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_vert_spirv, sizeof(grid_vert_spirv),
            grid_vert_dxil, sizeof(grid_vert_dxil), 0, 1);
        /* 1 sampler: shadow (slot 0). */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_frag_spirv, sizeof(grid_frag_spirv),
            grid_frag_dxil, sizeof(grid_frag_dxil), 1, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot       = 0;
        vb_desc.pitch      = sizeof(float) * 3;
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attr;
        SDL_zero(attr);
        attr.location = 0;
        attr.format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attr.offset   = 0;

        /* MRT: same as scene pipeline. */
        SDL_GPUColorTargetDescription color_descs[2];
        SDL_zero(color_descs);
        color_descs[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        color_descs[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = &attr;
        pi.vertex_input_state.num_vertex_attributes      = 1;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        /* No culling — grid quad is visible from above and below. */
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        /* Depth-tested so grid occludes with scene geometry. */
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions  = color_descs;
        pi.target_info.num_color_targets          = 2;
        pi.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.target_info.has_depth_stencil_target   = true;

        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->grid_pipeline) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── SSAO pipeline (fullscreen quad → R8_UNORM) ─────────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
            fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
        /* 3 samplers: normals (0), depth (1), noise (2). 1 UBO. */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            ssao_frag_spirv, sizeof(ssao_frag_spirv),
            ssao_frag_dxil, sizeof(ssao_frag_dxil), 3, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        /* Single-channel: AO is a grayscale [0,1] occlusion factor. */
        color_desc.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        /* Fullscreen post-process — no geometry depth needed. */
        pi.depth_stencil_state.enable_depth_test  = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.has_depth_stencil_target   = false;

        state->ssao_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->ssao_pipeline) {
            SDL_Log("Failed to create SSAO pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Blur pipeline (fullscreen quad → R8_UNORM) ─────────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
            fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
        /* 1 sampler: raw SSAO (0). 1 UBO. */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            blur_frag_spirv, sizeof(blur_frag_spirv),
            blur_frag_dxil, sizeof(blur_frag_dxil), 1, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        /* Matches SSAO output — blur stays single-channel R8. */
        color_desc.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        /* Fullscreen post-process — no geometry depth needed. */
        pi.depth_stencil_state.enable_depth_test  = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.has_depth_stencil_target   = false;

        state->blur_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->blur_pipeline) {
            SDL_Log("Failed to create blur pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Composite pipeline (fullscreen quad → swapchain) ───────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
            fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
        /* 2 samplers: scene color (0), blurred AO (1). 1 UBO. */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            composite_frag_spirv, sizeof(composite_frag_spirv),
            composite_frag_dxil, sizeof(composite_frag_dxil), 2, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        /* Must match the window swapchain for final presentation. */
        color_desc.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        /* Final blit to swapchain — no depth involvement. */
        pi.depth_stencil_state.enable_depth_test  = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.has_depth_stencil_target   = false;

        state->composite_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->composite_pipeline) {
            SDL_Log("Failed to create composite pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid geometry (flat quad on XZ plane) ──────────────────── */
    {
        float verts[] = {
            -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
            -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
        };
        Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vertex_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, verts, sizeof(verts));
        state->grid_index_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));

        if (!state->grid_vertex_buffer || !state->grid_index_buffer)
            goto init_fail;
    }

    /* ── Shadow depth texture (2048x2048) ───────────────────────── */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type                = SDL_GPU_TEXTURETYPE_2D;
        ti.format              = SHADOW_DEPTH_FMT;
        ti.width               = SHADOW_MAP_SIZE;
        ti.height              = SHADOW_MAP_SIZE;
        ti.layer_count_or_depth = 1;
        ti.num_levels          = 1;
        ti.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                 SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->shadow_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->shadow_depth) {
            SDL_Log("Failed to create shadow depth texture: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Geometry pass render targets (fixed size) ──────────────── */
    {
        /* Scene color (R8G8B8A8_UNORM). */
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type                = SDL_GPU_TEXTURETYPE_2D;
        ti.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ti.width               = WINDOW_WIDTH;
        ti.height              = WINDOW_HEIGHT;
        ti.layer_count_or_depth = 1;
        ti.num_levels          = 1;
        ti.usage               = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                 SDL_GPU_TEXTUREUSAGE_SAMPLER;

        state->scene_color = SDL_CreateGPUTexture(device, &ti);
        if (!state->scene_color) {
            SDL_Log("Failed to create scene_color: %s", SDL_GetError());
            goto init_fail;
        }

        /* View normals (R16G16B16A16_FLOAT). */
        ti.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        state->view_normals = SDL_CreateGPUTexture(device, &ti);
        if (!state->view_normals) {
            SDL_Log("Failed to create view_normals: %s", SDL_GetError());
            goto init_fail;
        }

        /* Scene depth (D32_FLOAT) — SAMPLER + DEPTH_STENCIL_TARGET. */
        ti.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        ti.usage  = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                    SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->scene_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->scene_depth) {
            SDL_Log("Failed to create scene_depth: %s", SDL_GetError());
            goto init_fail;
        }

        /* SSAO raw (R8_UNORM). */
        ti.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        ti.usage  = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                    SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->ssao_raw = SDL_CreateGPUTexture(device, &ti);
        if (!state->ssao_raw) {
            SDL_Log("Failed to create ssao_raw: %s", SDL_GetError());
            goto init_fail;
        }

        /* SSAO blurred (R8_UNORM). */
        state->ssao_blurred = SDL_CreateGPUTexture(device, &ti);
        if (!state->ssao_blurred) {
            SDL_Log("Failed to create ssao_blurred: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Noise texture (4x4 random rotations) ──────────────────── */
    state->noise_texture = create_noise_texture(device);
    if (!state->noise_texture) goto init_fail;

    /* ── SSAO kernel ────────────────────────────────────────────── */
    generate_ssao_kernel(state->ssao_kernel);

    /* ── Box placements ─────────────────────────────────────────── */
    {
        const vec3 positions[BOX_COUNT] = {
            { -3.5f, 0.5f,  2.0f },
            { -2.5f, 0.5f,  0.5f },
            {  3.0f, 0.5f, -2.0f },
            { -1.0f, 0.5f, -3.0f },
            { -3.5f, 1.5f,  2.0f },
        };
        const float rotations[BOX_COUNT] = {
            0.3f, 1.1f, 0.7f, 2.0f, 0.9f
        };

        for (int i = 0; i < BOX_COUNT; i++) {
            state->box_placements[i].position   = positions[i];
            state->box_placements[i].y_rotation = rotations[i];
        }
    }

    /* ── Directional light view-projection (orthographic) ──────── */
    {
        vec3 light_dir_v = vec3_normalize(
            vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
        /* Position the light "above" the scene looking down the light direction. */
        vec3 light_pos = vec3_scale(light_dir_v, -LIGHT_DISTANCE);
        vec3 light_target = vec3_create(0.0f, 0.0f, 0.0f);
        vec3 light_up = vec3_create(0.0f, 1.0f, 0.0f);
        /* Avoid degenerate up vector if light is nearly vertical. */
        if (SDL_fabsf(vec3_dot(light_dir_v, light_up)) > PARALLEL_THRESHOLD)
            light_up = vec3_create(0.0f, 0.0f, 1.0f);

        mat4 light_view = mat4_look_at(light_pos, light_target, light_up);
        mat4 light_proj = mat4_orthographic(
            -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
            -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
            SHADOW_NEAR, SHADOW_FAR);
        state->light_vp = mat4_multiply(light_proj, light_view);
    }

    /* ── Camera initial state ───────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = CAM_START_YAW_DEG * FORGE_DEG2RAD;
    state->cam_pitch    = CAM_START_PITCH_DEG * FORGE_DEG2RAD;

    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        goto init_fail;
    }
    state->mouse_captured = true;

    state->last_ticks = SDL_GetPerformanceCounter();

    return SDL_APP_CONTINUE;

init_fail:
    /* SDL_AppQuit is called even when SDL_AppInit returns failure, and
     * *appstate was set right after allocation, so SDL_AppQuit handles
     * all resource cleanup via its NULL-checked release sequence. */
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ───────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key = event->key.key;

        if (key == SDLK_ESCAPE) {
            if (state->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                    return SDL_APP_FAILURE;
                }
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
        }

        /* Display mode switching. */
        if (key == SDLK_1) state->display_mode = MODE_AO_ONLY;
        if (key == SDLK_2) state->display_mode = MODE_WITH_AO;
        if (key == SDLK_3) state->display_mode = MODE_NO_AO;

        /* Toggle IGN dithering. */
        if (key == SDLK_D) {
            state->use_dither     = !state->use_dither;
            state->use_ign_jitter = !state->use_ign_jitter;
        }
    }

    /* Re-capture mouse on click. */
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
        if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
            SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        state->mouse_captured = true;
    }

    /* Mouse look. */
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->mouse_captured) {
        state->cam_yaw   -= event->motion.xrel * MOUSE_SENS;
        state->cam_pitch -= event->motion.yrel * MOUSE_SENS;
        if (state->cam_pitch > PITCH_CLAMP) state->cam_pitch = PITCH_CLAMP;
        if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── Delta time ────────────────────────────────────────────────── */
    Uint64 now  = SDL_GetPerformanceCounter();
    float  freq = (float)SDL_GetPerformanceFrequency();
    float  dt   = (float)(now - state->last_ticks) / freq;
    state->last_ticks = now;
    if (dt > MAX_FRAME_DT) dt = MAX_FRAME_DT;

    /* ── Keyboard movement ─────────────────────────────────────────── */
    {
        const bool *keys = SDL_GetKeyboardState(NULL);
        if (state->mouse_captured) {
            quat orientation = quat_from_euler(
                state->cam_yaw, state->cam_pitch, 0.0f);
            vec3 forward = quat_forward(orientation);
            vec3 right   = quat_right(orientation);
            vec3 up      = vec3_create(0.0f, 1.0f, 0.0f);
            float speed  = CAM_SPEED * dt;

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
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(up, speed));
            if (keys[SDL_SCANCODE_LSHIFT])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(up, -speed));
        }
    }

    /* ── Camera matrices ───────────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view   = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    mat4 proj   = mat4_perspective(
        (float)FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);
    mat4 inv_proj = mat4_inverse(proj);

    /* ── Acquire swapchain ─────────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw, sh;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain_tex, &sw, &sh)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_CancelGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_FAILURE;
    }
    if (!swapchain_tex) {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        return SDL_APP_CONTINUE;
    }

    /* ══ PASS 1: Shadow pass ═══════════════════════════════════════ */
    {
        SDL_GPUDepthStencilTargetInfo shadow_dti;
        SDL_zero(shadow_dti);
        shadow_dti.texture     = state->shadow_depth;
        shadow_dti.load_op     = SDL_GPU_LOADOP_CLEAR;
        shadow_dti.store_op    = SDL_GPU_STOREOP_STORE;
        shadow_dti.clear_depth = 1.0f;

        SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &shadow_dti);
        if (!shadow_pass) {
            SDL_Log("SDL_BeginGPURenderPass (shadow) failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

        mat4 truck_placement = mat4_identity();
        draw_model_shadow(shadow_pass, cmd, &state->truck,
                          &truck_placement, &state->light_vp);

        for (int i = 0; i < BOX_COUNT; i++) {
            BoxPlacement *bp = &state->box_placements[i];
            mat4 box_placement = mat4_multiply(
                mat4_translate(bp->position),
                mat4_rotate_y(bp->y_rotation));
            draw_model_shadow(shadow_pass, cmd, &state->box,
                              &box_placement, &state->light_vp);
        }

        SDL_EndGPURenderPass(shadow_pass);
    }

    /* ══ PASS 2: Geometry pass (MRT: color + view normals + depth) ═ */
    {
        SDL_GPUColorTargetInfo color_targets[2];
        SDL_zero(color_targets);

        color_targets[0].texture     = state->scene_color;
        color_targets[0].load_op     = SDL_GPU_LOADOP_CLEAR;
        color_targets[0].store_op    = SDL_GPU_STOREOP_STORE;
        color_targets[0].clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

        color_targets[1].texture     = state->view_normals;
        color_targets[1].load_op     = SDL_GPU_LOADOP_CLEAR;
        color_targets[1].store_op    = SDL_GPU_STOREOP_STORE;
        color_targets[1].clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 0.0f };

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture     = state->scene_depth;
        depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op    = SDL_GPU_STOREOP_STORE;
        depth_target.clear_depth = 1.0f;

        SDL_GPURenderPass *geo_pass = SDL_BeginGPURenderPass(
            cmd, color_targets, 2, &depth_target);
        if (!geo_pass) {
            SDL_Log("SDL_BeginGPURenderPass (geometry) failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        /* ── Draw grid ─────────────────────────────────────────── */
        SDL_BindGPUGraphicsPipeline(geo_pass, state->grid_pipeline);
        {
            GridVertUniforms grid_vu;
            grid_vu.vp       = cam_vp;
            grid_vu.view     = view;
            grid_vu.light_vp = state->light_vp;
            SDL_PushGPUVertexUniformData(cmd, 0, &grid_vu, sizeof(grid_vu));

            GridFragUniforms grid_fu;
            SDL_zero(grid_fu);
            grid_fu.line_color[0] = GRID_LINE_R;
            grid_fu.line_color[1] = GRID_LINE_G;
            grid_fu.line_color[2] = GRID_LINE_B;
            grid_fu.line_color[3] = 1.0f;
            grid_fu.bg_color[0]   = GRID_BG_R;
            grid_fu.bg_color[1]   = GRID_BG_G;
            grid_fu.bg_color[2]   = GRID_BG_B;
            grid_fu.bg_color[3]   = 1.0f;
            grid_fu.eye_pos[0]    = state->cam_position.x;
            grid_fu.eye_pos[1]    = state->cam_position.y;
            grid_fu.eye_pos[2]    = state->cam_position.z;
            grid_fu.grid_spacing  = GRID_SPACING;
            grid_fu.line_width    = GRID_LINE_WIDTH;
            grid_fu.fade_distance = GRID_FADE_DISTANCE;
            grid_fu.ambient       = MATERIAL_AMBIENT;
            grid_fu.light_intensity = LIGHT_INTENSITY;
            grid_fu.light_dir[0]  = LIGHT_DIR_X;
            grid_fu.light_dir[1]  = LIGHT_DIR_Y;
            grid_fu.light_dir[2]  = LIGHT_DIR_Z;
            grid_fu.light_dir[3]  = 0.0f;
            grid_fu.light_color[0] = LIGHT_COLOR_R;
            grid_fu.light_color[1] = LIGHT_COLOR_G;
            grid_fu.light_color[2] = LIGHT_COLOR_B;

            SDL_PushGPUFragmentUniformData(cmd, 0, &grid_fu, sizeof(grid_fu));

            SDL_GPUTextureSamplerBinding grid_tex_binds[1];
            grid_tex_binds[0] = (SDL_GPUTextureSamplerBinding){
                .texture = state->shadow_depth,
                .sampler = state->nearest_clamp };
            SDL_BindGPUFragmentSamplers(geo_pass, 0, grid_tex_binds, 1);

            SDL_GPUBufferBinding vb_bind = { state->grid_vertex_buffer, 0 };
            SDL_BindGPUVertexBuffers(geo_pass, 0, &vb_bind, 1);

            SDL_GPUBufferBinding ib_bind = { state->grid_index_buffer, 0 };
            SDL_BindGPUIndexBuffer(geo_pass, &ib_bind,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_DrawGPUIndexedPrimitives(geo_pass, GRID_INDEX_COUNT, 1, 0, 0, 0);
        }

        /* ── Draw scene models ─────────────────────────────────── */
        SDL_BindGPUGraphicsPipeline(geo_pass, state->scene_pipeline);

        {
            mat4 truck_placement = mat4_identity();
            draw_model_scene(geo_pass, cmd, &state->truck, state,
                             &truck_placement, &cam_vp, &view);
        }

        for (int i = 0; i < BOX_COUNT; i++) {
            BoxPlacement *bp = &state->box_placements[i];
            mat4 box_placement = mat4_multiply(
                mat4_translate(bp->position),
                mat4_rotate_y(bp->y_rotation));
            draw_model_scene(geo_pass, cmd, &state->box, state,
                             &box_placement, &cam_vp, &view);
        }

        SDL_EndGPURenderPass(geo_pass);
    }

    /* ══ PASS 3: SSAO pass ═════════════════════════════════════════ */
    {
        SDL_GPUColorTargetInfo ssao_ct;
        SDL_zero(ssao_ct);
        ssao_ct.texture  = state->ssao_raw;
        ssao_ct.load_op  = SDL_GPU_LOADOP_CLEAR;
        ssao_ct.store_op = SDL_GPU_STOREOP_STORE;
        ssao_ct.clear_color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f };

        SDL_GPURenderPass *ssao_pass = SDL_BeginGPURenderPass(
            cmd, &ssao_ct, 1, NULL);
        if (!ssao_pass) {
            SDL_Log("SDL_BeginGPURenderPass (SSAO) failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(ssao_pass, state->ssao_pipeline);

        /* Push SSAO uniforms. */
        SSAOUniforms ssao_u;
        SDL_memcpy(ssao_u.samples, state->ssao_kernel,
                   sizeof(state->ssao_kernel));
        ssao_u.projection     = proj;
        ssao_u.inv_projection = inv_proj;
        ssao_u.noise_scale[0] = (float)WINDOW_WIDTH / (float)NOISE_TEX_SIZE;
        ssao_u.noise_scale[1] = (float)WINDOW_HEIGHT / (float)NOISE_TEX_SIZE;
        ssao_u.radius         = SSAO_RADIUS;
        ssao_u.bias           = SSAO_BIAS;
        ssao_u.use_ign_jitter = state->use_ign_jitter ? 1 : 0;
        ssao_u._pad[0] = 0.0f;
        ssao_u._pad[1] = 0.0f;
        ssao_u._pad[2] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &ssao_u, sizeof(ssao_u));

        /* Bind 3 samplers: normals, depth, noise. */
        SDL_GPUTextureSamplerBinding ssao_tex_binds[3];
        ssao_tex_binds[0] = (SDL_GPUTextureSamplerBinding){
            .texture = state->view_normals,
            .sampler = state->nearest_clamp };
        ssao_tex_binds[1] = (SDL_GPUTextureSamplerBinding){
            .texture = state->scene_depth,
            .sampler = state->nearest_clamp };
        ssao_tex_binds[2] = (SDL_GPUTextureSamplerBinding){
            .texture = state->noise_texture,
            .sampler = state->nearest_repeat };
        SDL_BindGPUFragmentSamplers(ssao_pass, 0, ssao_tex_binds, 3);

        SDL_DrawGPUPrimitives(ssao_pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);

        SDL_EndGPURenderPass(ssao_pass);
    }

    /* ══ PASS 4: Blur pass ═════════════════════════════════════════ */
    {
        SDL_GPUColorTargetInfo blur_ct;
        SDL_zero(blur_ct);
        blur_ct.texture  = state->ssao_blurred;
        blur_ct.load_op  = SDL_GPU_LOADOP_CLEAR;
        blur_ct.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *blur_pass = SDL_BeginGPURenderPass(
            cmd, &blur_ct, 1, NULL);
        if (!blur_pass) {
            SDL_Log("SDL_BeginGPURenderPass (blur) failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(blur_pass, state->blur_pipeline);

        BlurUniforms blur_u;
        blur_u.texel_size[0] = 1.0f / (float)WINDOW_WIDTH;
        blur_u.texel_size[1] = 1.0f / (float)WINDOW_HEIGHT;
        blur_u._pad[0] = 0.0f;
        blur_u._pad[1] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &blur_u, sizeof(blur_u));

        SDL_GPUTextureSamplerBinding blur_tex_bind = {
            .texture = state->ssao_raw,
            .sampler = state->nearest_clamp
        };
        SDL_BindGPUFragmentSamplers(blur_pass, 0, &blur_tex_bind, 1);

        SDL_DrawGPUPrimitives(blur_pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);

        SDL_EndGPURenderPass(blur_pass);
    }

    /* ══ PASS 5: Composite pass ════════════════════════════════════ */
    {
        SDL_GPUColorTargetInfo comp_ct;
        SDL_zero(comp_ct);
        comp_ct.texture  = swapchain_tex;
        comp_ct.load_op  = SDL_GPU_LOADOP_DONT_CARE;
        comp_ct.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *comp_pass = SDL_BeginGPURenderPass(
            cmd, &comp_ct, 1, NULL);
        if (!comp_pass) {
            SDL_Log("SDL_BeginGPURenderPass (composite) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(comp_pass, state->composite_pipeline);

        CompositeUniforms comp_u;
        comp_u.display_mode = state->display_mode;
        comp_u.use_dither   = state->use_dither ? 1 : 0;
        comp_u._pad[0] = 0.0f;
        comp_u._pad[1] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &comp_u, sizeof(comp_u));

        SDL_GPUTextureSamplerBinding comp_tex_binds[2];
        comp_tex_binds[0] = (SDL_GPUTextureSamplerBinding){
            .texture = state->scene_color,
            .sampler = state->linear_clamp };
        comp_tex_binds[1] = (SDL_GPUTextureSamplerBinding){
            .texture = state->ssao_blurred,
            .sampler = state->linear_clamp };
        SDL_BindGPUFragmentSamplers(comp_pass, 0, comp_tex_binds, 2);

        SDL_DrawGPUPrimitives(comp_pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);

        SDL_EndGPURenderPass(comp_pass);
    }

    /* ── Submit ─────────────────────────────────────────────────── */
#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain_tex)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
                return SDL_APP_FAILURE;
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
            return SDL_APP_FAILURE;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    app_state *state = (app_state *)appstate;
    if (!state) return;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    free_model_gpu(state->device, &state->truck);
    free_model_gpu(state->device, &state->box);

    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->ssao_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->ssao_pipeline);
    if (state->blur_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->blur_pipeline);
    if (state->composite_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->composite_pipeline);

    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(state->device, state->grid_vertex_buffer);
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(state->device, state->grid_index_buffer);

    if (state->white_texture)
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
    if (state->shadow_depth)
        SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->scene_color)
        SDL_ReleaseGPUTexture(state->device, state->scene_color);
    if (state->view_normals)
        SDL_ReleaseGPUTexture(state->device, state->view_normals);
    if (state->scene_depth)
        SDL_ReleaseGPUTexture(state->device, state->scene_depth);
    if (state->ssao_raw)
        SDL_ReleaseGPUTexture(state->device, state->ssao_raw);
    if (state->ssao_blurred)
        SDL_ReleaseGPUTexture(state->device, state->ssao_blurred);
    if (state->noise_texture)
        SDL_ReleaseGPUTexture(state->device, state->noise_texture);

    if (state->sampler)
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    if (state->nearest_clamp)
        SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->nearest_repeat)
        SDL_ReleaseGPUSampler(state->device, state->nearest_repeat);
    if (state->linear_clamp)
        SDL_ReleaseGPUSampler(state->device, state->linear_clamp);

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);
    SDL_free(state);
}
