/*
 * Lesson 13 — Instanced Rendering
 *
 * Draw many objects with a single draw call by passing per-instance transforms
 * through a vertex buffer.  Instead of pushing a model matrix uniform for each
 * object (requiring one draw call per object), ALL instance transforms are
 * packed into a second vertex buffer with SDL_GPU_VERTEXINPUTRATE_INSTANCE.
 * The GPU reads a new model matrix for every instance, placing each object at
 * its own position/rotation/scale — all in one draw call.
 *
 * This lesson renders TWO different glTF models (BoxTextured and Duck) in a
 * shared scene, demonstrating that instanced rendering works across multiple
 * meshes.  The scene contains ~36 boxes arranged in a grid with some stacked,
 * and ~8 ducks placed around and on top of the boxes.  3 draw calls render
 * ~44 objects (grid + boxes + ducks), vs. 44+ calls in the non-instanced
 * approach from earlier lessons.
 *
 * What's new compared to Lesson 12:
 *   - Per-instance vertex buffer (SDL_GPU_VERTEXINPUTRATE_INSTANCE)
 *   - Instance data as vertex attributes (model matrix in 4 × float4 columns)
 *   - Two vertex buffer slots on one pipeline (per-vertex + per-instance)
 *   - Loading and rendering two separate glTF models in one scene
 *   - Deterministic instance layout (computed from index, no randomness)
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline             (Lesson 02)
 *   - Push uniforms for matrices + fragment data             (Lesson 03)
 *   - Texture + sampler binding, mipmaps                     (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize         (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time        (Lesson 07)
 *   - glTF parsing, GPU upload, material handling            (Lesson 09)
 *   - Blinn-Phong lighting with normal transformation        (Lesson 10)
 *   - Procedural grid floor with fwidth anti-aliasing        (Lesson 12)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   Escape             — release mouse / quit
 *
 * Models: BoxTextured and Duck (loaded from shared assets/models/).
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
/* Grid shaders — procedural anti-aliased grid on a flat quad (from L12) */
#include "shaders/grid_vert_spirv.h"
#include "shaders/grid_frag_spirv.h"
#include "shaders/grid_vert_dxil.h"
#include "shaders/grid_frag_dxil.h"

/* Instanced shaders — per-instance model matrix + Blinn-Phong lighting */
#include "shaders/instanced_vert_spirv.h"
#include "shaders/instanced_frag_spirv.h"
#include "shaders/instanced_vert_dxil.h"
#include "shaders/instanced_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 13 Instanced Rendering"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Dark background — the grid lines pop against this dark blue-black surface.
 * Values are in linear space (SDR_LINEAR swapchain auto-converts to sRGB). */
#define CLEAR_R 0.0099f
#define CLEAR_G 0.0099f
#define CLEAR_B 0.0267f
#define CLEAR_A 1.0f

/* Depth buffer — same setup as Lesson 06-12. */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D16_UNORM

/* ── Grid pipeline constants (same as L12) ──────────────────────────────── */

#define GRID_NUM_VERTEX_ATTRIBUTES 1
#define GRID_VERTEX_PITCH          12   /* 3 floats * 4 bytes */

#define GRID_VERT_NUM_SAMPLERS         0
#define GRID_VERT_NUM_STORAGE_TEXTURES 0
#define GRID_VERT_NUM_STORAGE_BUFFERS  0
#define GRID_VERT_NUM_UNIFORM_BUFFERS  1

#define GRID_FRAG_NUM_SAMPLERS         0
#define GRID_FRAG_NUM_STORAGE_TEXTURES 0
#define GRID_FRAG_NUM_STORAGE_BUFFERS  0
#define GRID_FRAG_NUM_UNIFORM_BUFFERS  1

/* Grid geometry: a large quad on the XZ plane (Y=0). */
#define GRID_HALF_SIZE  50.0f
#define GRID_NUM_VERTS  4
#define GRID_NUM_INDICES 6

/* Grid appearance (linear space for SDR_LINEAR swapchain). */
#define GRID_LINE_R       0.068f
#define GRID_LINE_G       0.534f
#define GRID_LINE_B       0.932f
#define GRID_LINE_A       1.0f

#define GRID_BG_R         0.014f
#define GRID_BG_G         0.014f
#define GRID_BG_B         0.045f
#define GRID_BG_A         1.0f

#define GRID_SPACING      1.0f
#define GRID_LINE_WIDTH   0.02f
#define GRID_FADE_DIST    40.0f
#define GRID_AMBIENT      0.3f
#define GRID_SHININESS    32.0f
#define GRID_SPECULAR_STR 0.2f

/* ── Instanced pipeline constants ──────────────────────────────────────── */

/* Vertex attributes: 3 per-vertex + 4 per-instance = 7 total.
 * This is the core of instanced rendering: the pipeline declares BOTH
 * per-vertex and per-instance attributes in the same attribute array. */
#define INST_NUM_VERTEX_ATTRIBUTES 7

/* Two vertex buffer slots:
 *   Slot 0: per-vertex data  (position, normal, UV from the mesh)
 *   Slot 1: per-instance data (model matrix as 4 × float4 columns) */
#define INST_NUM_VERTEX_BUFFERS 2

/* Instance data: 4 × vec4 = 64 bytes per instance (one mat4). */
#define INSTANCE_DATA_PITCH 64

