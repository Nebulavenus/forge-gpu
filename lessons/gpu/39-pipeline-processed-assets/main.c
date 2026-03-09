/*
 * Lesson 39 — Pipeline-Processed Assets
 *
 * Demonstrates loading and rendering .fmesh binary meshes and pipeline-
 * processed textures (PNG with .meta.json sidecars), comparing them against
 * raw glTF+PNG loading.  Three render modes let you see both paths side by
 * side: pipeline-only, raw-only, and split-screen comparison.
 *
 * Key concepts:
 *   - .fmesh binary mesh format with LOD levels and tangent data
 *   - Pipeline-processed textures via .meta.json sidecar metadata
 *   - Dual vertex formats: 48-byte (with tangents) vs 32-byte (no tangents)
 *   - Gram-Schmidt TBN re-orthogonalization for normal mapping
 *   - LOD selection based on camera distance
 *   - Split-screen comparison of pipeline vs raw rendering
 *
 * Controls:
 *   WASD / Mouse  — move/look
 *   Space / Shift — fly up/down
 *   1             — pipeline-only mode
 *   2             — raw-only mode
 *   3             — split-screen comparison
 *   L             — cycle LOD level
 *   I             — toggle info overlay
 *   Escape        — release mouse cursor
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include <string.h>    /* memset   */
#include <stdio.h>     /* snprintf */
#include <math.h>      /* sqrtf    */

#include "math/forge_math.h"
#include "gltf/forge_gltf.h"

/* Pipeline runtime library — .fmesh + texture loader.
 * Implementation is compiled here (exactly one .c file). */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecode ─────────────────────────────────────────── */

/* Pipeline scene shaders — 48-byte vertex (with tangent), Gram-Schmidt TBN,
 * normal mapping, Blinn-Phong with shadow */
#include "shaders/compiled/scene_pipeline_vert_spirv.h"
#include "shaders/compiled/scene_pipeline_vert_dxil.h"
#include "shaders/compiled/scene_pipeline_frag_spirv.h"
#include "shaders/compiled/scene_pipeline_frag_dxil.h"

/* Raw scene shaders — 32-byte vertex (no tangent), simple Blinn-Phong
 * with shadow, uses interpolated vertex normal only */
#include "shaders/compiled/scene_raw_vert_spirv.h"
#include "shaders/compiled/scene_raw_vert_dxil.h"
#include "shaders/compiled/scene_raw_frag_spirv.h"
#include "shaders/compiled/scene_raw_frag_dxil.h"

/* Shadow shaders — depth-only pass, shared between pipeline and raw
 * (same HLSL, but separate pipelines for different vertex strides) */
#include "shaders/compiled/shadow_vert_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"

/* Sky shaders — fullscreen triangle vertical gradient */
#include "shaders/compiled/sky_vert_spirv.h"
#include "shaders/compiled/sky_vert_dxil.h"
#include "shaders/compiled/sky_frag_spirv.h"
#include "shaders/compiled/sky_frag_dxil.h"

/* Grid shaders — procedural anti-aliased grid floor with shadow */
#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH       1280
#define WINDOW_HEIGHT      720

/* Camera */
#define FOV_DEG            60.0f
#define NEAR_PLANE         0.1f
#define FAR_PLANE          200.0f
#define MOVE_SPEED         4.0f
#define MOUSE_SENSITIVITY  0.003f
#define PITCH_CLAMP        1.5f
#define MAX_DELTA_TIME     0.1f

/* Initial camera position — angled view of the model */
#define CAM_START_X        0.0f
#define CAM_START_Y        0.15f
#define CAM_START_Z        0.5f
#define CAM_START_YAW_DEG  0.0f
#define CAM_START_PITCH_DEG 0.0f

/* Lighting */
#define LIGHT_DIR_X        0.6f
#define LIGHT_DIR_Y        1.0f
#define LIGHT_DIR_Z        0.4f

/* Scene lighting parameters */
#define SCENE_AMBIENT      0.12f
#define SCENE_SHININESS    64.0f
#define SCENE_SPECULAR_STR 0.4f

/* Shadow map */
#define SHADOW_MAP_SIZE    2048
#define SHADOW_DEPTH_FMT   SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define DEPTH_FMT          SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_BIAS        0.002f
#define SHADOW_ORTHO_HALF  2.0f
#define SHADOW_LIGHT_DIST  5.0f
#define SHADOW_NEAR        0.1f   /* shadow ortho near plane */
#define SHADOW_FAR_MULT    2.0f   /* shadow ortho far = dist * multiplier */

/* Grid */
#define GRID_HALF_SIZE     20.0f
#define GRID_NUM_VERTS     4
#define GRID_NUM_INDICES   6
#define GRID_SPACING       0.1f
#define GRID_LINE_WIDTH    0.02f
#define GRID_FADE_DIST     15.0f
#define GRID_AMBIENT       0.15f
#define GRID_SHININESS     32.0f
#define GRID_SPECULAR_STR  0.2f

/* Grid colors (linear space) */
#define GRID_LINE_R        0.068f
#define GRID_LINE_G        0.534f
#define GRID_LINE_B        0.932f
#define GRID_BG_R          0.014f
#define GRID_BG_G          0.014f
#define GRID_BG_B          0.045f

/* Clear color (dark background) */
#define CLEAR_R            0.02f
#define CLEAR_G            0.02f
#define CLEAR_B            0.03f

/* Model scale — WaterBottle is ~0.3m, scale up for visibility */
#define MODEL_SCALE        1.0f

/* LOD distance thresholds */
#define LOD_DIST_0         3.0f   /* LOD 0 = full detail within this distance */
#define LOD_DIST_1         8.0f   /* LOD 1 = medium detail */
/* Beyond LOD_DIST_1 = LOD 2 (lowest detail) */

/* Render modes */
#define MODE_PIPELINE      0
#define MODE_RAW           1
#define MODE_SPLIT         2
#define NUM_MODES          3

/* Texture */
#define BYTES_PER_PIXEL    4
#define WHITE_TEX_DIM      1
#define WHITE_TEX_LAYERS   1
#define WHITE_TEX_LEVELS   1
#define WHITE_RGBA         255
#define MAX_LOD            1000.0f

/* Path buffer */
#define PATH_BUFFER_SIZE   512

/* Box model offset — render BoxTextured offset to the right */
#define BOX_OFFSET_X       0.5f
#define BOX_OFFSET_Y       0.0f
#define BOX_SCALE          0.15f  /* BoxTextured is large, scale down */

/* ── Shader resource counts ───────────────────────────────────────────── */

/* Pipeline scene vertex: model + view-projection + light VP */
#define PIPE_VERT_NUM_SAMPLERS         0
#define PIPE_VERT_NUM_STORAGE_TEXTURES 0
#define PIPE_VERT_NUM_STORAGE_BUFFERS  0
#define PIPE_VERT_NUM_UNIFORM_BUFFERS  1

/* Pipeline scene fragment: diffuse + normal + shadow map samplers */
#define PIPE_FRAG_NUM_SAMPLERS         3  /* diffuse + normal + shadow */
#define PIPE_FRAG_NUM_STORAGE_TEXTURES 0
#define PIPE_FRAG_NUM_STORAGE_BUFFERS  0
#define PIPE_FRAG_NUM_UNIFORM_BUFFERS  1

/* Raw scene vertex: same uniform layout as pipeline */
#define RAW_VERT_NUM_SAMPLERS          0
#define RAW_VERT_NUM_STORAGE_TEXTURES  0
#define RAW_VERT_NUM_STORAGE_BUFFERS   0
#define RAW_VERT_NUM_UNIFORM_BUFFERS   1

/* Raw scene fragment: diffuse + shadow (no normal map) */
#define RAW_FRAG_NUM_SAMPLERS          2  /* diffuse + shadow */
#define RAW_FRAG_NUM_STORAGE_TEXTURES  0
#define RAW_FRAG_NUM_STORAGE_BUFFERS   0
#define RAW_FRAG_NUM_UNIFORM_BUFFERS   1

/* Shadow vertex: light VP uniform only */
#define SHADOW_VERT_NUM_SAMPLERS         0
#define SHADOW_VERT_NUM_STORAGE_TEXTURES 0
#define SHADOW_VERT_NUM_STORAGE_BUFFERS  0
#define SHADOW_VERT_NUM_UNIFORM_BUFFERS  1

/* Shadow fragment: depth-only, no resources */
#define SHADOW_FRAG_NUM_SAMPLERS         0
#define SHADOW_FRAG_NUM_STORAGE_TEXTURES 0
#define SHADOW_FRAG_NUM_STORAGE_BUFFERS  0
#define SHADOW_FRAG_NUM_UNIFORM_BUFFERS  0

/* Sky vertex: no resources (uses SV_VertexID) */
#define SKY_VERT_NUM_SAMPLERS         0
#define SKY_VERT_NUM_STORAGE_TEXTURES 0
#define SKY_VERT_NUM_STORAGE_BUFFERS  0
#define SKY_VERT_NUM_UNIFORM_BUFFERS  0

/* Sky fragment: no resources (hardcoded gradient) */
#define SKY_FRAG_NUM_SAMPLERS         0
#define SKY_FRAG_NUM_STORAGE_TEXTURES 0
#define SKY_FRAG_NUM_STORAGE_BUFFERS  0
#define SKY_FRAG_NUM_UNIFORM_BUFFERS  0

/* Grid vertex: VP matrix only */
#define GRID_VERT_NUM_SAMPLERS         0
#define GRID_VERT_NUM_STORAGE_TEXTURES 0
#define GRID_VERT_NUM_STORAGE_BUFFERS  0
#define GRID_VERT_NUM_UNIFORM_BUFFERS  1

/* Grid fragment: shadow map sampler + lighting params */
#define GRID_FRAG_NUM_SAMPLERS         1  /* shadow map */
#define GRID_FRAG_NUM_STORAGE_TEXTURES 0
#define GRID_FRAG_NUM_STORAGE_BUFFERS  0
#define GRID_FRAG_NUM_UNIFORM_BUFFERS  1

