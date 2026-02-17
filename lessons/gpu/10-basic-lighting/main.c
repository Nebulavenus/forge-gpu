/*
 * Lesson 10 — Basic Lighting (Blinn-Phong)
 *
 * Add real lighting to a 3D model!  Lesson 09 loaded glTF scenes but used
 * flat shading — normals were passed through but never used.  This lesson
 * introduces Blinn-Phong lighting, the most fundamental real-time lighting
 * model, and uses those normals to shade Suzanne (the Blender monkey head).
 *
 * The three components of Blinn-Phong:
 *   1. Ambient  — constant minimum brightness (fakes indirect light)
 *   2. Diffuse  — Lambert's cosine law: surfaces facing the light are bright
 *   3. Specular — shiny highlight via the Blinn half-vector
 *
 * What's new compared to Lesson 09:
 *   - Two vertex uniforms: MVP (clip space) + Model (world space)
 *   - Fragment uniforms: light direction, camera position, material params
 *   - Normal transformation: adjugate transpose via cross products
 *   - Per-pixel normalize of interpolated normals in the fragment shader
 *   - Single model (Suzanne) instead of multi-model scene selector
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline             (Lesson 02)
 *   - Push uniforms for matrices + fragment data             (Lesson 03)
 *   - Texture + sampler binding, mipmaps                     (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize         (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time        (Lesson 07)
 *   - glTF parsing, GPU upload, material handling            (Lesson 09)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   Escape             — release mouse / quit
 *
 * Model: Suzanne (loaded from shared assets/models/Suzanne/).
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include "math/forge_math.h"
#include "gltf/forge_gltf.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
#include "shaders/lighting_vert_spirv.h"
#include "shaders/lighting_frag_spirv.h"
#include "shaders/lighting_vert_dxil.h"
#include "shaders/lighting_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 10 Basic Lighting (Blinn-Phong)"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Dark clear color so the lit model stands out. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.04f
#define CLEAR_A 1.0f

/* Depth buffer — same setup as Lesson 06-09. */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D16_UNORM

/* Vertex attributes: position (float3) + normal (float3) + uv (float2). */
#define NUM_VERTEX_ATTRIBUTES 3

/* Shader resource counts.
 * Vertex:   0 samplers, 0 storage tex, 0 storage buf, 1 uniform buf (MVP+Model)
 * Fragment: 1 sampler (diffuse), 0 storage tex, 0 storage buf, 1 uniform buf */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1

#define FRAG_NUM_SAMPLERS         1
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  1

/* Default glTF file — relative to executable directory.
 * Suzanne lives in the shared assets directory. */
#define DEFAULT_MODEL_PATH "assets/models/Suzanne/Suzanne.gltf"
#define PATH_BUFFER_SIZE   512

/* Bytes per pixel for RGBA textures. */
#define BYTES_PER_PIXEL 4

/* White placeholder texture — 1x1 fully opaque white. */
#define WHITE_TEX_DIM    1     /* width and height in pixels */
#define WHITE_TEX_LAYERS 1
#define WHITE_TEX_LEVELS 1
#define WHITE_RGBA       255   /* R, G, B, A — fully white, fully opaque */

/* Maximum LOD — effectively unlimited, standard GPU convention. */
#define MAX_LOD_UNLIMITED 1000.0f

/* ── Camera parameters ───────────────────────────────────────────────────── */

/* Start position: front of Suzanne, close enough to see lighting detail. */
#define CAM_START_X    0.0f
#define CAM_START_Y    0.0f
#define CAM_START_Z    4.0f
#define CAM_START_YAW  0.0f   /* degrees — looking toward -Z (at origin) */
#define CAM_START_PITCH 0.0f  /* degrees — level */

/* Movement speed (units per second). */
#define MOVE_SPEED 3.0f

/* Mouse sensitivity: radians per pixel. */
#define MOUSE_SENSITIVITY 0.002f

/* Pitch clamp to prevent flipping (same as Lesson 07). */
#define MAX_PITCH_DEG 89.0f

/* Perspective projection. */
#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

/* Time conversion and delta time clamping. */
#define MS_TO_SEC      1000.0f
#define MAX_DELTA_TIME 0.1f