/* Instanced vertex shader: no samplers, no storage, 1 uniform (VP matrix). */
#define INST_VERT_NUM_SAMPLERS         0
#define INST_VERT_NUM_STORAGE_TEXTURES 0
#define INST_VERT_NUM_STORAGE_BUFFERS  0
#define INST_VERT_NUM_UNIFORM_BUFFERS  1

/* Instanced fragment shader: 1 sampler (diffuse), no storage, 1 uniform. */
#define INST_FRAG_NUM_SAMPLERS         1
#define INST_FRAG_NUM_STORAGE_TEXTURES 0
#define INST_FRAG_NUM_STORAGE_BUFFERS  0
#define INST_FRAG_NUM_UNIFORM_BUFFERS  1

/* ── Scene layout constants ───────────────────────────────────────────── */

/* Box grid: 6×6 ground layer + ~11 stacked on a second layer.
 * Spaced 3 units apart so the boxes don't overlap. */
#define BOX_GRID_COLS     6
#define BOX_GRID_ROWS     6
#define BOX_GRID_SPACING  3.0f
#define BOX_GROUND_Y      0.5f   /* half-box above ground */
#define BOX_STACK_Y       1.5f   /* second layer above ground */
#define BOX_STACK_COUNT   11     /* boxes in the stacked layer */
#define BOX_TOTAL_COUNT   (BOX_GRID_COLS * BOX_GRID_ROWS + BOX_STACK_COUNT)

/* Duck placement: 8 ducks at hand-picked positions around the scene.
 * The Duck glTF node hierarchy includes a 0.01 scale that is baked into
 * the instance matrix via mesh_base_transform, bringing the duck to its
 * intended size.  DUCK_SCALE adjusts the final size relative to the
 * boxes (~1 unit) — 0.5 makes ducks half the box height. */
/* Duck army: a large grid of ducks surrounding the boxes, demonstrating
 * that instanced rendering handles hundreds of objects in a single draw
 * call with no CPU bottleneck. */
#define DUCK_GRID_COLS    16
#define DUCK_GRID_ROWS    16
#define DUCK_COUNT        (DUCK_GRID_COLS * DUCK_GRID_ROWS)  /* 256 ducks */
#define DUCK_GRID_SPACING 2.0f
#define DUCK_SCALE        0.5f

/* ── Model paths ─────────────────────────────────────────────────────── */

#define BOX_MODEL_PATH   "assets/models/BoxTextured/BoxTextured.gltf"
#define DUCK_MODEL_PATH  "assets/models/Duck/Duck.gltf"
#define PATH_BUFFER_SIZE 512

/* ── Texture constants ────────────────────────────────────────────────── */

#define BYTES_PER_PIXEL  4
#define WHITE_TEX_DIM    1
#define WHITE_TEX_LAYERS 1
#define WHITE_TEX_LEVELS 1
#define WHITE_RGBA       255
#define MAX_LOD_UNLIMITED 1000.0f

/* ── Camera parameters ────────────────────────────────────────────────── */

/* Elevated 3/4 view looking at the box grid and ducks. */
#define CAM_START_X     12.0f
#define CAM_START_Y     8.0f
#define CAM_START_Z     12.0f
#define CAM_START_YAW   45.0f   /* degrees — look toward the center */
#define CAM_START_PITCH -25.0f  /* degrees — looking down at the scene */

#define MOVE_SPEED       5.0f   /* faster to navigate the larger scene */
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG    89.0f

#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  200.0f

#define MS_TO_SEC      1000.0f
#define MAX_DELTA_TIME 0.1f

/* ── Lighting parameters ──────────────────────────────────────────────── */

#define LIGHT_DIR_X 1.0f
#define LIGHT_DIR_Y 1.0f
#define LIGHT_DIR_Z 1.0f

#define MODEL_SHININESS     64.0f
#define MODEL_AMBIENT_STR   0.15f
#define MODEL_SPECULAR_STR  0.5f

/* ── Uniform data ────────────────────────────────────────────────────── */

/* Grid vertex uniforms: just the VP matrix (64 bytes). */
typedef struct GridVertUniforms {
    mat4 vp;
} GridVertUniforms;

/* Grid fragment uniforms — must match the HLSL cbuffer layout (96 bytes). */
typedef struct GridFragUniforms {
    float line_color[4];
    float bg_color[4];
    float light_dir[4];
    float eye_pos[4];
    float grid_spacing;
    float line_width;
    float fade_distance;
    float ambient;
    float shininess;
    float specular_str;
    float _pad0;
    float _pad1;
} GridFragUniforms;

/* Instanced vertex uniforms: VP matrix only (64 bytes).
 * The model matrix comes from the per-instance vertex buffer instead. */
typedef struct InstVertUniforms {
    mat4 vp;
} InstVertUniforms;

/* Instanced fragment uniforms: material + lighting (64 bytes, same as L12). */
typedef struct InstFragUniforms {
    float base_color[4];
    float light_dir[4];
    float eye_pos[4];
    Uint32 has_texture;
    float shininess;
    float ambient;
    float specular_str;
} InstFragUniforms;

/* ── Per-instance data (uploaded to vertex buffer slot 1) ────────────── */
/* Each instance gets its own 4×4 model matrix, stored as 4 contiguous
 * vec4 columns.  This matches the mat4 layout in forge_math.h. */
typedef struct InstanceData {
    mat4 model;
} InstanceData;

/* ── GPU-side scene data ─────────────────────────────────────────────── */

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