/* ── Vertex layouts ───────────────────────────────────────────────────── */

/* PipelineVertex — 48-byte stride, matches ForgePipelineVertexTan.
 * Used for pipeline-processed meshes that include tangent data for
 * normal mapping with Gram-Schmidt TBN. */
typedef struct PipelineVertex {
    float position[3]; /* 12 bytes — model-space position */
    float normal[3];   /* 12 bytes — outward surface normal */
    float uv[2];       /* 8 bytes  — texture coordinates */
    float tangent[4];  /* 16 bytes — xyz=tangent, w=bitangent sign (±1) */
} PipelineVertex;      /* 48 bytes total */

/* RawVertex — 32-byte stride, matches ForgeGltfVertex.
 * Used for raw glTF meshes loaded without tangent data. */
typedef struct RawVertex {
    float position[3]; /* 12 bytes — model-space position */
    float normal[3];   /* 12 bytes — outward surface normal */
    float uv[2];       /* 8 bytes  — texture coordinates */
} RawVertex;           /* 32 bytes total */

/* GridVertex — position-only for the ground plane quad. */
typedef struct GridVertex {
    float position[3]; /* 12 bytes — world-space position */
} GridVertex;          /* 12 bytes total */

/* ── Uniform structures ──────────────────────────────────────────────── */

/* Vertex uniforms for scene rendering: MVP, model, and light VP matrices.
 * Both pipeline and raw vertex shaders use this same layout. */
typedef struct SceneVertUniforms {
    mat4 mvp;        /* model-view-projection for clip-space transform */
    mat4 model;      /* model-to-world for lighting calculations */
    mat4 light_vp;   /* light view-projection for shadow coordinates */
} SceneVertUniforms; /* 192 bytes */

/* Fragment uniforms for scene lighting.
 * Both pipeline and raw fragment shaders use this same layout. */
typedef struct SceneFragUniforms {
    float light_dir[4];  /* xyz = normalized direction toward light, w = pad */
    float eye_pos[4];    /* xyz = camera world position, w = pad */
    float shadow_texel;  /* 1.0 / shadow_map_size for PCF offset */
    float shininess;     /* specular exponent */
    float ambient;       /* ambient light intensity */
    float specular_str;  /* specular strength multiplier */
} SceneFragUniforms;     /* 48 bytes */

/* Shadow pass vertex uniforms: just the light VP * model matrix. */
typedef struct ShadowVertUniforms {
    mat4 mvp;            /* light_vp * model — transforms to light clip space */
} ShadowVertUniforms;    /* 64 bytes */

/* Grid vertex uniforms: just the view-projection matrix. */
typedef struct GridVertUniforms {
    mat4 vp;             /* camera view-projection */
} GridVertUniforms;      /* 64 bytes */

/* Grid fragment uniforms: procedural grid pattern + Blinn-Phong lighting
 * with shadow map sampling.  Must match the HLSL cbuffer layout. */
typedef struct GridFragUniforms {
    float line_color[4]; /* RGBA grid line color */
    float bg_color[4];   /* RGBA grid background color */
    float light_dir[4];  /* xyz = light direction, w = pad */
    float eye_pos[4];    /* xyz = camera position, w = pad */
    mat4  light_vp;      /* light VP for shadow projection */
    float grid_spacing;  /* world units between grid lines */
    float line_width;    /* line thickness [0..0.5] */
    float fade_distance; /* distance where grid fades to background */
    float ambient;       /* ambient light intensity */
    float shininess;     /* specular exponent */
    float specular_str;  /* specular strength multiplier */
    float shadow_texel;  /* 1.0 / shadow_map_size */
    float _pad;          /* 16-byte alignment padding */
} GridFragUniforms;      /* 160 bytes */

/* ── Loaded model data ────────────────────────────────────────────────── */

/* Pipeline-loaded model: .fmesh mesh with GPU buffers and textures. */
typedef struct PipelineModel {
    ForgePipelineMesh  mesh;           /* CPU-side mesh data (vertices, indices, LODs) */
    SDL_GPUBuffer     *vertex_buffer;  /* GPU vertex buffer (48-byte stride) */
    SDL_GPUBuffer     *index_buffer;   /* GPU index buffer (all LODs concatenated) */
    SDL_GPUTexture    *diffuse_tex;    /* base color texture */
    SDL_GPUTexture    *normal_tex;     /* normal map texture */
    uint32_t           current_lod;    /* active LOD level */
} PipelineModel;

/* Raw-loaded model: glTF mesh with GPU buffers and textures. */
typedef struct RawModel {
    ForgeGltfScene  scene;             /* CPU-side glTF scene data */
    SDL_GPUBuffer  *vertex_buffer;     /* GPU vertex buffer (32-byte stride) */
    SDL_GPUBuffer  *index_buffer;      /* GPU index buffer */
    SDL_GPUTexture *diffuse_tex;       /* base color texture */
    uint32_t        index_count;       /* number of indices to draw */
    uint32_t        vertex_count;      /* number of vertices */
} RawModel;

/* ── Application state ────────────────────────────────────────────────── */

typedef struct AppState {
    SDL_Window              *window;                /* main application window */
    SDL_GPUDevice           *device;                /* GPU device for all rendering */

    /* ── Pipelines ──────────────────────────────────────────────────── */
    SDL_GPUGraphicsPipeline *pipeline_scene_pipe;   /* pipeline scene (48-byte vert) */
    SDL_GPUGraphicsPipeline *pipeline_scene_raw;    /* raw scene (32-byte vert) */
    SDL_GPUGraphicsPipeline *pipeline_shadow_pipe;  /* shadow pass, 48-byte stride */
    SDL_GPUGraphicsPipeline *pipeline_shadow_raw;   /* shadow pass, 32-byte stride */
    SDL_GPUGraphicsPipeline *pipeline_sky;          /* fullscreen sky gradient */
    SDL_GPUGraphicsPipeline *pipeline_grid;         /* procedural grid floor */

    /* ── Shared textures and samplers ───────────────────────────────── */
    SDL_GPUTexture          *depth_texture;         /* main pass depth (window-sized) */
    SDL_GPUTexture          *shadow_depth_texture;  /* shadow map (SHADOW_MAP_SIZE) */
    SDL_GPUTexture          *white_texture;         /* 1x1 white placeholder */
    SDL_GPUSampler          *sampler;               /* linear wrap for diffuse textures */
    SDL_GPUSampler          *shadow_sampler;        /* comparison sampler for shadow PCF */
    SDL_GPUSampler          *normal_sampler;        /* linear clamp for normal maps */
    Uint32                   depth_w, depth_h;      /* current depth texture dimensions */

    /* ── Models ─────────────────────────────────────────────────────── */
    PipelineModel            pipe_water;            /* pipeline-loaded WaterBottle */
    PipelineModel            pipe_box;              /* pipeline-loaded BoxTextured */
    RawModel                 raw_water;             /* raw-loaded WaterBottle */
    RawModel                 raw_box;               /* raw-loaded BoxTextured */

    /* ── Grid ───────────────────────────────────────────────────────── */
    SDL_GPUBuffer           *grid_vertex_buf;       /* 4-vertex ground quad */
    SDL_GPUBuffer           *grid_index_buf;        /* 6 indices for 2 triangles */

    /* ── Camera state ───────────────────────────────────────────────── */
    vec3  cam_position;                             /* world-space camera position */
    float cam_yaw;                                  /* horizontal rotation (radians) */
    float cam_pitch;                                /* vertical rotation (radians) */

    /* ── Timing ─────────────────────────────────────────────────────── */
    Uint64 last_ticks;                              /* SDL_GetTicks() from previous frame */

    /* ── Input state ────────────────────────────────────────────────── */
    bool mouse_captured;                            /* true = relative mouse mode active */

    /* ── Render mode ────────────────────────────────────────────────── */
    int  render_mode;                               /* MODE_PIPELINE, MODE_RAW, MODE_SPLIT */
    bool lod_auto;                                  /* true = auto LOD by distance */
    int  lod_override;                              /* manual LOD level (-1 = auto) */
    bool show_info;                                 /* show on-screen info overlay */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;                           /* screenshot/GIF capture state */
#endif
} AppState;

/* ── Depth texture helper ─────────────────────────────────────────────── */

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device,
                                             Uint32 w, Uint32 h,
                                             SDL_GPUTextureFormat fmt,
                                             SDL_GPUTextureUsageFlags usage)
{
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = fmt;
    info.usage                = usage;
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

/* ── Shader helper ────────────────────────────────────────────────────── */

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

/* ── GPU buffer upload helper ─────────────────────────────────────────── */

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

/* ── Texture loading helpers ──────────────────────────────────────────── */

/* load_texture_from_surface — Creates a GPU texture from an SDL_Surface.
 * Used by both the pipeline path (after decoding PNG from raw bytes) and
 * the raw path (after loading image from disk).  Creates mipmaps. */
static SDL_GPUTexture *load_texture_from_surface(SDL_GPUDevice *device,
                                                  SDL_Surface *surface,
                                                  bool srgb)
{
    /* Convert to ABGR8888 (SDL's name for R8G8B8A8 in memory). */
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
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
    tex_info.format               = srgb
        ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
        : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
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

    /* Copy row-by-row to handle pitch differences. */
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

    /* Generate mipmaps from the uploaded base level. */
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

/* load_texture_from_path — Loads an image file from disk and uploads to GPU.
 * srgb=true for diffuse/base-color textures, false for normal maps. */
static SDL_GPUTexture *load_texture_from_path(SDL_GPUDevice *device,
                                               const char *path,
                                               bool srgb)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }
    SDL_Log("Loaded texture: %dx%d from '%s'", surface->w, surface->h, path);
    SDL_GPUTexture *tex = load_texture_from_surface(device, surface, srgb);
    SDL_DestroySurface(surface);
    return tex;
}

/* load_pipeline_texture — Loads a pipeline-processed texture using the
 * .meta.json sidecar.  The mip data is raw PNG bytes, so we decode with
 * SDL_LoadSurface via an SDL_IOStream. */
