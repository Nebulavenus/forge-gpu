/*
 * Lesson 14 — Environment Mapping
 *
 * Render a space shuttle floating in a Milky Way starscape, with its surface
 * blending diffuse texture and reflections of the surrounding stars.  This
 * lesson introduces cube map textures (SDL_GPU_TEXTURETYPE_CUBE) and the
 * environment mapping technique.
 *
 * Two pipelines in one render pass:
 *   1. SKYBOX — a unit cube with depth=1.0, textured by the cube map.
 *      The view matrix has translation stripped so the skybox follows camera
 *      rotation only (the camera can never "move through" the stars).
 *   2. SHUTTLE — the OBJ model with Blinn-Phong lighting (from Lesson 10)
 *      plus environment reflections.  The fragment shader computes
 *      R = reflect(-V, N) and samples the same cube map for reflected color,
 *      then blends it with the diffuse texture.
 *
 * What's new compared to Lesson 10:
 *   - Cube map textures (SDL_GPU_TEXTURETYPE_CUBE, 6 faces)
 *   - Skybox rendering (pos.xyww depth technique, rotation-only VP)
 *   - Environment reflection mapping (reflect + cube map sample + lerp)
 *   - Two graphics pipelines in one render pass (different depth/cull settings)
 *   - OBJ model loading (instead of glTF) — re-uses Lesson 08's shuttle
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline             (Lesson 02)
 *   - Push uniforms for matrices + fragment data             (Lesson 03)
 *   - Texture + sampler binding, mipmaps                     (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize         (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time        (Lesson 07)
 *   - OBJ parsing, GPU upload, texture loading               (Lesson 08)
 *   - Blinn-Phong lighting (ambient + diffuse + specular)    (Lesson 10)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   Escape             — release mouse / quit
 *
 * Skybox panorama: ESO / S. Brunier — Milky Way, CC BY 4.0
 * Model: Space Shuttle by Microsoft, CC Attribution
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include "math/forge_math.h"
#include "obj/forge_obj.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
/* Skybox shaders: unit cube with cube map sampling */
#include "shaders/compiled/skybox_vert_spirv.h"
#include "shaders/compiled/skybox_frag_spirv.h"
#include "shaders/compiled/skybox_vert_dxil.h"
#include "shaders/compiled/skybox_frag_dxil.h"

/* Shuttle shaders: Blinn-Phong + environment reflection */
#include "shaders/compiled/shuttle_vert_spirv.h"
#include "shaders/compiled/shuttle_frag_spirv.h"
#include "shaders/compiled/shuttle_vert_dxil.h"
#include "shaders/compiled/shuttle_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 14 Environment Mapping"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Black clear color — the skybox covers everything anyway. */
#define CLEAR_R 0.0f
#define CLEAR_G 0.0f
#define CLEAR_B 0.0f
#define CLEAR_A 1.0f

/* Depth buffer configuration. */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D16_UNORM

/* Cube map face count — always 6 (the definition of a cube map). */
#define CUBEMAP_FACE_COUNT 6

/* Cube map face size — matches the output of equirect_to_cubemap.py. */
#define CUBEMAP_FACE_SIZE 1024

/* Bytes per pixel for RGBA textures. */
#define BYTES_PER_PIXEL 4

/* 1x1 white placeholder texture dimensions. */
#define WHITE_TEX_DIM    1
#define WHITE_TEX_LAYERS 1
#define WHITE_TEX_LEVELS 1
#define WHITE_RGBA       255

/* Maximum LOD for sampler — effectively unlimited. */
#define MAX_LOD_UNLIMITED 1000.0f

/* Skybox shader resource counts.
 * Vertex:   0 samplers, 0 storage tex, 0 storage buf, 1 uniform buf
 * Fragment: 1 sampler (cube map), 0 storage tex, 0 storage buf, 0 uniform buf */
#define SKY_VERT_NUM_SAMPLERS         0
#define SKY_VERT_NUM_STORAGE_TEXTURES 0
#define SKY_VERT_NUM_STORAGE_BUFFERS  0
#define SKY_VERT_NUM_UNIFORM_BUFFERS  1

#define SKY_FRAG_NUM_SAMPLERS         1
#define SKY_FRAG_NUM_STORAGE_TEXTURES 0
#define SKY_FRAG_NUM_STORAGE_BUFFERS  0
#define SKY_FRAG_NUM_UNIFORM_BUFFERS  0

/* Skybox vertex attributes: just position (float3). */
#define SKY_NUM_VERTEX_ATTRIBUTES 1

/* Shuttle shader resource counts.
 * Vertex:   0 samplers, 0 storage tex, 0 storage buf, 1 uniform buf (MVP+model)
 * Fragment: 2 samplers (diffuse + env), 0 storage tex, 0 storage buf, 1 uniform buf */
#define SHUTTLE_VERT_NUM_SAMPLERS         0
#define SHUTTLE_VERT_NUM_STORAGE_TEXTURES 0
#define SHUTTLE_VERT_NUM_STORAGE_BUFFERS  0
#define SHUTTLE_VERT_NUM_UNIFORM_BUFFERS  1