/* ── Per-model data ───────────────────────────────────────────────────── */
/* Groups all data for one loaded glTF model: CPU scene, GPU primitives/
 * materials, and instance buffer with transforms. */

typedef struct ModelData {
    ForgeGltfScene  scene;
    GpuPrimitive   *primitives;
    int             primitive_count;
    GpuMaterial    *materials;
    int             material_count;
    SDL_GPUBuffer  *instance_buffer;
    int             instance_count;
} ModelData;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window              *window;
    SDL_GPUDevice           *device;

    /* Two pipelines:
     *   grid_pipeline      — procedural grid floor (from L12)
     *   instanced_pipeline — instanced rendering for boxes and ducks */
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUGraphicsPipeline *instanced_pipeline;

    /* Grid geometry (from L12). */
    SDL_GPUBuffer           *grid_vertex_buffer;
    SDL_GPUBuffer           *grid_index_buffer;

    /* Shared resources */
    SDL_GPUTexture          *depth_texture;
    SDL_GPUSampler          *sampler;
    SDL_GPUTexture          *white_texture;
    Uint32                   depth_width;
    Uint32                   depth_height;

    /* Two models loaded from glTF */
    ModelData box;
    ModelData duck;

    /* Camera state */
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

/* ── Depth texture helper ────────────────────────────────────────────── */

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

/* ── Shader helper ───────────────────────────────────────────────────── */

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

/* ── GPU buffer upload helper ────────────────────────────────────────── */

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

/* ── Texture loading helper ──────────────────────────────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }
    SDL_Log("Loaded texture: %dx%d from '%s'", surface->w, surface->h, path);

    /* Convert to ABGR8888 (SDL's name for R8G8B8A8 bytes in memory). */
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert surface: %s", SDL_GetError());
        return NULL;
    }

    int tex_w = converted->w;
    int tex_h = converted->h;
    int num_levels = (int)forge_log2f((float)(tex_w > tex_h ? tex_w : tex_h)) + 1;

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

/* ── 1x1 white placeholder texture ──────────────────────────────────── */

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

/* ── Free GPU-side model resources ───────────────────────────────────── */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model)
{
    if (model->primitives) {
        for (int i = 0; i < model->primitive_count; i++) {
            if (model->primitives[i].vertex_buffer)
                SDL_ReleaseGPUBuffer(device, model->primitives[i].vertex_buffer);
            if (model->primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device, model->primitives[i].index_buffer);
        }
        SDL_free(model->primitives);
        model->primitives = NULL;
    }

    if (model->materials) {
        /* Avoid double-free on shared textures. */
        SDL_GPUTexture *released[FORGE_GLTF_MAX_IMAGES];
        int released_count = 0;
        SDL_memset(released, 0, sizeof(released));

        for (int i = 0; i < model->material_count; i++) {
            SDL_GPUTexture *tex = model->materials[i].texture;
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
        SDL_free(model->materials);
        model->materials = NULL;
    }

    if (model->instance_buffer) {
        SDL_ReleaseGPUBuffer(device, model->instance_buffer);
        model->instance_buffer = NULL;
    }
}

/* ── Upload parsed scene to GPU ──────────────────────────────────────── */

static bool upload_model_to_gpu(SDL_GPUDevice *device, ModelData *model,
                                SDL_GPUTexture *white_texture)
{
    ForgeGltfScene *scene = &model->scene;

    /* ── Upload primitives (vertex + index buffers) ─────────────────── */
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
        dst->index_count = src->index_count;
        dst->has_uvs = src->has_uvs;

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

    /* ── Load material textures ─────────────────────────────────────── */
    model->material_count = scene->material_count;
    model->materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!model->materials) {
        SDL_Log("Failed to allocate GPU materials");
        free_model_gpu(device, model);
        return false;
    }

    SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
    const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
    int loaded_count = 0;
    SDL_memset(loaded_textures, 0, sizeof(loaded_textures));
    SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

    for (int i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &model->materials[i];

        dst->base_color[0] = src->base_color[0];
        dst->base_color[1] = src->base_color[1];
        dst->base_color[2] = src->base_color[2];
        dst->base_color[3] = src->base_color[3];
        dst->has_texture = src->has_texture;
        dst->texture = NULL;

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
                    loaded_paths[loaded_count] = src->texture_path;
                    loaded_count++;
                } else {
                    dst->has_texture = false;
                }
            }
        }

        SDL_Log("  Material %d: '%s' color=(%.2f,%.2f,%.2f) tex=%s",
                i, src->name,
                dst->base_color[0], dst->base_color[1], dst->base_color[2],
                dst->has_texture ? "yes" : "no");
    }

    (void)white_texture;   /* Used at render time, not during upload */
    return true;
}

/* ── Upload grid geometry to GPU ─────────────────────────────────────── */

static bool upload_grid_geometry(SDL_GPUDevice *device, app_state *state)
{
    float vertices[GRID_NUM_VERTS * 3] = {
        -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
         GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
         GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
        -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
    };

    Uint16 indices[GRID_NUM_INDICES] = { 0, 1, 2, 0, 2, 3 };

    state->grid_vertex_buffer = upload_gpu_buffer(
        device, SDL_GPU_BUFFERUSAGE_VERTEX,
        vertices, sizeof(vertices));
    if (!state->grid_vertex_buffer) return false;

    state->grid_index_buffer = upload_gpu_buffer(
        device, SDL_GPU_BUFFERUSAGE_INDEX,
        indices, sizeof(indices));
    if (!state->grid_index_buffer) {
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
        state->grid_vertex_buffer = NULL;
        return false;
    }

    return true;
}