static SDL_GPUTexture *load_pipeline_texture(SDL_GPUDevice *device,
                                              const char *path,
                                              bool srgb)
{
    ForgePipelineTexture tex;
    if (!forge_pipeline_load_texture(path, &tex)) {
        SDL_Log("Failed to load pipeline texture '%s'", path);
        return NULL;
    }

    /* The pipeline currently outputs PNG files — decode mip 0 from the
     * raw bytes using SDL_IOStream. */
    if (tex.mip_count == 0 || !tex.mips[0].data) {
        SDL_Log("Pipeline texture '%s' has no mip data", path);
        forge_pipeline_free_texture(&tex);
        return NULL;
    }

    SDL_IOStream *io = SDL_IOFromMem(tex.mips[0].data, tex.mips[0].size);
    if (!io) {
        SDL_Log("Failed to create IOStream for '%s': %s", path, SDL_GetError());
        forge_pipeline_free_texture(&tex);
        return NULL;
    }

    SDL_Surface *surface = SDL_LoadBMP_IO(io, false);
    if (!surface) {
        /* BMP failed — try loading as PNG via the file path directly.
         * The meta.json points to actual PNG files on disk. */
        SDL_CloseIO(io);
        forge_pipeline_free_texture(&tex);
        /* Fall back to loading the PNG directly from the path */
        return load_texture_from_path(device, path, srgb);
    }

    SDL_CloseIO(io);
    SDL_GPUTexture *gpu_tex = load_texture_from_surface(device, surface, srgb);
    SDL_DestroySurface(surface);
    forge_pipeline_free_texture(&tex);
    return gpu_tex;
}

/* ── 1x1 white placeholder texture ────────────────────────────────────── */

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

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = transfer;
    tex_src.pixels_per_row  = WHITE_TEX_DIM;
    tex_src.rows_per_layer  = WHITE_TEX_DIM;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = texture;
    tex_dst.w       = WHITE_TEX_DIM;
    tex_dst.h       = WHITE_TEX_DIM;
    tex_dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &tex_src, &tex_dst, false);
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

/* ── LOD selection ────────────────────────────────────────────────────── */

/* Select the appropriate LOD level based on camera distance to the model.
 * Returns a LOD index clamped to the mesh's available LOD count. */
static uint32_t select_lod(float distance, uint32_t max_lods)
{
    if (max_lods <= 1) return 0;
    if (distance < LOD_DIST_0) return 0;
    if (distance < LOD_DIST_1 && max_lods > 1) return 1;
    if (max_lods > 2) return 2;
    return max_lods - 1;
}

