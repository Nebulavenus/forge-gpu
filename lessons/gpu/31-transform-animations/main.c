/*
 * GPU Lesson 31 — Transform Animations
 *
 * Keyframe animation drives a CesiumMilkTruck around a modular racetrack.
 * Two animation layers combine each frame:
 *
 *   1. Wheel rotation — evaluates glTF keyframe data (binary search over
 *      timestamps + quaternion slerp) and applies the result to the wheel
 *      nodes in the truck's scene hierarchy.
 *
 *   2. Path following — interpolates an array of waypoints (position + yaw)
 *      to move the entire truck along an elliptical loop around the track.
 *
 * After both layers update their respective transforms, the node hierarchy
 * is rebuilt (parent world * local) so every mesh renders at the correct
 * final position.
 *
 * Architecture — 2 render passes per frame:
 *   1. Shadow pass   — directional light depth map (2048x2048)
 *   2. Main scene    — Blinn-Phong + shadows + skybox to swapchain
 *
 * Scene elements:
 *   - CesiumMilkTruck (glTF, animated wheels, path-following body)
 *   - Modular racetrack (glTF)
 *   - Procedural dirt ground plane (Perlin noise texture)
 *   - HDR skybox (cube map, 6 PNG faces)
 *   - Directional shadow with 2x2 PCF
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
#include "gltf/forge_gltf_anim.h"
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

#include "shaders/compiled/skybox_vert_spirv.h"
#include "shaders/compiled/skybox_vert_dxil.h"
#include "shaders/compiled/skybox_frag_spirv.h"
#include "shaders/compiled/skybox_frag_dxil.h"

/* ── Constants ──────────────────────────────────────────────────────── */

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Camera. */
#define FOV_DEG            60
#define NEAR_PLANE         0.1f
#define FAR_PLANE          200.0f
#define CAM_SPEED          5.0f
#define MOUSE_SENS         0.003f
#define PITCH_CLAMP        1.5f

/* Camera initial position — elevated, looking down at the track. */
#define CAM_START_X         0.0f
#define CAM_START_Y         20.0f
#define CAM_START_Z         45.0f
#define CAM_START_YAW_DEG   0.0f
#define CAM_START_PITCH_DEG -25.0f

/* Directional light — sun from behind-right, pointing into the scene. */
#define LIGHT_DIR_X     -0.4f
#define LIGHT_DIR_Y     -0.7f
#define LIGHT_DIR_Z     -0.5f
#define LIGHT_INTENSITY  0.9f
#define LIGHT_COLOR_R    1.0f
#define LIGHT_COLOR_G    0.95f
#define LIGHT_COLOR_B    0.85f

/* Scene material defaults. */
#define MATERIAL_AMBIENT      0.2f
#define MATERIAL_SHININESS    64.0f
#define MATERIAL_SPECULAR_STR 0.3f

/* Shadow map. */
#define SHADOW_MAP_SIZE   2048
#define SHADOW_DEPTH_FMT  SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_ORTHO_SIZE 50.0f
#define SHADOW_NEAR       0.1f
#define SHADOW_FAR        100.0f
#define LIGHT_DISTANCE    40.0f

/* Ground plane. */
#define GROUND_Y         -0.05f
#define GROUND_HALF_SIZE  60.0f
#define GROUND_SHININESS  16.0f
#define GROUND_SPECULAR   0.1f
#define GROUND_UV_REPEAT  8

/* Procedural dirt texture. */
#define DIRT_TEX_SIZE          256
#define DIRT_NOISE_SCALE       8.0f
#define DIRT_NOISE_SEED        42
#define DIRT_NOISE_OCTAVES     4
#define DIRT_NOISE_LACUNARITY  2.0f
#define DIRT_NOISE_PERSISTENCE 0.5f
#define DIRT_BASE_R            0.35f
#define DIRT_RANGE_R           0.25f
#define DIRT_BASE_G            0.30f
#define DIRT_RANGE_G           0.20f
#define DIRT_BASE_B            0.15f
#define DIRT_RANGE_B           0.10f

/* Skybox. */
#define CUBEMAP_FACE_SIZE  512
#define CUBEMAP_FACE_COUNT 6
#define SKYBOX_FACE_DIR    "assets/skybox/"
#define SKYBOX_VERTEX_COUNT 8
#define SKYBOX_INDEX_COUNT  36
#define PATH_BUFFER_SIZE    512

/* Texture. */
#define BYTES_PER_PIXEL 4
#define MAX_ANISOTROPY    8
#define MAX_LOD_UNLIMITED 1000.0f /* Allow all mip levels */

/* Clear color — sky blue. */
#define CLEAR_R 0.5f
#define CLEAR_G 0.7f
#define CLEAR_B 0.9f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* Model asset paths. */
#define TRUCK_MODEL_PATH "assets/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define TRACK_MODEL_PATH "assets/track/scene.gltf"

/* Light direction degeneracy threshold. */
#define PARALLEL_THRESHOLD 0.99f

/* Quad geometry — two triangles (6 indices). */
#define QUAD_INDEX_COUNT 6

/* Animation playback. */
#define ANIM_SPEED         1.0f
#define TRUCK_DRIVE_SPEED  8.0f   /* forward movement speed (units/sec)   */
#define TRUCK_Y            0.0f

/* Path waypoint count. */
#define PATH_WAYPOINT_COUNT 14

/* ── Uniform structures ─────────────────────────────────────────────── */

