/*
 * Lesson 35 — Decals
 *
 * Demonstrates deferred decal projection — projecting flat textures onto
 * existing geometry using oriented bounding boxes and inverse depth
 * reconstruction.  The scene shows several Suzanne models covered in
 * procedurally generated colorful shape decals.
 *
 * Three-pass rendering architecture:
 *   Pass 1: Shadow map (depth-only, negotiated format 2048x2048)
 *   Pass 2: Scene (Suzannes + grid floor, writes scene_depth)
 *   Pass 3: Decals (reads scene_depth, draws back faces of unit cubes)
 *
 * The decal fragment shader reconstructs world position from the depth
 * buffer, projects it into decal local space, and samples a decal texture
 * to apply color with alpha blending.
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include <string.h>    /* memset   */
#include <math.h>      /* sinf, cosf, sqrtf, fabsf, acosf */
#include <stdint.h>    /* uint8_t  */

#include "math/forge_math.h"
#include "gltf/forge_gltf.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecode ─────────────────────────────────────────── */

#include "shaders/compiled/scene_vert_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_frag_dxil.h"

#include "shaders/compiled/shadow_vert_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"

#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"

#include "shaders/compiled/decal_vert_spirv.h"
#include "shaders/compiled/decal_vert_dxil.h"
#include "shaders/compiled/decal_frag_spirv.h"
#include "shaders/compiled/decal_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH       1280
#define WINDOW_HEIGHT      720
#define SHADOW_MAP_SIZE    2048
/* Shadow depth format is probed at init (stored in app_state.shadow_depth_fmt) */
#define FOV_DEG            60.0f
#define NEAR_PLANE         0.1f
#define FAR_PLANE          200.0f
#define MOVE_SPEED         5.0f
#define MOUSE_SENSITIVITY  0.003f
#define GRID_HALF_SIZE     50.0f
#define GRID_INDEX_COUNT   6
#define CLEAR_R            0.05f
#define CLEAR_G            0.05f
#define CLEAR_B            0.08f
#define PITCH_CLAMP        1.5f

/* Timing */
#define MAX_DT             0.1f     /* clamp delta-time to prevent spiral of death */

/* Material and lighting defaults */
#define BASE_COLOR_GREY    0.6f     /* neutral grey Blinn-Phong albedo (per channel) */
#define AMBIENT_SCENE      0.12f    /* ambient light for Suzanne surfaces (0-1) */
#define AMBIENT_GRID       0.15f    /* ambient light for grid floor (0-1) */
#define SHININESS          64.0f    /* Blinn-Phong specular exponent */
#define SPECULAR_STR       0.4f     /* specular intensity multiplier (0-1) */

/* Grid floor appearance */
#define GRID_LINE_COLOR_R  0.4f
#define GRID_LINE_COLOR_G  0.4f
#define GRID_LINE_COLOR_B  0.5f
#define GRID_BG_COLOR_R    0.08f
#define GRID_BG_COLOR_G    0.08f
#define GRID_BG_COLOR_B    0.12f
#define GRID_SPACING       1.0f     /* world units between grid lines */
#define GRID_LINE_WIDTH    0.02f    /* line thickness in world units */
#define GRID_FADE_DIST     40.0f    /* distance at which grid fades out */

/* Light setup */
#define LIGHT_ORTHO_SIZE   15.0f    /* half-extent of shadow ortho frustum */
#define LIGHT_ORTHO_NEAR   0.1f     /* shadow near plane */
#define LIGHT_ORTHO_FAR    60.0f    /* shadow far plane */
#define LIGHT_DISTANCE     30.0f    /* distance from origin along light direction */

/* Decal placement */
#define DECAL_SURFACE_DIST 0.9f     /* surface offset as fraction of object scale */
#define DECAL_SIZE_MIN     0.15f    /* smallest decal half-extent */
#define DECAL_SIZE_RANGE   0.35f    /* random range added to DECAL_SIZE_MIN */
#define DECAL_DEPTH_RATIO  0.6f     /* projection depth relative to width */

/* Initial camera */
#define CAM_START_X        0.0f
#define CAM_START_Y        7.0f
#define CAM_START_Z       14.0f
#define CAM_START_YAW      0.0f     /* initial horizontal rotation (radians) */
#define CAM_START_PITCH   -0.4f     /* initial vertical rotation (radians) */

/* Decal system */
#define MAX_DECALS         120
#define DECAL_TEX_SIZE     128
#define NUM_DECAL_SHAPES   8
#define DECAL_SEED         0xDECA1u
#define SUZANNE_COUNT      6
#define DECALS_PER_OBJECT  12
#define CUBE_INDEX_COUNT   36

/* Scene layout */
#define RING_RADIUS        5.0f     /* radius of Suzanne ring around origin */
#define FLOOR_DECAL_COUNT  15       /* number of decals scattered on the grid */
#define FLOOR_DECAL_OFFSET 0.05f    /* Y offset above floor to prevent z-fight */
#define FLOOR_SIZE_BASE    0.4f     /* minimum floor decal half-extent */
#define FLOOR_SIZE_RANGE   0.8f     /* random range added to FLOOR_SIZE_BASE */
#define FLOOR_SCATTER      7.0f     /* half-extent of floor decal scatter area */

/* ── Vertex layout ────────────────────────────────────────────────────── */

/* Position + normal — used for decal box geometry. */
typedef struct Vertex {
    vec3 position;   /* 12 bytes — world-space position */
    vec3 normal;     /* 12 bytes — outward surface normal */
} Vertex;            /* 24 bytes total */

/* ── Uniform structures ───────────────────────────────────────────────── */

/* Scene vertex: MVP + model + light VP matrices (192 bytes). */
typedef struct SceneVertUniforms {
    mat4 mvp;      /* model-view-projection for clip-space transform */
    mat4 model;    /* model-to-world for lighting in world space */
    mat4 light_vp; /* light view-projection for shadow map lookup */
} SceneVertUniforms;

/* Scene fragment: Blinn-Phong lighting parameters (80 bytes). */
typedef struct SceneFragUniforms {
    float base_color[4];      /* RGBA surface albedo (linear, 0-1) */
    float eye_pos[3];         /* world-space camera position */
    float ambient;            /* ambient light contribution (0-1) */
    float light_dir[4];       /* normalized world-space light direction; w unused */
    float light_color[3];     /* RGB light color (linear, 0-1) */
    float light_intensity;    /* light brightness multiplier (>0) */
    float shininess;          /* Blinn-Phong specular exponent (higher = tighter) */
    float specular_str;       /* specular intensity multiplier (0-1) */
    float _pad[2];            /* padding to 16-byte alignment */
} SceneFragUniforms;

/* Grid vertex: VP + light VP (128 bytes). */
typedef struct GridVertUniforms {
    mat4 vp;       /* view-projection for clip-space transform */
    mat4 light_vp; /* light view-projection for shadow map lookup */
} GridVertUniforms;

/* Grid fragment: grid pattern + lighting (80 bytes). */
typedef struct GridFragUniforms {
    float line_color[4];    /* RGBA color for grid lines */
    float bg_color[4];      /* RGBA color for grid background */
    float light_dir[3];     /* normalized world-space light direction */
    float light_intensity;  /* light brightness multiplier (>0) */
    float eye_pos[3];       /* world-space camera position */
    float grid_spacing;     /* distance between grid lines in world units */
    float line_width;       /* grid line thickness in world units */
    float fade_distance;    /* distance at which grid fades to background */
    float ambient;          /* ambient light contribution (0-1) */
    float _pad;             /* padding to 16-byte alignment */
} GridFragUniforms;

/* Decal vertex: MVP (64 bytes). */
typedef struct DecalVertUniforms {
    mat4 mvp; /* decal-model × view × projection for clip-space transform */
} DecalVertUniforms;

