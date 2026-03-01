/*
 * GPU Lesson 29 — Screen-Space Reflections (SSR)
 *
 * Screen-space reflections approximate specular reflections by ray marching
 * through the depth buffer along the reflected view direction. For each
 * pixel, the shader reflects the view ray around the surface normal, then
 * steps along that reflected ray in screen space until it intersects scene
 * geometry (i.e. the ray's depth exceeds the stored depth). When a hit is
 * found, the scene color at that screen position becomes the reflection.
 *
 * Architecture — 4 render passes per frame:
 *   1. Shadow pass    — directional light depth map (2048x2048)
 *   2. Geometry pass  — lit color + view normals + world position + depth (MRT)
 *   3. SSR pass       — fullscreen ray marching in screen space
 *   4. Composite pass — blends SSR reflections with scene color
 *
 * Controls:
 *   1                       — Final render (SSR composited)
 *   2                       — SSR reflection only
 *   3                       — View-space normals
 *   4                       — Depth buffer
 *   5                       — World-space position
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

#include "shaders/compiled/ssr_frag_spirv.h"
#include "shaders/compiled/ssr_frag_dxil.h"
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
#define CAM_START_X         4.0f
#define CAM_START_Y         3.0f
#define CAM_START_Z         7.0f
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

/* SSR parameters. */
#define SSR_MAX_DISTANCE    20.0f  /* max view-space ray travel (units)    */
#define SSR_STEP_SIZE       0.15f  /* view-space distance per march step   */
#define SSR_MAX_STEPS       128    /* max iterations (128*0.15 = 19.2 max) */
#define SSR_THICKNESS       0.15f  /* depth tolerance for hit detection    */
#define SSR_REFLECTION_STR  0.8f   /* global reflection blend strength   */
#define GRID_REFLECTIVITY   0.9f   /* how reflective the grid floor is   */

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
#define BOX_COUNT 8

/* Texture sampler — trilinear filtering with anisotropy. */
#define MAX_ANISOTROPY 4
#define BYTES_PER_PIXEL 4

/* Display modes. */
#define MODE_FINAL     0
#define MODE_SSR_ONLY  1
#define MODE_NORMALS   2
#define MODE_DEPTH     3
#define MODE_WORLD_POS 4

/* Light direction degeneracy — skip if light is nearly parallel to up. */
#define PARALLEL_THRESHOLD 0.99f

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
    float reflectivity;     /* SSR reflection strength for the grid  */
} GridFragUniforms;

/* SSR fragment uniforms — passed to the ray marching shader. */
typedef struct SSRUniforms {
    mat4  projection;      /* camera projection matrix          */
    mat4  inv_projection;  /* inverse projection for pos recon  */
    mat4  view;            /* camera view matrix                */
    float screen_width;
    float screen_height;
    float step_size;       /* ray march step size               */
    float max_distance;    /* max ray travel distance           */
    int   max_steps;       /* max ray march iterations          */
    float thickness;       /* depth comparison threshold        */
    float _pad[2];         /* align to 16 bytes                 */
} SSRUniforms;