/* ── Lighting parameters ─────────────────────────────────────────────────── */

/* Directional light from upper-right-front.  Direction points TOWARD the
 * light (i.e. from surface to light), which is the convention our shader
 * expects.  We normalize on the CPU before passing to the shader. */
#define LIGHT_DIR_X 1.0f
#define LIGHT_DIR_Y 1.0f
#define LIGHT_DIR_Z 1.0f

/* Blinn-Phong material parameters. */
#define SHININESS     64.0f   /* specular exponent — higher = tighter highlight */
#define AMBIENT_STR   0.15f   /* ambient intensity [0..1] */
#define SPECULAR_STR  0.5f    /* specular intensity [0..1] */

/* ── Uniform data ─────────────────────────────────────────────────────────── */

/* Vertex uniforms: two matrices (128 bytes).
 * mvp   — combined Model-View-Projection for clip-space output
 * model — the model (world) matrix for world-space lighting calculations */
typedef struct VertUniforms {
    mat4 mvp;
    mat4 model;
} VertUniforms;

/* Fragment uniforms must match the HLSL cbuffer layout exactly (64 bytes):
 *   float4 base_color    (16 bytes) — material base color
 *   float4 light_dir     (16 bytes) — world-space light direction (xyz)
 *   float4 eye_pos       (16 bytes) — world-space camera position (xyz)
 *   uint   has_texture    (4 bytes)
 *   float  shininess      (4 bytes)
 *   float  ambient        (4 bytes)
 *   float  specular       (4 bytes)
 * Total: 64 bytes, 16-byte aligned. */
typedef struct FragUniforms {
    float base_color[4];
    float light_dir[4];
    float eye_pos[4];
    Uint32 has_texture;
    float shininess;
    float ambient;
    float specular_str;
} FragUniforms;

/* ── GPU-side scene data ──────────────────────────────────────────────────── */
/* Same structure as Lesson 09 — after loading with forge_gltf.h, we upload
 * vertex/index data to GPU buffers and load textures. */

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
    SDL_GPUTexture *texture;   /* NULL = use placeholder white texture */
    bool           has_texture;
} GpuMaterial;

/* ── Application state ───────────────────────────────────────────────────── */

typedef struct app_state {
    /* GPU resources */
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUTexture          *depth_texture;
    SDL_GPUSampler          *sampler;
    SDL_GPUTexture          *white_texture;  /* 1x1 placeholder */
    Uint32                   depth_width;
    Uint32                   depth_height;

    /* Scene data: CPU-side from forge_gltf.h, GPU-side uploaded here. */
    ForgeGltfScene  scene;
    GpuPrimitive   *gpu_primitives;
    int             gpu_primitive_count;
    GpuMaterial    *gpu_materials;
    int             gpu_material_count;

    /* Camera state (same pattern as Lesson 07-09) */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Timing */
    Uint64 last_ticks;

    /* Input */
    bool mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;

/* ── Depth texture helper ────────────────────────────────────────────────── */
/* Same as Lesson 06-09 — creates a depth texture matching the window. */

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device,
                                             Uint32 w, Uint32 h)
{
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = DEPTH_FORMAT;
    info.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width                = w;
    info.height               = h;
    info.layer_count_or_depth = 1;
    info.num_levels           = 1;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &info);
    if (!texture) {
        SDL_Log("Failed to create depth texture (%ux%u): %s",
                w, h, SDL_GetError());
    }
    return texture;
}

/* ── Shader helper ───────────────────────────────────────────────────────── */
/* Same as Lesson 07-09 — creates a shader from SPIRV or DXIL bytecodes. */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice       *device,
    SDL_GPUShaderStage   stage,
    const unsigned char *spirv_code,  unsigned int spirv_size,
    const unsigned char *dxil_code,   unsigned int dxil_size,
    int                  num_samplers,
    int                  num_storage_textures,
    int                  num_storage_buffers,
    int                  num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage                = stage;
    info.entrypoint           = "main";
    info.num_samplers         = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers  = num_storage_buffers;
    info.num_uniform_buffers  = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else {
        SDL_Log("No supported shader format (need SPIRV or DXIL)");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("Failed to create %s shader: %s",
                stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
                SDL_GetError());
    }
    return shader;
}