#define SHUTTLE_FRAG_NUM_SAMPLERS         2
#define SHUTTLE_FRAG_NUM_STORAGE_TEXTURES 0
#define SHUTTLE_FRAG_NUM_STORAGE_BUFFERS  0
#define SHUTTLE_FRAG_NUM_UNIFORM_BUFFERS  1

/* Shuttle vertex attributes: position + normal + uv. */
#define SHUTTLE_NUM_VERTEX_ATTRIBUTES 3

/* Model and skybox asset paths — relative to executable directory. */
#define MODEL_OBJ_PATH     "assets/models/space-shuttle/space-shuttle.obj"
#define MODEL_TEXTURE_PATH "assets/models/space-shuttle/ShuttleDiffuseMap.png"
#define SKYBOX_FACE_DIR    "assets/skyboxes/milkyway/"
#define PATH_BUFFER_SIZE   512

/* ── Camera parameters ───────────────────────────────────────────────────── */

/* Start at the front-left of the shuttle, looking down at it.
 * The shuttle's nose points toward -Z.  Positioning the camera at
 * negative-X, positive-Y, negative-Z gives a 3/4 front-left view
 * with plenty of sky visible around the shuttle. */
#define CAM_START_X    -35.0f
#define CAM_START_Y     21.0f
#define CAM_START_Z     28.0f
#define CAM_START_YAW  -51.0f   /* degrees — facing toward the shuttle */
#define CAM_START_PITCH -25.0f  /* degrees — looking slightly down */

/* Movement speed (units per second). */
#define MOVE_SPEED 3.0f

/* Mouse sensitivity: radians per pixel of mouse motion. */
#define MOUSE_SENSITIVITY 0.002f

/* Pitch clamp to prevent camera flipping (same as Lesson 07). */
#define MAX_PITCH_DEG 89.0f

/* Perspective projection. */
#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

/* Time conversion and delta time clamping. */
#define MS_TO_SEC      1000.0f
#define MAX_DELTA_TIME 0.1f

/* ── Lighting parameters ─────────────────────────────────────────────────── */

/* Directional "sun" light from the rear-right of the shuttle.
 * Direction points TOWARD the light (from surface to light).
 * Placing the sun behind-right means the camera-facing side of
 * the shuttle is mostly in shadow, making the environment
 * reflections clearly visible on those surfaces. */
#define LIGHT_DIR_X  1.0f
#define LIGHT_DIR_Y  0.3f
#define LIGHT_DIR_Z  1.0f

/* Blinn-Phong material parameters. */
#define SHININESS     64.0f    /* specular exponent — higher = tighter highlight */
#define AMBIENT_STR   0.08f    /* ambient intensity [0..1] — kept low so
                                 * env reflections dominate in shadow areas */
#define SPECULAR_STR  0.5f     /* specular intensity [0..1] */

/* Environment reflectivity: 60% reflected starscape, 40% diffuse texture.
 * Higher than physically realistic, but makes the star reflections clearly
 * visible on the shuttle's hull — the point of this lesson. */
#define REFLECTIVITY  0.6f

/* ── Skybox cube geometry ─────────────────────────────────────────────────── */
/* A unit cube centered at the origin — 8 vertices, 36 indices (12 triangles).
 * Each vertex position doubles as the cube map sample direction. */

#define SKYBOX_VERTEX_COUNT 8
#define SKYBOX_INDEX_COUNT  36

/* ── Uniform data ─────────────────────────────────────────────────────────── */

/* Skybox vertex uniforms: rotation-only VP matrix (64 bytes). */
typedef struct SkyboxVertUniforms {
    mat4 vp_no_translation;  /* View (rotation only) * Projection */
} SkyboxVertUniforms;

/* Shuttle vertex uniforms: two matrices (128 bytes). */
typedef struct ShuttleVertUniforms {
    mat4 mvp;    /* Model-View-Projection (64 bytes) */
    mat4 model;  /* Model (world) matrix (64 bytes)  */
} ShuttleVertUniforms;

/* Shuttle fragment uniforms must match the HLSL cbuffer layout (80 bytes):
 *   float4 base_color      (16 bytes)
 *   float4 light_dir       (16 bytes)
 *   float4 eye_pos         (16 bytes)
 *   uint   has_texture      (4 bytes)
 *   float  shininess        (4 bytes)
 *   float  ambient          (4 bytes)
 *   float  specular_str     (4 bytes)
 *   float  reflectivity     (4 bytes)
 *   float  padding[3]      (12 bytes) — pad to 16-byte boundary
 * Total: 80 bytes, 16-byte aligned. */
typedef struct ShuttleFragUniforms {
    float base_color[4];
    float light_dir[4];
    float eye_pos[4];
    Uint32 has_texture;
    float shininess;
    float ambient;
    float specular_str;
    float reflectivity;
    float _padding[3];
} ShuttleFragUniforms;

/* ── Application state ───────────────────────────────────────────────────── */