/* ── Generate box instance transforms ────────────────────────────────── */
/* Deterministic layout: 6×6 ground grid + 11 stacked boxes on a second
 * layer.  Each box gets a unique Y rotation derived from its index so
 * the scene looks varied but screenshots are reproducible. */

static void generate_box_instances(InstanceData *out, int *count)
{
    int idx = 0;

    /* Grid center offset so the grid is centered around the origin. */
    float offset_x = (float)(BOX_GRID_COLS - 1) * BOX_GRID_SPACING * 0.5f;
    float offset_z = (float)(BOX_GRID_ROWS - 1) * BOX_GRID_SPACING * 0.5f;

    /* Ground layer: 6×6 grid of boxes at y = BOX_GROUND_Y. */
    for (int row = 0; row < BOX_GRID_ROWS; row++) {
        for (int col = 0; col < BOX_GRID_COLS; col++) {
            float x = (float)col * BOX_GRID_SPACING - offset_x;
            float z = (float)row * BOX_GRID_SPACING - offset_z;

            /* Small unique Y rotation per box — deterministic from index. */
            float angle = (float)idx * 0.3f;

            mat4 t = mat4_translate(vec3_create(x, BOX_GROUND_Y, z));
            mat4 r = mat4_rotate_y(angle);
            out[idx].model = mat4_multiply(t, r);
            idx++;
        }
    }

    /* Stacked layer: 11 boxes placed on top of selected ground boxes.
     * We pick every 3rd ground box position for the second layer. */
    for (int i = 0; i < BOX_STACK_COUNT; i++) {
        int base_idx = i * 3;   /* every 3rd box */
        int base_row = base_idx / BOX_GRID_COLS;
        int base_col = base_idx % BOX_GRID_COLS;

        float x = (float)base_col * BOX_GRID_SPACING - offset_x;
        float z = (float)base_row * BOX_GRID_SPACING - offset_z;
        float angle = (float)idx * 0.5f;   /* different rotation pattern */

        mat4 t = mat4_translate(vec3_create(x, BOX_STACK_Y, z));
        mat4 r = mat4_rotate_y(angle);
        out[idx].model = mat4_multiply(t, r);
        idx++;
    }

    *count = idx;
}

/* ── Generate duck instance transforms ───────────────────────────────── */
/* 256 ducks in a 16×16 grid surrounding the boxes — a duck army that
 * demonstrates the power of instanced rendering.  All 256 ducks are
 * drawn in a single draw call.  The glTF node hierarchy (including the
 * 0.01 root scale) is baked in by setup_model via mesh_base_transform.
 * Each duck has a deterministic Y rotation for visual variety. */

static void generate_duck_instances(InstanceData *out, int *count)
{
    int idx = 0;

    /* Center the duck grid around the origin, offset so it surrounds
     * and overlaps with the box grid. */
    float offset_x = (float)(DUCK_GRID_COLS - 1) * DUCK_GRID_SPACING * 0.5f;
    float offset_z = (float)(DUCK_GRID_ROWS - 1) * DUCK_GRID_SPACING * 0.5f;

    for (int row = 0; row < DUCK_GRID_ROWS; row++) {
        for (int col = 0; col < DUCK_GRID_COLS; col++) {
            float x = (float)col * DUCK_GRID_SPACING - offset_x;
            float z = (float)row * DUCK_GRID_SPACING - offset_z;

            /* Each duck faces a unique direction — deterministic from
             * its index so screenshots are reproducible.  The golden
             * angle (≈2.4 radians) avoids repetitive patterns. */
            float yaw = (float)idx * 2.3998f;

            mat4 t = mat4_translate(vec3_create(x, 0.0f, z));
            mat4 r = mat4_rotate_y(yaw);
            mat4 s = mat4_scale(vec3_create(DUCK_SCALE, DUCK_SCALE, DUCK_SCALE));

            out[idx].model = mat4_multiply(t, mat4_multiply(r, s));
            idx++;
        }
    }

    *count = idx;
}

/* ── Load and set up one model ───────────────────────────────────────── */
/* Loads the glTF file, uploads geometry and textures to GPU, generates
 * instance transforms and uploads the instance buffer. */