/* Scene vertex uniforms — pushed per draw call. */
typedef struct SceneVertUniforms {
    mat4 mvp;      /* model-view-projection matrix */
    mat4 model;    /* model (world) matrix         */
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
    mat4 light_mvp; /* light view-projection * model */
} ShadowVertUniforms;

/* Skybox vertex uniforms. */
typedef struct SkyboxVertUniforms {
    mat4 vp_no_translation; /* view (rotation only) * projection */
} SkyboxVertUniforms;

/* A waypoint along the truck's driving path. */
typedef struct PathWaypoint {
    vec3  position;
    float yaw;              /* rotation around Y axis in radians          */
} PathWaypoint;

/* ── GPU-side model types ───────────────────────────────────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;       /* GPU vertex buffer (position+normal+uv) */
    SDL_GPUBuffer *index_buffer;        /* GPU index buffer for indexed drawing    */
    Uint32 index_count;                 /* number of indices in the index buffer   */
    int material_index;                 /* index into ModelData.materials (-1=none)*/
    SDL_GPUIndexElementSize index_type; /* 16-bit or 32-bit indices               */
    bool has_uvs;                       /* true if vertices include texture coords */
} GpuPrimitive;

typedef struct GpuMaterial {
    float base_color[4];        /* linear RGBA color factor                   */
    SDL_GPUTexture *texture;    /* diffuse texture (NULL = use white fallback) */
    bool has_texture;           /* true if a valid diffuse texture was loaded  */
} GpuMaterial;

typedef struct ModelData {
    ForgeGltfScene scene;       /* parsed glTF data (nodes, hierarchy, buffers)*/
    ForgeArena     gltf_arena;  /* arena backing glTF allocations              */
    GpuPrimitive  *primitives;  /* GPU-uploaded primitives (heap-allocated)    */
    int            primitive_count; /* number of primitives in the array       */
    GpuMaterial   *materials;   /* GPU-uploaded materials (heap-allocated)     */
    int            material_count;  /* number of materials in the array        */
} ModelData;

/* ── Modular track layout ───────────────────────────────────────────── */
/* Build a rectangular loop from the modular track pieces.
 *
 *                         Top (start/finish)
 *            ┌──────────────────────────────────┐
 *            │  TL corner      15m       TR corner │
 *            │                                     │
 *     Left   │  30m                         30m    │ Right
 *     side   │  straight                  straight │ side
 *            │                                     │
 *            │  BL corner      15m       BR corner │
 *            └──────────────────────────────────┘
 *                        Bottom straight
 *
 * Corner arm length: 7.5 units (half of 15m corner piece).
 * Corner positions: (±15, 0, ±22.5).
 * Right/left straights: 30m along Z, centered at (±15, 0, 0).
 * Top/bottom straights: 15m along X, centered at (0, 0, ±22.5).
 */

/* Track mesh indices (from the track glTF model). */
#define TRACK_MESH_CORNER_15  3   /* 90-degree corner, 15x15m        */
#define TRACK_MESH_LINE_15    5   /* straight road, 15m              */
#define TRACK_MESH_LINE_30    6   /* straight road, 30m              */
#define TRACK_MESH_START_FIN  7   /* start/finish line, 15m          */

/* Track layout geometry.  The 15x15m corner mesh origin is offset 1.16
 * from its geometric center (bounds X[-8.66,6.34], Z[-6.34,8.66]).
 * Corner inner arm extent is 6.34 — so corners sit at CX/CZ such that
 * 6.34 reaches the straight piece ends (7.5 for 15m, 15 for 30m).
 * Straight pieces are then pushed outward by the same 1.16 offset so
 * their road centers align with the corner road centers. */
#define TRACK_CX      13.84f      /* corner X offset from center     */
#define TRACK_CZ      21.34f      /* corner Z offset from center     */
#define TRACK_OFFSET   1.16f      /* corner origin-to-center offset  */

/* A track piece — one mesh placed at a custom world transform. */
typedef struct TrackPiece {
    int  mesh_index;  /* index into track model's ForgeGltfScene.meshes[] */
    mat4 transform;   /* world transform for this instance               */
} TrackPiece;

#define TRACK_PIECE_COUNT 8

/* ── Path waypoints (clockwise loop matching the track layout) ─────── */

static const PathWaypoint PATH_WAYPOINTS[PATH_WAYPOINT_COUNT] = {
    /* The truck model faces +Z at yaw=0, so:
     *   yaw = π/2  → heading +X   yaw = π    → heading -Z
     *   yaw = -π/2 → heading -X   yaw = 0    → heading +Z
     *
     * Track rectangle corners at (±TRACK_CX, ±TRACK_CZ).  Waypoints trace
     * the road centerline clockwise when viewed from above.  Each 90-degree
     * corner uses a midpoint on the circular arc (radius 7.5, center at the
     * inner rectangle corner) to prevent diagonal drift. */
    { {  -5.0f, TRUCK_Y,  22.5f },  FORGE_PI *  0.5f  },  /* top straight          */
    { {   5.0f, TRUCK_Y,  22.5f },  FORGE_PI *  0.5f  },  /* approaching TR corner */
    { {  12.8f, TRUCK_Y,  20.3f },  FORGE_PI *  0.75f },  /* TR arc midpoint       */
    { {  15.0f, TRUCK_Y,  14.0f },  FORGE_PI *  1.0f  },  /* exiting TR, heading -Z*/
    { {  15.0f, TRUCK_Y,   0.0f },  FORGE_PI *  1.0f  },  /* right straight        */
    { {  15.0f, TRUCK_Y, -14.0f },  FORGE_PI *  1.0f  },  /* approaching BR corner */
    { {  12.8f, TRUCK_Y, -20.3f }, -FORGE_PI *  0.75f },  /* BR arc midpoint       */
    { {   5.0f, TRUCK_Y, -22.5f }, -FORGE_PI *  0.5f  },  /* exiting BR, heading -X*/
    { {  -5.0f, TRUCK_Y, -22.5f }, -FORGE_PI *  0.5f  },  /* bottom straight       */
    { { -12.8f, TRUCK_Y, -20.3f }, -FORGE_PI *  0.25f },  /* BL arc midpoint       */
    { { -15.0f, TRUCK_Y, -14.0f },  FORGE_PI *  0.0f  },  /* exiting BL, heading +Z*/
    { { -15.0f, TRUCK_Y,   0.0f },  FORGE_PI *  0.0f  },  /* left straight         */
    { { -15.0f, TRUCK_Y,  14.0f },  FORGE_PI *  0.0f  },  /* approaching TL corner */
    { { -12.8f, TRUCK_Y,  20.3f },  FORGE_PI *  0.25f },  /* TL arc midpoint       */
};