/* ── GPU buffer upload helper ────────────────────────────────────────────── */
/* Creates a GPU buffer and uploads data via the transfer buffer pattern.
 * Same pattern as Lesson 09. */

static SDL_GPUBuffer *upload_gpu_buffer(SDL_GPUDevice *device,
                                        SDL_GPUBufferUsageFlags usage,
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

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for buffer upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = transfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size   = size;

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

/* ── Texture loading helper ──────────────────────────────────────────────── */
/* Same pattern as Lesson 08/09: load image -> convert to RGBA -> upload with
 * mipmaps.  Works with BMP, PNG, QOI, and JPG (SDL3). */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }
    SDL_Log("Loaded texture: %dx%d from '%s'", surface->w, surface->h, path);

    /* Convert to ABGR8888 (SDL's name for R8G8B8A8 bytes in memory).
     * See MEMORY.md: GPU R8G8B8A8 = SDL ABGR8888. */
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert surface: %s", SDL_GetError());
        return NULL;
    }

    int tex_w = converted->w;
    int tex_h = converted->h;
    int num_levels = (int)forge_log2f((float)(tex_w > tex_h ? tex_w : tex_h)) + 1;

    /* Create GPU texture with mip levels. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width                = (Uint32)tex_w;
    tex_info.height               = (Uint32)tex_h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = num_levels;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    /* Upload pixel data to GPU. */
    Uint32 total_bytes = (Uint32)(tex_w * tex_h * BYTES_PER_PIXEL);

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    /* Copy row-by-row to respect SDL_Surface.pitch (may have padding). */
    Uint32 dest_row_bytes = (Uint32)(tex_w * BYTES_PER_PIXEL);
    const Uint8 *row_src = (const Uint8 *)converted->pixels;
    Uint8 *row_dst = (Uint8 *)mapped;
    for (Uint32 row = 0; row < (Uint32)tex_h; row++) {
        SDL_memcpy(row_dst + row * dest_row_bytes,
                   row_src + row * converted->pitch,
                   dest_row_bytes);
    }
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(converted);

    /* Copy pass -> upload base level -> generate mipmaps. */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass for texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = transfer;
    tex_src.pixels_per_row  = (Uint32)tex_w;
    tex_src.rows_per_layer  = (Uint32)tex_h;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = texture;
    tex_dst.w       = (Uint32)tex_w;
    tex_dst.h       = (Uint32)tex_h;
    tex_dst.d       = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GenerateMipmapsForGPUTexture(cmd, texture);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return texture;
}

/* ── 1x1 white placeholder texture ──────────────────────────────────────── */
/* Materials without a texture still need a valid texture bound to the
 * fragment sampler.  We always bind this 1x1 white texture instead. */

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device)
{
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = WHITE_TEX_DIM;
    tex_info.height               = WHITE_TEX_DIM;
    tex_info.layer_count_or_depth = WHITE_TEX_LAYERS;
    tex_info.num_levels           = WHITE_TEX_LEVELS;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create white texture: %s", SDL_GetError());
        return NULL;
    }

    Uint8 white_pixel[BYTES_PER_PIXEL] = {
        WHITE_RGBA, WHITE_RGBA, WHITE_RGBA, WHITE_RGBA
    };

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = sizeof(white_pixel);

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create white texture transfer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map white texture transfer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_memcpy(mapped, white_pixel, sizeof(white_pixel));
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for white texture: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass for white texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = transfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = texture;
    dst.w = WHITE_TEX_DIM;
    dst.h = WHITE_TEX_DIM;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit white texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return texture;
}

/* ── Upload parsed scene to GPU ──────────────────────────────────────────── */
/* Forward declaration: free_gpu_scene is defined later but called on error */
static void free_gpu_scene(SDL_GPUDevice *device, app_state *state);

/* Takes the CPU-side data from forge_gltf_load() and creates GPU buffers
 * and textures.  Same pattern as Lesson 09. */