static bool setup_model(SDL_GPUDevice *device, ModelData *model,
                        const char *gltf_path, const char *name,
                        SDL_GPUTexture *white_texture,
                        void (*gen_instances)(InstanceData *, int *))
{
    SDL_Log("Loading %s from '%s'...", name, gltf_path);

    if (!forge_gltf_load(gltf_path, &model->scene)) {
        SDL_Log("Failed to load %s from '%s'", name, gltf_path);
        return false;
    }

    SDL_Log("%s scene: %d nodes, %d meshes, %d primitives, %d materials",
            name,
            model->scene.node_count, model->scene.mesh_count,
            model->scene.primitive_count, model->scene.material_count);

    if (!upload_model_to_gpu(device, model, white_texture)) {
        SDL_Log("Failed to upload %s to GPU", name);
        forge_gltf_free(&model->scene);
        return false;
    }

    /* Generate placement transforms, then bake in the glTF node hierarchy.
     *
     * glTF models have a node hierarchy with transforms (translation,
     * rotation, scale) that position the mesh in the model's own coordinate
     * system.  For example, the Duck model has a root node with 0.01 scale.
     * In non-instanced rendering (L9-12), each draw uses the node's
     * world_transform as the model matrix.  With instancing, we need to
     * pre-multiply that transform into each instance's model matrix so the
     * vertex shader can use the instance matrix directly.
     *
     * We find the first node with a mesh and use its world_transform as the
     * "mesh base transform".  Final instance matrix = placement * base. */
    mat4 mesh_base_transform = mat4_identity();
    for (int ni = 0; ni < model->scene.node_count; ni++) {
        if (model->scene.nodes[ni].mesh_index >= 0) {
            mesh_base_transform = model->scene.nodes[ni].world_transform;
            break;
        }
    }

    /* Use whichever count is larger — duck grid is 256, boxes are 47. */
    InstanceData instances[DUCK_COUNT > BOX_TOTAL_COUNT ? DUCK_COUNT : BOX_TOTAL_COUNT];
    int instance_count = 0;
    gen_instances(instances, &instance_count);

    /* Bake the node hierarchy transform into every instance. */
    for (int i = 0; i < instance_count; i++) {
        instances[i].model = mat4_multiply(instances[i].model,
                                           mesh_base_transform);
    }

    Uint32 inst_size = (Uint32)(instance_count * (int)sizeof(InstanceData));
    model->instance_buffer = upload_gpu_buffer(
        device, SDL_GPU_BUFFERUSAGE_VERTEX, instances, inst_size);
    if (!model->instance_buffer) {
        SDL_Log("Failed to upload %s instance buffer", name);
        free_model_gpu(device, model);
        forge_gltf_free(&model->scene);
        return false;
    }
    model->instance_count = instance_count;

    SDL_Log("%s: %d instances uploaded (%u bytes)",
            name, instance_count, inst_size);

    return true;
}

/* ── Render all instances of a model ─────────────────────────────────── */
/* Binds the instanced pipeline, pushes per-material fragment uniforms,
 * and issues ONE instanced draw call per primitive.  This is the payoff
 * of instanced rendering: all N instances of each primitive are drawn
 * in a single call. */

static void render_instanced_model(SDL_GPURenderPass *pass,
                                   SDL_GPUCommandBuffer *cmd,
                                   const ModelData *model,
                                   const app_state *state,
                                   const vec3 *light_dir,
                                   const vec3 *cam_pos)
{
    const ForgeGltfScene *scene = &model->scene;

    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            const GpuPrimitive *prim = &model->primitives[prim_idx];

            if (!prim->vertex_buffer || !prim->index_buffer) continue;

            /* Set up fragment uniforms (material + lighting). */
            InstFragUniforms fu;
            SDL_GPUTexture *tex = state->white_texture;

            if (prim->material_index >= 0 &&
                prim->material_index < model->material_count) {
                const GpuMaterial *mat =
                    &model->materials[prim->material_index];

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

            fu.light_dir[0] = light_dir->x;
            fu.light_dir[1] = light_dir->y;
            fu.light_dir[2] = light_dir->z;
            fu.light_dir[3] = 0.0f;

            fu.eye_pos[0] = cam_pos->x;
            fu.eye_pos[1] = cam_pos->y;
            fu.eye_pos[2] = cam_pos->z;
            fu.eye_pos[3] = 0.0f;

            fu.shininess    = MODEL_SHININESS;
            fu.ambient      = MODEL_AMBIENT_STR;
            fu.specular_str = MODEL_SPECULAR_STR;

            SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

            /* Bind texture + sampler. */
            SDL_GPUTextureSamplerBinding tex_binding;
            SDL_zero(tex_binding);
            tex_binding.texture = tex;
            tex_binding.sampler = state->sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

            /* Bind TWO vertex buffers:
             *   Slot 0: per-vertex data (mesh geometry)
             *   Slot 1: per-instance data (model matrices)
             * This is the core pattern of instanced rendering. */
            SDL_GPUBufferBinding vb_bindings[INST_NUM_VERTEX_BUFFERS];
            SDL_zero(vb_bindings);
            vb_bindings[0].buffer = prim->vertex_buffer;   /* per-vertex  */
            vb_bindings[1].buffer = model->instance_buffer; /* per-instance */
            SDL_BindGPUVertexBuffers(pass, 0, vb_bindings,
                                     INST_NUM_VERTEX_BUFFERS);

            /* Bind index buffer. */
            SDL_GPUBufferBinding ib_binding;
            SDL_zero(ib_binding);
            ib_binding.buffer = prim->index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib_binding, prim->index_type);

            /* THE instanced draw call: render all instances at once.
             * The 2nd parameter is the instance count — the GPU loops
             * over the instance buffer, reading a new model matrix for
             * each instance, while reusing the same vertex/index data. */
            SDL_DrawGPUIndexedPrimitives(pass, prim->index_count,
                                         (Uint32)model->instance_count,
                                         0, 0, 0);
        }
    }
}

