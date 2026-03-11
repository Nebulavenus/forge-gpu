/*
 * GPU Lesson 32 — Skinning Animations
 *
 * Deforms a single mesh on the GPU by blending multiple joint transforms
 * per vertex.  This is the technique behind every animated character in games.
 *
 * The CesiumMan model (19 joints, 57 animation channels, 3273 skinned
 * vertices) walks in a loop.  Each frame:
 *   1. Evaluate all 57 animation channels (binary search + slerp/lerp)
 *   2. Rebuild the node hierarchy (parent_world * local)
 *   3. Compute joint matrices: worldTransform * inverseBindMatrix
 *   4. Push joint matrices to the GPU as uniform data
 *   5. The vertex shader blends 4 joint transforms per vertex
 *
 * Architecture — 2 render passes per frame:
 *   1. Shadow pass   — skinned mesh into depth map (shadow_skin pipeline)
 *   2. Scene pass    — skinned mesh with Blinn-Phong + shadows, then grid
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

#include "shaders/compiled/skin_vert_spirv.h"
#include "shaders/compiled/skin_vert_dxil.h"
#include "shaders/compiled/skin_frag_spirv.h"
#include "shaders/compiled/skin_frag_dxil.h"

#include "shaders/compiled/shadow_skin_vert_spirv.h"
#include "shaders/compiled/shadow_skin_vert_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"

#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"

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

/* Camera initial position — close-up, behind the character. */
#define CAM_START_X         1.5f
#define CAM_START_Y         1.0f
#define CAM_START_Z        -2.5f
#define CAM_START_YAW_DEG   180.0f
#define CAM_START_PITCH_DEG -5.0f

/* Directional light — from above-left-front, illuminating the face. */
#define LIGHT_DIR_X      0.4f
#define LIGHT_DIR_Y     -0.8f
#define LIGHT_DIR_Z      0.4f
#define LIGHT_INTENSITY  0.9f
#define LIGHT_COLOR_R    1.0f
#define LIGHT_COLOR_G    0.95f
#define LIGHT_COLOR_B    0.85f

/* Scene material defaults. */
#define MATERIAL_AMBIENT      0.25f
#define MATERIAL_SHININESS    32.0f
#define MATERIAL_SPECULAR_STR 0.4f

/* Circular walk path — character walks around a loop on the XZ plane. */
#define WALK_RADIUS       1.5f   /* radius of the walking circle (meters)  */
#define WALK_SPEED        0.8f   /* angular speed (radians per second)     */

/* Shadow map. */
#define SHADOW_MAP_SIZE   2048
#define SHADOW_DEPTH_FMT  SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_ORTHO_SIZE 5.0f
#define SHADOW_NEAR       0.1f
#define SHADOW_FAR        20.0f
#define LIGHT_DISTANCE    8.0f
#define SHADOW_DEPTH_BIAS       2.0f   /* constant depth bias (in depth units)       */
#define SHADOW_SLOPE_BIAS       2.0f   /* slope-scaled bias for grazing-angle faces   */

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

/* Model asset path. */
#define CESIUMMAN_MODEL_PATH "assets/CesiumMan/CesiumMan.gltf"

/* Light target (world-space point the shadow camera looks at). */
#define LIGHT_TARGET_Y    0.8f

/* Light direction degeneracy threshold. */
#define PARALLEL_THRESHOLD 0.99f

/* Path buffer. */
#define PATH_BUFFER_SIZE 512

/* Animation. */
#define ANIM_SPEED        1.0f

/* CesiumMan mesh-local coordinate system (from POSITION accessor bounds):
 *   +X = forward (face direction)   range [-0.13, 0.18]
 *   +Y = left                       range [-0.57, 0.57]
 *   +Z = up                         range [ 0.00, 1.51]
 *
 * After the node hierarchy (Z_UP * Armature), these map to world space:
 *   local +X  ->  world +Z   (forward)
 *   local +Y  ->  world +X   (left)
 *   local +Z  ->  world +Y   (up)
 */

/* CesiumMan skeleton — 19 joints. */
#define MAX_JOINTS 19