/* ════════════════════════════════════════════════════════════════════════
 *  SDL_AppInit — Create device, window, pipelines, load models + textures
 * ════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    /* ── 1. Initialise SDL ─────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ─────────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU driver: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create window ─────────────────────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        "Lesson 39 — Pipeline-Processed Assets",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE);
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

    /* Request sRGB swapchain for correct color output */
    if (!SDL_SetGPUSwapchainParameters(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
            SDL_GPU_PRESENTMODE_VSYNC)) {
        SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
    }

    /* ── 4. Allocate application state ────────────────────────────────── */
    AppState *state = SDL_calloc(1, sizeof(AppState));
    if (!state) {
        SDL_Log("Failed to allocate AppState");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->device = device;
    state->window = window;
    *appstate = state;

    /* ── 5. Depth textures ────────────────────────────────────────────── */
    Uint32 win_w, win_h;
    SDL_GetWindowSizeInPixels(window, (int *)&win_w, (int *)&win_h);

    state->depth_texture = create_depth_texture(device, win_w, win_h,
        DEPTH_FMT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    if (!state->depth_texture) goto fail_cleanup;
    state->depth_w = win_w;
    state->depth_h = win_h;

    state->shadow_depth_texture = create_depth_texture(device,
        SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, SHADOW_DEPTH_FMT,
        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (!state->shadow_depth_texture) goto fail_cleanup;

    /* ── 6. Samplers ──────────────────────────────────────────────────── */
    {
        /* Linear-repeat sampler for diffuse textures */
        SDL_GPUSamplerCreateInfo s_info;
        SDL_zero(s_info);
        s_info.min_filter       = SDL_GPU_FILTER_LINEAR;
        s_info.mag_filter       = SDL_GPU_FILTER_LINEAR;
        s_info.mipmap_mode      = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        s_info.address_mode_u   = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        s_info.address_mode_v   = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        s_info.address_mode_w   = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        s_info.max_anisotropy   = 1;
        s_info.min_lod          = 0.0f;
        s_info.max_lod          = MAX_LOD;
        state->sampler = SDL_CreateGPUSampler(device, &s_info);
        if (!state->sampler) {
            SDL_Log("Failed to create sampler: %s", SDL_GetError());
            goto fail_cleanup;
        }

        /* Shadow comparison sampler for PCF */
        SDL_GPUSamplerCreateInfo shadow_info;
        SDL_zero(shadow_info);
        shadow_info.min_filter       = SDL_GPU_FILTER_LINEAR;
        shadow_info.mag_filter       = SDL_GPU_FILTER_LINEAR;
        shadow_info.mipmap_mode      = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        shadow_info.address_mode_u   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        shadow_info.address_mode_v   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        shadow_info.address_mode_w   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        shadow_info.compare_op       = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        shadow_info.enable_compare   = true;
        state->shadow_sampler = SDL_CreateGPUSampler(device, &shadow_info);
        if (!state->shadow_sampler) {
            SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
            goto fail_cleanup;
        }

        /* Linear-clamp sampler for normal maps */
        SDL_GPUSamplerCreateInfo nm_info;
        SDL_zero(nm_info);
        nm_info.min_filter       = SDL_GPU_FILTER_LINEAR;
        nm_info.mag_filter       = SDL_GPU_FILTER_LINEAR;
        nm_info.mipmap_mode      = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        nm_info.address_mode_u   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        nm_info.address_mode_v   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        nm_info.address_mode_w   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        nm_info.max_anisotropy   = 1;
        nm_info.min_lod          = 0.0f;
        nm_info.max_lod          = MAX_LOD;
        state->normal_sampler = SDL_CreateGPUSampler(device, &nm_info);
        if (!state->normal_sampler) {
            SDL_Log("Failed to create normal sampler: %s", SDL_GetError());
            goto fail_cleanup;
        }
    }

    /* ── 7. White placeholder texture ─────────────────────────────────── */
    state->white_texture = create_white_texture(device);
    if (!state->white_texture) goto fail_cleanup;

    /* ── 8. Create all shaders (10 total) ─────────────────────────────── */

    /* Pipeline scene shaders — 48-byte vertex with tangent, normal mapping */
    SDL_GPUShader *pipe_vs = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_pipeline_vert_spirv, scene_pipeline_vert_spirv_size,
        scene_pipeline_vert_dxil,  scene_pipeline_vert_dxil_size,
        PIPE_VERT_NUM_SAMPLERS, PIPE_VERT_NUM_STORAGE_TEXTURES,
        PIPE_VERT_NUM_STORAGE_BUFFERS, PIPE_VERT_NUM_UNIFORM_BUFFERS);
    if (!pipe_vs) goto fail_cleanup;

    SDL_GPUShader *pipe_fs = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_pipeline_frag_spirv, scene_pipeline_frag_spirv_size,
        scene_pipeline_frag_dxil,  scene_pipeline_frag_dxil_size,
        PIPE_FRAG_NUM_SAMPLERS, PIPE_FRAG_NUM_STORAGE_TEXTURES,
        PIPE_FRAG_NUM_STORAGE_BUFFERS, PIPE_FRAG_NUM_UNIFORM_BUFFERS);
    if (!pipe_fs) { SDL_ReleaseGPUShader(device, pipe_vs); goto fail_cleanup; }

    /* Raw scene shaders — 32-byte vertex, no tangent, simple lighting */
    SDL_GPUShader *raw_vs = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_raw_vert_spirv, scene_raw_vert_spirv_size,
        scene_raw_vert_dxil,  scene_raw_vert_dxil_size,
        RAW_VERT_NUM_SAMPLERS, RAW_VERT_NUM_STORAGE_TEXTURES,
        RAW_VERT_NUM_STORAGE_BUFFERS, RAW_VERT_NUM_UNIFORM_BUFFERS);
    if (!raw_vs) {
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    SDL_GPUShader *raw_fs = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_raw_frag_spirv, scene_raw_frag_spirv_size,
        scene_raw_frag_dxil,  scene_raw_frag_dxil_size,
        RAW_FRAG_NUM_SAMPLERS, RAW_FRAG_NUM_STORAGE_TEXTURES,
        RAW_FRAG_NUM_STORAGE_BUFFERS, RAW_FRAG_NUM_UNIFORM_BUFFERS);
    if (!raw_fs) {
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    /* Shadow shaders — shared code, depth-only */
    SDL_GPUShader *shadow_vs = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, shadow_vert_spirv_size,
        shadow_vert_dxil,  shadow_vert_dxil_size,
        SHADOW_VERT_NUM_SAMPLERS, SHADOW_VERT_NUM_STORAGE_TEXTURES,
        SHADOW_VERT_NUM_STORAGE_BUFFERS, SHADOW_VERT_NUM_UNIFORM_BUFFERS);
    if (!shadow_vs) {
        SDL_ReleaseGPUShader(device, raw_fs);
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    SDL_GPUShader *shadow_fs = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, shadow_frag_spirv_size,
        shadow_frag_dxil,  shadow_frag_dxil_size,
        SHADOW_FRAG_NUM_SAMPLERS, SHADOW_FRAG_NUM_STORAGE_TEXTURES,
        SHADOW_FRAG_NUM_STORAGE_BUFFERS, SHADOW_FRAG_NUM_UNIFORM_BUFFERS);
    if (!shadow_fs) {
        SDL_ReleaseGPUShader(device, shadow_vs);
        SDL_ReleaseGPUShader(device, raw_fs);
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    /* Sky shaders — fullscreen triangle gradient */
    SDL_GPUShader *sky_vs = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        sky_vert_spirv, sky_vert_spirv_size,
        sky_vert_dxil,  sky_vert_dxil_size,
        SKY_VERT_NUM_SAMPLERS, SKY_VERT_NUM_STORAGE_TEXTURES,
        SKY_VERT_NUM_STORAGE_BUFFERS, SKY_VERT_NUM_UNIFORM_BUFFERS);
    if (!sky_vs) {
        SDL_ReleaseGPUShader(device, shadow_fs);
        SDL_ReleaseGPUShader(device, shadow_vs);
        SDL_ReleaseGPUShader(device, raw_fs);
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    SDL_GPUShader *sky_fs = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        sky_frag_spirv, sky_frag_spirv_size,
        sky_frag_dxil,  sky_frag_dxil_size,
        SKY_FRAG_NUM_SAMPLERS, SKY_FRAG_NUM_STORAGE_TEXTURES,
        SKY_FRAG_NUM_STORAGE_BUFFERS, SKY_FRAG_NUM_UNIFORM_BUFFERS);
    if (!sky_fs) {
        SDL_ReleaseGPUShader(device, sky_vs);
        SDL_ReleaseGPUShader(device, shadow_fs);
        SDL_ReleaseGPUShader(device, shadow_vs);
        SDL_ReleaseGPUShader(device, raw_fs);
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    /* Grid shaders — procedural anti-aliased grid with shadow */
    SDL_GPUShader *grid_vs = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, grid_vert_spirv_size,
        grid_vert_dxil,  grid_vert_dxil_size,
        GRID_VERT_NUM_SAMPLERS, GRID_VERT_NUM_STORAGE_TEXTURES,
        GRID_VERT_NUM_STORAGE_BUFFERS, GRID_VERT_NUM_UNIFORM_BUFFERS);
    if (!grid_vs) {
        SDL_ReleaseGPUShader(device, sky_fs);
        SDL_ReleaseGPUShader(device, sky_vs);
        SDL_ReleaseGPUShader(device, shadow_fs);
        SDL_ReleaseGPUShader(device, shadow_vs);
        SDL_ReleaseGPUShader(device, raw_fs);
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    SDL_GPUShader *grid_fs = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, grid_frag_spirv_size,
        grid_frag_dxil,  grid_frag_dxil_size,
        GRID_FRAG_NUM_SAMPLERS, GRID_FRAG_NUM_STORAGE_TEXTURES,
        GRID_FRAG_NUM_STORAGE_BUFFERS, GRID_FRAG_NUM_UNIFORM_BUFFERS);
    if (!grid_fs) {
        SDL_ReleaseGPUShader(device, grid_vs);
        SDL_ReleaseGPUShader(device, sky_fs);
        SDL_ReleaseGPUShader(device, sky_vs);
        SDL_ReleaseGPUShader(device, shadow_fs);
        SDL_ReleaseGPUShader(device, shadow_vs);
        SDL_ReleaseGPUShader(device, raw_fs);
        SDL_ReleaseGPUShader(device, raw_vs);
        SDL_ReleaseGPUShader(device, pipe_fs);
        SDL_ReleaseGPUShader(device, pipe_vs);
        goto fail_cleanup;
    }

    /* ── 9. Create graphics pipelines ─────────────────────────────────── */

    /* --- Pipeline scene (48-byte vertex with tangent) --- */
    {
        SDL_GPUVertexAttribute attrs[4];
        SDL_zero(attrs);
        attrs[0].location = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = offsetof(PipelineVertex, position);
        attrs[1].location = 1; attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset = offsetof(PipelineVertex, normal);
        attrs[2].location = 2; attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset = offsetof(PipelineVertex, uv);
        attrs[3].location = 3; attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[3].offset = offsetof(PipelineVertex, tangent);

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot            = 0;
        vbd.pitch           = sizeof(PipelineVertex);
        vbd.input_rate      = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vbd.instance_step_rate = 0;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 4;

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = SDL_GetGPUSwapchainTextureFormat(device, window);

        SDL_GPUGraphicsPipelineCreateInfo gpi;
        SDL_zero(gpi);
        gpi.vertex_shader   = pipe_vs;
        gpi.fragment_shader = pipe_fs;
        gpi.vertex_input_state = vis;
        gpi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gpi.target_info.num_color_targets      = 1;
        gpi.target_info.color_target_descriptions = &ctd;
        gpi.target_info.has_depth_stencil_target = true;
        gpi.target_info.depth_stencil_format     = DEPTH_FMT;
        gpi.depth_stencil_state.enable_depth_test  = true;
        gpi.depth_stencil_state.enable_depth_write = true;
        gpi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        gpi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        gpi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gpi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->pipeline_scene_pipe = SDL_CreateGPUGraphicsPipeline(device, &gpi);
        if (!state->pipeline_scene_pipe) {
            SDL_Log("Failed to create pipeline scene pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* --- Raw scene (32-byte vertex, no tangent) --- */
    {
        SDL_GPUVertexAttribute attrs[3];
        SDL_zero(attrs);
        attrs[0].location = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = offsetof(RawVertex, position);
        attrs[1].location = 1; attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset = offsetof(RawVertex, normal);
        attrs[2].location = 2; attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset = offsetof(RawVertex, uv);

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot            = 0;
        vbd.pitch           = sizeof(RawVertex);
        vbd.input_rate      = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vbd.instance_step_rate = 0;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 3;

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = SDL_GetGPUSwapchainTextureFormat(device, window);

        SDL_GPUGraphicsPipelineCreateInfo gpi;
        SDL_zero(gpi);
        gpi.vertex_shader   = raw_vs;
        gpi.fragment_shader = raw_fs;
        gpi.vertex_input_state = vis;
        gpi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gpi.target_info.num_color_targets      = 1;
        gpi.target_info.color_target_descriptions = &ctd;
        gpi.target_info.has_depth_stencil_target = true;
        gpi.target_info.depth_stencil_format     = DEPTH_FMT;
        gpi.depth_stencil_state.enable_depth_test  = true;
        gpi.depth_stencil_state.enable_depth_write = true;
        gpi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        gpi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        gpi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gpi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->pipeline_scene_raw = SDL_CreateGPUGraphicsPipeline(device, &gpi);
        if (!state->pipeline_scene_raw) {
            SDL_Log("Failed to create raw scene pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* --- Shadow pipeline (48-byte stride) --- */
    {
        SDL_GPUVertexAttribute attrs[1];
        SDL_zero(attrs);
        attrs[0].location = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = 0;

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot  = 0;
        vbd.pitch = sizeof(PipelineVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 1;

        SDL_GPUGraphicsPipelineCreateInfo gpi;
        SDL_zero(gpi);
        gpi.vertex_shader   = shadow_vs;
        gpi.fragment_shader = shadow_fs;
        gpi.vertex_input_state = vis;
        gpi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gpi.target_info.num_color_targets        = 0;
        gpi.target_info.has_depth_stencil_target = true;
        gpi.target_info.depth_stencil_format     = SHADOW_DEPTH_FMT;
        gpi.depth_stencil_state.enable_depth_test  = true;
        gpi.depth_stencil_state.enable_depth_write = true;
        gpi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        gpi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        gpi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gpi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        gpi.rasterizer_state.depth_bias_constant_factor = 2.0f;
        gpi.rasterizer_state.depth_bias_slope_factor    = 2.0f;

        state->pipeline_shadow_pipe = SDL_CreateGPUGraphicsPipeline(device, &gpi);
        if (!state->pipeline_shadow_pipe) {
            SDL_Log("Failed to create pipeline shadow pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* --- Shadow pipeline (32-byte stride) --- */
    {
        SDL_GPUVertexAttribute attrs[1];
        SDL_zero(attrs);
        attrs[0].location = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = 0;

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot  = 0;
        vbd.pitch = sizeof(RawVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 1;

        SDL_GPUGraphicsPipelineCreateInfo gpi;
        SDL_zero(gpi);
        gpi.vertex_shader   = shadow_vs;
        gpi.fragment_shader = shadow_fs;
        gpi.vertex_input_state = vis;
        gpi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gpi.target_info.num_color_targets        = 0;
        gpi.target_info.has_depth_stencil_target = true;
        gpi.target_info.depth_stencil_format     = SHADOW_DEPTH_FMT;
        gpi.depth_stencil_state.enable_depth_test  = true;
        gpi.depth_stencil_state.enable_depth_write = true;
        gpi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        gpi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        gpi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gpi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        gpi.rasterizer_state.depth_bias_constant_factor = 2.0f;
        gpi.rasterizer_state.depth_bias_slope_factor    = 2.0f;

        state->pipeline_shadow_raw = SDL_CreateGPUGraphicsPipeline(device, &gpi);
        if (!state->pipeline_shadow_raw) {
            SDL_Log("Failed to create raw shadow pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* --- Sky pipeline (fullscreen triangle, no vertex buffer) --- */
    {
        SDL_GPUVertexInputState vis;
        SDL_zero(vis);

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = SDL_GetGPUSwapchainTextureFormat(device, window);

        SDL_GPUGraphicsPipelineCreateInfo gpi;
        SDL_zero(gpi);
        gpi.vertex_shader   = sky_vs;
        gpi.fragment_shader = sky_fs;
        gpi.vertex_input_state = vis;
        gpi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gpi.target_info.num_color_targets      = 1;
        gpi.target_info.color_target_descriptions = &ctd;
        gpi.target_info.has_depth_stencil_target = true;
        gpi.target_info.depth_stencil_format     = DEPTH_FMT;
        /* Draw behind everything — depth test passes only at far plane */
        gpi.depth_stencil_state.enable_depth_test  = true;
        gpi.depth_stencil_state.enable_depth_write = false;
        gpi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        gpi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        gpi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->pipeline_sky = SDL_CreateGPUGraphicsPipeline(device, &gpi);
        if (!state->pipeline_sky) {
            SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* --- Grid pipeline (blended floor) --- */
    {
        SDL_GPUVertexAttribute attrs[1];
        SDL_zero(attrs);
        attrs[0].location = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = 0;

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot  = 0;
        vbd.pitch = sizeof(GridVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 1;

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = SDL_GetGPUSwapchainTextureFormat(device, window);
        ctd.blend_state.enable_blend          = true;
        ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.color_blend_op        = SDL_GPU_BLENDOP_ADD;
        ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo gpi;
        SDL_zero(gpi);
        gpi.vertex_shader   = grid_vs;
        gpi.fragment_shader = grid_fs;
        gpi.vertex_input_state = vis;
        gpi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gpi.target_info.num_color_targets      = 1;
        gpi.target_info.color_target_descriptions = &ctd;
        gpi.target_info.has_depth_stencil_target = true;
        gpi.target_info.depth_stencil_format     = DEPTH_FMT;
        gpi.depth_stencil_state.enable_depth_test  = true;
        gpi.depth_stencil_state.enable_depth_write = false;
        gpi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        gpi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        gpi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->pipeline_grid = SDL_CreateGPUGraphicsPipeline(device, &gpi);
        if (!state->pipeline_grid) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* Release shader objects — now owned by pipelines */
    SDL_ReleaseGPUShader(device, pipe_vs);
    SDL_ReleaseGPUShader(device, pipe_fs);
    SDL_ReleaseGPUShader(device, raw_vs);
    SDL_ReleaseGPUShader(device, raw_fs);
    SDL_ReleaseGPUShader(device, shadow_vs);
    SDL_ReleaseGPUShader(device, shadow_fs);
    SDL_ReleaseGPUShader(device, sky_vs);
    SDL_ReleaseGPUShader(device, sky_fs);
    SDL_ReleaseGPUShader(device, grid_vs);
    SDL_ReleaseGPUShader(device, grid_fs);

    /* ── 10. Build base path for asset loading ────────────────────────── */
    char base_path[PATH_BUFFER_SIZE];
    const char *bp = SDL_GetBasePath();
    SDL_snprintf(base_path, sizeof(base_path), "%s", bp ? bp : "");

    /* ── 11. Load pipeline models (.fmesh) ────────────────────────────── */

    /* WaterBottle — pipeline path */
    {
        char mesh_path[PATH_BUFFER_SIZE];
        SDL_snprintf(mesh_path, sizeof(mesh_path),
                     "%sassets/processed/WaterBottle.fmesh", base_path);

        if (!forge_pipeline_load_mesh(mesh_path, &state->pipe_water.mesh)) {
            SDL_Log("Failed to load pipeline WaterBottle mesh");
            goto fail_cleanup;
        }

        SDL_Log("Pipeline WaterBottle: %u verts, %u LODs, stride %u",
                state->pipe_water.mesh.vertex_count,
                state->pipe_water.mesh.lod_count,
                state->pipe_water.mesh.vertex_stride);

        /* Upload vertices to GPU */
        Uint32 vb_size = state->pipe_water.mesh.vertex_count
                       * state->pipe_water.mesh.vertex_stride;
        state->pipe_water.vertex_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, state->pipe_water.mesh.vertices, vb_size);
        if (!state->pipe_water.vertex_buffer) goto fail_cleanup;

        /* Upload all LOD indices to GPU (concatenated) */
        uint32_t total_indices = 0;
        for (uint32_t i = 0; i < state->pipe_water.mesh.lod_count; i++) {
            total_indices += state->pipe_water.mesh.lods[i].index_count;
        }
        Uint32 ib_size = total_indices * sizeof(uint32_t);
        state->pipe_water.index_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, state->pipe_water.mesh.indices, ib_size);
        if (!state->pipe_water.index_buffer) goto fail_cleanup;

        /* Load textures — pipeline path uses .meta.json sidecars */
        char tex_path[PATH_BUFFER_SIZE];
        SDL_snprintf(tex_path, sizeof(tex_path),
                     "%sassets/processed/WaterBottle_baseColor.png", base_path);
        state->pipe_water.diffuse_tex = load_pipeline_texture(device, tex_path, true);
        if (!state->pipe_water.diffuse_tex) {
            state->pipe_water.diffuse_tex = state->white_texture;
        }

        SDL_snprintf(tex_path, sizeof(tex_path),
                     "%sassets/processed/WaterBottle_normal.png", base_path);
        state->pipe_water.normal_tex = load_pipeline_texture(device, tex_path, false);
        if (!state->pipe_water.normal_tex) {
            state->pipe_water.normal_tex = state->white_texture;
        }

        state->pipe_water.current_lod = 0;
    }

    /* BoxTextured — pipeline path */
    {
        char mesh_path[PATH_BUFFER_SIZE];
        SDL_snprintf(mesh_path, sizeof(mesh_path),
                     "%sassets/processed/BoxTextured.fmesh", base_path);

        if (!forge_pipeline_load_mesh(mesh_path, &state->pipe_box.mesh)) {
            SDL_Log("Failed to load pipeline BoxTextured mesh");
            goto fail_cleanup;
        }

        Uint32 vb_size = state->pipe_box.mesh.vertex_count
                       * state->pipe_box.mesh.vertex_stride;
        state->pipe_box.vertex_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, state->pipe_box.mesh.vertices, vb_size);
        if (!state->pipe_box.vertex_buffer) goto fail_cleanup;

        uint32_t total_indices = 0;
        for (uint32_t i = 0; i < state->pipe_box.mesh.lod_count; i++) {
            total_indices += state->pipe_box.mesh.lods[i].index_count;
        }
        Uint32 ib_size = total_indices * sizeof(uint32_t);
        state->pipe_box.index_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, state->pipe_box.mesh.indices, ib_size);
        if (!state->pipe_box.index_buffer) goto fail_cleanup;

        char tex_path[PATH_BUFFER_SIZE];
        SDL_snprintf(tex_path, sizeof(tex_path),
                     "%sassets/processed/BoxTextured_baseColor.png", base_path);
        state->pipe_box.diffuse_tex = load_pipeline_texture(device, tex_path, true);
        if (!state->pipe_box.diffuse_tex) {
            state->pipe_box.diffuse_tex = state->white_texture;
        }
        state->pipe_box.normal_tex = state->white_texture;
        state->pipe_box.current_lod = 0;
    }

    /* ── 12. Load raw models (glTF) ───────────────────────────────────── */

    /* WaterBottle — raw path */
    {
        char gltf_path[PATH_BUFFER_SIZE];
        SDL_snprintf(gltf_path, sizeof(gltf_path),
                     "%sassets/WaterBottle/WaterBottle.gltf", base_path);

        if (!forge_gltf_load(gltf_path, &state->raw_water.scene)) {
            SDL_Log("Failed to load raw WaterBottle glTF");
            goto fail_cleanup;
        }

        /* Extract first primitive's vertices into RawVertex format */
        if (state->raw_water.scene.primitive_count > 0) {
            ForgeGltfPrimitive *prim = &state->raw_water.scene.primitives[0];

            state->raw_water.vertex_count = prim->vertex_count;
            Uint32 vb_size = prim->vertex_count * sizeof(RawVertex);
            state->raw_water.vertex_buffer = upload_gpu_buffer(device,
                SDL_GPU_BUFFERUSAGE_VERTEX, prim->vertices, vb_size);
            if (!state->raw_water.vertex_buffer) goto fail_cleanup;

            /* Convert indices to uint32 if needed */
            if (prim->index_stride == 2) {
                uint32_t *idx32 = SDL_malloc(prim->index_count * sizeof(uint32_t));
                if (!idx32) goto fail_cleanup;
                const uint16_t *idx16 = (const uint16_t *)prim->indices;
                for (uint32_t i = 0; i < prim->index_count; i++) {
                    idx32[i] = idx16[i];
                }
                Uint32 ib_size = prim->index_count * sizeof(uint32_t);
                state->raw_water.index_buffer = upload_gpu_buffer(device,
                    SDL_GPU_BUFFERUSAGE_INDEX, idx32, ib_size);
                SDL_free(idx32);
            } else {
                Uint32 ib_size = prim->index_count * sizeof(uint32_t);
                state->raw_water.index_buffer = upload_gpu_buffer(device,
                    SDL_GPU_BUFFERUSAGE_INDEX, prim->indices, ib_size);
            }
            if (!state->raw_water.index_buffer) goto fail_cleanup;
            state->raw_water.index_count = prim->index_count;
        }

        /* Load raw texture */
        char tex_path[PATH_BUFFER_SIZE];
        SDL_snprintf(tex_path, sizeof(tex_path),
                     "%sassets/WaterBottle/WaterBottle_baseColor.png", base_path);
        state->raw_water.diffuse_tex = load_texture_from_path(device, tex_path, true);
        if (!state->raw_water.diffuse_tex) {
            state->raw_water.diffuse_tex = state->white_texture;
        }
    }

    /* BoxTextured — raw path */
    {
        char gltf_path[PATH_BUFFER_SIZE];
        SDL_snprintf(gltf_path, sizeof(gltf_path),
                     "%sassets/models/BoxTextured/BoxTextured.gltf", base_path);

        if (!forge_gltf_load(gltf_path, &state->raw_box.scene)) {
            SDL_Log("Failed to load raw BoxTextured glTF");
            goto fail_cleanup;
        }

        if (state->raw_box.scene.primitive_count > 0) {
            ForgeGltfPrimitive *prim = &state->raw_box.scene.primitives[0];

            state->raw_box.vertex_count = prim->vertex_count;
            Uint32 vb_size = prim->vertex_count * sizeof(RawVertex);
            state->raw_box.vertex_buffer = upload_gpu_buffer(device,
                SDL_GPU_BUFFERUSAGE_VERTEX, prim->vertices, vb_size);
            if (!state->raw_box.vertex_buffer) goto fail_cleanup;

            if (prim->index_stride == 2) {
                uint32_t *idx32 = SDL_malloc(prim->index_count * sizeof(uint32_t));
                if (!idx32) goto fail_cleanup;
                const uint16_t *idx16 = (const uint16_t *)prim->indices;
                for (uint32_t i = 0; i < prim->index_count; i++) {
                    idx32[i] = idx16[i];
                }
                Uint32 ib_size = prim->index_count * sizeof(uint32_t);
                state->raw_box.index_buffer = upload_gpu_buffer(device,
                    SDL_GPU_BUFFERUSAGE_INDEX, idx32, ib_size);
                SDL_free(idx32);
            } else {
                Uint32 ib_size = prim->index_count * sizeof(uint32_t);
                state->raw_box.index_buffer = upload_gpu_buffer(device,
                    SDL_GPU_BUFFERUSAGE_INDEX, prim->indices, ib_size);
            }
            if (!state->raw_box.index_buffer) goto fail_cleanup;
            state->raw_box.index_count = prim->index_count;
        }

        char tex_path[PATH_BUFFER_SIZE];
        SDL_snprintf(tex_path, sizeof(tex_path),
                     "%sassets/models/BoxTextured/CesiumLogoFlat.png", base_path);
        state->raw_box.diffuse_tex = load_texture_from_path(device, tex_path, true);
        if (!state->raw_box.diffuse_tex) {
            state->raw_box.diffuse_tex = state->white_texture;
        }
    }

    /* ── 13. Grid geometry ────────────────────────────────────────────── */
    {
        GridVertex grid_verts[GRID_NUM_VERTS] = {
            {{ -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE }},
            {{  GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE }},
            {{  GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE }},
            {{ -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE }},
        };
        Uint16 grid_indices[GRID_NUM_INDICES] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vertex_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, grid_verts, sizeof(grid_verts));
        if (!state->grid_vertex_buf) goto fail_cleanup;

        state->grid_index_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, grid_indices, sizeof(grid_indices));
        if (!state->grid_index_buf) goto fail_cleanup;
    }

    /* ── 14. Camera initialization ────────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw   = CAM_START_YAW_DEG * FORGE_DEG2RAD;
    state->cam_pitch = CAM_START_PITCH_DEG * FORGE_DEG2RAD;
    state->last_ticks = SDL_GetTicks();
    state->render_mode = MODE_SPLIT; /* start in split-screen comparison */
    state->lod_override = -1;        /* auto LOD */
    state->lod_auto = true;
    state->show_info = false;

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            SDL_free(state);
            return SDL_APP_FAILURE;
        }
    }
#else
    (void)argc; (void)argv;
#endif

    SDL_Log("Lesson 39 initialized — Mode: Split-Screen (press 1/2/3)");
    return SDL_APP_CONTINUE;

fail_release_shaders:
    SDL_ReleaseGPUShader(device, pipe_vs);
    SDL_ReleaseGPUShader(device, pipe_fs);
    SDL_ReleaseGPUShader(device, raw_vs);
    SDL_ReleaseGPUShader(device, raw_fs);
    SDL_ReleaseGPUShader(device, shadow_vs);
    SDL_ReleaseGPUShader(device, shadow_fs);
    SDL_ReleaseGPUShader(device, sky_vs);
    SDL_ReleaseGPUShader(device, sky_fs);
    SDL_ReleaseGPUShader(device, grid_vs);
    SDL_ReleaseGPUShader(device, grid_fs);

fail_cleanup:
    /* Best-effort cleanup of partially initialized state */
    if (state) {
        /* Model resources (may be partially loaded) */
        if (state->grid_index_buf)  SDL_ReleaseGPUBuffer(device, state->grid_index_buf);
        if (state->grid_vertex_buf) SDL_ReleaseGPUBuffer(device, state->grid_vertex_buf);

        if (state->raw_box.diffuse_tex && state->raw_box.diffuse_tex != state->white_texture)
            SDL_ReleaseGPUTexture(device, state->raw_box.diffuse_tex);
        if (state->raw_box.index_buffer)  SDL_ReleaseGPUBuffer(device, state->raw_box.index_buffer);
        if (state->raw_box.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->raw_box.vertex_buffer);
        forge_gltf_free(&state->raw_box.scene);

        if (state->raw_water.diffuse_tex && state->raw_water.diffuse_tex != state->white_texture)
            SDL_ReleaseGPUTexture(device, state->raw_water.diffuse_tex);
        if (state->raw_water.index_buffer)  SDL_ReleaseGPUBuffer(device, state->raw_water.index_buffer);
        if (state->raw_water.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->raw_water.vertex_buffer);
        forge_gltf_free(&state->raw_water.scene);

        if (state->pipe_box.normal_tex && state->pipe_box.normal_tex != state->white_texture)
            SDL_ReleaseGPUTexture(device, state->pipe_box.normal_tex);
        if (state->pipe_box.diffuse_tex && state->pipe_box.diffuse_tex != state->white_texture)
            SDL_ReleaseGPUTexture(device, state->pipe_box.diffuse_tex);
        if (state->pipe_box.index_buffer)  SDL_ReleaseGPUBuffer(device, state->pipe_box.index_buffer);
        if (state->pipe_box.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->pipe_box.vertex_buffer);
        forge_pipeline_free_mesh(&state->pipe_box.mesh);

        if (state->pipe_water.normal_tex && state->pipe_water.normal_tex != state->white_texture)
            SDL_ReleaseGPUTexture(device, state->pipe_water.normal_tex);
        if (state->pipe_water.diffuse_tex && state->pipe_water.diffuse_tex != state->white_texture)
            SDL_ReleaseGPUTexture(device, state->pipe_water.diffuse_tex);
        if (state->pipe_water.index_buffer)  SDL_ReleaseGPUBuffer(device, state->pipe_water.index_buffer);
        if (state->pipe_water.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->pipe_water.vertex_buffer);
        forge_pipeline_free_mesh(&state->pipe_water.mesh);

        /* Shared resources */
        if (state->pipeline_scene_pipe) SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_scene_pipe);
        if (state->pipeline_scene_raw)  SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_scene_raw);
        if (state->pipeline_shadow_pipe) SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_shadow_pipe);
        if (state->pipeline_shadow_raw) SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_shadow_raw);
        if (state->pipeline_sky)        SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_sky);
        if (state->pipeline_grid)       SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_grid);
        if (state->white_texture)       SDL_ReleaseGPUTexture(device, state->white_texture);
        if (state->shadow_sampler)      SDL_ReleaseGPUSampler(device, state->shadow_sampler);
        if (state->normal_sampler)      SDL_ReleaseGPUSampler(device, state->normal_sampler);
        if (state->sampler)             SDL_ReleaseGPUSampler(device, state->sampler);
        if (state->shadow_depth_texture) SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
        if (state->depth_texture)       SDL_ReleaseGPUTexture(device, state->depth_texture);
        SDL_free(state);
    }
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
}

/* ════════════════════════════════════════════════════════════════════════
 *  SDL_AppEvent — Handle input: quit, mouse, keyboard (mode/LOD/info)
 * ════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *state = (AppState *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            if (state->mouse_captured) {
                SDL_SetWindowRelativeMouseMode(state->window, false);
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        /* Mode selection: 1 = pipeline, 2 = raw, 3 = split */
        if (event->key.key == SDLK_1) {
            state->render_mode = MODE_PIPELINE;
            SDL_Log("Mode: Pipeline only");
        }
        if (event->key.key == SDLK_2) {
            state->render_mode = MODE_RAW;
            SDL_Log("Mode: Raw only");
        }
        if (event->key.key == SDLK_3) {
            state->render_mode = MODE_SPLIT;
            SDL_Log("Mode: Split-screen comparison");
        }
        /* LOD cycling */
        if (event->key.key == SDLK_L) {
            if (state->lod_override < 0) {
                state->lod_override = 0;
                state->lod_auto = false;
                SDL_Log("LOD: manual 0 (full detail)");
            } else {
                state->lod_override++;
                if ((uint32_t)state->lod_override >= state->pipe_water.mesh.lod_count) {
                    state->lod_override = -1;
                    state->lod_auto = true;
                    SDL_Log("LOD: auto (distance-based)");
                } else {
                    SDL_Log("LOD: manual %d", state->lod_override);
                }
            }
        }
        /* Info overlay toggle */
        if (event->key.key == SDLK_I) {
            state->show_info = !state->show_info;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            SDL_SetWindowRelativeMouseMode(state->window, true);
            state->mouse_captured = true;
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            /* Yaw DECREMENTS on positive xrel — mandatory camera convention */
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
        }
        break;
    }

    return SDL_APP_CONTINUE;
}

/* ── Helper: draw a model with the pipeline path ──────────────────────── */

static void draw_pipeline_model(SDL_GPURenderPass *pass,
                                 SDL_GPUCommandBuffer *cmd,
                                 AppState *state,
                                 PipelineModel *model,
                                 mat4 model_mat,
                                 mat4 cam_vp,
                                 mat4 light_vp,
                                 const float *light_dir_n,
                                 bool is_shadow_pass)
{
    if (!model->vertex_buffer || !model->index_buffer) return;

    /* Determine LOD */
    uint32_t lod = model->current_lod;
    uint32_t idx_count = forge_pipeline_lod_index_count(&model->mesh, lod);
    uint32_t idx_offset = model->mesh.lods[lod].index_offset / sizeof(uint32_t);

    if (is_shadow_pass) {
        /* Shadow pass: bind shadow pipeline and push light MVP */
        SDL_BindGPUGraphicsPipeline(pass, state->pipeline_shadow_pipe);

        ShadowVertUniforms su;
        su.mvp = mat4_multiply(light_vp, model_mat);
        SDL_PushGPUVertexUniformData(cmd, 0, &su, sizeof(su));
    } else {
        /* Scene pass: bind pipeline scene pipeline */
        SDL_BindGPUGraphicsPipeline(pass, state->pipeline_scene_pipe);

        SceneVertUniforms vu;
        vu.mvp      = mat4_multiply(cam_vp, model_mat);
        vu.model    = model_mat;
        vu.light_vp = light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

        SceneFragUniforms fu;
        fu.light_dir[0] = light_dir_n[0];
        fu.light_dir[1] = light_dir_n[1];
        fu.light_dir[2] = light_dir_n[2];
        fu.light_dir[3] = 0.0f;
        fu.eye_pos[0] = state->cam_position.x;
        fu.eye_pos[1] = state->cam_position.y;
        fu.eye_pos[2] = state->cam_position.z;
        fu.eye_pos[3] = 0.0f;
        fu.shadow_texel = 1.0f / (float)SHADOW_MAP_SIZE;
        fu.shininess    = SCENE_SHININESS;
        fu.ambient      = SCENE_AMBIENT;
        fu.specular_str = SCENE_SPECULAR_STR;
        SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

        /* Bind textures: diffuse (slot 0), normal (slot 1), shadow (slot 2) */
        SDL_GPUTextureSamplerBinding tex_bindings[3];
        tex_bindings[0].texture = model->diffuse_tex;
        tex_bindings[0].sampler = state->sampler;
        tex_bindings[1].texture = model->normal_tex;
        tex_bindings[1].sampler = state->normal_sampler;
        tex_bindings[2].texture = state->shadow_depth_texture;
        tex_bindings[2].sampler = state->shadow_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 3);
    }

    /* Bind vertex and index buffers */
    SDL_GPUBufferBinding vb_bind;
    SDL_zero(vb_bind);
    vb_bind.buffer = model->vertex_buffer;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);

    SDL_GPUBufferBinding ib_bind;
    SDL_zero(ib_bind);
    ib_bind.buffer = model->index_buffer;
    SDL_BindGPUIndexBuffer(pass, &ib_bind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_DrawGPUIndexedPrimitives(pass, idx_count, 1, idx_offset, 0, 0);
}