/* Composite fragment uniforms. */
typedef struct CompositeUniforms {
    int   display_mode;    /* MODE_FINAL / MODE_SSR_ONLY / etc. */
    float reflection_str;  /* global reflection strength [0..1] */
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
    SDL_GPUGraphicsPipeline *scene_pipeline;     /* Blinn-Phong + shadow MRT pass      */
    SDL_GPUGraphicsPipeline *grid_pipeline;      /* procedural grid MRT pass           */
    SDL_GPUGraphicsPipeline *shadow_pipeline;    /* depth-only shadow map pass         */
    SDL_GPUGraphicsPipeline *ssr_pipeline;       /* screen-space reflection pass       */
    SDL_GPUGraphicsPipeline *composite_pipeline; /* SSR + scene color to swapchain     */

    /* Geometry pass render targets. */
    SDL_GPUTexture *scene_color;      /* R8G8B8A8_UNORM — lit color          */
    SDL_GPUTexture *view_normals;     /* R16G16B16A16_FLOAT — view normals   */
    SDL_GPUTexture *world_position;   /* R16G16B16A16_FLOAT — world position */
    SDL_GPUTexture *scene_depth;      /* D32_FLOAT — depth buffer            */

    /* SSR render target. */
    SDL_GPUTexture *ssr_output;       /* R8G8B8A8_UNORM — SSR color output   */

    /* Shadow map. */
    SDL_GPUTexture *shadow_depth;     /* D32_FLOAT — directional shadow      */

    /* Samplers. */
    SDL_GPUSampler *sampler;          /* trilinear + anisotropy (textures) */
    SDL_GPUSampler *nearest_clamp;    /* nearest, clamp (G-buffer reads)   */
    SDL_GPUSampler *linear_clamp;     /* linear, clamp (SSR/composite)     */

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

    /* Swapchain format. */
    SDL_GPUTextureFormat swapchain_format; /* queried after swapchain setup */

    /* Camera. */
    vec3  cam_position; /* world-space camera position                */
    float cam_yaw;      /* horizontal rotation (radians, 0 = +Z)     */
    float cam_pitch;    /* vertical rotation (radians, clamped +-1.5) */

    /* Display mode. */
    int display_mode; /* MODE_FINAL, MODE_SSR_ONLY, etc. */

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
        "Lesson 29 \xe2\x80\x94 Screen-Space Reflections",
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

    /* Set appstate early so SDL_AppQuit can clean up on init failure. */
    *appstate = state;

    state->window           = window;
    state->device           = device;
    state->swapchain_format = swapchain_format;
    state->display_mode     = MODE_FINAL;

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
        /* Nearest, clamp — for G-buffer reads (normals, depth, position). */
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
        /* Linear, clamp — for SSR and composite reads. */
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

    /* ── Scene pipeline (3 color targets: color + view normals + world pos) ── */
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

        /* MRT: Target 0 = scene color, Target 1 = view normals,
         * Target 2 = world-space position. */
        SDL_GPUColorTargetDescription color_descs[3];
        SDL_zero(color_descs);
        /* LDR scene color — sufficient for non-HDR forward shading. */
        color_descs[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        /* Float16 preserves negative view-space normals without clamping. */
        color_descs[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        /* Float16 for world-space position — needs range beyond [0,1]. */
        color_descs[2].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

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
        pi.target_info.num_color_targets          = 3;
        /* 32-bit depth gives SSR enough precision to reconstruct positions. */
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

    /* ── Grid pipeline (3 color targets: color + view normals + world pos) ── */
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

        /* MRT: same 3 targets as scene pipeline. */
        SDL_GPUColorTargetDescription color_descs[3];
        SDL_zero(color_descs);
        color_descs[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        color_descs[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        color_descs[2].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

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
        pi.target_info.num_color_targets          = 3;
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

    /* ── SSR pipeline (fullscreen quad -> R8G8B8A8_UNORM) ────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
            fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
        /* 4 samplers: scene color (0), depth (1), view normals (2),
         * world position (3). 1 UBO for SSR params. */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            ssr_frag_spirv, sizeof(ssr_frag_spirv),
            ssr_frag_dxil, sizeof(ssr_frag_dxil), 4, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        /* SSR output stores the reflected color — RGBA is sufficient. */
        color_desc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

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

        state->ssr_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->ssr_pipeline) {
            SDL_Log("Failed to create SSR pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Composite pipeline (fullscreen quad -> swapchain) ───────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
            fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
        /* 5 samplers: scene color (0), SSR output (1), view normals (2),
         * depth (3), world position (4). 1 UBO for display mode. */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            composite_frag_spirv, sizeof(composite_frag_spirv),
            composite_frag_dxil, sizeof(composite_frag_dxil), 5, 1);
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

        /* World-space position (R16G16B16A16_FLOAT). */
        state->world_position = SDL_CreateGPUTexture(device, &ti);
        if (!state->world_position) {
            SDL_Log("Failed to create world_position: %s", SDL_GetError());
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

        /* SSR output (R8G8B8A8_UNORM). */
        ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ti.usage  = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                    SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->ssr_output = SDL_CreateGPUTexture(device, &ti);
        if (!state->ssr_output) {
            SDL_Log("Failed to create ssr_output: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Box placements ─────────────────────────────────────────── */
    {
        const vec3 positions[BOX_COUNT] = {
            { -3.5f, 0.5f,  2.0f },
            { -2.5f, 0.5f,  0.5f },
            {  3.0f, 0.5f, -2.0f },
            { -1.0f, 0.5f, -3.0f },
            { -3.5f, 1.5f,  2.0f },
            {  4.0f, 0.5f,  1.5f },
            { -4.5f, 0.5f, -1.0f },
            {  2.0f, 0.5f,  3.5f },
        };
        const float rotations[BOX_COUNT] = {
            0.3f, 1.1f, 0.7f, 2.0f, 0.9f, 1.5f, 0.2f, 2.5f
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

        /* Display mode switching: 1-5 for different debug views. */
        if (key == SDLK_1) state->display_mode = MODE_FINAL;
        if (key == SDLK_2) state->display_mode = MODE_SSR_ONLY;
        if (key == SDLK_3) state->display_mode = MODE_NORMALS;
        if (key == SDLK_4) state->display_mode = MODE_DEPTH;
        if (key == SDLK_5) state->display_mode = MODE_WORLD_POS;
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

    /* ══ PASS 2: Geometry pass (MRT: color + view normals + world pos + depth) ═ */
    {
        SDL_GPUColorTargetInfo color_targets[3];
        SDL_zero(color_targets);

        color_targets[0].texture     = state->scene_color;
        color_targets[0].load_op     = SDL_GPU_LOADOP_CLEAR;
        color_targets[0].store_op    = SDL_GPU_STOREOP_STORE;
        color_targets[0].clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

        color_targets[1].texture     = state->view_normals;
        color_targets[1].load_op     = SDL_GPU_LOADOP_CLEAR;
        color_targets[1].store_op    = SDL_GPU_STOREOP_STORE;
        color_targets[1].clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 0.0f };

        color_targets[2].texture     = state->world_position;
        color_targets[2].load_op     = SDL_GPU_LOADOP_CLEAR;
        color_targets[2].store_op    = SDL_GPU_STOREOP_STORE;
        color_targets[2].clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 0.0f };

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture     = state->scene_depth;
        depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op    = SDL_GPU_STOREOP_STORE;
        depth_target.clear_depth = 1.0f;

        SDL_GPURenderPass *geo_pass = SDL_BeginGPURenderPass(
            cmd, color_targets, 3, &depth_target);
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
            grid_fu.reflectivity   = GRID_REFLECTIVITY;

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

    /* ══ PASS 3: SSR pass ═════════════════════════════════════════ */
    {
        SDL_GPUColorTargetInfo ssr_ct;
        SDL_zero(ssr_ct);
        ssr_ct.texture  = state->ssr_output;
        ssr_ct.load_op  = SDL_GPU_LOADOP_CLEAR;
        ssr_ct.store_op = SDL_GPU_STOREOP_STORE;
        ssr_ct.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 0.0f };

        SDL_GPURenderPass *ssr_pass = SDL_BeginGPURenderPass(
            cmd, &ssr_ct, 1, NULL);
        if (!ssr_pass) {
            SDL_Log("SDL_BeginGPURenderPass (SSR) failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(ssr_pass, state->ssr_pipeline);

        /* Push SSR uniforms. */
        SSRUniforms ssr_u;
        ssr_u.projection     = proj;
        ssr_u.inv_projection = inv_proj;
        ssr_u.view           = view;
        ssr_u.screen_width   = (float)WINDOW_WIDTH;
        ssr_u.screen_height  = (float)WINDOW_HEIGHT;
        ssr_u.step_size      = SSR_STEP_SIZE;
        ssr_u.max_distance   = SSR_MAX_DISTANCE;
        ssr_u.max_steps      = SSR_MAX_STEPS;
        ssr_u.thickness      = SSR_THICKNESS;
        ssr_u._pad[0] = 0.0f;
        ssr_u._pad[1] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &ssr_u, sizeof(ssr_u));

        /* Bind 4 samplers: scene color, depth, view normals, world position. */
        SDL_GPUTextureSamplerBinding ssr_tex_binds[4];
        ssr_tex_binds[0] = (SDL_GPUTextureSamplerBinding){
            .texture = state->scene_color,
            .sampler = state->linear_clamp };
        ssr_tex_binds[1] = (SDL_GPUTextureSamplerBinding){
            .texture = state->scene_depth,
            .sampler = state->nearest_clamp };
        ssr_tex_binds[2] = (SDL_GPUTextureSamplerBinding){
            .texture = state->view_normals,
            .sampler = state->nearest_clamp };
        ssr_tex_binds[3] = (SDL_GPUTextureSamplerBinding){
            .texture = state->world_position,
            .sampler = state->nearest_clamp };
        SDL_BindGPUFragmentSamplers(ssr_pass, 0, ssr_tex_binds, 4);

        SDL_DrawGPUPrimitives(ssr_pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);

        SDL_EndGPURenderPass(ssr_pass);
    }

    /* ══ PASS 4: Composite pass ════════════════════════════════════ */
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
        comp_u.display_mode    = state->display_mode;
        comp_u.reflection_str  = SSR_REFLECTION_STR;
        comp_u._pad[0] = 0.0f;
        comp_u._pad[1] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &comp_u, sizeof(comp_u));

        /* Bind 5 samplers: scene color (0), SSR output (1), depth (2),
         * view normals (3), world position (4). Must match shader slots. */
        SDL_GPUTextureSamplerBinding comp_tex_binds[5];
        comp_tex_binds[0] = (SDL_GPUTextureSamplerBinding){
            .texture = state->scene_color,
            .sampler = state->linear_clamp };
        comp_tex_binds[1] = (SDL_GPUTextureSamplerBinding){
            .texture = state->ssr_output,
            .sampler = state->linear_clamp };
        comp_tex_binds[2] = (SDL_GPUTextureSamplerBinding){
            .texture = state->scene_depth,
            .sampler = state->nearest_clamp };
        comp_tex_binds[3] = (SDL_GPUTextureSamplerBinding){
            .texture = state->view_normals,
            .sampler = state->nearest_clamp };
        comp_tex_binds[4] = (SDL_GPUTextureSamplerBinding){
            .texture = state->world_position,
            .sampler = state->nearest_clamp };
        SDL_BindGPUFragmentSamplers(comp_pass, 0, comp_tex_binds, 5);

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
    if (state->ssr_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->ssr_pipeline);
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
    if (state->world_position)
        SDL_ReleaseGPUTexture(state->device, state->world_position);
    if (state->scene_depth)
        SDL_ReleaseGPUTexture(state->device, state->scene_depth);
    if (state->ssr_output)
        SDL_ReleaseGPUTexture(state->device, state->ssr_output);

    if (state->sampler)
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    if (state->nearest_clamp)
        SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->linear_clamp)
        SDL_ReleaseGPUSampler(state->device, state->linear_clamp);

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);
    SDL_free(state);
}