/* Decal fragment: inverse matrices + screen info + tint + lighting (288 bytes). */
typedef struct DecalFragUniforms {
    mat4  inv_vp;            /* inverse view-projection for depth reconstruction */
    mat4  inv_decal_model;   /* inverse decal model matrix (world -> decal local) */
    mat4  light_vp;          /* light view-projection for shadow map lookup */
    float screen_size[2];    /* viewport width and height in pixels */
    float near_plane;        /* camera near plane distance */
    float far_plane;         /* camera far plane distance */
    float decal_tint[4];     /* RGBA per-decal color tint (linear, 0-1) */
    float eye_pos[3];        /* world-space camera position */
    float ambient;           /* ambient light intensity (0-1) */
    float light_dir[3];      /* normalized world-space light direction */
    float light_intensity;   /* directional light brightness multiplier */
    float light_color[3];    /* RGB light color (linear, 0-1) */
    float shininess;         /* Blinn-Phong specular exponent */
    float specular_str;      /* specular intensity multiplier (0-1) */
    float _pad[3];           /* padding to 16-byte alignment */
} DecalFragUniforms;

/* ── Scene object description ─────────────────────────────────────────── */

typedef struct SceneObject {
    vec3  position;    /* world-space center position */
    float scale;       /* uniform scale factor (1.0 = default size) */
    float rotation_y;  /* Y-axis rotation in radians */
} SceneObject;

/* ── Decal description ────────────────────────────────────────────────── */

typedef struct Decal {
    vec3  position;
    quat  orientation;
    float size[3];      /* half-extents (width, depth, height) */
    int   tex_index;    /* [0..NUM_DECAL_SHAPES-1] */
    float tint[4];      /* RGBA tint color */
} Decal;

/* ── Application state ────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;  /* main application window */
    SDL_GPUDevice *device;  /* GPU device for all rendering operations */

    /* Pipelines */
    SDL_GPUGraphicsPipeline *shadow_pipeline; /* depth-only shadow map pass */
    SDL_GPUGraphicsPipeline *scene_pipeline;  /* lit Suzanne geometry pass */
    SDL_GPUGraphicsPipeline *grid_pipeline;   /* procedural grid floor pass */
    SDL_GPUGraphicsPipeline *decal_pipeline;  /* projected decal pass (back-face, alpha blend) */

    /* Render targets */
    SDL_GPUTexture *shadow_depth;           /* shadow map texture; format negotiated at runtime (may be D32_FLOAT or other depth formats) */
    SDL_GPUTexture *scene_depth;            /* window-sized depth texture used as depth-stencil target and sampled for decal reconstruction */
    SDL_GPUTexture *scene_normal;           /* window-sized RGBA8 normal G-buffer (encoded world normals) sampled by decal shader */
    SDL_GPUTextureFormat depth_stencil_fmt; /* negotiated depth-stencil format for scene rendering (D24S8 or D32_FLOAT — stencil may be absent) */
    SDL_GPUTextureFormat shadow_depth_fmt;  /* negotiated shadow map depth format (selected from preferred list at init) */

    /* Samplers */
    SDL_GPUSampler *nearest_clamp; /* point sampling for depth textures */
    SDL_GPUSampler *linear_clamp;  /* bilinear sampling for decal shape textures */

    /* Suzanne geometry (from glTF) */
    ForgeGltfScene  gltf_scene;           /* parsed glTF scene (CPU-side, freed in quit) */
    ForgeArena      gltf_arena;           /* arena backing glTF allocations */
    SDL_GPUBuffer  *suzanne_vb;           /* vertex buffer (ForgeGltfVertex, 32 bytes/vert) */
    SDL_GPUBuffer  *suzanne_ib;           /* index buffer (16-bit or 32-bit indices) */
    Uint32          suzanne_index_count;  /* total index count for draw calls */
    Uint32          suzanne_index_stride; /* bytes per index (2 or 4) */

    /* Decal box geometry */
    SDL_GPUBuffer *cube_vb; /* unit cube vertices (24 verts, Vertex struct) */
    SDL_GPUBuffer *cube_ib; /* unit cube indices (36 indices, Uint16) */

    /* Grid floor */
    SDL_GPUBuffer *grid_vb; /* 4 corner vertices (vec3) */
    SDL_GPUBuffer *grid_ib; /* 6 indices for 2 triangles (Uint16) */

    /* Decal textures (one per shape) */
    SDL_GPUTexture *decal_textures[NUM_DECAL_SHAPES]; /* RGBA8 shape textures (128×128) */

    /* Scene objects */
    SceneObject suzannes[SUZANNE_COUNT]; /* Suzanne instance transforms */

    /* Decals */
    Decal decals[MAX_DECALS]; /* all decal instances in the scene */
    int   decal_count;        /* number of active decals (<= MAX_DECALS) */

    /* Light */
    vec3 light_dir;  /* normalized world-space directional light direction */
    mat4 light_vp;   /* light view-projection for shadow map rendering */

    SDL_GPUTextureFormat swapchain_format; /* swapchain color format (sRGB) */

    /* Camera */
    vec3  cam_position; /* world-space camera position */
    float cam_yaw;      /* horizontal rotation (radians, 0 = +Z) */
    float cam_pitch;    /* vertical rotation (radians, clamped ±PITCH_CLAMP) */

    /* Timing & input */
    Uint64 last_ticks;     /* SDL_GetTicks() from previous frame */
    bool   mouse_captured; /* true when mouse is in relative mode */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;  /* screenshot infrastructure — see note above */
#endif
} app_state;

/* ── Helper: create_shader ────────────────────────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
    Uint32 num_samplers,
    Uint32 num_storage_buffers,
    Uint32 num_uniform_buffers)
{
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage = stage;
    info.num_samplers = num_samplers;
    info.num_storage_buffers = num_storage_buffers;
    info.num_uniform_buffers = num_uniform_buffers;
    info.entrypoint = "main";

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil_code;
        info.code_size = dxil_size;
    } else {
        SDL_Log("ERROR: No supported shader format available");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("ERROR: SDL_CreateGPUShader failed: %s", SDL_GetError());
    }
    return shader;
}

/* ── Helper: upload_gpu_buffer ────────────────────────────────────────── */