/* ── Skinned vertex layout ─────────────────────────────────────────── */
/* Interleaved: position + normal + uv + joints + weights.
 * This matches the vertex attributes declared in the shader. */

typedef struct SkinVertex {
    vec3   position;    /* 12 bytes — FLOAT3   */
    vec3   normal;      /* 12 bytes — FLOAT3   */
    vec2   uv;          /*  8 bytes — FLOAT2   */
    Uint16 joints[4];   /*  8 bytes — USHORT4  */
    float  weights[4];  /* 16 bytes — FLOAT4   */
} SkinVertex;           /* 56 bytes total      */

/* Grid floor vertex — position only. */
typedef struct GridVertex {
    vec3 position;  /* world-space corner of the grid quad */
} GridVertex;

/* ── Uniform structures ─────────────────────────────────────────────── */

/* Skinned scene vertex uniforms — slot 0. */
typedef struct SkinVertUniforms {
    mat4 mvp;      /* VP * mesh_world — projects skinned verts to clip space */
    mat4 model;    /* mesh node's world transform — for world-space normals  */
    mat4 light_vp; /* light VP * mesh_world — for shadow map projection      */
} SkinVertUniforms;

/* Joint matrices — slot 1. */
typedef struct JointUniforms {
    mat4 joints[MAX_JOINTS]; /* per-joint skin matrix: inv(meshWorld) * jointWorld * IBM */
} JointUniforms;

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

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;          /* interleaved SkinVertex data     */
    SDL_GPUBuffer *index_buffer;           /* index data (16 or 32 bit)       */
    Uint32 index_count;                    /* number of indices to draw       */
    int material_index;                    /* index into materials[], -1=none */
    SDL_GPUIndexElementSize index_type;    /* 16BIT or 32BIT                  */
    bool has_uvs;                          /* true if primitive has UV coords */
} GpuPrimitive;

typedef struct GpuMaterial {
    float base_color[4];                   /* material RGBA color             */
    SDL_GPUTexture *texture;               /* diffuse texture (NULL=white)    */
    bool has_texture;                      /* true if texture was loaded      */
} GpuMaterial;

/* ── Application state ──────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;              /* main application window           */
    SDL_GPUDevice *device;              /* GPU device handle                 */

    /* Pipelines. */
    SDL_GPUGraphicsPipeline *skin_pipeline;     /* skinned Blinn-Phong     */
    SDL_GPUGraphicsPipeline *shadow_pipeline;   /* skinned shadow depth    */
    SDL_GPUGraphicsPipeline *grid_pipeline;     /* procedural grid floor   */

    /* Render targets. */
    SDL_GPUTexture *shadow_depth;   /* D32 depth (SHADOW_MAP_SIZE^2)       */
    SDL_GPUTexture *main_depth;     /* D32 depth (window-sized)            */

    /* Samplers. */
    SDL_GPUSampler *sampler;        /* trilinear + aniso (diffuse)         */
    SDL_GPUSampler *nearest_clamp;  /* nearest + clamp (shadow)            */

    /* Scene objects. */
    SDL_GPUTexture *white_texture;  /* 1x1 white fallback texture          */

    /* CesiumMan model. */
    ForgeGltfScene scene;           /* parsed glTF data (nodes, skins)     */
    ForgeArena     gltf_arena;      /* arena backing glTF allocations      */
    GpuPrimitive  *primitives;      /* GPU buffers per mesh primitive      */
    int            primitive_count;  /* number of uploaded primitives       */
    GpuMaterial   *materials;       /* GPU materials with loaded textures  */
    int            material_count;  /* number of materials                 */
    mat4           mesh_world;      /* mesh node world * path transform    */

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

    /* Animation. */
    float    anim_time;             /* current playback time (seconds)     */
    float    walk_angle;            /* angle around walk circle (radians)  */

    /* Joint matrices — computed each frame and pushed to GPU. */
    JointUniforms joint_uniforms;   /* 19 joint matrices for skinning      */

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

/* ── Animation: evaluate all channels and rebuild hierarchy ─────────── */