/* ── Helper: draw a model with the raw path ───────────────────────────── */

static void draw_raw_model(SDL_GPURenderPass *pass,
                            SDL_GPUCommandBuffer *cmd,
                            AppState *state,
                            RawModel *model,
                            mat4 model_mat,
                            mat4 cam_vp,
                            mat4 light_vp,
                            const float *light_dir_n,
                            bool is_shadow_pass)
{
    if (!model->vertex_buffer || !model->index_buffer) return;

    if (is_shadow_pass) {
        SDL_BindGPUGraphicsPipeline(pass, state->pipeline_shadow_raw);

        ShadowVertUniforms su;
        su.mvp = mat4_multiply(light_vp, model_mat);
        SDL_PushGPUVertexUniformData(cmd, 0, &su, sizeof(su));
    } else {
        SDL_BindGPUGraphicsPipeline(pass, state->pipeline_scene_raw);

        SceneVertUniforms vu;
        vu.mvp      = mat4_multiply(cam_vp, model_mat);
        vu.model    = model_mat;
        vu.light_vp = light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

        SceneFragUniforms fu;
        fu.light_dir[0] = light_dir_n[0];
        fu.light_dir[1] = light_dir_n[1];
        fu.light_dir[2] = light_dir_n[2];
        fu.light_dir[3] = 0.0f;
        fu.eye_pos[0] = state->cam_position.x;
        fu.eye_pos[1] = state->cam_position.y;
        fu.eye_pos[2] = state->cam_position.z;
        fu.eye_pos[3] = 0.0f;
        fu.shadow_texel = 1.0f / (float)SHADOW_MAP_SIZE;
        fu.shininess    = SCENE_SHININESS;
        fu.ambient      = SCENE_AMBIENT;
        fu.specular_str = SCENE_SPECULAR_STR;
        SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

        /* Bind textures: diffuse (slot 0), shadow (slot 1) */
        SDL_GPUTextureSamplerBinding tex_bindings[2];
        tex_bindings[0].texture = model->diffuse_tex;
        tex_bindings[0].sampler = state->sampler;
        tex_bindings[1].texture = state->shadow_depth_texture;
        tex_bindings[1].sampler = state->shadow_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 2);
    }

    SDL_GPUBufferBinding vb_bind;
    SDL_zero(vb_bind);
    vb_bind.buffer = model->vertex_buffer;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);

    SDL_GPUBufferBinding ib_bind;
    SDL_zero(ib_bind);
    ib_bind.buffer = model->index_buffer;
    SDL_BindGPUIndexBuffer(pass, &ib_bind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_DrawGPUIndexedPrimitives(pass, model->index_count, 1, 0, 0, 0);
}