static bool upload_scene_to_gpu(SDL_GPUDevice *device, app_state *state)
{
    ForgeGltfScene *scene = &state->scene;

    /* ── Upload primitives (vertex + index buffers) ─────────────────── */
    state->gpu_primitive_count = scene->primitive_count;
    state->gpu_primitives = (GpuPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(GpuPrimitive));
    if (!state->gpu_primitives) {
        SDL_Log("Failed to allocate GPU primitives");
        return false;
    }

    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *src = &scene->primitives[i];
        GpuPrimitive *dst = &state->gpu_primitives[i];

        dst->material_index = src->material_index;
        dst->index_count = src->index_count;
        dst->has_uvs = src->has_uvs;

        /* Upload vertex buffer. */
        if (src->vertices && src->vertex_count > 0) {
            Uint32 vb_size = src->vertex_count * (Uint32)sizeof(ForgeGltfVertex);
            dst->vertex_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_VERTEX, src->vertices, vb_size);
            if (!dst->vertex_buffer) {
                free_gpu_scene(device, state);
                return false;
            }
        }

        /* Upload index buffer. */
        if (src->indices && src->index_count > 0) {
            Uint32 ib_size = src->index_count * src->index_stride;
            dst->index_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_INDEX, src->indices, ib_size);
            if (!dst->index_buffer) {
                free_gpu_scene(device, state);
                return false;
            }

            dst->index_type = (src->index_stride == 2)
                ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                : SDL_GPU_INDEXELEMENTSIZE_32BIT;
        }
    }

    /* ── Load material textures ─────────────────────────────────────── */
    state->gpu_material_count = scene->material_count;
    state->gpu_materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!state->gpu_materials) {
        SDL_Log("Failed to allocate GPU materials");
        free_gpu_scene(device, state);
        return false;
    }

    /* Track loaded textures to avoid loading the same image twice. */
    SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
    const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
    int loaded_count = 0;
    SDL_memset(loaded_textures, 0, sizeof(loaded_textures));
    SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

    for (int i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &state->gpu_materials[i];

        dst->base_color[0] = src->base_color[0];
        dst->base_color[1] = src->base_color[1];
        dst->base_color[2] = src->base_color[2];
        dst->base_color[3] = src->base_color[3];
        dst->has_texture = src->has_texture;
        dst->texture = NULL;

        if (src->has_texture && src->texture_path[0] != '\0') {
            /* Check if we already loaded this path. */
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
                    loaded_paths[loaded_count] = src->texture_path;
                    loaded_count++;
                } else {
                    /* Texture load failed — fall back to solid color. */
                    dst->has_texture = false;
                }
            }
        }

        SDL_Log("GPU Material %d: '%s' color=(%.2f,%.2f,%.2f) tex=%s",
                i, src->name,
                dst->base_color[0], dst->base_color[1], dst->base_color[2],
                dst->has_texture ? "yes" : "no");
    }

    return true;
}

/* ── Free GPU-side scene resources ───────────────────────────────────────── */

static void free_gpu_scene(SDL_GPUDevice *device, app_state *state)
{
    /* Release GPU buffers. */
    if (state->gpu_primitives) {
        for (int i = 0; i < state->gpu_primitive_count; i++) {
            if (state->gpu_primitives[i].vertex_buffer)
                SDL_ReleaseGPUBuffer(device, state->gpu_primitives[i].vertex_buffer);
            if (state->gpu_primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device, state->gpu_primitives[i].index_buffer);
        }
        SDL_free(state->gpu_primitives);
    }

    /* Release material textures (avoid double-free on shared textures). */
    if (state->gpu_materials) {
        SDL_GPUTexture *released[FORGE_GLTF_MAX_IMAGES];
        int released_count = 0;
        SDL_memset(released, 0, sizeof(released));

        for (int i = 0; i < state->gpu_material_count; i++) {
            SDL_GPUTexture *tex = state->gpu_materials[i].texture;
            if (!tex) continue;

            bool already = false;
            for (int j = 0; j < released_count; j++) {
                if (released[j] == tex) { already = true; break; }
            }
            if (!already && released_count < FORGE_GLTF_MAX_IMAGES) {
                SDL_ReleaseGPUTexture(device, tex);
                released[released_count++] = tex;
            }
        }
        SDL_free(state->gpu_materials);
    }
}