static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    const void *data,
    Uint32 size)
{
    SDL_GPUBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = usage;
    buf_info.size = size;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
    if (!buffer) {
        SDL_Log("ERROR: SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size = size;
    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("ERROR: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("ERROR: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("ERROR: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size = size;

    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, xfer);

    return buffer;
}

/* ── Helper: upload_texture_rgba ──────────────────────────────────────── */

/* Upload RGBA8 pixels to a GPU texture via a transfer buffer. */
static SDL_GPUTexture *upload_texture_rgba(
    SDL_GPUDevice *device,
    Uint32 width, Uint32 height,
    const uint8_t *pixels)
{
    Uint32 data_size = width * height * 4;

    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.width = width;
    ti.height = height;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &ti);
    if (!texture) {
        SDL_Log("ERROR: SDL_CreateGPUTexture failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size = data_size;
    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("ERROR: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("ERROR: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_memcpy(mapped, pixels, data_size);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("ERROR: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;
    src.offset = 0;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = texture;
    dst.w = width;
    dst.h = height;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, xfer);

    return texture;
}

/* ── Geometry: generate_cube ──────────────────────────────────────────── */

/* Generate a unit cube centered at origin with given half-size.
 * 24 vertices (4 per face) + 36 indices. */
static void generate_cube(float half_size,
                           Vertex *verts, Uint32 *vert_count,
                           Uint16 *indices, Uint32 *idx_count)
{
    *vert_count = 0;
    *idx_count = 0;

    const float h = half_size;
    const float faces[6][4][3] = {
        /* +Z front */ {{ -h, -h, h }, { h, -h, h }, { h, h, h }, { -h, h, h }},
        /* -Z back  */ {{ h, -h, -h }, { -h, -h, -h }, { -h, h, -h }, { h, h, -h }},
        /* +X right */ {{ h, -h, h }, { h, -h, -h }, { h, h, -h }, { h, h, h }},
        /* -X left  */ {{ -h, -h, -h }, { -h, -h, h }, { -h, h, h }, { -h, h, -h }},
        /* +Y top   */ {{ -h, h, h }, { h, h, h }, { h, h, -h }, { -h, h, -h }},
        /* -Y bot   */ {{ -h, -h, -h }, { h, -h, -h }, { h, -h, h }, { -h, -h, h }},
    };
    const float normals[6][3] = {
        { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 },
        { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 },
    };

    Uint32 v = 0, idx = 0;
    for (int f = 0; f < 6; f++) {
        vec3 n = vec3_create(normals[f][0], normals[f][1], normals[f][2]);
        for (int c = 0; c < 4; c++) {
            verts[v].position = vec3_create(
                faces[f][c][0], faces[f][c][1], faces[f][c][2]);
            verts[v].normal = n;
            v++;
        }
        Uint16 base = (Uint16)(f * 4);
        indices[idx++] = base + 0;
        indices[idx++] = base + 1;
        indices[idx++] = base + 2;
        indices[idx++] = base + 0;
        indices[idx++] = base + 2;
        indices[idx++] = base + 3;
    }

    *vert_count = v;
    *idx_count = idx;
}

/* ── Scalar smoothstep (not in forge_math.h) ──────────────────────────── */

/* Hermite smoothstep: returns 0 when x <= edge0, 1 when x >= edge1,
 * and smoothly interpolates between using 3t^2 - 2t^3. */
static float smoothstepf(float edge0, float edge1, float x)
{
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* ── Procedural decal textures ────────────────────────────────────────── */

/* Set a pixel in an RGBA8 buffer. */
static void set_pixel(uint8_t *pixels, int x, int y, int w,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    int idx = (y * w + x) * 4;
    pixels[idx + 0] = r;
    pixels[idx + 1] = g;
    pixels[idx + 2] = b;
    pixels[idx + 3] = a;
}

/* Generate a decal shape texture (white on transparent, 128x128 RGBA8).
 * The tint color is applied per-decal via uniform, not baked into the texture. */
static void generate_decal_texture(int shape, uint8_t *pixels)
{
    const int S = DECAL_TEX_SIZE;
    const float hs = (float)S * 0.5f;
    const float inv_s = 1.0f / (float)S;

    /* Clear to transparent */
    SDL_memset(pixels, 0, (size_t)(S * S * 4));

    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            /* Normalized coords [-1, 1] */
            float nx = ((float)x + 0.5f) * inv_s * 2.0f - 1.0f;
            float ny = ((float)y + 0.5f) * inv_s * 2.0f - 1.0f;
            float dist, alpha;

            switch (shape) {
            case 0: /* Circle */
                dist = sqrtf(nx * nx + ny * ny);
                alpha = 1.0f - smoothstepf(0.7f, 0.8f, dist);
                break;

            case 1: { /* Heart */
                float px = fabsf(nx) * 1.1f;
                float py = -ny * 1.1f + 0.3f;
                float q = px * px + py * py - 0.5f;
                dist = sqrtf(fabsf(q)) * (q < 0.0f ? -1.0f : 1.0f);
                float heart = px * px + py * py;
                float h_val = heart - px * sqrtf(fabsf(py));
                alpha = 1.0f - smoothstepf(-0.05f, 0.05f, h_val - 0.5f);
                break;
            }

            case 2: { /* Star (5-point) */
                float angle = atan2f(ny, nx);
                float r = sqrtf(nx * nx + ny * ny);
                float star_r = 0.3f + 0.5f * fabsf(cosf(angle * 2.5f));
                alpha = 1.0f - smoothstepf(star_r - 0.05f, star_r + 0.05f, r);
                break;
            }

            case 3: { /* Checkerboard */
                int cx = (int)((nx * 0.5f + 0.5f) * 4.0f);
                int cy = (int)((ny * 0.5f + 0.5f) * 4.0f);
                float check = ((cx + cy) % 2 == 0) ? 1.0f : 0.0f;
                dist = sqrtf(nx * nx + ny * ny);
                alpha = check * (1.0f - smoothstepf(0.75f, 0.85f, dist));
                break;
            }

            case 4: { /* Ring */
                dist = sqrtf(nx * nx + ny * ny);
                float inner = 1.0f - smoothstepf(0.45f, 0.55f, dist);
                float outer = 1.0f - smoothstepf(0.7f, 0.8f, dist);
                alpha = outer - inner;
                if (alpha < 0.0f) alpha = 0.0f;
                break;
            }

            case 5: { /* Diamond */
                dist = fabsf(nx) + fabsf(ny);
                alpha = 1.0f - smoothstepf(0.65f, 0.75f, dist);
                break;
            }

            case 6: { /* Cross/Plus */
                float arm_x = (fabsf(nx) < 0.2f) ? 1.0f : 0.0f;
                float arm_y = (fabsf(ny) < 0.2f) ? 1.0f : 0.0f;
                float cross = (arm_x > 0.0f || arm_y > 0.0f) ? 1.0f : 0.0f;
                dist = sqrtf(nx * nx + ny * ny);
                alpha = cross * (1.0f - smoothstepf(0.7f, 0.8f, dist));
                break;
            }

            case 7: { /* Triangle */
                /* Equilateral triangle pointing up */
                float tri_y = ny + 0.3f;
                float tri_edge = fabsf(nx) - (0.8f - tri_y * 0.8f);
                float top = tri_y - 0.8f;
                float bottom = -tri_y - 0.3f;
                float d_max = tri_edge;
                if (top > d_max) d_max = top;
                if (bottom > d_max) d_max = bottom;
                alpha = 1.0f - smoothstepf(-0.03f, 0.03f, d_max);
                break;
            }

            default:
                alpha = 0.0f;
                break;
            }

            if (alpha > 0.01f) {
                uint8_t a = (uint8_t)(alpha * 255.0f + 0.5f);
                set_pixel(pixels, x, y, S, 255, 255, 255, a);
            }
        }
    }
}

/* ── End of Part A ────────────────────────────────────────────────────── */
/* ── Part B: Decal generation + SDL_AppInit ────────────────────────────── */

/* STYLE palette colors for decal tints (RGBA, premultiplied-ready). */
static const float DECAL_COLORS[][4] = {
    { 0.31f, 0.76f, 0.97f, 1.0f },  /* accent1 #4fc3f7 cyan   */
    { 1.00f, 0.44f, 0.26f, 1.0f },  /* accent2 #ff7043 orange */
    { 0.40f, 0.73f, 0.42f, 1.0f },  /* accent3 #66bb6a green  */
    { 0.67f, 0.28f, 0.74f, 1.0f },  /* accent4 #ab47bc purple */
    { 1.00f, 0.84f, 0.31f, 1.0f },  /* warn    #ffd54f yellow */
};
#define NUM_DECAL_COLORS 5

/* Generate decals for all Suzanne instances using deterministic hashing. */
static void generate_decals(app_state *state)
{
    state->decal_count = 0;
    uint32_t hash = forge_hash_wang(DECAL_SEED);

    for (int obj = 0; obj < SUZANNE_COUNT; obj++) {
        SceneObject *so = &state->suzannes[obj];
        int count = DECALS_PER_OBJECT;

        for (int d = 0; d < count && state->decal_count < MAX_DECALS; d++) {
            Decal *decal = &state->decals[state->decal_count];

            /* Generate random direction on unit sphere (spherical coords) */
            hash = forge_hash_wang(hash);
            float theta = acosf(1.0f - 2.0f * forge_hash_to_float(hash));
            hash = forge_hash_wang(hash);
            float phi = 2.0f * FORGE_PI * forge_hash_to_float(hash);

            float sin_t = sinf(theta);
            float dir_x = sin_t * cosf(phi);
            float dir_y = sin_t * sinf(phi);
            float dir_z = cosf(theta);

            /* Position decal at the Suzanne surface (~0.9 radius from center) */
            float surface_dist = DECAL_SURFACE_DIST * so->scale;
            decal->position = vec3_create(
                so->position.x + dir_x * surface_dist,
                so->position.y + dir_y * surface_dist,
                so->position.z + dir_z * surface_dist);

            /* Orient decal box to project inward toward center */
            vec3 forward = vec3_create(-dir_x, -dir_y, -dir_z);
            vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
            /* Avoid degenerate case when forward is parallel to up */
            if (fabsf(vec3_dot(forward, up)) > 0.99f) {
                up = vec3_create(1.0f, 0.0f, 0.0f);
            }
            vec3 right = vec3_normalize(vec3_cross(up, forward));
            up = vec3_cross(forward, right);

            /* Build rotation matrix -> quaternion */
            mat4 rot = mat4_identity();
            rot.m[0] = right.x;   rot.m[1] = right.y;   rot.m[2] = right.z;
            rot.m[4] = up.x;      rot.m[5] = up.y;      rot.m[6] = up.z;
            rot.m[8] = forward.x; rot.m[9] = forward.y;  rot.m[10] = forward.z;

            /* Add random roll */
            hash = forge_hash_wang(hash);
            float roll = forge_hash_to_float(hash) * 2.0f * FORGE_PI;
            quat roll_q = quat_from_axis_angle(forward, roll);

            /* Extract quaternion from rotation matrix */
            decal->orientation = quat_from_mat4(rot);
            decal->orientation = quat_multiply(roll_q, decal->orientation);
            decal->orientation = quat_normalize(decal->orientation);

            /* Random size */
            hash = forge_hash_wang(hash);
            float sz = DECAL_SIZE_MIN + forge_hash_to_float(hash) * DECAL_SIZE_RANGE;
            sz *= so->scale;
            decal->size[0] = sz;
            decal->size[1] = sz * DECAL_DEPTH_RATIO;  /* depth (projection distance) */
            decal->size[2] = sz;

            /* Random texture index */
            hash = forge_hash_wang(hash);
            decal->tex_index = (int)(forge_hash_to_float(hash) * (float)NUM_DECAL_SHAPES) % NUM_DECAL_SHAPES;

            /* Tint from palette */
            hash = forge_hash_wang(hash);
            int color_idx = (int)(forge_hash_to_float(hash) * (float)NUM_DECAL_COLORS) % NUM_DECAL_COLORS;
            decal->tint[0] = DECAL_COLORS[color_idx][0];
            decal->tint[1] = DECAL_COLORS[color_idx][1];
            decal->tint[2] = DECAL_COLORS[color_idx][2];
            decal->tint[3] = DECAL_COLORS[color_idx][3];

            state->decal_count++;
        }
    }

    /* Floor decals — scattered on the grid around and between the Suzannes */
    const int floor_count = FLOOR_DECAL_COUNT;
    for (int f = 0; f < floor_count && state->decal_count < MAX_DECALS; f++) {
        Decal *decal = &state->decals[state->decal_count];

        /* Random position on the floor within the scene area */
        hash = forge_hash_wang(hash);
        float fx = forge_hash_to_sfloat(hash) * FLOOR_SCATTER;
        hash = forge_hash_wang(hash);
        float fz = forge_hash_to_sfloat(hash) * FLOOR_SCATTER;

        decal->position = vec3_create(fx, FLOOR_DECAL_OFFSET, fz);

        /* Project straight down onto the floor */
        vec3 forward = vec3_create(0.0f, -1.0f, 0.0f);
        vec3 right   = vec3_create(1.0f, 0.0f, 0.0f);
        vec3 up_dir  = vec3_create(0.0f, 0.0f, -1.0f);

        mat4 rot = mat4_identity();
        rot.m[0] = right.x;   rot.m[1] = right.y;   rot.m[2] = right.z;
        rot.m[4] = up_dir.x;  rot.m[5] = up_dir.y;  rot.m[6] = up_dir.z;
        rot.m[8] = forward.x; rot.m[9] = forward.y;  rot.m[10] = forward.z;

        /* Random yaw rotation for variety */
        hash = forge_hash_wang(hash);
        float yaw = forge_hash_to_float(hash) * 2.0f * FORGE_PI;
        quat yaw_q = quat_from_axis_angle(vec3_create(0.0f, 1.0f, 0.0f), yaw);

        decal->orientation = quat_from_mat4(rot);
        decal->orientation = quat_multiply(yaw_q, decal->orientation);
        decal->orientation = quat_normalize(decal->orientation);

        /* Larger sizes for floor decals */
        hash = forge_hash_wang(hash);
        float sz = FLOOR_SIZE_BASE + forge_hash_to_float(hash) * FLOOR_SIZE_RANGE;
        decal->size[0] = sz;
        decal->size[1] = sz * DECAL_DEPTH_RATIO;
        decal->size[2] = sz;

        /* Random texture and color */
        hash = forge_hash_wang(hash);
        decal->tex_index = (int)(forge_hash_to_float(hash) * (float)NUM_DECAL_SHAPES) % NUM_DECAL_SHAPES;
        hash = forge_hash_wang(hash);
        int color_idx = (int)(forge_hash_to_float(hash) * (float)NUM_DECAL_COLORS) % NUM_DECAL_COLORS;
        decal->tint[0] = DECAL_COLORS[color_idx][0];
        decal->tint[1] = DECAL_COLORS[color_idx][1];
        decal->tint[2] = DECAL_COLORS[color_idx][2];
        decal->tint[3] = DECAL_COLORS[color_idx][3];

        state->decal_count++;
    }

    SDL_Log("Generated %d decals (%d on objects, %d on floor)",
            state->decal_count, state->decal_count - floor_count, floor_count);
}

/* ── SDL_AppInit ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── 1. Device & window ──────────────────────────────────────────── */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true, NULL);
    if (!device) {
        SDL_Log("ERROR: SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 35 \xe2\x80\x94 Decals",
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("ERROR: SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("ERROR: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->device = device;
    state->window = window;
    state->swapchain_format = swapchain_format;
    *appstate = state;

    /* ── 2. Load Suzanne via glTF ────────────────────────────────────── */

    state->gltf_arena = forge_arena_create(0);
    if (!state->gltf_arena.first) {
        SDL_Log("ERROR: Out of memory creating arena for Suzanne glTF");
        return SDL_APP_FAILURE;
    }
    if (!forge_gltf_load("assets/models/Suzanne/Suzanne.gltf", &state->gltf_scene, &state->gltf_arena)) {
        SDL_Log("ERROR: Failed to load Suzanne glTF");
        forge_arena_destroy(&state->gltf_arena);
        return SDL_APP_FAILURE;
    }

    /* Upload the first primitive's geometry to the GPU */
    if (!state->gltf_scene.primitives || state->gltf_scene.primitive_count == 0) {
        SDL_Log("ERROR: Suzanne glTF has no primitives");
        return SDL_APP_FAILURE;  /* SDL_AppQuit handles cleanup */
    }
    {
        ForgeGltfPrimitive *prim = &state->gltf_scene.primitives[0];
        Uint32 vb_size = prim->vertex_count * (Uint32)sizeof(ForgeGltfVertex);
        state->suzanne_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, prim->vertices, vb_size);

        Uint32 ib_size = prim->index_count * prim->index_stride;
        state->suzanne_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, prim->indices, ib_size);

        state->suzanne_index_count = prim->index_count;
        state->suzanne_index_stride = prim->index_stride;

        if (!state->suzanne_vb || !state->suzanne_ib) {
            SDL_Log("ERROR: Failed to upload Suzanne geometry");
            return SDL_APP_FAILURE;
        }
        SDL_Log("Loaded Suzanne: %u verts, %u indices (stride=%u)",
                prim->vertex_count, prim->index_count, prim->index_stride);
    }

    /* ── 3. Depth format probe ───────────────────────────────────────── */

    /* Shadow map: depth-only, needs DEPTH_STENCIL_TARGET | SAMPLER for PCF */
    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        state->shadow_depth_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        SDL_Log("Shadow depth format: D32_FLOAT");
    } else if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        state->shadow_depth_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        SDL_Log("Shadow depth format: D16_UNORM (fallback)");
    } else {
        SDL_Log("ERROR: No shadow depth format supports DEPTH_STENCIL_TARGET | SAMPLER");
        return SDL_APP_FAILURE;
    }

    /* Scene depth: needs stencil for exercises, plus SAMPLER for decal pass */
    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        SDL_Log("Depth format: D24_UNORM_S8_UINT (with SAMPLER)");
    } else if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        SDL_Log("Depth format: D32_FLOAT_S8_UINT (fallback, with SAMPLER)");
    } else if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        /* Last resort: D32_FLOAT without stencil */
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        SDL_Log("Depth format: D32_FLOAT (no stencil, fallback)");
    } else {
        SDL_Log("ERROR: No depth format supports DEPTH_STENCIL_TARGET | SAMPLER");
        return SDL_APP_FAILURE;
    }

    /* ── 4. Depth textures ───────────────────────────────────────────── */

    /* Scene depth — window-sized, usable as both DS target and sampler */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = state->depth_stencil_fmt;
        ti.width = WINDOW_WIDTH;
        ti.height = WINDOW_HEIGHT;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                   SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->scene_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->scene_depth) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (scene_depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Scene normal G-buffer — stores encoded world-space normals for decal lighting */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ti.width = WINDOW_WIDTH;
        ti.height = WINDOW_HEIGHT;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                   SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->scene_normal = SDL_CreateGPUTexture(device, &ti);
        if (!state->scene_normal) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (scene_normal) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Shadow map depth — 2048x2048, D32_FLOAT */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = state->shadow_depth_fmt;
        ti.width = SHADOW_MAP_SIZE;
        ti.height = SHADOW_MAP_SIZE;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                   SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->shadow_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->shadow_depth) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (shadow_depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 5. Samplers ─────────────────────────────────────────────────── */

    /* Nearest-clamp: shadow map + scene depth sampling */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter = SDL_GPU_FILTER_NEAREST;
        si.mag_filter = SDL_GPU_FILTER_NEAREST;
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->nearest_clamp = SDL_CreateGPUSampler(device, &si);
        if (!state->nearest_clamp) {
            SDL_Log("ERROR: SDL_CreateGPUSampler (nearest) failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Linear-clamp: decal texture sampling */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter = SDL_GPU_FILTER_LINEAR;
        si.mag_filter = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->linear_clamp = SDL_CreateGPUSampler(device, &si);
        if (!state->linear_clamp) {
            SDL_Log("ERROR: SDL_CreateGPUSampler (linear) failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 6. Shaders ──────────────────────────────────────────────────── */

    SDL_GPUShader *scene_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil),
        0, 0, 1);

    SDL_GPUShader *scene_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil),
        1, 0, 1);

    SDL_GPUShader *shadow_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil, sizeof(shadow_vert_dxil),
        0, 0, 1);

    SDL_GPUShader *shadow_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil, sizeof(shadow_frag_dxil),
        0, 0, 0);

    SDL_GPUShader *grid_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil, sizeof(grid_vert_dxil),
        0, 0, 1);

    SDL_GPUShader *grid_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil, sizeof(grid_frag_dxil),
        1, 0, 1);

    SDL_GPUShader *decal_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        decal_vert_spirv, sizeof(decal_vert_spirv),
        decal_vert_dxil, sizeof(decal_vert_dxil),
        0, 0, 1);

    SDL_GPUShader *decal_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        decal_frag_spirv, sizeof(decal_frag_spirv),
        decal_frag_dxil, sizeof(decal_frag_dxil),
        4, 0, 1);  /* 4 samplers: scene depth, decal texture, shadow map, scene normals */

    if (!scene_vert || !scene_frag || !shadow_vert || !shadow_frag ||
        !grid_vert || !grid_frag || !decal_vert || !decal_frag) {
        SDL_Log("ERROR: One or more shaders failed to compile");
        /* Release any shaders that were successfully created */
        if (scene_vert)  SDL_ReleaseGPUShader(device, scene_vert);
        if (scene_frag)  SDL_ReleaseGPUShader(device, scene_frag);
        if (shadow_vert) SDL_ReleaseGPUShader(device, shadow_vert);
        if (shadow_frag) SDL_ReleaseGPUShader(device, shadow_frag);
        if (grid_vert)   SDL_ReleaseGPUShader(device, grid_vert);
        if (grid_frag)   SDL_ReleaseGPUShader(device, grid_frag);
        if (decal_vert)  SDL_ReleaseGPUShader(device, decal_vert);
        if (decal_frag)  SDL_ReleaseGPUShader(device, decal_frag);
        return SDL_APP_FAILURE;
    }

    /* ── 7. Vertex input states ──────────────────────────────────────── */

    /* Scene (Suzanne): 3 attributes — pos(float3) + normal(float3) + uv(float2) = 32 bytes */
    SDL_GPUVertexBufferDescription scene_vb_desc;
    SDL_zero(scene_vb_desc);
    scene_vb_desc.slot = 0;
    scene_vb_desc.pitch = sizeof(ForgeGltfVertex);
    scene_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute scene_attrs[3];
    SDL_zero(scene_attrs);
    scene_attrs[0].location = 0;
    scene_attrs[0].buffer_slot = 0;
    scene_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    scene_attrs[0].offset = offsetof(ForgeGltfVertex, position);

    scene_attrs[1].location = 1;
    scene_attrs[1].buffer_slot = 0;
    scene_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    scene_attrs[1].offset = offsetof(ForgeGltfVertex, normal);

    scene_attrs[2].location = 2;
    scene_attrs[2].buffer_slot = 0;
    scene_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    scene_attrs[2].offset = offsetof(ForgeGltfVertex, uv);

    SDL_GPUVertexInputState scene_vertex_input;
    SDL_zero(scene_vertex_input);
    scene_vertex_input.vertex_buffer_descriptions = &scene_vb_desc;
    scene_vertex_input.num_vertex_buffers = 1;
    scene_vertex_input.vertex_attributes = scene_attrs;
    scene_vertex_input.num_vertex_attributes = 3;

    /* Shadow: position-only from ForgeGltfVertex (stride 32, only first float3 used) */
    SDL_GPUVertexBufferDescription shadow_vb_desc;
    SDL_zero(shadow_vb_desc);
    shadow_vb_desc.slot = 0;
    shadow_vb_desc.pitch = sizeof(ForgeGltfVertex);
    shadow_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute shadow_attr;
    SDL_zero(shadow_attr);
    shadow_attr.location = 0;
    shadow_attr.buffer_slot = 0;
    shadow_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shadow_attr.offset = 0;

    SDL_GPUVertexInputState shadow_vertex_input;
    SDL_zero(shadow_vertex_input);
    shadow_vertex_input.vertex_buffer_descriptions = &shadow_vb_desc;
    shadow_vertex_input.num_vertex_buffers = 1;
    shadow_vertex_input.vertex_attributes = &shadow_attr;
    shadow_vertex_input.num_vertex_attributes = 1;

    /* Grid: position-only (vec3, 12 bytes) */
    SDL_GPUVertexBufferDescription grid_vb_desc;
    SDL_zero(grid_vb_desc);
    grid_vb_desc.slot = 0;
    grid_vb_desc.pitch = sizeof(vec3);
    grid_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute grid_attr;
    SDL_zero(grid_attr);
    grid_attr.location = 0;
    grid_attr.buffer_slot = 0;
    grid_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    grid_attr.offset = 0;

    SDL_GPUVertexInputState grid_vertex_input;
    SDL_zero(grid_vertex_input);
    grid_vertex_input.vertex_buffer_descriptions = &grid_vb_desc;
    grid_vertex_input.num_vertex_buffers = 1;
    grid_vertex_input.vertex_attributes = &grid_attr;
    grid_vertex_input.num_vertex_attributes = 1;

    /* Decal box: 2 attributes — pos(float3) + normal(float3) = 24 bytes */
    SDL_GPUVertexBufferDescription decal_vb_desc;
    SDL_zero(decal_vb_desc);
    decal_vb_desc.slot = 0;
    decal_vb_desc.pitch = sizeof(Vertex);
    decal_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute decal_attrs[2];
    SDL_zero(decal_attrs);
    decal_attrs[0].location = 0;
    decal_attrs[0].buffer_slot = 0;
    decal_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    decal_attrs[0].offset = 0;

    decal_attrs[1].location = 1;
    decal_attrs[1].buffer_slot = 0;
    decal_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    decal_attrs[1].offset = offsetof(Vertex, normal);

    SDL_GPUVertexInputState decal_vertex_input;
    SDL_zero(decal_vertex_input);
    decal_vertex_input.vertex_buffer_descriptions = &decal_vb_desc;
    decal_vertex_input.num_vertex_buffers = 1;
    decal_vertex_input.vertex_attributes = decal_attrs;
    decal_vertex_input.num_vertex_attributes = 2;

    /* ── 8. Pipelines ────────────────────────────────────────────────── */

    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = swapchain_format;

    /* Scene pass writes to two color targets: swapchain (color) + normal G-buffer */
    SDL_GPUColorTargetDescription scene_color_targets[2];
    SDL_zero(scene_color_targets);
    scene_color_targets[0].format = swapchain_format;
    scene_color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    /* Pipeline 1: Shadow (depth-only, cull back) */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = shadow_vert;
        pi.fragment_shader = shadow_frag;
        pi.vertex_input_state = shadow_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = state->shadow_depth_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* Pipeline 2: Scene (Suzanne, cull back, depth test+write, 2 color targets) */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = scene_vert;
        pi.fragment_shader = scene_frag;
        pi.vertex_input_state = scene_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions = scene_color_targets;
        pi.target_info.num_color_targets = 2;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* Pipeline 3: Grid (cull none, depth test LESS_OR_EQUAL, 2 color targets) */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = grid_vert;
        pi.fragment_shader = grid_frag;
        pi.vertex_input_state = grid_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions = scene_color_targets;
        pi.target_info.num_color_targets = 2;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* Pipeline 4: Decal (cull FRONT = draw back faces, no depth, alpha blend) */
    {
        SDL_GPUColorTargetDescription decal_ct;
        SDL_zero(decal_ct);
        decal_ct.format = swapchain_format;
        decal_ct.blend_state.enable_blend = true;
        decal_ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        decal_ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        decal_ct.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        decal_ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        decal_ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        decal_ct.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        decal_ct.blend_state.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
            SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = decal_vert;
        pi.fragment_shader = decal_frag;
        pi.vertex_input_state = decal_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        /* No depth test or write — decal pass has no depth-stencil target */
        pi.depth_stencil_state.enable_depth_test = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions = &decal_ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.has_depth_stencil_target = false;
        state->decal_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── 9. Release shaders (safe with NULL — release before pipeline
     *        check so shaders are freed even if a pipeline failed) ──── */

    SDL_ReleaseGPUShader(device, scene_vert);
    SDL_ReleaseGPUShader(device, scene_frag);
    SDL_ReleaseGPUShader(device, shadow_vert);
    SDL_ReleaseGPUShader(device, shadow_frag);
    SDL_ReleaseGPUShader(device, grid_vert);
    SDL_ReleaseGPUShader(device, grid_frag);
    SDL_ReleaseGPUShader(device, decal_vert);
    SDL_ReleaseGPUShader(device, decal_frag);

    if (!state->shadow_pipeline || !state->scene_pipeline ||
        !state->grid_pipeline || !state->decal_pipeline) {
        SDL_Log("ERROR: One or more pipelines failed to create: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 10. Cube geometry (decal boxes) ─────────────────────────────── */

    {
        Vertex cube_verts[24];
        Uint16 cube_indices[36];
        Uint32 vcount = 0, icount = 0;
        generate_cube(0.5f, cube_verts, &vcount, cube_indices, &icount);

        state->cube_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
            cube_verts, vcount * (Uint32)sizeof(Vertex));
        state->cube_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
            cube_indices, icount * (Uint32)sizeof(Uint16));

        if (!state->cube_vb || !state->cube_ib) {
            SDL_Log("ERROR: Failed to upload cube geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11. Grid geometry ───────────────────────────────────────────── */

    {
        vec3 grid_verts[4] = {
            { -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE },
            {  GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE },
            {  GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE },
            { -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE },
        };
        Uint16 grid_indices[6] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
            grid_verts, sizeof(grid_verts));
        state->grid_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
            grid_indices, sizeof(grid_indices));

        if (!state->grid_vb || !state->grid_ib) {
            SDL_Log("ERROR: Failed to upload grid geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* ── 12. Decal textures ──────────────────────────────────────────── */

    {
        static uint8_t tex_pixels[DECAL_TEX_SIZE * DECAL_TEX_SIZE * 4];
        for (int i = 0; i < NUM_DECAL_SHAPES; i++) {
            generate_decal_texture(i, tex_pixels);
            state->decal_textures[i] = upload_texture_rgba(device,
                DECAL_TEX_SIZE, DECAL_TEX_SIZE, tex_pixels);
            if (!state->decal_textures[i]) {
                SDL_Log("ERROR: Failed to upload decal texture %d", i);
                return SDL_APP_FAILURE;
            }
        }
        SDL_Log("Generated %d decal shape textures (%dx%d)",
                NUM_DECAL_SHAPES, DECAL_TEX_SIZE, DECAL_TEX_SIZE);
    }

    /* ── 13. Scene objects (Suzanne instances) ───────────────────────── */

    /* Arrange Suzannes in a ring around the origin so the camera can see all */
    {
        const float base_scales[SUZANNE_COUNT] = { 1.0f, 0.8f, 1.2f, 0.9f, 1.1f, 0.7f };
        for (int i = 0; i < SUZANNE_COUNT; i++) {
            float angle = (float)i / (float)SUZANNE_COUNT * 2.0f * FORGE_PI;
            float x = RING_RADIUS * sinf(angle);
            float z = RING_RADIUS * cosf(angle);
            /* Face each Suzanne toward the center */
            float face_angle = angle + FORGE_PI;
            state->suzannes[i] = (SceneObject){
                vec3_create(x, 1.0f, z), base_scales[i], face_angle
            };
        }
    }

    /* ── 14. Generate decals ─────────────────────────────────────────── */

    generate_decals(state);

    /* ── 15. Light setup ─────────────────────────────────────────────── */

    state->light_dir = vec3_normalize(vec3_create(0.4f, -0.8f, -0.6f));
    vec3 light_pos = vec3_scale(state->light_dir, -LIGHT_DISTANCE);
    mat4 light_view = mat4_look_at(light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -LIGHT_ORTHO_SIZE, LIGHT_ORTHO_SIZE,
        -LIGHT_ORTHO_SIZE, LIGHT_ORTHO_SIZE,
        LIGHT_ORTHO_NEAR, LIGHT_ORTHO_FAR);
    state->light_vp = mat4_multiply(light_proj, light_view);

    /* ── 16. Camera ──────────────────────────────────────────────────── */

    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_START_YAW;
    state->cam_pitch = CAM_START_PITCH;
    state->last_ticks = SDL_GetTicks();

    /* ── 17. Mouse capture ───────────────────────────────────────────── */

    if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
        SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }
    state->mouse_captured = true;

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, state->device, state->window)) {
            SDL_Log("Failed to initialise capture");
            return SDL_APP_FAILURE;
        }
    }
#endif

    SDL_Log("Lesson 35 initialized: %d Suzannes, %d decals",
            SUZANNE_COUNT, state->decal_count);
    return SDL_APP_CONTINUE;
}