static void evaluate_animation(app_state *state, float dt)
{
    if (state->scene.animation_count == 0) return;
    const ForgeGltfAnimation *anim = &state->scene.animations[0];
    if (anim->channel_count == 0 || anim->duration <= 0.0f) return;

    /* Advance time and wrap at clip duration. */
    state->anim_time += dt * ANIM_SPEED;
    while (state->anim_time >= anim->duration) {
        state->anim_time -= anim->duration;
    }

    /* Evaluate all animation channels — binary search + slerp/lerp for
     * each channel, writing results to node TRS and rebuilding local
     * transforms from the updated values. */
    forge_gltf_anim_apply(anim,
                          state->scene.nodes,
                          state->scene.node_count,
                          state->anim_time, true);

    /* Rebuild world transforms: walk the node tree from roots to leaves,
     * propagating world = parent_world * local for every node. */
    {
        mat4 identity = mat4_identity();
        for (int i = 0; i < state->scene.root_node_count; i++)
            forge_gltf_compute_world_transforms(
                &state->scene,
                state->scene.root_nodes[i], &identity);
    }

    /* Compute joint matrices per glTF spec:
     *   jointMatrix[j] = inverse(meshNode.world) * jointNode.world * IBM[j]
     *
     * The inverse mesh node transform accounts for the mesh node's position
     * in the hierarchy (e.g. Z_UP and Armature rotations in CesiumMan).
     * The resulting skinned vertices are in the mesh node's local space,
     * so we use meshNode.worldTransform as the model matrix when drawing. */
    if (state->scene.skin_count > 0) {
        const ForgeGltfSkin *skin = &state->scene.skins[0];

        /* Find the node that references this skin (the mesh node). */
        mat4 inv_mesh_world = mat4_identity();
        for (int i = 0; i < state->scene.node_count; i++) {
            if (state->scene.nodes[i].skin_index == 0) {
                inv_mesh_world = mat4_inverse(
                    state->scene.nodes[i].world_transform);
                state->mesh_world = state->scene.nodes[i].world_transform;
                break;
            }
        }

        for (int i = 0; i < skin->joint_count && i < MAX_JOINTS; i++) {
            int joint_node = skin->joints[i];
            if (joint_node >= 0 && joint_node < state->scene.node_count) {
                state->joint_uniforms.joints[i] = mat4_multiply(
                    inv_mesh_world,
                    mat4_multiply(
                        state->scene.nodes[joint_node].world_transform,
                        skin->inverse_bind_matrices[i]));
            }
        }
    }

    /* Advance the walk angle and build a path transform so the character
     * walks in a circle on the XZ plane.  The facing direction is the
     * tangent to the circle (perpendicular to the radius vector). */
    state->walk_angle += WALK_SPEED * dt;
    if (state->walk_angle > 2.0f * FORGE_PI)
        state->walk_angle -= 2.0f * FORGE_PI;

    float px = WALK_RADIUS * SDL_sinf(state->walk_angle);
    float pz = WALK_RADIUS * SDL_cosf(state->walk_angle);

    /* Tangent to the circle = derivative of (sin(a), cos(a)) = (cos(a), -sin(a)).
     * This points along the direction of travel (counterclockwise). */
    float tx = SDL_cosf(state->walk_angle);
    float tz = -SDL_sinf(state->walk_angle);

    /* The character's rest-pose forward is +Z after the glTF node hierarchy.
     * atan2(tx, tz) gives the Y-rotation angle that aligns +Z with the
     * tangent vector (tx, tz), so the character faces its walk direction. */
    float facing = SDL_atan2f(tx, tz);

    mat4 path_translate = mat4_translate(vec3_create(px, 0.0f, pz));
    mat4 path_rotate    = mat4_rotate_y(facing);
    mat4 path_transform = mat4_multiply(path_translate, path_rotate);

    state->mesh_world = mat4_multiply(path_transform, state->mesh_world);
}

/* ── Upload skinned model to GPU ────────────────────────────────────── */