/* ── Application state ──────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;  /* main application window                     */
    SDL_GPUDevice *device;  /* GPU device handle for all resource creation */

    /* Pipelines. */
    SDL_GPUGraphicsPipeline *scene_pipeline;  /* Blinn-Phong lit geometry    */
    SDL_GPUGraphicsPipeline *shadow_pipeline; /* depth-only shadow map       */
    SDL_GPUGraphicsPipeline *skybox_pipeline; /* cube map sky behind scene   */

    /* Render targets. */
    SDL_GPUTexture *shadow_depth; /* D32 depth texture (SHADOW_MAP_SIZE^2)  */
    SDL_GPUTexture *main_depth;   /* D32 depth texture (window-sized)       */

    /* Samplers. */
    SDL_GPUSampler *sampler;         /* trilinear + aniso repeat (diffuse)  */
    SDL_GPUSampler *nearest_clamp;   /* nearest + clamp (shadow sampling)   */
    SDL_GPUSampler *cubemap_sampler; /* linear + clamp (skybox cube map)    */

    /* Scene objects. */
    SDL_GPUTexture *white_texture;   /* 1x1 white fallback for untextured   */
    SDL_GPUTexture *cubemap_texture; /* 6-face skybox cube map              */
    SDL_GPUTexture *dirt_texture;    /* procedural Perlin noise ground      */
    ModelData truck;                 /* CesiumMilkTruck glTF + GPU data     */
    ModelData track;                 /* modular racetrack glTF + GPU data   */
    TrackPiece track_pieces[TRACK_PIECE_COUNT]; /* custom layout instances */

    /* Floor geometry. */
    SDL_GPUBuffer *floor_vb; /* 4-vertex XZ quad at GROUND_Y               */
    SDL_GPUBuffer *floor_ib; /* 6-index quad (two triangles)               */

    /* Skybox geometry. */
    SDL_GPUBuffer *skybox_vb; /* 8-vertex unit cube                        */
    SDL_GPUBuffer *skybox_ib; /* 36-index cube (12 triangles)              */

    /* Light. */
    mat4 light_vp; /* orthographic view-projection for shadow mapping      */

    /* Swapchain format. */
    SDL_GPUTextureFormat swapchain_format; /* pixel format of the swapchain */

    /* Camera. */
    vec3  cam_position; /* world-space camera position                     */
    float cam_yaw;      /* horizontal rotation in radians (0 = +Z)        */
    float cam_pitch;    /* vertical rotation in radians (clamped ±1.5)    */

    /* Animation. */
    float     wheel_time;  /* current wheel animation playback time        */
    float     path_distance;  /* distance traveled along the path (wraps)  */
    float     total_path_len; /* total loop perimeter                      */
    float     seg_cumulative[PATH_WAYPOINT_COUNT]; /* cumulative dist per segment */
    vec3      truck_pos;   /* current truck world position                 */
    float     truck_yaw;   /* current truck heading                        */

    /* Timing and input. */
    Uint64 last_ticks;     /* performance counter at previous frame        */
    bool   mouse_captured; /* true when mouse is in relative (FPS) mode    */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;  /* screenshot/GIF capture state                 */
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

/* ── Helper: create cubemap texture from 6 face PNGs ─────────────── */