/* ════════════════════════════════════════════════════════════════════════
 *  SDL_AppIterate — Update camera, render shadow + scene passes
 * ════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Timing ────────────────────────────────────────────────────────── */
    Uint64 now = SDL_GetTicks();
    float dt = (float)(now - state->last_ticks) / 1000.0f;
    state->last_ticks = now;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── Camera update (quaternion-based, mandatory pattern) ──────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(cam_orient);
    vec3 right   = quat_right(cam_orient);

    const bool *keys = SDL_GetKeyboardState(NULL);
    vec3 move = vec3_create(0.0f, 0.0f, 0.0f);
    if (keys[SDL_SCANCODE_W]) move = vec3_add(move, forward);
    if (keys[SDL_SCANCODE_S]) move = vec3_sub(move, forward);
    if (keys[SDL_SCANCODE_D]) move = vec3_add(move, right);
    if (keys[SDL_SCANCODE_A]) move = vec3_sub(move, right);
    if (keys[SDL_SCANCODE_SPACE])  move.y += 1.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) move.y -= 1.0f;

    if (vec3_length(move) > 0.001f) {
        move = vec3_scale(vec3_normalize(move), MOVE_SPEED * dt);
        state->cam_position = vec3_add(state->cam_position, move);
    }

    /* ── LOD selection based on camera distance ──────────────────────── */
    float water_dist = vec3_length(state->cam_position);
    if (state->lod_auto) {
        state->pipe_water.current_lod = select_lod(water_dist,
            state->pipe_water.mesh.lod_count);
        state->pipe_box.current_lod = 0; /* box is simple, always LOD 0 */
    } else if (state->lod_override >= 0) {
        state->pipe_water.current_lod = (uint32_t)state->lod_override;
        if (state->pipe_water.current_lod >= state->pipe_water.mesh.lod_count) {
            state->pipe_water.current_lod = state->pipe_water.mesh.lod_count - 1;
        }
        state->pipe_box.current_lod = 0;
    }

    /* ── Matrices ─────────────────────────────────────────────────────── */
    Uint32 win_w, win_h;
    SDL_GetWindowSizeInPixels(state->window, (int *)&win_w, (int *)&win_h);
    float aspect = (float)win_w / (float)(win_h > 0 ? win_h : 1);

    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    mat4 proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD, aspect,
                                 NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* Light direction (normalized) */
    vec3 light_raw = vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z);
    vec3 light_dir = vec3_normalize(light_raw);
    float light_dir_n[3] = { light_dir.x, light_dir.y, light_dir.z };

    /* Light-space VP for shadow map */
    vec3 light_center = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 light_pos = vec3_add(light_center,
                              vec3_scale(light_dir, SHADOW_LIGHT_DIST));
    mat4 light_view = mat4_look_at(light_pos, light_center,
                                    vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -SHADOW_ORTHO_HALF, SHADOW_ORTHO_HALF,
        -SHADOW_ORTHO_HALF, SHADOW_ORTHO_HALF,
        SHADOW_NEAR, SHADOW_LIGHT_DIST * SHADOW_FAR_MULT);
    mat4 light_vp = mat4_multiply(light_proj, light_view);

    /* Model transforms */
    mat4 water_model = mat4_identity();  /* WaterBottle at origin */
    mat4 box_model = mat4_multiply(
        mat4_translate(vec3_create(BOX_OFFSET_X, BOX_OFFSET_Y, 0.0f)),
        mat4_scale(vec3_create(BOX_SCALE, BOX_SCALE, BOX_SCALE)));

    /* ── Resize depth texture if window changed ──────────────────────── */
    if (win_w != state->depth_w || win_h != state->depth_h) {
        SDL_ReleaseGPUTexture(device, state->depth_texture);
        state->depth_texture = create_depth_texture(device, win_w, win_h,
            DEPTH_FMT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
        state->depth_w = win_w;
        state->depth_h = win_h;
    }

    /* ── Acquire command buffer ───────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_CONTINUE;
    }

    /* ── Shadow pass ──────────────────────────────────────────────────── */
    {
        SDL_GPUDepthStencilTargetInfo ds_target;
        SDL_zero(ds_target);
        ds_target.texture     = state->shadow_depth_texture;
        ds_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        ds_target.store_op    = SDL_GPU_STOREOP_STORE;
        ds_target.clear_depth = 1.0f;

        SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &ds_target);
        if (shadow_pass) {
            /* Draw WaterBottle shadow */
            draw_pipeline_model(shadow_pass, cmd, state, &state->pipe_water,
                                water_model, cam_vp, light_vp, light_dir_n, true);
            /* Draw BoxTextured shadow */
            draw_pipeline_model(shadow_pass, cmd, state, &state->pipe_box,
                                box_model, cam_vp, light_vp, light_dir_n, true);
            SDL_EndGPURenderPass(shadow_pass);
        }
    }

    /* ── Acquire swapchain ────────────────────────────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    Uint32 sc_w, sc_h;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain, &sc_w, &sc_h)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }
    if (!swapchain) {
        /* Window minimized — must submit (not cancel) after swapchain acquire */
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    /* ── Scene pass ───────────────────────────────────────────────────── */
    {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture   = swapchain;
        color_target.load_op   = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op  = SDL_GPU_STOREOP_STORE;
        color_target.clear_color.r = CLEAR_R;
        color_target.clear_color.g = CLEAR_G;
        color_target.clear_color.b = CLEAR_B;
        color_target.clear_color.a = 1.0f;

        SDL_GPUDepthStencilTargetInfo ds_target;
        SDL_zero(ds_target);
        ds_target.texture     = state->depth_texture;
        ds_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        ds_target.store_op    = SDL_GPU_STOREOP_DONT_CARE;
        ds_target.clear_depth = 1.0f;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, &ds_target);
        if (pass) {
            if (state->render_mode == MODE_PIPELINE) {
                /* Full viewport — pipeline path only */
                SDL_GPUViewport vp = { 0, 0, (float)sc_w, (float)sc_h, 0.0f, 1.0f };
                SDL_SetGPUViewport(pass, &vp);
                SDL_Rect scissor = { 0, 0, (int)sc_w, (int)sc_h };
                SDL_SetGPUScissor(pass, &scissor);

                draw_pipeline_model(pass, cmd, state, &state->pipe_water,
                                    water_model, cam_vp, light_vp, light_dir_n, false);
                draw_pipeline_model(pass, cmd, state, &state->pipe_box,
                                    box_model, cam_vp, light_vp, light_dir_n, false);
            }
            else if (state->render_mode == MODE_RAW) {
                /* Full viewport — raw path only */
                SDL_GPUViewport vp = { 0, 0, (float)sc_w, (float)sc_h, 0.0f, 1.0f };
                SDL_SetGPUViewport(pass, &vp);
                SDL_Rect scissor = { 0, 0, (int)sc_w, (int)sc_h };
                SDL_SetGPUScissor(pass, &scissor);

                draw_raw_model(pass, cmd, state, &state->raw_water,
                               water_model, cam_vp, light_vp, light_dir_n, false);
                draw_raw_model(pass, cmd, state, &state->raw_box,
                               box_model, cam_vp, light_vp, light_dir_n, false);
            }
            else {
                /* Split-screen: left = pipeline, right = raw */
                Uint32 half_w = sc_w / 2;

                /* Left half — pipeline */
                {
                    float half_aspect = (float)half_w / (float)(sc_h > 0 ? sc_h : 1);
                    mat4 half_proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD,
                                                      half_aspect, NEAR_PLANE, FAR_PLANE);
                    mat4 half_vp = mat4_multiply(half_proj, view);

                    SDL_GPUViewport vp = { 0, 0, (float)half_w, (float)sc_h, 0.0f, 1.0f };
                    SDL_SetGPUViewport(pass, &vp);
                    SDL_Rect scissor = { 0, 0, (int)half_w, (int)sc_h };
                    SDL_SetGPUScissor(pass, &scissor);

                    draw_pipeline_model(pass, cmd, state, &state->pipe_water,
                                        water_model, half_vp, light_vp, light_dir_n, false);
                    draw_pipeline_model(pass, cmd, state, &state->pipe_box,
                                        box_model, half_vp, light_vp, light_dir_n, false);
                }

                /* Right half — raw */
                {
                    Uint32 right_w = sc_w - half_w;
                    float half_aspect = (float)right_w / (float)(sc_h > 0 ? sc_h : 1);
                    mat4 half_proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD,
                                                      half_aspect, NEAR_PLANE, FAR_PLANE);
                    mat4 half_vp = mat4_multiply(half_proj, view);

                    SDL_GPUViewport vp = { (float)half_w, 0, (float)right_w, (float)sc_h, 0.0f, 1.0f };
                    SDL_SetGPUViewport(pass, &vp);
                    SDL_Rect scissor = { (int)half_w, 0, (int)right_w, (int)sc_h };
                    SDL_SetGPUScissor(pass, &scissor);

                    draw_raw_model(pass, cmd, state, &state->raw_water,
                                   water_model, half_vp, light_vp, light_dir_n, false);
                    draw_raw_model(pass, cmd, state, &state->raw_box,
                                   box_model, half_vp, light_vp, light_dir_n, false);
                }
            }

            /* ── Sky (fullscreen triangle, behind everything) ─────────── */
            {
                SDL_GPUViewport vp = { 0, 0, (float)sc_w, (float)sc_h, 0.0f, 1.0f };
                SDL_SetGPUViewport(pass, &vp);
                SDL_Rect scissor = { 0, 0, (int)sc_w, (int)sc_h };
                SDL_SetGPUScissor(pass, &scissor);

                SDL_BindGPUGraphicsPipeline(pass, state->pipeline_sky);
                SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            }

            /* ── Grid floor ───────────────────────────────────────────── */
            {
                SDL_BindGPUGraphicsPipeline(pass, state->pipeline_grid);

                GridVertUniforms gvu;
                gvu.vp = cam_vp;
                SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));

                GridFragUniforms gfu;
                gfu.line_color[0] = GRID_LINE_R;
                gfu.line_color[1] = GRID_LINE_G;
                gfu.line_color[2] = GRID_LINE_B;
                gfu.line_color[3] = 1.0f;
                gfu.bg_color[0] = GRID_BG_R;
                gfu.bg_color[1] = GRID_BG_G;
                gfu.bg_color[2] = GRID_BG_B;
                gfu.bg_color[3] = 0.5f;
                gfu.light_dir[0] = light_dir_n[0];
                gfu.light_dir[1] = light_dir_n[1];
                gfu.light_dir[2] = light_dir_n[2];
                gfu.light_dir[3] = 0.0f;
                gfu.eye_pos[0] = state->cam_position.x;
                gfu.eye_pos[1] = state->cam_position.y;
                gfu.eye_pos[2] = state->cam_position.z;
                gfu.eye_pos[3] = 0.0f;
                gfu.light_vp      = light_vp;
                gfu.grid_spacing  = GRID_SPACING;
                gfu.line_width    = GRID_LINE_WIDTH;
                gfu.fade_distance = GRID_FADE_DIST;
                gfu.ambient       = GRID_AMBIENT;
                gfu.shininess     = GRID_SHININESS;
                gfu.specular_str  = GRID_SPECULAR_STR;
                gfu.shadow_texel  = 1.0f / (float)SHADOW_MAP_SIZE;
                gfu._pad = 0.0f;
                SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

                SDL_GPUTextureSamplerBinding shadow_bind;
                shadow_bind.texture = state->shadow_depth_texture;
                shadow_bind.sampler = state->shadow_sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

                SDL_GPUBufferBinding grid_vb;
                SDL_zero(grid_vb);
                grid_vb.buffer = state->grid_vertex_buf;
                SDL_BindGPUVertexBuffers(pass, 0, &grid_vb, 1);

                SDL_GPUBufferBinding grid_ib;
                SDL_zero(grid_ib);
                grid_ib.buffer = state->grid_index_buf;
                SDL_BindGPUIndexBuffer(pass, &grid_ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

                SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES, 1, 0, 0, 0);
            }

            SDL_EndGPURenderPass(pass);
        }
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            SDL_SubmitGPUCommandBuffer(cmd);
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