/* ── End of Part B ────────────────────────────────────────────────────── */
/* ── Part C: Event handling and rendering ──────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat) break;  /* ignore auto-repeat */
        if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
            bool next = !state->mouse_captured;
            if (!SDL_SetWindowRelativeMouseMode(state->window, next)) {
                SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            } else {
                state->mouse_captured = next;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
                SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            } else {
                state->mouse_captured = true;
            }
        }
        break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Timing ─────────────────────────────────────────────────── */

    Uint64 now = SDL_GetTicks();
    float dt = (float)(now - state->last_ticks) / 1000.0f;
    state->last_ticks = now;
    if (dt > MAX_DT) dt = MAX_DT;

    /* ── 2. Camera movement ────────────────────────────────────────── */

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

    /* ── 3. View-projection matrix ─────────────────────────────────── */

    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    mat4 proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD, aspect,
                                 NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* Compute inverse VP once per frame (used by all decals) */
    mat4 inv_vp = mat4_inverse(cam_vp);

    vec3 light_dir = state->light_dir;

    /* ── 4. Acquire swapchain ──────────────────────────────────────── */

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("ERROR: SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError());
        return SDL_APP_CONTINUE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw, sh;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain_tex, &sw, &sh)) {
        SDL_Log("ERROR: SDL_AcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }
    if (!swapchain_tex) {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }
    if (sw != WINDOW_WIDTH || sh != WINDOW_HEIGHT) {
        SDL_Log("ERROR: Swapchain size (%ux%u) does not match scene dimensions (%dx%d)",
                sw, sh, WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ── 5. Pass 1: Shadow ─────────────────────────────────────────── */

    SDL_GPUDepthStencilTargetInfo shadow_ds;
    SDL_zero(shadow_ds);
    shadow_ds.texture     = state->shadow_depth;
    shadow_ds.load_op     = SDL_GPU_LOADOP_CLEAR;
    shadow_ds.store_op    = SDL_GPU_STOREOP_STORE;
    shadow_ds.clear_depth = 1.0f;

    SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
        cmd, NULL, 0, &shadow_ds);
    SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

    SDL_GPUBufferBinding suzanne_vb_bind = { state->suzanne_vb, 0 };
    SDL_GPUBufferBinding suzanne_ib_bind = { state->suzanne_ib, 0 };
    SDL_BindGPUVertexBuffers(shadow_pass, 0, &suzanne_vb_bind, 1);
    SDL_BindGPUIndexBuffer(shadow_pass, &suzanne_ib_bind,
        (state->suzanne_index_stride == 2) ?
            SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);

    for (int i = 0; i < SUZANNE_COUNT; i++) {
        SceneObject *so = &state->suzannes[i];
        mat4 model = mat4_multiply(
            mat4_translate(so->position),
            mat4_multiply(
                mat4_rotate_y(so->rotation_y),
                mat4_scale_uniform(so->scale)));
        mat4 shadow_mvp = mat4_multiply(state->light_vp, model);

        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_mvp, sizeof(shadow_mvp));
        SDL_DrawGPUIndexedPrimitives(shadow_pass, state->suzanne_index_count,
                                     1, 0, 0, 0);
    }

    SDL_EndGPURenderPass(shadow_pass);

    /* ── 6. Pass 2: Scene (Suzannes + grid) ────────────────────────── */

    SDL_GPUColorTargetInfo color_targets[2];
    SDL_zero(color_targets);
    /* Target 0: swapchain color */
    color_targets[0].texture    = swapchain_tex;
    color_targets[0].load_op    = SDL_GPU_LOADOP_CLEAR;
    color_targets[0].store_op   = SDL_GPU_STOREOP_STORE;
    color_targets[0].clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };
    /* Target 1: world-space normal G-buffer */
    color_targets[1].texture    = state->scene_normal;
    color_targets[1].load_op    = SDL_GPU_LOADOP_CLEAR;
    color_targets[1].store_op   = SDL_GPU_STOREOP_STORE;
    color_targets[1].clear_color = (SDL_FColor){ 0.5f, 1.0f, 0.5f, 1.0f }; /* default +Y normal encoded */

    bool has_stencil =
        state->depth_stencil_fmt == SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT ||
        state->depth_stencil_fmt == SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;

    SDL_GPUDepthStencilTargetInfo scene_ds;
    SDL_zero(scene_ds);
    scene_ds.texture         = state->scene_depth;
    scene_ds.load_op         = SDL_GPU_LOADOP_CLEAR;
    scene_ds.store_op        = SDL_GPU_STOREOP_STORE;
    scene_ds.clear_depth     = 1.0f;
    if (has_stencil) {
        scene_ds.stencil_load_op  = SDL_GPU_LOADOP_CLEAR;
        scene_ds.stencil_store_op = SDL_GPU_STOREOP_STORE;
        scene_ds.clear_stencil    = 0;
    }

    SDL_GPURenderPass *scene_pass = SDL_BeginGPURenderPass(
        cmd, color_targets, 2, &scene_ds);
    if (!scene_pass) {
        SDL_Log("SDL_BeginGPURenderPass (scene) failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    /* Bind scene pipeline and shadow map */
    SDL_BindGPUGraphicsPipeline(scene_pass, state->scene_pipeline);

    SDL_GPUTextureSamplerBinding shadow_bind;
    shadow_bind.texture = state->shadow_depth;
    shadow_bind.sampler = state->nearest_clamp;
    SDL_BindGPUFragmentSamplers(scene_pass, 0, &shadow_bind, 1);

    /* Bind Suzanne vertex/index buffers for scene pass */
    SDL_BindGPUVertexBuffers(scene_pass, 0, &suzanne_vb_bind, 1);
    SDL_BindGPUIndexBuffer(scene_pass, &suzanne_ib_bind,
        (state->suzanne_index_stride == 2) ?
            SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Draw each Suzanne instance */
    for (int i = 0; i < SUZANNE_COUNT; i++) {
        SceneObject *so = &state->suzannes[i];
        mat4 model = mat4_multiply(
            mat4_translate(so->position),
            mat4_multiply(
                mat4_rotate_y(so->rotation_y),
                mat4_scale_uniform(so->scale)));
        mat4 mvp  = mat4_multiply(cam_vp, model);
        mat4 lmvp = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        /* Neutral grey Blinn-Phong — no texture, uniform base_color */
        frag_u.base_color[0] = BASE_COLOR_GREY;
        frag_u.base_color[1] = BASE_COLOR_GREY;
        frag_u.base_color[2] = BASE_COLOR_GREY;
        frag_u.base_color[3] = 1.0f;
        frag_u.eye_pos[0] = state->cam_position.x;
        frag_u.eye_pos[1] = state->cam_position.y;
        frag_u.eye_pos[2] = state->cam_position.z;
        frag_u.ambient       = AMBIENT_SCENE;
        frag_u.light_dir[0]  = light_dir.x;
        frag_u.light_dir[1]  = light_dir.y;
        frag_u.light_dir[2]  = light_dir.z;
        frag_u.light_dir[3]  = 0.0f;
        frag_u.light_color[0] = 1.0f;
        frag_u.light_color[1] = 1.0f;
        frag_u.light_color[2] = 1.0f;
        frag_u.light_intensity = 1.0f;
        frag_u.shininess     = SHININESS;
        frag_u.specular_str  = SPECULAR_STR;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        SDL_DrawGPUIndexedPrimitives(scene_pass, state->suzanne_index_count,
                                     1, 0, 0, 0);
    }

    /* Grid floor */
    SDL_BindGPUGraphicsPipeline(scene_pass, state->grid_pipeline);
    SDL_BindGPUFragmentSamplers(scene_pass, 0, &shadow_bind, 1);

    {
        GridVertUniforms gvu;
        gvu.vp = cam_vp;
        gvu.light_vp = state->light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));

        GridFragUniforms gfu;
        SDL_zero(gfu);
        gfu.line_color[0] = GRID_LINE_COLOR_R;
        gfu.line_color[1] = GRID_LINE_COLOR_G;
        gfu.line_color[2] = GRID_LINE_COLOR_B;
        gfu.line_color[3] = 1.0f;
        gfu.bg_color[0]   = GRID_BG_COLOR_R;
        gfu.bg_color[1]   = GRID_BG_COLOR_G;
        gfu.bg_color[2]   = GRID_BG_COLOR_B;
        gfu.bg_color[3]   = 1.0f;
        gfu.light_dir[0]  = light_dir.x;
        gfu.light_dir[1]  = light_dir.y;
        gfu.light_dir[2]  = light_dir.z;
        gfu.light_intensity = 1.0f;
        gfu.eye_pos[0]    = state->cam_position.x;
        gfu.eye_pos[1]    = state->cam_position.y;
        gfu.eye_pos[2]    = state->cam_position.z;
        gfu.grid_spacing  = GRID_SPACING;
        gfu.line_width    = GRID_LINE_WIDTH;
        gfu.fade_distance = GRID_FADE_DIST;
        gfu.ambient       = AMBIENT_GRID;
        SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

        SDL_GPUBufferBinding gvb_bind = { state->grid_vb, 0 };
        SDL_BindGPUVertexBuffers(scene_pass, 0, &gvb_bind, 1);
        SDL_GPUBufferBinding gib_bind = { state->grid_ib, 0 };
        SDL_BindGPUIndexBuffer(scene_pass, &gib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(scene_pass, GRID_INDEX_COUNT,
                                     1, 0, 0, 0);
    }

    SDL_EndGPURenderPass(scene_pass);

    /* ── 7. Pass 3: Decals ─────────────────────────────────────────── */
    /* Bind scene_depth as a sampler input (read-only).
     * No depth-stencil target — only swapchain color with alpha blending. */

    SDL_GPUColorTargetInfo decal_color;
    SDL_zero(decal_color);
    decal_color.texture  = swapchain_tex;
    decal_color.load_op  = SDL_GPU_LOADOP_LOAD;
    decal_color.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *decal_pass = SDL_BeginGPURenderPass(
        cmd, &decal_color, 1, NULL);

    SDL_BindGPUGraphicsPipeline(decal_pass, state->decal_pipeline);

    /* Bind scene depth as fragment sampler slot 0 */
    SDL_GPUTextureSamplerBinding depth_bind;
    depth_bind.texture = state->scene_depth;
    depth_bind.sampler = state->nearest_clamp;

    /* Bind decal box vertex/index buffers */
    SDL_GPUBufferBinding cube_vb_bind = { state->cube_vb, 0 };
    SDL_BindGPUVertexBuffers(decal_pass, 0, &cube_vb_bind, 1);
    SDL_GPUBufferBinding cube_ib_bind = { state->cube_ib, 0 };
    SDL_BindGPUIndexBuffer(decal_pass, &cube_ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_16BIT);

    for (int i = 0; i < state->decal_count; i++) {
        Decal *d = &state->decals[i];

        /* Build decal model matrix: translate * rotate * scale */
        mat4 rot = quat_to_mat4(d->orientation);
        mat4 scale = mat4_identity();
        scale.m[0]  = d->size[0] * 2.0f;  /* full width (half-extent * 2) */
        scale.m[5]  = d->size[1] * 2.0f;  /* full depth */
        scale.m[10] = d->size[2] * 2.0f;  /* full height */

        mat4 decal_model = mat4_multiply(
            mat4_translate(d->position),
            mat4_multiply(rot, scale));

        mat4 inv_decal_model = mat4_inverse(decal_model);

        /* Vertex uniforms: MVP */
        DecalVertUniforms dvu;
        dvu.mvp = mat4_multiply(cam_vp, decal_model);
        SDL_PushGPUVertexUniformData(cmd, 0, &dvu, sizeof(dvu));

        /* Fragment uniforms — projection, lighting, and shadow */
        DecalFragUniforms dfu;
        SDL_zero(dfu);
        dfu.inv_vp = inv_vp;
        dfu.inv_decal_model = inv_decal_model;
        dfu.light_vp = state->light_vp;
        dfu.screen_size[0] = (float)WINDOW_WIDTH;
        dfu.screen_size[1] = (float)WINDOW_HEIGHT;
        dfu.near_plane = NEAR_PLANE;
        dfu.far_plane = FAR_PLANE;
        dfu.decal_tint[0] = d->tint[0];
        dfu.decal_tint[1] = d->tint[1];
        dfu.decal_tint[2] = d->tint[2];
        dfu.decal_tint[3] = d->tint[3];
        dfu.eye_pos[0] = state->cam_position.x;
        dfu.eye_pos[1] = state->cam_position.y;
        dfu.eye_pos[2] = state->cam_position.z;
        dfu.ambient = AMBIENT_SCENE;
        dfu.light_dir[0] = light_dir.x;
        dfu.light_dir[1] = light_dir.y;
        dfu.light_dir[2] = light_dir.z;
        dfu.light_intensity = 1.0f;
        dfu.light_color[0] = 1.0f;
        dfu.light_color[1] = 1.0f;
        dfu.light_color[2] = 1.0f;
        dfu.shininess = SHININESS;
        dfu.specular_str = SPECULAR_STR;
        SDL_PushGPUFragmentUniformData(cmd, 0, &dfu, sizeof(dfu));

        /* Bind samplers: slot 0 = depth, slot 1 = decal texture,
         *                 slot 2 = shadow map, slot 3 = scene normals */
        SDL_GPUTextureSamplerBinding decal_tex_bind;
        decal_tex_bind.texture = state->decal_textures[d->tex_index];
        decal_tex_bind.sampler = state->linear_clamp;

        SDL_GPUTextureSamplerBinding shadow_bind_decal;
        shadow_bind_decal.texture = state->shadow_depth;
        shadow_bind_decal.sampler = state->nearest_clamp;

        SDL_GPUTextureSamplerBinding normal_bind;
        normal_bind.texture = state->scene_normal;
        normal_bind.sampler = state->nearest_clamp;

        SDL_GPUTextureSamplerBinding frag_samplers[4] = {
            depth_bind, decal_tex_bind, shadow_bind_decal, normal_bind
        };
        SDL_BindGPUFragmentSamplers(decal_pass, 0, frag_samplers, 4);

        SDL_DrawGPUIndexedPrimitives(decal_pass, CUBE_INDEX_COUNT,
                                     1, 0, 0, 0);
    }

    SDL_EndGPURenderPass(decal_pass);

    /* ── 8. Submit command buffer + capture ─────────────────────── */

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain_tex)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
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
            SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── End of Part C ────────────────────────────────────────────────────── */
/* ── Part D: Cleanup ──────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    if (!SDL_WaitForGPUIdle(state->device)) {
        SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

    /* Release pipelines */
    if (state->shadow_pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->scene_pipeline)  SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->grid_pipeline)   SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->decal_pipeline)  SDL_ReleaseGPUGraphicsPipeline(state->device, state->decal_pipeline);

    /* Release Suzanne GPU buffers */
    if (state->suzanne_vb) SDL_ReleaseGPUBuffer(state->device, state->suzanne_vb);
    if (state->suzanne_ib) SDL_ReleaseGPUBuffer(state->device, state->suzanne_ib);

    /* Release cube (decal box) buffers */
    if (state->cube_vb) SDL_ReleaseGPUBuffer(state->device, state->cube_vb);
    if (state->cube_ib) SDL_ReleaseGPUBuffer(state->device, state->cube_ib);

    /* Release grid buffers */
    if (state->grid_vb) SDL_ReleaseGPUBuffer(state->device, state->grid_vb);
    if (state->grid_ib) SDL_ReleaseGPUBuffer(state->device, state->grid_ib);

    /* Release decal textures */
    for (int i = 0; i < NUM_DECAL_SHAPES; i++) {
        if (state->decal_textures[i]) {
            SDL_ReleaseGPUTexture(state->device, state->decal_textures[i]);
        }
    }

    /* Release samplers */
    if (state->nearest_clamp) SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->linear_clamp)  SDL_ReleaseGPUSampler(state->device, state->linear_clamp);

    /* Release depth textures */
    if (state->shadow_depth) SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->scene_depth)  SDL_ReleaseGPUTexture(state->device, state->scene_depth);
    if (state->scene_normal) SDL_ReleaseGPUTexture(state->device, state->scene_normal);

    /* Free glTF scene */
    forge_arena_destroy(&state->gltf_arena);

    /* Release window from GPU device before destroying */
    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