static SDL_GPUTexture *create_cubemap_texture(SDL_GPUDevice *device,
                                               const char *face_dir)
{
    static const char *face_names[CUBEMAP_FACE_COUNT] = {
        "px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"
    };

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

    for (int face = 0; face < CUBEMAP_FACE_COUNT; face++) {
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

        SDL_Surface *converted = SDL_ConvertSurface(surface,
                                                     SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(surface);
        if (!converted) {
            SDL_Log("Failed to convert cubemap face: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

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

        void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (!mapped) {
            SDL_Log("Failed to map cubemap transfer: %s", SDL_GetError());
            SDL_DestroySurface(converted);
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return NULL;
        }

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
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
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
        tex_dst.layer   = (Uint32)face;
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

/* ── Helper: procedural dirt texture (Perlin noise) ─────────────────── */

static SDL_GPUTexture *create_dirt_texture(SDL_GPUDevice *device)
{
    Uint32 total_bytes = DIRT_TEX_SIZE * DIRT_TEX_SIZE * BYTES_PER_PIXEL;

    Uint8 *pixels = (Uint8 *)SDL_calloc(1, total_bytes);
    if (!pixels) {
        SDL_Log("Failed to allocate dirt texture pixels");
        return NULL;
    }

    /* Generate brown/green dirt pattern using fractal Brownian motion.
     * The noise range is approximately [-1, 1], remapped to [0, 1]. */
    for (int y = 0; y < DIRT_TEX_SIZE; y++) {
        for (int x = 0; x < DIRT_TEX_SIZE; x++) {
            float nx = (float)x / (float)DIRT_TEX_SIZE * DIRT_NOISE_SCALE;
            float ny = (float)y / (float)DIRT_TEX_SIZE * DIRT_NOISE_SCALE;
            float n = forge_noise_fbm2d(nx, ny, DIRT_NOISE_SEED,
                                         DIRT_NOISE_OCTAVES,
                                         DIRT_NOISE_LACUNARITY,
                                         DIRT_NOISE_PERSISTENCE);
            n = n * 0.5f + 0.5f; /* remap [-1..1] to [0..1] */

            /* Brown/green dirt base color */
            float r = DIRT_BASE_R + n * DIRT_RANGE_R;
            float g = DIRT_BASE_G + n * DIRT_RANGE_G;
            float b = DIRT_BASE_B + n * DIRT_RANGE_B;

            /* Store as sRGB since texture format is R8G8B8A8_UNORM_SRGB */
            int idx = (y * DIRT_TEX_SIZE + x) * BYTES_PER_PIXEL;
            pixels[idx + 0] = (Uint8)(r * 255.0f);
            pixels[idx + 1] = (Uint8)(g * 255.0f);
            pixels[idx + 2] = (Uint8)(b * 255.0f);
            pixels[idx + 3] = 255;
        }
    }

    /* Create GPU texture with mipmaps. */
    Uint32 mip_levels = (Uint32)(forge_log2f((float)DIRT_TEX_SIZE)) + 1;

    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.width               = DIRT_TEX_SIZE;
    tex_info.height              = DIRT_TEX_SIZE;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = mip_levels;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                   SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create dirt texture: %s", SDL_GetError());
        SDL_free(pixels);
        return NULL;
    }

    /* Upload via transfer buffer. */
    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create dirt texture xfer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        SDL_free(pixels);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map dirt texture xfer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        SDL_free(pixels);
        return NULL;
    }
    SDL_memcpy(mapped, pixels, total_bytes);
    SDL_UnmapGPUTransferBuffer(device, xfer);
    SDL_free(pixels);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for dirt texture: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Failed to begin copy pass for dirt texture: %s", SDL_GetError());
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
    dst.w       = DIRT_TEX_SIZE;
    dst.h       = DIRT_TEX_SIZE;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_GenerateMipmapsForGPUTexture(cmd, tex);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit dirt texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_Log("Dirt texture created (%dx%d, %u mip levels)",
            DIRT_TEX_SIZE, DIRT_TEX_SIZE, mip_levels);
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
            if (src->index_stride != 2 && src->index_stride != 4) {
                SDL_Log("Unsupported index stride %u for primitive %d",
                        (unsigned)src->index_stride, i);
                free_model_gpu(device, model);
                return false;
            }
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

                if (!found) {
                    if (loaded_count < FORGE_GLTF_MAX_IMAGES) {
                        dst->texture = load_texture(device, src->texture_path);
                        if (dst->texture) {
                            loaded_textures[loaded_count] = dst->texture;
                            loaded_paths[loaded_count]    = src->texture_path;
                            loaded_count++;
                        } else {
                            dst->has_texture = false;
                        }
                    } else {
                        SDL_Log("Texture cache full (%d), skipping texture for material %d",
                                FORGE_GLTF_MAX_IMAGES, i);
                        dst->has_texture = false;
                        dst->texture     = NULL;
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
    model->gltf_arena = forge_arena_create(0);
    if (!model->gltf_arena.first) {
        SDL_Log("Out of memory creating arena for glTF: %s", path);
        return false;
    }
    if (!forge_gltf_load(path, &model->scene, &model->gltf_arena)) {
        SDL_Log("Failed to load glTF: %s", path);
        forge_arena_destroy(&model->gltf_arena);
        return false;
    }
    if (!upload_model_to_gpu(device, model)) {
        forge_arena_destroy(&model->gltf_arena);
        return false;
    }
    return true;
}


/* ── Animation: evaluate path position and yaw at a given fraction ──── */

/* Returns position and yaw by interpolating between waypoints.
 * path_t is in [0, 1] representing one full loop around the track. */
static float evaluate_path_yaw(float distance,
                                const float *seg_cumulative)
{
    /* Find which segment the truck is in based on cumulative distance.
     * seg_cumulative[i] holds the total distance up to the END of
     * segment i.  Binary search would work, but with only 14 segments
     * a linear scan is simpler and fast enough. */
    int seg = 0;
    while (seg < PATH_WAYPOINT_COUNT - 1 &&
           distance >= seg_cumulative[seg])
        seg++;

    /* Fractional position within this segment. */
    float seg_start = (seg > 0) ? seg_cumulative[seg - 1] : 0.0f;
    float seg_len   = seg_cumulative[seg] - seg_start;
    float frac      = (seg_len > 1e-6f)
                    ? (distance - seg_start) / seg_len
                    : 0.0f;

    int i0 = seg;
    int i1 = (seg + 1) % PATH_WAYPOINT_COUNT;

    const PathWaypoint *wp0 = &PATH_WAYPOINTS[i0];
    const PathWaypoint *wp1 = &PATH_WAYPOINTS[i1];

    /* Yaw: slerp via quaternions to handle the 0/2pi wrap-around correctly.
     * Build a yaw-only quaternion for each waypoint and slerp between them. */
    vec3 y_axis = vec3_create(0.0f, 1.0f, 0.0f);
    quat q0 = quat_from_axis_angle(y_axis, wp0->yaw);
    quat q1 = quat_from_axis_angle(y_axis, wp1->yaw);
    quat qr = quat_slerp(q0, q1, frac);

    return 2.0f * SDL_atan2f(qr.y, qr.w);
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
            if (prim_idx < 0 || prim_idx >= model->primitive_count) {
                SDL_Log("Skipping invalid primitive index %d (count=%d)",
                        prim_idx, model->primitive_count);
                continue;
            }
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

/* ── Helper: draw a model with the scene pipeline ───────────────────── */

static void draw_model_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    const mat4 *placement,
    const mat4 *cam_vp,
    const vec3 *eye_pos)
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
        vert_u.light_vp = state->light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            if (prim_idx < 0 || prim_idx >= model->primitive_count) {
                SDL_Log("Skipping invalid primitive index %d (count=%d)",
                        prim_idx, model->primitive_count);
                continue;
            }
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

            frag_u.eye_pos[0]       = eye_pos->x;
            frag_u.eye_pos[1]       = eye_pos->y;
            frag_u.eye_pos[2]       = eye_pos->z;
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

/* ── Helper: draw one track mesh at a custom transform (shadow) ────── */

static void draw_track_piece_shadow(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    int mesh_index,
    const mat4 *world,
    const mat4 *light_vp)
{
    const ForgeGltfScene *scene = &model->scene;
    if (mesh_index < 0 || mesh_index >= scene->mesh_count)
        return;

    ShadowVertUniforms vert_u;
    vert_u.light_mvp = mat4_multiply(*light_vp, *world);
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    const ForgeGltfMesh *mesh = &scene->meshes[mesh_index];
    for (int pi = 0; pi < mesh->primitive_count; pi++) {
        int prim_idx = mesh->first_primitive + pi;
        if (prim_idx < 0 || prim_idx >= model->primitive_count) continue;
        const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];
        if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer) continue;

        SDL_GPUBufferBinding vb = { gpu_prim->vertex_buffer, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_GPUBufferBinding ib = { gpu_prim->index_buffer, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib, gpu_prim->index_type);
        SDL_DrawGPUIndexedPrimitives(pass, gpu_prim->index_count, 1, 0, 0, 0);
    }
}

/* ── Helper: draw one track mesh at a custom transform (scene) ───── */

static void draw_track_piece_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    int mesh_index,
    const mat4 *world,
    const mat4 *cam_vp,
    const vec3 *eye_pos)
{
    const ForgeGltfScene *scene = &model->scene;
    if (mesh_index < 0 || mesh_index >= scene->mesh_count)
        return;

    mat4 mvp = mat4_multiply(*cam_vp, *world);
    SceneVertUniforms vert_u;
    vert_u.mvp      = mvp;
    vert_u.model    = *world;
    vert_u.light_vp = state->light_vp;
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    const ForgeGltfMesh *mesh = &scene->meshes[mesh_index];
    for (int pi = 0; pi < mesh->primitive_count; pi++) {
        int prim_idx = mesh->first_primitive + pi;
        if (prim_idx < 0 || prim_idx >= model->primitive_count) continue;
        const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];
        if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer) continue;

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
            if (mat->texture) tex = mat->texture;
        } else {
            frag_u.base_color[0] = 1.0f;
            frag_u.base_color[1] = 1.0f;
            frag_u.base_color[2] = 1.0f;
            frag_u.base_color[3] = 1.0f;
            frag_u.has_texture   = 0.0f;
        }

        frag_u.eye_pos[0]       = eye_pos->x;
        frag_u.eye_pos[1]       = eye_pos->y;
        frag_u.eye_pos[2]       = eye_pos->z;
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

/* ── Helper: draw the ground plane with dirt texture ───────────────── */

static void draw_floor(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const app_state *state,
    const mat4 *cam_vp,
    const vec3 *eye_pos)
{
    mat4 model_mat = mat4_identity();
    mat4 mvp = mat4_multiply(*cam_vp, model_mat);

    SceneVertUniforms vert_u;
    vert_u.mvp      = mvp;
    vert_u.model    = model_mat;
    vert_u.light_vp = state->light_vp;
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    SceneFragUniforms frag_u;
    SDL_zero(frag_u);
    frag_u.base_color[0] = 1.0f; /* modulated by texture */
    frag_u.base_color[1] = 1.0f;
    frag_u.base_color[2] = 1.0f;
    frag_u.base_color[3] = 1.0f;
    frag_u.has_texture    = 1.0f;
    frag_u.eye_pos[0]    = eye_pos->x;
    frag_u.eye_pos[1]    = eye_pos->y;
    frag_u.eye_pos[2]    = eye_pos->z;
    frag_u.ambient        = MATERIAL_AMBIENT;
    frag_u.shininess      = GROUND_SHININESS;
    frag_u.specular_str   = GROUND_SPECULAR;
    frag_u.light_dir[0]   = LIGHT_DIR_X;
    frag_u.light_dir[1]   = LIGHT_DIR_Y;
    frag_u.light_dir[2]   = LIGHT_DIR_Z;
    frag_u.light_dir[3]   = 0.0f;
    frag_u.light_color[0] = LIGHT_COLOR_R;
    frag_u.light_color[1] = LIGHT_COLOR_G;
    frag_u.light_color[2] = LIGHT_COLOR_B;
    frag_u.light_intensity = LIGHT_INTENSITY;
    SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

    /* Bind dirt texture (slot 0) and shadow map (slot 1). */
    SDL_GPUTextureSamplerBinding tex_binds[2];
    tex_binds[0] = (SDL_GPUTextureSamplerBinding){
        .texture = state->dirt_texture, .sampler = state->sampler };
    tex_binds[1] = (SDL_GPUTextureSamplerBinding){
        .texture = state->shadow_depth,
        .sampler = state->nearest_clamp };
    SDL_BindGPUFragmentSamplers(pass, 0, tex_binds, 2);

    SDL_GPUBufferBinding vb = { state->floor_vb, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

    SDL_GPUBufferBinding ib = { state->floor_ib, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_DrawGPUIndexedPrimitives(pass, QUAD_INDEX_COUNT, 1, 0, 0, 0);
}

/* ── Helper: draw the skybox ────────────────────────────────────────── */

static void draw_skybox(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const app_state *state,
    const mat4 *view,
    const mat4 *proj)
{
    /* Remove translation from view matrix — skybox follows rotation only. */
    mat4 view_rot = *view;
    view_rot.m[12] = 0.0f;
    view_rot.m[13] = 0.0f;
    view_rot.m[14] = 0.0f;

    SkyboxVertUniforms sky_u;
    sky_u.vp_no_translation = mat4_multiply(*proj, view_rot);
    SDL_PushGPUVertexUniformData(cmd, 0, &sky_u, sizeof(sky_u));

    SDL_GPUTextureSamplerBinding sky_tex = {
        .texture = state->cubemap_texture,
        .sampler = state->cubemap_sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &sky_tex, 1);

    SDL_GPUBufferBinding vb = { state->skybox_vb, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

    SDL_GPUBufferBinding ib = { state->skybox_ib, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_DrawGPUIndexedPrimitives(pass, SKYBOX_INDEX_COUNT, 1, 0, 0, 0);
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
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 31 \xe2\x80\x94 Transform Animations",
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

    /* ── Dirt texture (procedural ground) ───────────────────────── */
    state->dirt_texture = create_dirt_texture(device);
    if (!state->dirt_texture) goto init_fail;

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
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_LINEAR;
        si.mag_filter     = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.max_lod        = MAX_LOD_UNLIMITED;
        state->cubemap_sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->cubemap_sampler) {
            SDL_Log("Failed to create cubemap sampler: %s", SDL_GetError());
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
        char path[PATH_BUFFER_SIZE];

        SDL_snprintf(path, sizeof(path), "%s%s", base, TRUCK_MODEL_PATH);
        if (!setup_model(device, &state->truck, path))
            goto init_fail;

        SDL_snprintf(path, sizeof(path), "%s%s", base, TRACK_MODEL_PATH);
        if (!setup_model(device, &state->track, path))
            goto init_fail;

        /* Load cubemap. */
        SDL_snprintf(path, sizeof(path), "%s%s", base, SKYBOX_FACE_DIR);
        state->cubemap_texture = create_cubemap_texture(device, path);
        if (!state->cubemap_texture)
            goto init_fail;
    }

    /* ── Wheel animation — the glTF parser already loaded keyframes ── */
    state->wheel_time = 0.0f;
    if (state->truck.scene.animation_count > 0) {
        SDL_Log("Loaded animation '%s': duration=%.3fs, %d channels",
                state->truck.scene.animations[0].name,
                state->truck.scene.animations[0].duration,
                state->truck.scene.animations[0].channel_count);
    }
    /* Precompute cumulative arc lengths for the waypoint loop so the
     * yaw schedule advances proportionally to distance traveled. */
    {
        float cumulative = 0.0f;
        for (int i = 0; i < PATH_WAYPOINT_COUNT; i++) {
            int next = (i + 1) % PATH_WAYPOINT_COUNT;
            vec3 d = vec3_sub(PATH_WAYPOINTS[next].position,
                              PATH_WAYPOINTS[i].position);
            cumulative += vec3_length(d);
            state->seg_cumulative[i] = cumulative;
        }
        state->total_path_len = cumulative;
    }
    state->path_distance = 0.0f;
    state->truck_pos     = PATH_WAYPOINTS[0].position;
    state->truck_yaw     = PATH_WAYPOINTS[0].yaw;

    /* ── Build the modular track layout ────────────────────────── */
    /* Place each piece with a transform: translate to position, then
     * rotate around Y so the road connections line up.  The corner
     * mesh has arms extending toward local -X and +Z. */
    {
        /* Helper: build translate * rotate_y transform. */
        #define PIECE(mi, px, pz, angle) \
            { (mi), mat4_multiply(                           \
                mat4_translate(vec3_create((px), 0.0f, (pz))),  \
                mat4_rotate_y(angle)) }

        /* Side straights pushed outward by TRACK_OFFSET so their road
         * centers align with the corner road centers. */
        const float sx = TRACK_CX + TRACK_OFFSET; /* side straight X */
        const float tz = TRACK_CZ + TRACK_OFFSET; /* top/bottom straight Z */

        TrackPiece layout[TRACK_PIECE_COUNT] = {
            /* Top-right corner  — curve faces inward     */
            PIECE(TRACK_MESH_CORNER_15,  TRACK_CX,  TRACK_CZ,  FORGE_PI * 0.5f),
            /* Right straight    — road along Z           */
            PIECE(TRACK_MESH_LINE_30,    sx,         0.0f,       0.0f),
            /* Bottom-right corner — curve faces inward   */
            PIECE(TRACK_MESH_CORNER_15,  TRACK_CX, -TRACK_CZ,  FORGE_PI),
            /* Bottom straight   — road along X           */
            PIECE(TRACK_MESH_LINE_15,    0.0f,      -tz,         FORGE_PI * 0.5f),
            /* Bottom-left corner — curve faces inward    */
            PIECE(TRACK_MESH_CORNER_15, -TRACK_CX, -TRACK_CZ, -FORGE_PI * 0.5f),
            /* Left straight     — road along Z           */
            PIECE(TRACK_MESH_LINE_30,   -sx,         0.0f,       0.0f),
            /* Top-left corner   — curve faces inward     */
            PIECE(TRACK_MESH_CORNER_15, -TRACK_CX,  TRACK_CZ,  0.0f),
            /* Top start/finish  — road along X           */
            PIECE(TRACK_MESH_START_FIN,  0.0f,       tz,         FORGE_PI * 0.5f),
        };
        SDL_memcpy(state->track_pieces, layout, sizeof(layout));
        #undef PIECE
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
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_FRONT;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
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

    /* ── Scene pipeline (single color target + depth) ───────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            scene_vert_spirv, sizeof(scene_vert_spirv),
            scene_vert_dxil, sizeof(scene_vert_dxil), 0, 1);
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

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        color_desc.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 3;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
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

    /* ── Skybox pipeline (depth test <= to draw at far plane) ────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            skybox_vert_spirv, sizeof(skybox_vert_spirv),
            skybox_vert_dxil, sizeof(skybox_vert_dxil), 0, 1);
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            skybox_frag_spirv, sizeof(skybox_frag_spirv),
            skybox_frag_dxil, sizeof(skybox_frag_dxil), 1, 0);
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

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        color_desc.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = &attr;
        pi.vertex_input_state.num_vertex_attributes      = 1;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_FRONT;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.target_info.has_depth_stencil_target   = true;

        state->skybox_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->skybox_pipeline) {
            SDL_Log("Failed to create skybox pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Floor geometry (XZ quad at GROUND_Y) ────────────────────── */
    {
        /* UV coordinates tile the dirt texture across the ground. */
        ForgeGltfVertex verts[4] = {
            {{ -GROUND_HALF_SIZE, GROUND_Y, -GROUND_HALF_SIZE }, { 0,1,0 }, { 0, 0 }},
            {{  GROUND_HALF_SIZE, GROUND_Y, -GROUND_HALF_SIZE }, { 0,1,0 }, { GROUND_UV_REPEAT, 0 }},
            {{  GROUND_HALF_SIZE, GROUND_Y,  GROUND_HALF_SIZE }, { 0,1,0 }, { GROUND_UV_REPEAT, GROUND_UV_REPEAT }},
            {{ -GROUND_HALF_SIZE, GROUND_Y,  GROUND_HALF_SIZE }, { 0,1,0 }, { 0, GROUND_UV_REPEAT }},
        };
        Uint16 indices[] = { 0, 2, 1, 0, 3, 2 };

        state->floor_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, verts, sizeof(verts));
        state->floor_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
        if (!state->floor_vb || !state->floor_ib) goto init_fail;
    }

    /* ── Skybox geometry ────────────────────────────────────────── */
    {
        float vertices[SKYBOX_VERTEX_COUNT * 3] = {
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
        };
        Uint16 indices[SKYBOX_INDEX_COUNT] = {
            0, 2, 1,  0, 3, 2,
            4, 5, 6,  4, 6, 7,
            0, 4, 7,  0, 7, 3,
            1, 2, 6,  1, 6, 5,
            0, 1, 5,  0, 5, 4,
            3, 7, 6,  3, 6, 2,
        };

        state->skybox_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, vertices, sizeof(vertices));
        state->skybox_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
        if (!state->skybox_vb || !state->skybox_ib) goto init_fail;
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

    /* ── Main scene depth texture ───────────────────────────────── */
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
            SDL_Log("Failed to create main_depth: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Directional light view-projection (orthographic) ──────── */
    {
        vec3 light_dir_v = vec3_normalize(
            vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
        vec3 light_pos = vec3_scale(light_dir_v, -LIGHT_DISTANCE);
        vec3 light_target = vec3_create(0.0f, 0.0f, 0.0f);
        vec3 light_up = vec3_create(0.0f, 1.0f, 0.0f);
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
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ───────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

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
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
        if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
            SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        state->mouse_captured = true;
    }

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

    /* ── Animation: advance wheel rotation ─────────────────────────── */
    /* The glTF parser loaded the truck's "Wheels" animation clip into
     * scene.animations[0].  forge_gltf_anim_apply() evaluates all
     * channels (binary search + slerp) and writes results to node TRS. */
    if (state->truck.scene.animation_count > 0) {
        state->wheel_time += dt * ANIM_SPEED;
        forge_gltf_anim_apply(&state->truck.scene.animations[0],
                              state->truck.scene.nodes,
                              state->truck.scene.node_count,
                              state->wheel_time, true);
    }

    /* ── Animation: advance path following ─────────────────────────── */
    /* The truck always moves forward in its heading direction.  The yaw
     * schedule uses arc-length parameterization: the yaw advances
     * proportionally to distance traveled, so the truck turns at exactly
     * the right position regardless of segment length. */
    {
        float step = TRUCK_DRIVE_SPEED * dt;
        state->path_distance += step;
        if (state->path_distance >= state->total_path_len)
            state->path_distance -= state->total_path_len;

        state->truck_yaw = evaluate_path_yaw(
            state->path_distance, state->seg_cumulative);

        float fwd_x = SDL_sinf(state->truck_yaw);
        float fwd_z = SDL_cosf(state->truck_yaw);
        state->truck_pos.x += fwd_x * step;
        state->truck_pos.z += fwd_z * step;
    }
    state->truck_pos.y = TRUCK_Y;

    /* Build the truck's world placement matrix: translate to current
     * position, then rotate around Y by the heading.  This positions
     * the entire truck hierarchy (body + wheels) in the world. */
    mat4 truck_placement = mat4_multiply(
        mat4_translate(state->truck_pos),
        mat4_rotate_y(state->truck_yaw));

    /* ── Rebuild truck node hierarchy ──────────────────────────────── */
    /* Rebuild world transforms: walk the node tree from roots to leaves,
     * propagating world = parent_world * local for every node. */
    {
        mat4 identity = mat4_identity();
        for (int i = 0; i < state->truck.scene.root_node_count; i++)
            forge_gltf_compute_world_transforms(
                &state->truck.scene,
                state->truck.scene.root_nodes[i], &identity);
    }

    /* Track uses the custom layout — individual piece transforms are
     * in state->track_pieces[], set once during AppInit. */

    /* ── Camera matrices ───────────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view   = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    mat4 proj   = mat4_perspective(
        (float)FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

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

        /* Shadow for truck (with animated hierarchy + path placement). */
        draw_model_shadow(shadow_pass, cmd, &state->truck,
                          &truck_placement, &state->light_vp);

        /* Shadow for track (custom layout — individual pieces). */
        for (int ti = 0; ti < TRACK_PIECE_COUNT; ti++) {
            draw_track_piece_shadow(shadow_pass, cmd, &state->track,
                state->track_pieces[ti].mesh_index,
                &state->track_pieces[ti].transform,
                &state->light_vp);
        }

        /* Shadow for floor quad. */
        {
            mat4 floor_model = mat4_identity();
            ShadowVertUniforms su;
            su.light_mvp = mat4_multiply(state->light_vp, floor_model);
            SDL_PushGPUVertexUniformData(cmd, 0, &su, sizeof(su));

            SDL_GPUBufferBinding vb = { state->floor_vb, 0 };
            SDL_BindGPUVertexBuffers(shadow_pass, 0, &vb, 1);
            SDL_GPUBufferBinding ib = { state->floor_ib, 0 };
            SDL_BindGPUIndexBuffer(shadow_pass, &ib,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);
            SDL_DrawGPUIndexedPrimitives(shadow_pass, QUAD_INDEX_COUNT, 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(shadow_pass);
    }

    /* ══ PASS 2: Main scene pass (to swapchain) ═══════════════════ */
    {
        SDL_GPUColorTargetInfo main_ct;
        SDL_zero(main_ct);
        main_ct.texture     = swapchain_tex;
        main_ct.load_op     = SDL_GPU_LOADOP_CLEAR;
        main_ct.store_op    = SDL_GPU_STOREOP_STORE;
        main_ct.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

        SDL_GPUDepthStencilTargetInfo main_dti;
        SDL_zero(main_dti);
        main_dti.texture     = state->main_depth;
        main_dti.load_op     = SDL_GPU_LOADOP_CLEAR;
        main_dti.store_op    = SDL_GPU_STOREOP_STORE;
        main_dti.clear_depth = 1.0f;

        SDL_GPURenderPass *main_pass = SDL_BeginGPURenderPass(
            cmd, &main_ct, 1, &main_dti);
        if (!main_pass) {
            SDL_Log("SDL_BeginGPURenderPass (main) failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        /* Draw scene objects. */
        SDL_BindGPUGraphicsPipeline(main_pass, state->scene_pipeline);

        draw_floor(main_pass, cmd, state, &cam_vp, &state->cam_position);

        draw_model_scene(main_pass, cmd, &state->truck, state,
                         &truck_placement, &cam_vp, &state->cam_position);
        /* Track (custom layout — individual pieces). */
        for (int ti = 0; ti < TRACK_PIECE_COUNT; ti++) {
            draw_track_piece_scene(main_pass, cmd, &state->track, state,
                state->track_pieces[ti].mesh_index,
                &state->track_pieces[ti].transform,
                &cam_vp, &state->cam_position);
        }

        /* Draw skybox. */
        SDL_BindGPUGraphicsPipeline(main_pass, state->skybox_pipeline);
        draw_skybox(main_pass, cmd, state, &view, &proj);

        SDL_EndGPURenderPass(main_pass);
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
    free_model_gpu(state->device, &state->track);

    forge_arena_destroy(&state->truck.gltf_arena);
    forge_arena_destroy(&state->track.gltf_arena);

    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->skybox_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->skybox_pipeline);

    if (state->shadow_depth)
        SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->main_depth)
        SDL_ReleaseGPUTexture(state->device, state->main_depth);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
    if (state->dirt_texture)
        SDL_ReleaseGPUTexture(state->device, state->dirt_texture);
    if (state->cubemap_texture)
        SDL_ReleaseGPUTexture(state->device, state->cubemap_texture);

    if (state->sampler)
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    if (state->nearest_clamp)
        SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->cubemap_sampler)
        SDL_ReleaseGPUSampler(state->device, state->cubemap_sampler);

    if (state->floor_vb)  SDL_ReleaseGPUBuffer(state->device, state->floor_vb);
    if (state->floor_ib)  SDL_ReleaseGPUBuffer(state->device, state->floor_ib);
    if (state->skybox_vb) SDL_ReleaseGPUBuffer(state->device, state->skybox_vb);
    if (state->skybox_ib) SDL_ReleaseGPUBuffer(state->device, state->skybox_ib);

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