/* ── Render the scene with lighting ──────────────────────────────────────── */
/* Iterates all nodes, draws every primitive with the correct material,
 * and pushes lighting uniforms (model matrix, light dir, eye pos). */

static void render_scene(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd,
                         const app_state *state, const mat4 *vp,
                         const vec3 *cam_pos)
{
    const ForgeGltfScene *scene = &state->scene;

    /* Pre-compute normalized light direction (constant for all draws). */
    vec3 light_raw = vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z);
    vec3 light_dir = vec3_normalize(light_raw);

    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        /* Model matrix = this node's accumulated world transform. */
        mat4 model = node->world_transform;
        mat4 mvp = mat4_multiply(*vp, model);

        /* Push vertex uniforms: MVP + model matrix. */
        VertUniforms vu;
        vu.mvp   = mvp;
        vu.model = model;
        SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            const GpuPrimitive *prim = &state->gpu_primitives[prim_idx];

            if (!prim->vertex_buffer || !prim->index_buffer) continue;

            /* Set up fragment uniforms (material + lighting). */
            FragUniforms fu;
            SDL_GPUTexture *tex = state->white_texture;

            if (prim->material_index >= 0 &&
                prim->material_index < state->gpu_material_count) {
                const GpuMaterial *mat =
                    &state->gpu_materials[prim->material_index];

                fu.base_color[0] = mat->base_color[0];
                fu.base_color[1] = mat->base_color[1];
                fu.base_color[2] = mat->base_color[2];
                fu.base_color[3] = mat->base_color[3];
                fu.has_texture = mat->has_texture ? 1 : 0;
                if (mat->texture) tex = mat->texture;
            } else {
                fu.base_color[0] = 1.0f;
                fu.base_color[1] = 1.0f;
                fu.base_color[2] = 1.0f;
                fu.base_color[3] = 1.0f;
                fu.has_texture = 0;
            }

            /* Lighting parameters. */
            fu.light_dir[0] = light_dir.x;
            fu.light_dir[1] = light_dir.y;
            fu.light_dir[2] = light_dir.z;
            fu.light_dir[3] = 0.0f;  /* unused padding */

            fu.eye_pos[0] = cam_pos->x;
            fu.eye_pos[1] = cam_pos->y;
            fu.eye_pos[2] = cam_pos->z;
            fu.eye_pos[3] = 0.0f;  /* unused padding */

            fu.shininess    = SHININESS;
            fu.ambient      = AMBIENT_STR;
            fu.specular_str = SPECULAR_STR;

            SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

            /* Bind texture + sampler. */
            SDL_GPUTextureSamplerBinding tex_binding;
            SDL_zero(tex_binding);
            tex_binding.texture = tex;
            tex_binding.sampler = state->sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

            /* Bind vertex buffer. */
            SDL_GPUBufferBinding vb_binding;
            SDL_zero(vb_binding);
            vb_binding.buffer = prim->vertex_buffer;
            SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

            /* Bind index buffer and draw. */
            SDL_GPUBufferBinding ib_binding;
            SDL_zero(ib_binding);
            ib_binding.buffer = prim->index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib_binding, prim->index_type);

            SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
        }
    }
}