typedef struct app_state {
    /* GPU core */
    SDL_Window              *window;
    SDL_GPUDevice           *device;

    /* Two pipelines — rendered in the same pass */
    SDL_GPUGraphicsPipeline *skybox_pipeline;
    SDL_GPUGraphicsPipeline *shuttle_pipeline;

    /* Shared GPU resources */
    SDL_GPUTexture          *depth_texture;
    SDL_GPUSampler          *sampler;          /* trilinear REPEAT for diffuse */
    SDL_GPUSampler          *cubemap_sampler;  /* trilinear CLAMP_TO_EDGE for cube maps */
    SDL_GPUTexture          *cubemap_texture;  /* 6-face environment cube map */
    SDL_GPUTexture          *white_texture;    /* 1x1 placeholder */
    Uint32                   depth_width;
    Uint32                   depth_height;

    /* Skybox geometry */
    SDL_GPUBuffer           *skybox_vb;        /* 8 vertices (float3 position) */
    SDL_GPUBuffer           *skybox_ib;        /* 36 indices (Uint16) */

    /* Shuttle geometry */
    SDL_GPUBuffer           *shuttle_vb;       /* De-indexed OBJ vertices */
    Uint32                   shuttle_vertex_count;
    SDL_GPUTexture          *shuttle_texture;  /* Diffuse texture */

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

/* ── Depth texture helper ────────────────────────────────────────────────── */

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
/* Load a 2D image, convert to RGBA, upload with mipmaps. */

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

    /* Copy row-by-row to respect SDL_Surface pitch (may have padding). */
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

/* ── 1x1 white placeholder texture ──────────────────────────────────────── */

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

/* ── Cube map texture creation ───────────────────────────────────────────── */
/* Loads 6 face PNG images and uploads them into a single cube map texture.
 *
 * SDL_GPU_TEXTURETYPE_CUBE requires layer_count_or_depth = 6.
 * Each face is uploaded to a different layer index matching the
 * SDL_GPUCubeMapFace enum:
 *   0 = +X (px), 1 = -X (nx), 2 = +Y (py),
 *   3 = -Y (ny), 4 = +Z (pz), 5 = -Z (nz)   */

static SDL_GPUTexture *create_cubemap_texture(SDL_GPUDevice *device,
                                               const char *face_dir)
{
    /* Face filenames in SDL_GPUCubeMapFace enum order. */
    static const char *face_names[CUBEMAP_FACE_COUNT] = {
        "px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"
    };

    /* Create the cube map GPU texture. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_CUBE;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = CUBEMAP_FACE_SIZE;
    tex_info.height               = CUBEMAP_FACE_SIZE;
    tex_info.layer_count_or_depth = CUBEMAP_FACE_COUNT;
    tex_info.num_levels           = 1;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create cube map texture: %s", SDL_GetError());
        return NULL;
    }

    /* Allocate a transfer buffer large enough for one face (reused per face). */
    Uint32 face_bytes = CUBEMAP_FACE_SIZE * CUBEMAP_FACE_SIZE * BYTES_PER_PIXEL;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = face_bytes;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create cubemap transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    /* Upload each face one at a time. */
    for (int face = 0; face < CUBEMAP_FACE_COUNT; face++) {
        /* Build the full path: face_dir + face_name */
        char face_path[PATH_BUFFER_SIZE];
        SDL_snprintf(face_path, sizeof(face_path), "%s%s",
                     face_dir, face_names[face]);

        SDL_Surface *surface = SDL_LoadSurface(face_path);
        if (!surface) {
            SDL_Log("Failed to load cubemap face '%s': %s",
                    face_path, SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        /* Convert to ABGR8888 (SDL's name for R8G8B8A8 in memory). */
        SDL_Surface *converted = SDL_ConvertSurface(surface,
                                                     SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(surface);
        if (!converted) {
            SDL_Log("Failed to convert cubemap face: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        /* Validate that the loaded face matches the expected dimensions. */
        if (converted->w != CUBEMAP_FACE_SIZE ||
            converted->h != CUBEMAP_FACE_SIZE) {
            SDL_Log("Cubemap face '%s' is %dx%d, expected %dx%d",
                    face_path, converted->w, converted->h,
                    CUBEMAP_FACE_SIZE, CUBEMAP_FACE_SIZE);
            SDL_DestroySurface(converted);
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        /* Map transfer buffer and copy pixel data. */
        void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (!mapped) {
            SDL_Log("Failed to map cubemap transfer: %s", SDL_GetError());
            SDL_DestroySurface(converted);
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        /* Copy row-by-row to respect pitch. */
        Uint32 dest_row_bytes = CUBEMAP_FACE_SIZE * BYTES_PER_PIXEL;
        const Uint8 *row_src = (const Uint8 *)converted->pixels;
        Uint8 *row_dst = (Uint8 *)mapped;
        for (Uint32 row = 0; row < CUBEMAP_FACE_SIZE; row++) {
            SDL_memcpy(row_dst + row * dest_row_bytes,
                       row_src + row * converted->pitch,
                       dest_row_bytes);
        }
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_DestroySurface(converted);

        /* Upload this face to the cube map. */
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
        if (!cmd) {
            SDL_Log("Failed to acquire cmd for cubemap face %d: %s",
                    face, SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        if (!copy) {
            SDL_Log("Failed to begin copy pass for cubemap face %d: %s",
                    face, SDL_GetError());
            SDL_CancelGPUCommandBuffer(cmd);
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        SDL_GPUTextureTransferInfo tex_src;
        SDL_zero(tex_src);
        tex_src.transfer_buffer = transfer;
        tex_src.pixels_per_row  = CUBEMAP_FACE_SIZE;
        tex_src.rows_per_layer  = CUBEMAP_FACE_SIZE;

        SDL_GPUTextureRegion tex_dst;
        SDL_zero(tex_dst);
        tex_dst.texture = texture;
        tex_dst.layer   = (Uint32)face;  /* Face index = layer index */
        tex_dst.w       = CUBEMAP_FACE_SIZE;
        tex_dst.h       = CUBEMAP_FACE_SIZE;
        tex_dst.d       = 1;

        SDL_UploadToGPUTexture(copy, &tex_src, &tex_dst, false);
        SDL_EndGPUCopyPass(copy);

        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed for cubemap face %d: %s",
                    face, SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

        SDL_Log("  Uploaded cubemap face %d (%s)", face, face_names[face]);
    }

    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_Log("Cube map texture created (%dx%d, 6 faces)",
            CUBEMAP_FACE_SIZE, CUBEMAP_FACE_SIZE);
    return texture;
}

/* ── Skybox geometry ─────────────────────────────────────────────────────── */
/* Creates vertex and index buffers for a unit cube [-1..1].
 * Each vertex position also serves as the cube map sample direction. */

static bool create_skybox_geometry(SDL_GPUDevice *device, app_state *state)
{
    /* 8 corner vertices of a unit cube. */
    float vertices[SKYBOX_VERTEX_COUNT * 3] = {
        -1.0f, -1.0f, -1.0f,  /* 0: left  bottom back  */
         1.0f, -1.0f, -1.0f,  /* 1: right bottom back  */
         1.0f,  1.0f, -1.0f,  /* 2: right top    back  */
        -1.0f,  1.0f, -1.0f,  /* 3: left  top    back  */
        -1.0f, -1.0f,  1.0f,  /* 4: left  bottom front */
         1.0f, -1.0f,  1.0f,  /* 5: right bottom front */
         1.0f,  1.0f,  1.0f,  /* 6: right top    front */
        -1.0f,  1.0f,  1.0f,  /* 7: left  top    front */
    };

    /* 12 triangles (36 indices) forming a cube.  Winding order is
     * clockwise when viewed from OUTSIDE — but we render the skybox
     * from the INSIDE, so we cull front faces (not back faces). */
    Uint16 indices[SKYBOX_INDEX_COUNT] = {
        /* Back face (-Z) */
        0, 2, 1,  0, 3, 2,
        /* Front face (+Z) */
        4, 5, 6,  4, 6, 7,
        /* Left face (-X) */
        0, 4, 7,  0, 7, 3,
        /* Right face (+X) */
        1, 2, 6,  1, 6, 5,
        /* Bottom face (-Y) */
        0, 1, 5,  0, 5, 4,
        /* Top face (+Y) */
        3, 7, 6,  3, 6, 2,
    };

    state->skybox_vb = upload_gpu_buffer(
        device, SDL_GPU_BUFFERUSAGE_VERTEX,
        vertices, sizeof(vertices));
    if (!state->skybox_vb) return false;

    state->skybox_ib = upload_gpu_buffer(
        device, SDL_GPU_BUFFERUSAGE_INDEX,
        indices, sizeof(indices));
    if (!state->skybox_ib) {
        SDL_ReleaseGPUBuffer(device, state->skybox_vb);
        state->skybox_vb = NULL;
        return false;
    }

    return true;
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

    /* ── 6. Allocate app state ────────────────────────────────────────── */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window        = window;
    state->device        = device;
    state->depth_texture = depth_texture;
    state->depth_width   = (Uint32)win_w;
    state->depth_height  = (Uint32)win_h;

    /* ── 7. Create sampler (shared by all textures) ──────────────────── */
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

    state->sampler = SDL_CreateGPUSampler(device, &smp_info);
    if (!state->sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Create a separate sampler for cube maps with CLAMP_TO_EDGE to
     * avoid visible seams at face boundaries. */
    SDL_GPUSamplerCreateInfo cube_smp_info;
    SDL_zero(cube_smp_info);
    cube_smp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
    cube_smp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
    cube_smp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    cube_smp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    cube_smp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    cube_smp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    cube_smp_info.min_lod        = 0.0f;
    cube_smp_info.max_lod        = MAX_LOD_UNLIMITED;

    state->cubemap_sampler = SDL_CreateGPUSampler(device, &cube_smp_info);
    if (!state->cubemap_sampler) {
        SDL_Log("Failed to create cubemap sampler: %s", SDL_GetError());
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Create white placeholder texture ──────────────────────────── */
    state->white_texture = create_white_texture(device);
    if (!state->white_texture) {
        SDL_ReleaseGPUSampler(device, state->cubemap_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 9. Load cube map from 6 face PNGs ───────────────────────────── */
    const char *base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, state->white_texture);
        SDL_ReleaseGPUSampler(device, state->cubemap_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    char cubemap_dir[PATH_BUFFER_SIZE];
    SDL_snprintf(cubemap_dir, sizeof(cubemap_dir), "%s%s",
                 base_path, SKYBOX_FACE_DIR);

    SDL_Log("Loading cube map from: %s", cubemap_dir);
    state->cubemap_texture = create_cubemap_texture(device, cubemap_dir);
    if (!state->cubemap_texture) {
        SDL_ReleaseGPUTexture(device, state->white_texture);
        SDL_ReleaseGPUSampler(device, state->cubemap_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 10. Create skybox geometry (unit cube) ──────────────────────── */
    if (!create_skybox_geometry(device, state)) {
        SDL_Log("Failed to create skybox geometry");
        SDL_ReleaseGPUTexture(device, state->cubemap_texture);
        SDL_ReleaseGPUTexture(device, state->white_texture);
        SDL_ReleaseGPUSampler(device, state->cubemap_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 11. Load shuttle OBJ model ──────────────────────────────────── */
    char obj_path[PATH_BUFFER_SIZE];
    char tex_path[PATH_BUFFER_SIZE];
    SDL_snprintf(obj_path, sizeof(obj_path), "%s%s", base_path, MODEL_OBJ_PATH);
    SDL_snprintf(tex_path, sizeof(tex_path), "%s%s", base_path, MODEL_TEXTURE_PATH);

    ForgeObjMesh mesh;
    if (!forge_obj_load(obj_path, &mesh)) {
        SDL_Log("Failed to load shuttle model from '%s'", obj_path);
        SDL_ReleaseGPUBuffer(device, state->skybox_ib);
        SDL_ReleaseGPUBuffer(device, state->skybox_vb);
        SDL_ReleaseGPUTexture(device, state->cubemap_texture);
        SDL_ReleaseGPUTexture(device, state->white_texture);
        SDL_ReleaseGPUSampler(device, state->cubemap_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_Log("Shuttle model: %u vertices", mesh.vertex_count);

    state->shuttle_vertex_count = mesh.vertex_count;
    state->shuttle_vb = upload_gpu_buffer(
        device, SDL_GPU_BUFFERUSAGE_VERTEX,
        mesh.vertices, mesh.vertex_count * (Uint32)sizeof(ForgeObjVertex));
    forge_obj_free(&mesh);

    if (!state->shuttle_vb) {
        SDL_ReleaseGPUBuffer(device, state->skybox_ib);
        SDL_ReleaseGPUBuffer(device, state->skybox_vb);
        SDL_ReleaseGPUTexture(device, state->cubemap_texture);
        SDL_ReleaseGPUTexture(device, state->white_texture);
        SDL_ReleaseGPUSampler(device, state->cubemap_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_free(state);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 12. Load shuttle diffuse texture ────────────────────────────── */
    state->shuttle_texture = load_texture(device, tex_path);
    if (!state->shuttle_texture) {
        SDL_Log("Warning: shuttle texture failed, using white placeholder");
        /* Non-fatal — we can render with the placeholder. */
    }

    /* ── 13. Create skybox shaders ───────────────────────────────────── */
    SDL_GPUShader *sky_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        skybox_vert_spirv, skybox_vert_spirv_size,
        skybox_vert_dxil,  skybox_vert_dxil_size,
        SKY_VERT_NUM_SAMPLERS, SKY_VERT_NUM_STORAGE_TEXTURES,
        SKY_VERT_NUM_STORAGE_BUFFERS, SKY_VERT_NUM_UNIFORM_BUFFERS);

    SDL_GPUShader *sky_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        skybox_frag_spirv, skybox_frag_spirv_size,
        skybox_frag_dxil,  skybox_frag_dxil_size,
        SKY_FRAG_NUM_SAMPLERS, SKY_FRAG_NUM_STORAGE_TEXTURES,
        SKY_FRAG_NUM_STORAGE_BUFFERS, SKY_FRAG_NUM_UNIFORM_BUFFERS);

    if (!sky_vs || !sky_fs) {
        if (sky_vs) SDL_ReleaseGPUShader(device, sky_vs);
        if (sky_fs) SDL_ReleaseGPUShader(device, sky_fs);
        /* cleanup omitted for brevity — SDL_AppQuit handles it */
        *appstate = state;
        return SDL_APP_FAILURE;
    }

    /* ── 14. Create shuttle shaders ──────────────────────────────────── */
    SDL_GPUShader *shuttle_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        shuttle_vert_spirv, shuttle_vert_spirv_size,
        shuttle_vert_dxil,  shuttle_vert_dxil_size,
        SHUTTLE_VERT_NUM_SAMPLERS, SHUTTLE_VERT_NUM_STORAGE_TEXTURES,
        SHUTTLE_VERT_NUM_STORAGE_BUFFERS, SHUTTLE_VERT_NUM_UNIFORM_BUFFERS);

    SDL_GPUShader *shuttle_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        shuttle_frag_spirv, shuttle_frag_spirv_size,
        shuttle_frag_dxil,  shuttle_frag_dxil_size,
        SHUTTLE_FRAG_NUM_SAMPLERS, SHUTTLE_FRAG_NUM_STORAGE_TEXTURES,
        SHUTTLE_FRAG_NUM_STORAGE_BUFFERS, SHUTTLE_FRAG_NUM_UNIFORM_BUFFERS);

    if (!shuttle_vs || !shuttle_fs) {
        if (shuttle_vs) SDL_ReleaseGPUShader(device, shuttle_vs);
        if (shuttle_fs) SDL_ReleaseGPUShader(device, shuttle_fs);
        SDL_ReleaseGPUShader(device, sky_vs);
        SDL_ReleaseGPUShader(device, sky_fs);
        *appstate = state;
        return SDL_APP_FAILURE;
    }

    /* ── 15. Create skybox pipeline ──────────────────────────────────── */
    /* Skybox vertex layout: just float3 position (12 bytes per vertex). */
    SDL_GPUVertexBufferDescription sky_vb_desc;
    SDL_zero(sky_vb_desc);
    sky_vb_desc.slot       = 0;
    sky_vb_desc.pitch      = sizeof(float) * 3;
    sky_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute sky_attrs[SKY_NUM_VERTEX_ATTRIBUTES];
    SDL_zero(sky_attrs);
    sky_attrs[0].location    = 0;
    sky_attrs[0].buffer_slot = 0;
    sky_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    sky_attrs[0].offset      = 0;

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = SDL_GetGPUSwapchainTextureFormat(device, window);

    SDL_GPUGraphicsPipelineCreateInfo sky_pipe_info;
    SDL_zero(sky_pipe_info);
    sky_pipe_info.vertex_shader   = sky_vs;
    sky_pipe_info.fragment_shader = sky_fs;

    sky_pipe_info.vertex_input_state.vertex_buffer_descriptions = &sky_vb_desc;
    sky_pipe_info.vertex_input_state.num_vertex_buffers          = 1;
    sky_pipe_info.vertex_input_state.vertex_attributes           = sky_attrs;
    sky_pipe_info.vertex_input_state.num_vertex_attributes       = SKY_NUM_VERTEX_ATTRIBUTES;

    sky_pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Cull FRONT faces — we're rendering from INSIDE the cube. */
    sky_pipe_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    sky_pipe_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_FRONT;
    sky_pipe_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth test LESS_OR_EQUAL so the skybox passes at depth=1.0 (the far
     * plane, set by the pos.xyww output).  Depth write DISABLED — the skybox
     * should never occlude any other geometry. */
    sky_pipe_info.depth_stencil_state.enable_depth_test  = true;
    sky_pipe_info.depth_stencil_state.enable_depth_write = false;
    sky_pipe_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    sky_pipe_info.target_info.color_target_descriptions = &color_desc;
    sky_pipe_info.target_info.num_color_targets         = 1;
    sky_pipe_info.target_info.has_depth_stencil_target  = true;
    sky_pipe_info.target_info.depth_stencil_format      = DEPTH_FORMAT;

    state->skybox_pipeline = SDL_CreateGPUGraphicsPipeline(device, &sky_pipe_info);
    if (!state->skybox_pipeline) {
        SDL_Log("Failed to create skybox pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, sky_vs);
        SDL_ReleaseGPUShader(device, sky_fs);
        SDL_ReleaseGPUShader(device, shuttle_vs);
        SDL_ReleaseGPUShader(device, shuttle_fs);
        *appstate = state;
        return SDL_APP_FAILURE;
    }

    /* ── 16. Create shuttle pipeline ─────────────────────────────────── */
    /* Shuttle vertex layout: ForgeObjVertex (pos + normal + uv = 32 bytes). */
    SDL_GPUVertexBufferDescription shuttle_vb_desc;
    SDL_zero(shuttle_vb_desc);
    shuttle_vb_desc.slot       = 0;
    shuttle_vb_desc.pitch      = sizeof(ForgeObjVertex);
    shuttle_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute shuttle_attrs[SHUTTLE_NUM_VERTEX_ATTRIBUTES];
    SDL_zero(shuttle_attrs);

    /* Location 0: position (float3) */
    shuttle_attrs[0].location    = 0;
    shuttle_attrs[0].buffer_slot = 0;
    shuttle_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shuttle_attrs[0].offset      = offsetof(ForgeObjVertex, position);

    /* Location 1: normal (float3) */
    shuttle_attrs[1].location    = 1;
    shuttle_attrs[1].buffer_slot = 0;
    shuttle_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shuttle_attrs[1].offset      = offsetof(ForgeObjVertex, normal);

    /* Location 2: uv (float2) */
    shuttle_attrs[2].location    = 2;
    shuttle_attrs[2].buffer_slot = 0;
    shuttle_attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    shuttle_attrs[2].offset      = offsetof(ForgeObjVertex, uv);

    SDL_GPUGraphicsPipelineCreateInfo shuttle_pipe_info;
    SDL_zero(shuttle_pipe_info);
    shuttle_pipe_info.vertex_shader   = shuttle_vs;
    shuttle_pipe_info.fragment_shader = shuttle_fs;

    shuttle_pipe_info.vertex_input_state.vertex_buffer_descriptions = &shuttle_vb_desc;
    shuttle_pipe_info.vertex_input_state.num_vertex_buffers          = 1;
    shuttle_pipe_info.vertex_input_state.vertex_attributes           = shuttle_attrs;
    shuttle_pipe_info.vertex_input_state.num_vertex_attributes       = SHUTTLE_NUM_VERTEX_ATTRIBUTES;

    shuttle_pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Standard back-face culling for the shuttle. */
    shuttle_pipe_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    shuttle_pipe_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    shuttle_pipe_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth test AND write enabled — the shuttle writes to the depth buffer. */
    shuttle_pipe_info.depth_stencil_state.enable_depth_test  = true;
    shuttle_pipe_info.depth_stencil_state.enable_depth_write = true;
    shuttle_pipe_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    shuttle_pipe_info.target_info.color_target_descriptions = &color_desc;
    shuttle_pipe_info.target_info.num_color_targets         = 1;
    shuttle_pipe_info.target_info.has_depth_stencil_target  = true;
    shuttle_pipe_info.target_info.depth_stencil_format      = DEPTH_FORMAT;

    state->shuttle_pipeline = SDL_CreateGPUGraphicsPipeline(
        device, &shuttle_pipe_info);
    if (!state->shuttle_pipeline) {
        SDL_Log("Failed to create shuttle pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, shuttle_vs);
        SDL_ReleaseGPUShader(device, shuttle_fs);
        SDL_ReleaseGPUShader(device, sky_vs);
        SDL_ReleaseGPUShader(device, sky_fs);
        *appstate = state;
        return SDL_APP_FAILURE;
    }

    /* Release shaders after pipeline creation — pipelines keep their own copy. */
    SDL_ReleaseGPUShader(device, sky_vs);
    SDL_ReleaseGPUShader(device, sky_fs);
    SDL_ReleaseGPUShader(device, shuttle_vs);
    SDL_ReleaseGPUShader(device, shuttle_fs);

    /* ── 17. Initialise camera ───────────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = CAM_START_YAW * FORGE_DEG2RAD;
    state->cam_pitch    = CAM_START_PITCH * FORGE_DEG2RAD;
    state->last_ticks   = SDL_GetTicks();

    /* Capture mouse for FPS-style look. */
#ifndef FORGE_CAPTURE
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        *appstate = state;
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
            *appstate = state;
            return SDL_APP_FAILURE;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    *appstate = state;

    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");
    SDL_Log("Lighting: Blinn-Phong (ambient=%.2f, specular=%.2f, shininess=%.0f)",
            AMBIENT_STR, SPECULAR_STR, SHININESS);
    SDL_Log("Environment reflectivity: %.0f%%", REFLECTIVITY * 100.0f);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────────── */

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

    /* ── 3. Build view and projection matrices ───────────────────────── */
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orientation);

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    float fov    = FOV_DEG * FORGE_DEG2RAD;
    mat4 proj    = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);

    /* Full VP for the shuttle (includes camera translation). */
    mat4 vp = mat4_multiply(proj, view);

    /* Rotation-only VP for the skybox — strip the translation from the
     * view matrix so the skybox always surrounds the camera. */
    mat4 view_rot = view;
    view_rot.m[12] = 0.0f;  /* Clear translation column */
    view_rot.m[13] = 0.0f;
    view_rot.m[14] = 0.0f;
    mat4 vp_sky = mat4_multiply(proj, view_rot);

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

        /* ── Draw 1: Skybox ──────────────────────────────────────────── */
        /* Drawn first.  Depth write disabled + depth=1.0 means the
         * shuttle (drawn next with depth write on) will always appear
         * in front of the skybox. */
        SDL_BindGPUGraphicsPipeline(pass, state->skybox_pipeline);

        /* Push rotation-only VP matrix. */
        SkyboxVertUniforms sky_vu;
        sky_vu.vp_no_translation = vp_sky;
        SDL_PushGPUVertexUniformData(cmd, 0, &sky_vu, sizeof(sky_vu));

        /* Bind cube map texture + CLAMP_TO_EDGE sampler. */
        SDL_GPUTextureSamplerBinding sky_binding;
        SDL_zero(sky_binding);
        sky_binding.texture = state->cubemap_texture;
        sky_binding.sampler = state->cubemap_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &sky_binding, 1);

        /* Bind skybox vertex and index buffers. */
        SDL_GPUBufferBinding sky_vb_bind;
        SDL_zero(sky_vb_bind);
        sky_vb_bind.buffer = state->skybox_vb;
        SDL_BindGPUVertexBuffers(pass, 0, &sky_vb_bind, 1);

        SDL_GPUBufferBinding sky_ib_bind;
        SDL_zero(sky_ib_bind);
        sky_ib_bind.buffer = state->skybox_ib;
        SDL_BindGPUIndexBuffer(pass, &sky_ib_bind, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, SKYBOX_INDEX_COUNT, 1, 0, 0, 0);

        /* ── Draw 2: Shuttle ─────────────────────────────────────────── */
        SDL_BindGPUGraphicsPipeline(pass, state->shuttle_pipeline);

        /* Shuttle sits at the origin — model matrix is identity. */
        mat4 model = mat4_identity();
        mat4 mvp   = mat4_multiply(vp, model);

        ShuttleVertUniforms shuttle_vu;
        shuttle_vu.mvp   = mvp;
        shuttle_vu.model = model;
        SDL_PushGPUVertexUniformData(cmd, 0, &shuttle_vu, sizeof(shuttle_vu));

        /* Set up fragment uniforms: lighting + reflectivity. */
        vec3 light_raw = vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z);
        vec3 light_dir = vec3_normalize(light_raw);

        ShuttleFragUniforms shuttle_fu;
        SDL_zero(shuttle_fu);
        shuttle_fu.base_color[0] = 1.0f;
        shuttle_fu.base_color[1] = 1.0f;
        shuttle_fu.base_color[2] = 1.0f;
        shuttle_fu.base_color[3] = 1.0f;
        shuttle_fu.light_dir[0]  = light_dir.x;
        shuttle_fu.light_dir[1]  = light_dir.y;
        shuttle_fu.light_dir[2]  = light_dir.z;
        shuttle_fu.light_dir[3]  = 0.0f;
        shuttle_fu.eye_pos[0]    = state->cam_position.x;
        shuttle_fu.eye_pos[1]    = state->cam_position.y;
        shuttle_fu.eye_pos[2]    = state->cam_position.z;
        shuttle_fu.eye_pos[3]    = 0.0f;
        shuttle_fu.has_texture   = state->shuttle_texture ? 1 : 0;
        shuttle_fu.shininess     = SHININESS;
        shuttle_fu.ambient       = AMBIENT_STR;
        shuttle_fu.specular_str  = SPECULAR_STR;
        shuttle_fu.reflectivity  = REFLECTIVITY;
        SDL_PushGPUFragmentUniformData(cmd, 0, &shuttle_fu, sizeof(shuttle_fu));

        /* Bind diffuse texture (slot 0) + cube map (slot 1). */
        SDL_GPUTextureSamplerBinding shuttle_bindings[2];
        SDL_zero(shuttle_bindings);

        /* Slot 0: diffuse texture. */
        shuttle_bindings[0].texture = state->shuttle_texture
                                      ? state->shuttle_texture
                                      : state->white_texture;
        shuttle_bindings[0].sampler = state->sampler;

        /* Slot 1: environment cube map (CLAMP_TO_EDGE sampler). */
        shuttle_bindings[1].texture = state->cubemap_texture;
        shuttle_bindings[1].sampler = state->cubemap_sampler;

        SDL_BindGPUFragmentSamplers(pass, 0, shuttle_bindings, 2);

        /* Bind shuttle vertex buffer. */
        SDL_GPUBufferBinding shuttle_vb_bind;
        SDL_zero(shuttle_vb_bind);
        shuttle_vb_bind.buffer = state->shuttle_vb;
        SDL_BindGPUVertexBuffers(pass, 0, &shuttle_vb_bind, 1);

        /* Non-indexed draw (OBJ vertices are de-indexed). */
        SDL_DrawGPUPrimitives(pass, state->shuttle_vertex_count, 1, 0, 0);

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
    if (!state) return;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    /* Release shuttle resources. */
    if (state->shuttle_texture)
        SDL_ReleaseGPUTexture(state->device, state->shuttle_texture);
    if (state->shuttle_vb)
        SDL_ReleaseGPUBuffer(state->device, state->shuttle_vb);

    /* Release skybox resources. */
    if (state->skybox_ib)
        SDL_ReleaseGPUBuffer(state->device, state->skybox_ib);
    if (state->skybox_vb)
        SDL_ReleaseGPUBuffer(state->device, state->skybox_vb);

    /* Release shared resources. */
    if (state->cubemap_texture)
        SDL_ReleaseGPUTexture(state->device, state->cubemap_texture);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
    if (state->cubemap_sampler)
        SDL_ReleaseGPUSampler(state->device, state->cubemap_sampler);
    if (state->sampler)
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);

    /* Release pipelines. */
    if (state->shuttle_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shuttle_pipeline);
    if (state->skybox_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->skybox_pipeline);

    /* Release device and window. */
    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);
    SDL_free(state);
}