/* ════════════════════════════════════════════════════════════════════════
 *  SDL_AppQuit — Release all GPU resources in reverse order
 * ════════════════════════════════════════════════════════════════════════ */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    if (!appstate) return;
    AppState *state = (AppState *)appstate;
    SDL_GPUDevice *device = state->device;

    /* Wait for GPU to finish before releasing resources */
    SDL_WaitForGPUIdle(device);

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* ── Grid buffers ─────────────────────────────────────────────────── */
    if (state->grid_index_buf)  SDL_ReleaseGPUBuffer(device, state->grid_index_buf);
    if (state->grid_vertex_buf) SDL_ReleaseGPUBuffer(device, state->grid_vertex_buf);

    /* ── Raw model resources ──────────────────────────────────────────── */
    if (state->raw_box.diffuse_tex && state->raw_box.diffuse_tex != state->white_texture)
        SDL_ReleaseGPUTexture(device, state->raw_box.diffuse_tex);
    if (state->raw_box.index_buffer)  SDL_ReleaseGPUBuffer(device, state->raw_box.index_buffer);
    if (state->raw_box.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->raw_box.vertex_buffer);
    forge_gltf_free(&state->raw_box.scene);

    if (state->raw_water.diffuse_tex && state->raw_water.diffuse_tex != state->white_texture)
        SDL_ReleaseGPUTexture(device, state->raw_water.diffuse_tex);
    if (state->raw_water.index_buffer)  SDL_ReleaseGPUBuffer(device, state->raw_water.index_buffer);
    if (state->raw_water.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->raw_water.vertex_buffer);
    forge_gltf_free(&state->raw_water.scene);

    /* ── Pipeline model resources ─────────────────────────────────────── */
    if (state->pipe_box.normal_tex && state->pipe_box.normal_tex != state->white_texture)
        SDL_ReleaseGPUTexture(device, state->pipe_box.normal_tex);
    if (state->pipe_box.diffuse_tex && state->pipe_box.diffuse_tex != state->white_texture)
        SDL_ReleaseGPUTexture(device, state->pipe_box.diffuse_tex);
    if (state->pipe_box.index_buffer)  SDL_ReleaseGPUBuffer(device, state->pipe_box.index_buffer);
    if (state->pipe_box.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->pipe_box.vertex_buffer);
    forge_pipeline_free_mesh(&state->pipe_box.mesh);

    if (state->pipe_water.normal_tex && state->pipe_water.normal_tex != state->white_texture)
        SDL_ReleaseGPUTexture(device, state->pipe_water.normal_tex);
    if (state->pipe_water.diffuse_tex && state->pipe_water.diffuse_tex != state->white_texture)
        SDL_ReleaseGPUTexture(device, state->pipe_water.diffuse_tex);
    if (state->pipe_water.index_buffer)  SDL_ReleaseGPUBuffer(device, state->pipe_water.index_buffer);
    if (state->pipe_water.vertex_buffer) SDL_ReleaseGPUBuffer(device, state->pipe_water.vertex_buffer);
    forge_pipeline_free_mesh(&state->pipe_water.mesh);

    /* ── Shared resources ─────────────────────────────────────────────── */
    if (state->white_texture)       SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->normal_sampler)      SDL_ReleaseGPUSampler(device, state->normal_sampler);
    if (state->shadow_sampler)      SDL_ReleaseGPUSampler(device, state->shadow_sampler);
    if (state->sampler)             SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->shadow_depth_texture) SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
    if (state->depth_texture)       SDL_ReleaseGPUTexture(device, state->depth_texture);

    /* ── Pipelines ────────────────────────────────────────────────────── */
    if (state->pipeline_grid)        SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_grid);
    if (state->pipeline_sky)         SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_sky);
    if (state->pipeline_shadow_raw)  SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_shadow_raw);
    if (state->pipeline_shadow_pipe) SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_shadow_pipe);
    if (state->pipeline_scene_raw)   SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_scene_raw);
    if (state->pipeline_scene_pipe)  SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline_scene_pipe);

    /* ── Window and device ────────────────────────────────────────────── */
    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_free(state);
    SDL_DestroyGPUDevice(device);
}