/* ── SDL_AppInit ─────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── 1. Initialise SDL ────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ─────────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV |
        SDL_GPU_SHADERFORMAT_DXIL,
        true,   /* debug mode */
        NULL    /* no backend preference */
    );
    if (!device) {
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create window & claim swapchain ───────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Request an sRGB swapchain ─────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 5. Create depth texture ──────────────────────────────────────── */
    int win_w = 0, win_h = 0;
    if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *depth_texture = create_depth_texture(
        device, (Uint32)win_w, (Uint32)win_h);
    if (!depth_texture) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 6. Create 1x1 white placeholder texture ─────────────────────── */
    SDL_GPUTexture *white_texture = create_white_texture(device);
    if (!white_texture) {
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 7. Create sampler ────────────────────────────────────────────── */
    /* Trilinear filtering with REPEAT address mode. */
    SDL_GPUSamplerCreateInfo smp_info;
    SDL_zero(smp_info);
    smp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
    smp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
    smp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    smp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    smp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    smp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    smp_info.min_lod        = 0.0f;
    smp_info.max_lod        = MAX_LOD_UNLIMITED;

    SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &smp_info);
    if (!sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Load Suzanne glTF model ───────────────────────────────────── */
    const char *base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    char gltf_path[PATH_BUFFER_SIZE];
    int path_len = SDL_snprintf(gltf_path, sizeof(gltf_path), "%s%s",
                                base_path, DEFAULT_MODEL_PATH);
    if (path_len < 0 || (size_t)path_len >= sizeof(gltf_path)) {
        SDL_Log("Model path too long or formatting error (len=%d, max=%u)",
                path_len, (unsigned)sizeof(gltf_path));
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Allocate state first so we can store the scene in it. */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window        = window;
    state->device        = device;
    state->depth_texture = depth_texture;
    state->sampler       = sampler;
    state->white_texture = white_texture;
    state->depth_width   = (Uint32)win_w;
    state->depth_height  = (Uint32)win_h;

    if (!forge_gltf_load(gltf_path, &state->scene)) {
        SDL_Log("Failed to load scene from '%s'", gltf_path);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    SDL_Log("Scene loaded: %d nodes, %d meshes, %d primitives, %d materials",
            state->scene.node_count, state->scene.mesh_count,
            state->scene.primitive_count, state->scene.material_count);

    /* ── 9. Upload parsed data to GPU ─────────────────────────────────── */
    if (!upload_scene_to_gpu(device, state)) {
        SDL_Log("Failed to upload scene to GPU");
        forge_gltf_free(&state->scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* ── 10. Create shaders ───────────────────────────────────────────── */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        lighting_vert_spirv, lighting_vert_spirv_size,
        lighting_vert_dxil,  lighting_vert_dxil_size,
        VERT_NUM_SAMPLERS,
        VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS,
        VERT_NUM_UNIFORM_BUFFERS);
    if (!vertex_shader) {
        free_gpu_scene(device, state);
        forge_gltf_free(&state->scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader *fragment_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        lighting_frag_spirv, lighting_frag_spirv_size,
        lighting_frag_dxil,  lighting_frag_dxil_size,
        FRAG_NUM_SAMPLERS,
        FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS,
        FRAG_NUM_UNIFORM_BUFFERS);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        free_gpu_scene(device, state);
        forge_gltf_free(&state->scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* ── 11. Create graphics pipeline ─────────────────────────────────── */
    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(ForgeGltfVertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    /* Location 0: position (float3) — maps to HLSL TEXCOORD0 */
    vertex_attributes[0].location    = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset      = offsetof(ForgeGltfVertex, position);

    /* Location 1: normal (float3) — maps to HLSL TEXCOORD1 */
    vertex_attributes[1].location    = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[1].offset      = offsetof(ForgeGltfVertex, normal);

    /* Location 2: uv (float2) — maps to HLSL TEXCOORD2 */
    vertex_attributes[2].location    = 2;
    vertex_attributes[2].buffer_slot = 0;
    vertex_attributes[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[2].offset      = offsetof(ForgeGltfVertex, uv);

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);

    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers          = 1;
    pipeline_info.vertex_input_state.vertex_attributes           = vertex_attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes       = NUM_VERTEX_ATTRIBUTES;

    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Back-face culling — same as Lesson 06-09. */
    pipeline_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth testing — same as Lesson 06-09. */
    pipeline_info.depth_stencil_state.enable_depth_test  = true;
    pipeline_info.depth_stencil_state.enable_depth_write = true;
    pipeline_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = SDL_GetGPUSwapchainTextureFormat(device, window);

    pipeline_info.target_info.color_target_descriptions = &color_target_desc;
    pipeline_info.target_info.num_color_targets         = 1;
    pipeline_info.target_info.has_depth_stencil_target  = true;
    pipeline_info.target_info.depth_stencil_format      = DEPTH_FORMAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(
        device, &pipeline_info);
    if (!pipeline) {
        SDL_Log("Failed to create graphics pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, fragment_shader);
        SDL_ReleaseGPUShader(device, vertex_shader);
        free_gpu_scene(device, state);
        forge_gltf_free(&state->scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* Shaders can be released after pipeline creation. */
    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    state->pipeline = pipeline;

    /* Initialize camera: facing -Z from (0,0,4), looking at the origin. */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = CAM_START_YAW * FORGE_DEG2RAD;
    state->cam_pitch    = CAM_START_PITCH * FORGE_DEG2RAD;
    state->last_ticks   = SDL_GetTicks();

    /* Capture mouse for FPS-style look. */
#ifndef FORGE_CAPTURE
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        free_gpu_scene(device, state);
        forge_gltf_free(&state->scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }
    state->mouse_captured = true;
#else
    state->mouse_captured = false;
#endif

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            free_gpu_scene(device, state);
            forge_gltf_free(&state->scene);
            SDL_ReleaseGPUSampler(device, sampler);
            SDL_ReleaseGPUTexture(device, white_texture);
            SDL_ReleaseGPUTexture(device, depth_texture);
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            SDL_free(state);
            return SDL_APP_FAILURE;
        }
    }
#endif

    *appstate = state;

    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");
    SDL_Log("Lighting: Blinn-Phong (ambient=%.2f, specular=%.2f, shininess=%.0f)",
            AMBIENT_STR, SPECULAR_STR, SHININESS);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────────── */
/* Same mouse/keyboard handling as Lesson 07-09. */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    /* Escape: release mouse or quit. */
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
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

    /* Click to recapture mouse. */
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
        if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
            SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
        state->mouse_captured = true;
    }

    /* Mouse motion: update camera yaw and pitch. */
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->mouse_captured) {
        state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
        state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;

        float max_pitch = MAX_PITCH_DEG * FORGE_DEG2RAD;
        if (state->cam_pitch >  max_pitch) state->cam_pitch =  max_pitch;
        if (state->cam_pitch < -max_pitch) state->cam_pitch = -max_pitch;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute delta time ────────────────────────────────────────── */
    Uint64 now_ms = SDL_GetTicks();
    float dt = (float)(now_ms - state->last_ticks) / MS_TO_SEC;
    state->last_ticks = now_ms;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── 2. Process keyboard input (same as Lesson 07-09) ────────────── */
    quat cam_orientation = quat_from_euler(
        state->cam_yaw, state->cam_pitch, 0.0f);

    vec3 forward = quat_forward(cam_orientation);
    vec3 right   = quat_right(cam_orientation);

    const bool *keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(forward, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(forward, -MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(right, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(right, -MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_SPACE]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_create(0.0f, MOVE_SPEED * dt, 0.0f));
    }
    if (keys[SDL_SCANCODE_LSHIFT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_create(0.0f, -MOVE_SPEED * dt, 0.0f));
    }

    /* ── 3. Build view-projection matrix ─────────────────────────────── */
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orientation);

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    float fov    = FOV_DEG * FORGE_DEG2RAD;
    mat4 proj    = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);

    mat4 vp = mat4_multiply(proj, view);

    /* ── 4. Handle window resize ──────────────────────────────────────── */
    Uint32 cur_w = (Uint32)w;
    Uint32 cur_h = (Uint32)h;

    if (cur_w != state->depth_width || cur_h != state->depth_height) {
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        state->depth_texture = create_depth_texture(state->device, cur_w, cur_h);
        if (!state->depth_texture) {
            return SDL_APP_FAILURE;
        }
        state->depth_width  = cur_w;
        state->depth_height = cur_h;
    }

    /* ── 5. Acquire command buffer ────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 6. Acquire swapchain & begin render pass ─────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain, NULL, NULL)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G,
                                                  CLEAR_B, CLEAR_A };

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture          = state->depth_texture;
        depth_target.load_op          = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op         = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
        depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.clear_depth      = DEPTH_CLEAR;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, &depth_target);
        if (!pass) {
            SDL_Log("Failed to begin render pass: %s", SDL_GetError());
            SDL_CancelGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

        /* Render the scene with lighting. */
        render_scene(pass, cmd, state, &vp, &state->cam_position);

        SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
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

/* ── SDL_AppQuit ─────────────────────────────────────────────────────────── */
/* Clean up in reverse order of creation. */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (state) {
#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, state->device);
#endif
        free_gpu_scene(state->device, state);
        forge_gltf_free(&state->scene);
        SDL_ReleaseGPUSampler(state->device, state->sampler);
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