static bool upload_skinned_model(SDL_GPUDevice *device, app_state *state)
{
    ForgeGltfScene *scene = &state->scene;

    state->primitive_count = scene->primitive_count;
    state->primitives = (GpuPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(GpuPrimitive));
    if (!state->primitives) {
        SDL_Log("Failed to allocate GPU primitives");
        return false;
    }

    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *src = &scene->primitives[i];
        GpuPrimitive *dst = &state->primitives[i];

        dst->material_index = src->material_index;
        dst->index_count    = src->index_count;
        dst->has_uvs        = src->has_uvs;

        /* Build interleaved SkinVertex array from separate glTF arrays. */
        if (src->vertices && src->vertex_count > 0) {
            SkinVertex *skin_verts = (SkinVertex *)SDL_calloc(
                (size_t)src->vertex_count, sizeof(SkinVertex));
            if (!skin_verts) {
                SDL_Log("Failed to allocate skin vertices");
                return false;
            }

            for (Uint32 v = 0; v < src->vertex_count; v++) {
                skin_verts[v].position = src->vertices[v].position;
                skin_verts[v].normal   = src->vertices[v].normal;
                skin_verts[v].uv       = src->vertices[v].uv;

                if (src->has_skin_data) {
                    /* Copy 4 joint indices and 4 weights. */
                    SDL_memcpy(skin_verts[v].joints,
                               src->joint_indices + v * 4,
                               4 * sizeof(Uint16));
                    SDL_memcpy(skin_verts[v].weights,
                               src->weights + v * 4,
                               4 * sizeof(float));
                }
            }

            Uint32 vb_size = src->vertex_count * (Uint32)sizeof(SkinVertex);
            dst->vertex_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_VERTEX, skin_verts, vb_size);
            SDL_free(skin_verts);

            if (!dst->vertex_buffer) return false;
        }

        /* Upload index buffer. */
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
    state->material_count = scene->material_count;
    state->materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!state->materials) {
        SDL_Log("Failed to allocate GPU materials");
        return false;
    }

    for (int i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &state->materials[i];

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
        "Lesson 32 \xe2\x80\x94 Skinning Animations",
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
            SDL_ReleaseWindowFromGPUDevice(device, window);
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
        SDL_ReleaseWindowFromGPUDevice(device, window);
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

    /* ── Load CesiumMan model ───────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            goto init_fail;
        }
        char path[PATH_BUFFER_SIZE];
        SDL_snprintf(path, sizeof(path), "%s%s", base, CESIUMMAN_MODEL_PATH);

        state->gltf_arena = forge_arena_create(0);
        if (!state->gltf_arena.first) {
            SDL_Log("ERROR: Out of memory creating arena for glTF");
            goto init_fail;
        }
        if (!forge_gltf_load(path, &state->scene, &state->gltf_arena)) {
            SDL_Log("Failed to load CesiumMan: %s", path);
            forge_arena_destroy(&state->gltf_arena);
            goto init_fail;
        }

        if (!upload_skinned_model(device, state)) {
            SDL_Log("Failed to upload skinned model to GPU");
            goto init_fail;
        }

        /* Animation data is already parsed by forge_gltf_load(). */
        if (state->scene.animation_count > 0) {
            SDL_Log("Loaded animation '%s': %.3fs, %d channels",
                    state->scene.animations[0].name,
                    state->scene.animations[0].duration,
                    state->scene.animations[0].channel_count);
        } else {
            SDL_Log("Warning: no animation data in model");
        }
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

    /* ── Skinned pipeline ───────────────────────────────────────── */
    {
        /* Vertex shader: 2 uniform buffers (scene + joints), 0 samplers. */
        SDL_GPUShader *vert = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            skin_vert_spirv, sizeof(skin_vert_spirv),
            skin_vert_dxil,  sizeof(skin_vert_dxil),
            0, 2);
        /* Fragment shader: 2 samplers (diffuse + shadow), 1 uniform buffer. */
        SDL_GPUShader *frag = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            skin_frag_spirv, sizeof(skin_frag_spirv),
            skin_frag_dxil,  sizeof(skin_frag_dxil),
            2, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        /* Vertex attributes: pos(0), normal(1), uv(2), joints(3), weights(4). */
        SDL_GPUVertexAttribute attrs[5];
        SDL_zero(attrs);
        attrs[0].location = 0;
        attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset   = (Uint32)offsetof(SkinVertex, position);
        attrs[1].location = 1;
        attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset   = (Uint32)offsetof(SkinVertex, normal);
        attrs[2].location = 2;
        attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset   = (Uint32)offsetof(SkinVertex, uv);
        attrs[3].location = 3;
        attrs[3].format   = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4;
        attrs[3].offset   = (Uint32)offsetof(SkinVertex, joints);
        attrs[4].location = 4;
        attrs[4].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[4].offset   = (Uint32)offsetof(SkinVertex, weights);

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot       = 0;
        vbd.pitch      = sizeof(SkinVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 5;

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

        state->skin_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->skin_pipeline) {
            SDL_Log("Failed to create skin pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Shadow pipeline (skinned) ──────────────────────────────── */
    {
        /* Same vertex layout as skin pipeline, but depth-only output. */
        SDL_GPUShader *vert = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            shadow_skin_vert_spirv, sizeof(shadow_skin_vert_spirv),
            shadow_skin_vert_dxil,  sizeof(shadow_skin_vert_dxil),
            0, 2);
        SDL_GPUShader *frag = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_frag_spirv, sizeof(shadow_frag_spirv),
            shadow_frag_dxil,  sizeof(shadow_frag_dxil),
            0, 0);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexAttribute attrs[5];
        SDL_zero(attrs);
        attrs[0].location = 0;
        attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset   = (Uint32)offsetof(SkinVertex, position);
        attrs[1].location = 1;
        attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset   = (Uint32)offsetof(SkinVertex, normal);
        attrs[2].location = 2;
        attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset   = (Uint32)offsetof(SkinVertex, uv);
        attrs[3].location = 3;
        attrs[3].format   = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4;
        attrs[3].offset   = (Uint32)offsetof(SkinVertex, joints);
        attrs[4].location = 4;
        attrs[4].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[4].offset   = (Uint32)offsetof(SkinVertex, weights);

        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot       = 0;
        vbd.pitch      = sizeof(SkinVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState vis;
        SDL_zero(vis);
        vis.vertex_buffer_descriptions = &vbd;
        vis.num_vertex_buffers         = 1;
        vis.vertex_attributes          = attrs;
        vis.num_vertex_attributes      = 5;

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
        pi.rasterizer_state.cull_mode       = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face      = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode       = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.depth_bias_constant_factor = SHADOW_DEPTH_BIAS;
        pi.rasterizer_state.depth_bias_slope_factor    = SHADOW_SLOPE_BIAS;
        pi.rasterizer_state.enable_depth_bias          = true;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->shadow_pipeline) {
            SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid pipeline ──────────────────────────────────────────── */
    {
        SDL_GPUShader *vert = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_vert_spirv, sizeof(grid_vert_spirv),
            grid_vert_dxil,  sizeof(grid_vert_dxil),
            0, 1);
        SDL_GPUShader *frag = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_frag_spirv, sizeof(grid_frag_spirv),
            grid_frag_dxil,  sizeof(grid_frag_dxil),
            1, 1);  /* 1 sampler (shadow map), 1 uniform buffer */
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
    state->mesh_world = mat4_identity();
    state->last_ticks = SDL_GetPerformanceCounter();
    state->anim_time  = 0.0f;
    state->walk_angle = FORGE_PI * 0.5f; /* start at (R, 0, 0) facing +Z toward camera */

    SDL_Log("Lesson 32 initialised: %d primitives, %d materials, %d joints",
            state->primitive_count, state->material_count,
            state->scene.skin_count > 0 ? state->scene.skins[0].joint_count : 0);

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

    /* ── Camera movement (quaternion-based, see forge-camera-and-input skill) */
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

    /* ── Evaluate animation ─────────────────────────────────────── */
    evaluate_animation(state, dt);

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

    /* ── Pass 1: Shadow map ─────────────────────────────────────── */
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

        /* Push shadow uniforms: light VP (slot 0) and joint matrices (slot 1).
         * Include mesh_world so the shadow maps to the same world position. */
        ShadowVertUniforms shadow_u;
        shadow_u.light_vp = mat4_multiply(state->light_vp, state->mesh_world);
        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_u, sizeof(shadow_u));
        SDL_PushGPUVertexUniformData(cmd, 1, &state->joint_uniforms,
                                     sizeof(state->joint_uniforms));

        /* Draw all skinned primitives. */
        for (int i = 0; i < state->primitive_count; i++) {
            const GpuPrimitive *prim = &state->primitives[i];
            if (!prim->vertex_buffer || !prim->index_buffer) continue;

            SDL_GPUBufferBinding vb = { prim->vertex_buffer, 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            SDL_GPUBufferBinding ib = { prim->index_buffer, 0 };
            SDL_BindGPUIndexBuffer(pass, &ib, prim->index_type);
            SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    /* ── Pass 2: Scene (skinned mesh + grid) ────────────────────── */
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

        /* ── Draw skinned mesh ──────────────────────────────────── */
        SDL_BindGPUGraphicsPipeline(pass, state->skin_pipeline);

        /* Vertex uniforms: MVP + light_vp (slot 0), joints (slot 1).
         * The skin matrix outputs vertices in the mesh node's local space,
         * so we use the mesh node's world transform as the model matrix. */
        SkinVertUniforms skin_vu;
        skin_vu.mvp      = mat4_multiply(cam_vp, state->mesh_world);
        skin_vu.model    = state->mesh_world;
        skin_vu.light_vp = mat4_multiply(state->light_vp, state->mesh_world);
        SDL_PushGPUVertexUniformData(cmd, 0, &skin_vu, sizeof(skin_vu));
        SDL_PushGPUVertexUniformData(cmd, 1, &state->joint_uniforms,
                                     sizeof(state->joint_uniforms));

        for (int i = 0; i < state->primitive_count; i++) {
            const GpuPrimitive *prim = &state->primitives[i];
            if (!prim->vertex_buffer || !prim->index_buffer) continue;

            SDL_GPUTexture *tex = state->white_texture;
            SceneFragUniforms frag_u;
            SDL_zero(frag_u);

            if (prim->material_index >= 0 &&
                prim->material_index < state->material_count) {
                const GpuMaterial *mat = &state->materials[prim->material_index];
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

            SDL_GPUBufferBinding vb = { prim->vertex_buffer, 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            SDL_GPUBufferBinding ib = { prim->index_buffer, 0 };
            SDL_BindGPUIndexBuffer(pass, &ib, prim->index_type);
            SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
        }

        /* ── Draw grid floor ────────────────────────────────────── */
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

        /* Release GPU primitives. */
        if (state->primitives) {
            for (int i = 0; i < state->primitive_count; i++) {
                if (state->primitives[i].vertex_buffer)
                    SDL_ReleaseGPUBuffer(device, state->primitives[i].vertex_buffer);
                if (state->primitives[i].index_buffer)
                    SDL_ReleaseGPUBuffer(device, state->primitives[i].index_buffer);
            }
            SDL_free(state->primitives);
        }

        /* Release materials / textures. */
        if (state->materials) {
            for (int i = 0; i < state->material_count; i++) {
                if (!state->materials[i].texture) continue;
                bool already_released = false;
                int j;
                for (j = 0; j < i; j++) {
                    if (state->materials[j].texture == state->materials[i].texture) {
                        already_released = true;
                        break;
                    }
                }
                if (!already_released)
                    SDL_ReleaseGPUTexture(device, state->materials[i].texture);
            }
            SDL_free(state->materials);
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
        if (state->skin_pipeline)   SDL_ReleaseGPUGraphicsPipeline(device, state->skin_pipeline);
        if (state->shadow_pipeline) SDL_ReleaseGPUGraphicsPipeline(device, state->shadow_pipeline);
        if (state->grid_pipeline)   SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);

#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, device);
#endif

        forge_arena_destroy(&state->gltf_arena);

        SDL_ReleaseWindowFromGPUDevice(device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(device);
    }

    SDL_free(state);
}