/* ── SDL_AppInit ─────────────────────────────────────────────────────── */

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
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
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

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

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

    /* ── 8. Allocate app state ────────────────────────────────────────── */
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

    /* ── 9. Load both glTF models ─────────────────────────────────────── */
    const char *base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* Build full paths for both models. */
    char box_path[PATH_BUFFER_SIZE];
    char duck_path[PATH_BUFFER_SIZE];
    int len;

    len = SDL_snprintf(box_path, sizeof(box_path), "%s%s",
                       base_path, BOX_MODEL_PATH);
    if (len < 0 || (size_t)len >= sizeof(box_path)) {
        SDL_Log("Box model path too long");
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    len = SDL_snprintf(duck_path, sizeof(duck_path), "%s%s",
                       base_path, DUCK_MODEL_PATH);
    if (len < 0 || (size_t)len >= sizeof(duck_path)) {
        SDL_Log("Duck model path too long");
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* Load BoxTextured with instance transforms. */
    if (!setup_model(device, &state->box, box_path, "BoxTextured",
                     white_texture, generate_box_instances)) {
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* Load Duck with instance transforms. */
    if (!setup_model(device, &state->duck, duck_path, "Duck",
                     white_texture, generate_duck_instances)) {
        free_model_gpu(device, &state->box);
        forge_gltf_free(&state->box.scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* ── 10. Upload grid geometry ─────────────────────────────────────── */
    if (!upload_grid_geometry(device, state)) {
        SDL_Log("Failed to upload grid geometry");
        free_model_gpu(device, &state->duck);
        forge_gltf_free(&state->duck.scene);
        free_model_gpu(device, &state->box);
        forge_gltf_free(&state->box.scene);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, white_texture);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    /* ── 11. Create grid shaders & pipeline (same as L12) ─────────────── */
    SDL_GPUShader *grid_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, grid_vert_spirv_size,
        grid_vert_dxil,  grid_vert_dxil_size,
        GRID_VERT_NUM_SAMPLERS,
        GRID_VERT_NUM_STORAGE_TEXTURES,
        GRID_VERT_NUM_STORAGE_BUFFERS,
        GRID_VERT_NUM_UNIFORM_BUFFERS);
    if (!grid_vs) goto fail_cleanup;

    SDL_GPUShader *grid_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, grid_frag_spirv_size,
        grid_frag_dxil,  grid_frag_dxil_size,
        GRID_FRAG_NUM_SAMPLERS,
        GRID_FRAG_NUM_STORAGE_TEXTURES,
        GRID_FRAG_NUM_STORAGE_BUFFERS,
        GRID_FRAG_NUM_UNIFORM_BUFFERS);
    if (!grid_fs) {
        SDL_ReleaseGPUShader(device, grid_vs);
        goto fail_cleanup;
    }

    /* Grid pipeline setup (identical to L12). */
    SDL_GPUVertexBufferDescription grid_vb_desc;
    SDL_zero(grid_vb_desc);
    grid_vb_desc.slot       = 0;
    grid_vb_desc.pitch      = GRID_VERTEX_PITCH;
    grid_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute grid_attrs[GRID_NUM_VERTEX_ATTRIBUTES];
    SDL_zero(grid_attrs);
    grid_attrs[0].location    = 0;
    grid_attrs[0].buffer_slot = 0;
    grid_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    grid_attrs[0].offset      = 0;

    SDL_GPUGraphicsPipelineCreateInfo grid_pipe_info;
    SDL_zero(grid_pipe_info);
    grid_pipe_info.vertex_shader   = grid_vs;
    grid_pipe_info.fragment_shader = grid_fs;

    grid_pipe_info.vertex_input_state.vertex_buffer_descriptions = &grid_vb_desc;
    grid_pipe_info.vertex_input_state.num_vertex_buffers          = 1;
    grid_pipe_info.vertex_input_state.vertex_attributes           = grid_attrs;
    grid_pipe_info.vertex_input_state.num_vertex_attributes       = GRID_NUM_VERTEX_ATTRIBUTES;

    grid_pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* No backface culling — grid visible from both sides. */
    grid_pipe_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    grid_pipe_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    grid_pipe_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    grid_pipe_info.depth_stencil_state.enable_depth_test  = true;
    grid_pipe_info.depth_stencil_state.enable_depth_write = true;
    grid_pipe_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_GPUColorTargetDescription grid_color_desc;
    SDL_zero(grid_color_desc);
    grid_color_desc.format = swapchain_format;

    grid_pipe_info.target_info.color_target_descriptions = &grid_color_desc;
    grid_pipe_info.target_info.num_color_targets         = 1;
    grid_pipe_info.target_info.has_depth_stencil_target  = true;
    grid_pipe_info.target_info.depth_stencil_format      = DEPTH_FORMAT;

    state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(
        device, &grid_pipe_info);
    if (!state->grid_pipeline) {
        SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, grid_fs);
        SDL_ReleaseGPUShader(device, grid_vs);
        goto fail_cleanup;
    }

    SDL_ReleaseGPUShader(device, grid_fs);
    SDL_ReleaseGPUShader(device, grid_vs);

    /* ── 12. Create instanced shaders & pipeline ──────────────────────── */
    SDL_GPUShader *inst_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        instanced_vert_spirv, instanced_vert_spirv_size,
        instanced_vert_dxil,  instanced_vert_dxil_size,
        INST_VERT_NUM_SAMPLERS,
        INST_VERT_NUM_STORAGE_TEXTURES,
        INST_VERT_NUM_STORAGE_BUFFERS,
        INST_VERT_NUM_UNIFORM_BUFFERS);
    if (!inst_vs) {
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
        goto fail_cleanup;
    }

    SDL_GPUShader *inst_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        instanced_frag_spirv, instanced_frag_spirv_size,
        instanced_frag_dxil,  instanced_frag_dxil_size,
        INST_FRAG_NUM_SAMPLERS,
        INST_FRAG_NUM_STORAGE_TEXTURES,
        INST_FRAG_NUM_STORAGE_BUFFERS,
        INST_FRAG_NUM_UNIFORM_BUFFERS);
    if (!inst_fs) {
        SDL_ReleaseGPUShader(device, inst_vs);
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
        goto fail_cleanup;
    }

    /* Instanced pipeline: TWO vertex buffer slots.
     * This is what makes instanced rendering work — the pipeline declares
     * that slot 0 advances per-vertex and slot 1 advances per-instance. */
    SDL_GPUVertexBufferDescription inst_vb_descs[INST_NUM_VERTEX_BUFFERS];
    SDL_zero(inst_vb_descs);

    /* Slot 0: per-vertex data from the mesh (position, normal, UV). */
    inst_vb_descs[0].slot       = 0;
    inst_vb_descs[0].pitch      = sizeof(ForgeGltfVertex);
    inst_vb_descs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    /* Slot 1: per-instance data (4 × float4 = one mat4 model matrix).
     * VERTEXINPUTRATE_INSTANCE tells the GPU to advance this buffer once
     * per instance, not once per vertex.  Every vertex in an instance
     * sees the same mat4 — the whole point of instanced rendering.
     * Note: SDL3 GPU requires instance_step_rate = 0 (the default from
     * SDL_zero); the input rate flag alone controls advancement. */
    inst_vb_descs[1].slot       = 1;
    inst_vb_descs[1].pitch      = INSTANCE_DATA_PITCH;
    inst_vb_descs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

    /* Seven vertex attributes: 3 per-vertex + 4 per-instance. */
    SDL_GPUVertexAttribute inst_attrs[INST_NUM_VERTEX_ATTRIBUTES];
    SDL_zero(inst_attrs);

    /* Per-vertex: position (float3) at location 0, slot 0 */
    inst_attrs[0].location    = 0;
    inst_attrs[0].buffer_slot = 0;
    inst_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    inst_attrs[0].offset      = offsetof(ForgeGltfVertex, position);

    /* Per-vertex: normal (float3) at location 1, slot 0 */
    inst_attrs[1].location    = 1;
    inst_attrs[1].buffer_slot = 0;
    inst_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    inst_attrs[1].offset      = offsetof(ForgeGltfVertex, normal);

    /* Per-vertex: UV (float2) at location 2, slot 0 */
    inst_attrs[2].location    = 2;
    inst_attrs[2].buffer_slot = 0;
    inst_attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    inst_attrs[2].offset      = offsetof(ForgeGltfVertex, uv);

    /* Per-instance: model matrix column 0 (float4) at location 3, slot 1.
     * mat4 in forge_math.h stores 4 columns × 4 floats contiguously:
     *   columns[0] at offset  0 (16 bytes)
     *   columns[1] at offset 16 (16 bytes)
     *   columns[2] at offset 32 (16 bytes)
     *   columns[3] at offset 48 (16 bytes) */
    inst_attrs[3].location    = 3;
    inst_attrs[3].buffer_slot = 1;
    inst_attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    inst_attrs[3].offset      = 0;

    /* Per-instance: model matrix column 1 */
    inst_attrs[4].location    = 4;
    inst_attrs[4].buffer_slot = 1;
    inst_attrs[4].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    inst_attrs[4].offset      = 16;

    /* Per-instance: model matrix column 2 */
    inst_attrs[5].location    = 5;
    inst_attrs[5].buffer_slot = 1;
    inst_attrs[5].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    inst_attrs[5].offset      = 32;

    /* Per-instance: model matrix column 3 */
    inst_attrs[6].location    = 6;
    inst_attrs[6].buffer_slot = 1;
    inst_attrs[6].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    inst_attrs[6].offset      = 48;

    SDL_GPUGraphicsPipelineCreateInfo inst_pipe_info;
    SDL_zero(inst_pipe_info);
    inst_pipe_info.vertex_shader   = inst_vs;
    inst_pipe_info.fragment_shader = inst_fs;

    inst_pipe_info.vertex_input_state.vertex_buffer_descriptions = inst_vb_descs;
    inst_pipe_info.vertex_input_state.num_vertex_buffers          = INST_NUM_VERTEX_BUFFERS;
    inst_pipe_info.vertex_input_state.vertex_attributes           = inst_attrs;
    inst_pipe_info.vertex_input_state.num_vertex_attributes       = INST_NUM_VERTEX_ATTRIBUTES;

    inst_pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Back-face culling for solid models. */
    inst_pipe_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    inst_pipe_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    inst_pipe_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    inst_pipe_info.depth_stencil_state.enable_depth_test  = true;
    inst_pipe_info.depth_stencil_state.enable_depth_write = true;
    inst_pipe_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_GPUColorTargetDescription inst_color_desc;
    SDL_zero(inst_color_desc);
    inst_color_desc.format = swapchain_format;

    inst_pipe_info.target_info.color_target_descriptions = &inst_color_desc;
    inst_pipe_info.target_info.num_color_targets         = 1;
    inst_pipe_info.target_info.has_depth_stencil_target  = true;
    inst_pipe_info.target_info.depth_stencil_format      = DEPTH_FORMAT;

    state->instanced_pipeline = SDL_CreateGPUGraphicsPipeline(
        device, &inst_pipe_info);
    if (!state->instanced_pipeline) {
        SDL_Log("Failed to create instanced pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, inst_fs);
        SDL_ReleaseGPUShader(device, inst_vs);
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
        goto fail_cleanup;
    }

    SDL_ReleaseGPUShader(device, inst_fs);
    SDL_ReleaseGPUShader(device, inst_vs);

    /* ── 13. Camera and input setup ───────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = CAM_START_YAW * FORGE_DEG2RAD;
    state->cam_pitch    = CAM_START_PITCH * FORGE_DEG2RAD;
    state->last_ticks   = SDL_GetTicks();

#ifndef FORGE_CAPTURE
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, state->instanced_pipeline);
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
        goto fail_cleanup;
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
            SDL_ReleaseGPUGraphicsPipeline(device, state->instanced_pipeline);
            SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
            goto fail_cleanup;
        }
    }
#endif

    *appstate = state;

    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");
    SDL_Log("Scene: %d boxes + %d ducks = %d instances, 3 draw calls",
            state->box.instance_count, state->duck.instance_count,
            state->box.instance_count + state->duck.instance_count);

    return SDL_APP_CONTINUE;

fail_cleanup:
    /* Common cleanup path for initialization failures after state allocation. */
    SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    free_model_gpu(device, &state->duck);
    forge_gltf_free(&state->duck.scene);
    free_model_gpu(device, &state->box);
    forge_gltf_free(&state->box.scene);
    SDL_ReleaseGPUSampler(device, sampler);
    SDL_ReleaseGPUTexture(device, white_texture);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

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

/* ── SDL_AppIterate ──────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute delta time ────────────────────────────────────────── */
    Uint64 now_ms = SDL_GetTicks();
    float dt = (float)(now_ms - state->last_ticks) / MS_TO_SEC;
    state->last_ticks = now_ms;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── 2. Process keyboard input ────────────────────────────────────── */
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
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
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

        /* Pre-compute shared lighting data. */
        vec3 light_raw = vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z);
        vec3 light_dir = vec3_normalize(light_raw);

        /* ── Draw 1: Procedural grid ──────────────────────────────────── */
        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

        GridVertUniforms gvu;
        gvu.vp = vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));

        GridFragUniforms gfu;
        gfu.line_color[0] = GRID_LINE_R;
        gfu.line_color[1] = GRID_LINE_G;
        gfu.line_color[2] = GRID_LINE_B;
        gfu.line_color[3] = GRID_LINE_A;

        gfu.bg_color[0] = GRID_BG_R;
        gfu.bg_color[1] = GRID_BG_G;
        gfu.bg_color[2] = GRID_BG_B;
        gfu.bg_color[3] = GRID_BG_A;

        gfu.light_dir[0] = light_dir.x;
        gfu.light_dir[1] = light_dir.y;
        gfu.light_dir[2] = light_dir.z;
        gfu.light_dir[3] = 0.0f;

        gfu.eye_pos[0] = state->cam_position.x;
        gfu.eye_pos[1] = state->cam_position.y;
        gfu.eye_pos[2] = state->cam_position.z;
        gfu.eye_pos[3] = 0.0f;

        gfu.grid_spacing  = GRID_SPACING;
        gfu.line_width    = GRID_LINE_WIDTH;
        gfu.fade_distance = GRID_FADE_DIST;
        gfu.ambient       = GRID_AMBIENT;
        gfu.shininess     = GRID_SHININESS;
        gfu.specular_str  = GRID_SPECULAR_STR;
        gfu._pad0         = 0.0f;
        gfu._pad1         = 0.0f;

        SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

        SDL_GPUBufferBinding grid_vb;
        SDL_zero(grid_vb);
        grid_vb.buffer = state->grid_vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &grid_vb, 1);

        SDL_GPUBufferBinding grid_ib;
        SDL_zero(grid_ib);
        grid_ib.buffer = state->grid_index_buffer;
        SDL_BindGPUIndexBuffer(pass, &grid_ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES, 1, 0, 0, 0);

        /* ── Draw 2+3: Instanced models ───────────────────────────────── */
        /* Switch to the instanced pipeline.  Both boxes and ducks use
         * the same pipeline — different vertex/instance/texture bindings. */
        SDL_BindGPUGraphicsPipeline(pass, state->instanced_pipeline);

        /* Push shared VP matrix for all instanced draws. */
        InstVertUniforms ivu;
        ivu.vp = vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &ivu, sizeof(ivu));

        /* Draw 2: All boxes (one instanced call per primitive). */
        render_instanced_model(pass, cmd, &state->box, state,
                               &light_dir, &state->cam_position);

        /* Draw 3: All ducks (one instanced call per primitive). */
        render_instanced_model(pass, cmd, &state->duck, state,
                               &light_dir, &state->cam_position);

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

/* ── SDL_AppQuit ─────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (state) {
#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, state->device);
#endif
        free_model_gpu(state->device, &state->duck);
        forge_gltf_free(&state->duck.scene);
        free_model_gpu(state->device, &state->box);
        forge_gltf_free(&state->box.scene);
        SDL_ReleaseGPUBuffer(state->device, state->grid_index_buffer);
        SDL_ReleaseGPUBuffer(state->device, state->grid_vertex_buffer);
        SDL_ReleaseGPUSampler(state->device, state->sampler);
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->instanced_pipeline);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
