/*
 * GPU Lesson 30 — Planar Reflections
 *
 * Planar reflections render a pixel-perfect mirror image of the scene for
 * flat reflective surfaces.  The technique mirrors the camera across the
 * reflection plane (here, a water surface at Y = WATER_LEVEL), renders
 * the scene from that mirrored viewpoint into an offscreen texture, and
 * composites the reflection onto the water surface using Fresnel blending.
 *
 * The key mathematical insight is Eric Lengyel's oblique near-plane
 * clipping method: the mirrored camera's projection matrix is modified
 * so its near plane coincides with the water surface.  This prevents
 * geometry below the water from "leaking" into the reflection — only
 * objects above the water plane appear in the reflection texture.
 *
 * Architecture — 4 render passes per frame:
 *   1. Shadow pass       — directional light depth map (2048x2048)
 *   2. Reflection pass   — mirrored camera to offscreen color + depth
 *   3. Main scene pass   — standard forward render to swapchain
 *   4. Water pass        — alpha-blended water quad with Fresnel reflection
 *
 * Scene elements:
 *   - Low-poly boat (glTF)
 *   - Rock cliffs (glTF, textured)
 *   - Sandy floor quad below the water
 *   - HDR skybox (cube map)
 *   - Reflective water surface
 *
 * Controls:
 *   WASD / Space / LShift  — Move camera
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
#include "shaders/compiled/shadow_vert_msl.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"
#include "shaders/compiled/shadow_frag_msl.h"

#include "shaders/compiled/scene_vert_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_vert_msl.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_frag_dxil.h"
#include "shaders/compiled/scene_frag_msl.h"

#include "shaders/compiled/skybox_vert_spirv.h"
#include "shaders/compiled/skybox_vert_dxil.h"
#include "shaders/compiled/skybox_vert_msl.h"
#include "shaders/compiled/skybox_frag_spirv.h"
#include "shaders/compiled/skybox_frag_dxil.h"
#include "shaders/compiled/skybox_frag_msl.h"

#include "shaders/compiled/water_vert_spirv.h"
#include "shaders/compiled/water_vert_dxil.h"
#include "shaders/compiled/water_vert_msl.h"
#include "shaders/compiled/water_frag_spirv.h"
#include "shaders/compiled/water_frag_dxil.h"
#include "shaders/compiled/water_frag_msl.h"

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

/* Camera initial position — elevated, looking at the boat. */
#define CAM_START_X         6.0f
#define CAM_START_Y         3.0f
#define CAM_START_Z         8.0f
#define CAM_START_YAW_DEG   30.0f
#define CAM_START_PITCH_DEG -10.0f

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
#define SHADOW_ORTHO_SIZE 20.0f
#define SHADOW_NEAR       0.1f
#define SHADOW_FAR        60.0f
#define LIGHT_DISTANCE    25.0f

/* Water surface. */
#define WATER_LEVEL       0.0f
#define WATER_HALF_SIZE   60.0f
#define WATER_TINT_R      0.05f
#define WATER_TINT_G      0.15f
#define WATER_TINT_B      0.20f
#define WATER_TINT_A      1.0f
#define FRESNEL_F0        0.02f

/* Sandy floor (below water). */
#define FLOOR_Y            -2.0f
#define FLOOR_HALF_SIZE     60.0f
#define FLOOR_COLOR_R       0.76f
#define FLOOR_COLOR_G       0.70f
#define FLOOR_COLOR_B       0.50f
#define FLOOR_SHININESS     16.0f
#define FLOOR_SPECULAR_STR  0.1f

/* Skybox. */
#define CUBEMAP_FACE_SIZE  512
#define CUBEMAP_FACE_COUNT 6
#define SKYBOX_FACE_DIR    "assets/skybox/"
#define SKYBOX_VERTEX_COUNT 8
#define SKYBOX_INDEX_COUNT  36
#define PATH_BUFFER_SIZE    512

/* Texture. */
#define BYTES_PER_PIXEL 4
#define MAX_ANISOTROPY  4

/* Clear color — sky blue. */
#define CLEAR_R 0.5f
#define CLEAR_G 0.7f
#define CLEAR_B 0.9f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* Model asset paths. */
#define BOAT_MODEL_PATH  "assets/boat/scene.gltf"
#define ROCKS_MODEL_PATH "assets/rocks/scene.gltf"

/* Light direction degeneracy threshold. */
#define PARALLEL_THRESHOLD 0.99f

/* Oblique near-plane epsilon — skip modification if dot product is near zero. */
#define OBLIQUE_EPSILON 1e-6f

/* Quad geometry — two triangles (6 indices). */
#define QUAD_INDEX_COUNT 6

/* Rock placement scale — the rocks glTF has transforms that need scaling. */
#define ROCK_SCALE 0.66f

/* Boat placement — position the boat in open water, away from the cliffs. */
#define BOAT_POS_X    3.0f
#define BOAT_POS_Y    0.3f
#define BOAT_POS_Z    3.0f

/* ── Lesson-local math: reflection matrix ────────────────────────────── */

/* Construct a 4x4 reflection matrix for a plane (a,b,c,d) where
 * ax + by + cz + d = 0.  Standard formula:
 *   M = I - 2 * n * n^T  (extended to 4x4 with the d component).
 *
 * For a water plane at Y = WATER_LEVEL: plane = (0, 1, 0, -WATER_LEVEL). */
static mat4 mat4_reflect(float a, float b, float c, float d)
{
    mat4 m;
    m.m[0]  = 1.0f - 2.0f * a * a;
    m.m[1]  =      - 2.0f * a * b;
    m.m[2]  =      - 2.0f * a * c;
    m.m[3]  = 0.0f;

    m.m[4]  =      - 2.0f * b * a;
    m.m[5]  = 1.0f - 2.0f * b * b;
    m.m[6]  =      - 2.0f * b * c;
    m.m[7]  = 0.0f;

    m.m[8]  =      - 2.0f * c * a;
    m.m[9]  =      - 2.0f * c * b;
    m.m[10] = 1.0f - 2.0f * c * c;
    m.m[11] = 0.0f;

    m.m[12] = -2.0f * a * d;
    m.m[13] = -2.0f * b * d;
    m.m[14] = -2.0f * c * d;
    m.m[15] = 1.0f;
    return m;
}

/* Return -1, 0, or +1 depending on the sign of x. */
static float signf_of(float x)
{
    if (x > 0.0f) return  1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

/* Eric Lengyel's oblique near-plane clipping method.
 *
 * Replaces the near plane of a projection matrix with an arbitrary clip
 * plane.  The clip plane must be given in VIEW SPACE.  This modifies
 * row 2 of the projection matrix so that the near plane coincides with
 * the given clip plane.
 *
 * Reference: "Oblique View Frustum Depth Projection and Clipping"
 *            Eric Lengyel, Journal of Game Development, Vol. 1, No. 2 (2005) */
static mat4 mat4_oblique_near_plane(mat4 proj, vec4 clip_plane_view)
{
    /* Transform clip plane from view space to clip space.
     * q = (proj^-T) * clip_plane, but we only need the sign-corrected
     * version using the inverse-transpose.  For a standard perspective
     * matrix this simplifies to: */
    vec4 q;
    q.x = (signf_of(clip_plane_view.x) + proj.m[8])  / proj.m[0];
    q.y = (signf_of(clip_plane_view.y) + proj.m[9])  / proj.m[5];
    q.z = -1.0f;
    q.w = (1.0f + proj.m[10]) / proj.m[14];

    /* Scale the clip plane so that dot(q, clip_plane) = 1 */
    float dot = clip_plane_view.x * q.x + clip_plane_view.y * q.y +
                clip_plane_view.z * q.z + clip_plane_view.w * q.w;
    if (SDL_fabsf(dot) < OBLIQUE_EPSILON) return proj; /* degenerate — skip */

    float scale = 1.0f / dot;
    vec4 c;
    c.x = clip_plane_view.x * scale;
    c.y = clip_plane_view.y * scale;
    c.z = clip_plane_view.z * scale;
    c.w = clip_plane_view.w * scale;

    /* Replace row 2 (the near-plane row) of the projection matrix.
     * Column-major layout: proj.m[2], proj.m[6], proj.m[10], proj.m[14]. */
    proj.m[2]  = c.x;
    proj.m[6]  = c.y;
    proj.m[10] = c.z;
    proj.m[14] = c.w;

    return proj;
}

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
    mat4 light_mvp; /* light view-projection * model — transforms to light clip space */
} ShadowVertUniforms;

/* Skybox vertex uniforms. */
typedef struct SkyboxVertUniforms {
    mat4 vp_no_translation; /* view (rotation only) * projection — no camera translation */
} SkyboxVertUniforms;

/* Water vertex uniforms. */
typedef struct WaterVertUniforms {
    mat4 mvp; /* model-view-projection for the water quad */
} WaterVertUniforms;

/* Water fragment uniforms. */
typedef struct WaterFragUniforms {
    float eye_pos[3];   /* world-space camera position           */
    float water_level;  /* Y coordinate of the water plane       */
    float water_tint[4]; /* RGBA tint color for the water body   */
    float fresnel_f0;   /* Fresnel reflectance at normal incidence (0.02 for water) */
    float _pad[3];      /* padding to 16-byte alignment          */
} WaterFragUniforms;

/* ── GPU-side model types (same pattern as lesson 29) ──────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;          /* interleaved position/normal/uv data */
    SDL_GPUBuffer *index_buffer;           /* triangle index data                 */
    Uint32 index_count;                    /* number of indices to draw           */
    int material_index;                    /* index into the model's materials[]  */
    SDL_GPUIndexElementSize index_type;    /* 16-bit or 32-bit indices            */
    bool has_uvs;                          /* true if vertices include UVs        */
} GpuPrimitive;

typedef struct GpuMaterial {
    float base_color[4];                   /* RGBA base color from glTF           */
    SDL_GPUTexture *texture;               /* diffuse texture (NULL if untextured)*/
    bool has_texture;                      /* true if texture is valid            */
} GpuMaterial;

typedef struct ModelData {
    ForgeGltfScene scene;                  /* parsed glTF scene data              */
    ForgeArena     gltf_arena;             /* arena backing glTF allocations      */
    GpuPrimitive  *primitives;             /* GPU-uploaded mesh primitives        */
    int            primitive_count;        /* number of primitives                */
    GpuMaterial   *materials;              /* GPU-uploaded materials              */
    int            material_count;         /* number of materials                 */
} ModelData;

/* ── Application state ──────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;  /* application window handle  */
    SDL_GPUDevice *device;  /* GPU device for all rendering */

    /* Pipelines. */
    SDL_GPUGraphicsPipeline *scene_pipeline;       /* Blinn-Phong + shadow        */
    SDL_GPUGraphicsPipeline *scene_pipeline_refl;  /* same, CW front face for reflection */
    SDL_GPUGraphicsPipeline *shadow_pipeline;      /* depth-only shadow pass      */
    SDL_GPUGraphicsPipeline *skybox_pipeline;      /* environment cube map        */
    SDL_GPUGraphicsPipeline *skybox_pipeline_refl; /* same, CW front face for reflection */
    SDL_GPUGraphicsPipeline *water_pipeline;       /* alpha-blended water surface */

    /* Render targets. */
    SDL_GPUTexture *reflection_color;  /* offscreen reflection texture */
    SDL_GPUTexture *reflection_depth;  /* depth for reflection pass    */
    SDL_GPUTexture *shadow_depth;      /* directional shadow map       */
    SDL_GPUTexture *main_depth;        /* depth for main scene pass    */

    /* Samplers. */
    SDL_GPUSampler *sampler;         /* trilinear + anisotropy (textures) */
    SDL_GPUSampler *nearest_clamp;   /* nearest, clamp (shadow reads)    */
    SDL_GPUSampler *linear_clamp;    /* linear, clamp (reflection reads) */
    SDL_GPUSampler *cubemap_sampler; /* linear, clamp for cube maps      */

    /* Scene objects. */
    SDL_GPUTexture *white_texture;   /* 1x1 white placeholder texture         */
    SDL_GPUTexture *cubemap_texture; /* HDR skybox as 6-face cube map         */
    ModelData boat;                  /* low-poly boat model (glTF)            */
    ModelData rocks;                 /* rock cliff model (glTF, textured)     */

    /* Floor geometry. */
    SDL_GPUBuffer *floor_vb;  /* sandy floor vertex buffer (quad)  */
    SDL_GPUBuffer *floor_ib;  /* sandy floor index buffer (2 tris) */

    /* Water geometry. */
    SDL_GPUBuffer *water_vb;  /* water surface vertex buffer (quad)  */
    SDL_GPUBuffer *water_ib;  /* water surface index buffer (2 tris) */

    /* Skybox geometry. */
    SDL_GPUBuffer *skybox_vb; /* skybox cube vertex buffer (8 verts)  */
    SDL_GPUBuffer *skybox_ib; /* skybox cube index buffer (36 indices) */

    /* Light. */
    mat4 light_vp; /* directional light view-projection matrix */

    /* Swapchain format. */
    SDL_GPUTextureFormat swapchain_format; /* sRGB format from the swapchain */

    /* Camera. */
    vec3  cam_position; /* world-space camera position               */
    float cam_yaw;      /* horizontal rotation (radians, 0 = +Z)    */
    float cam_pitch;    /* vertical rotation (radians, clamped ±1.5) */

    /* Timing and input. */
    Uint64 last_ticks;     /* previous frame's performance counter */
    bool   mouse_captured; /* true when mouse look is active       */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
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

    forge_arena_destroy(&model->gltf_arena);
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
    if (!forge_gltf_load(path, &model->scene, &model->gltf_arena)) {
        SDL_Log("Failed to load glTF: %s", path);
        forge_arena_destroy(&model->gltf_arena);
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

/* ── Helper: draw the sandy floor ───────────────────────────────────── */

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
    frag_u.base_color[0] = FLOOR_COLOR_R;
    frag_u.base_color[1] = FLOOR_COLOR_G;
    frag_u.base_color[2] = FLOOR_COLOR_B;
    frag_u.base_color[3] = 1.0f;
    frag_u.has_texture    = 0.0f;
    frag_u.eye_pos[0]    = eye_pos->x;
    frag_u.eye_pos[1]    = eye_pos->y;
    frag_u.eye_pos[2]    = eye_pos->z;
    frag_u.ambient        = MATERIAL_AMBIENT;
    frag_u.shininess      = FLOOR_SHININESS;
    frag_u.specular_str   = FLOOR_SPECULAR_STR;
    frag_u.light_dir[0]   = LIGHT_DIR_X;
    frag_u.light_dir[1]   = LIGHT_DIR_Y;
    frag_u.light_dir[2]   = LIGHT_DIR_Z;
    frag_u.light_dir[3]   = 0.0f;
    frag_u.light_color[0] = LIGHT_COLOR_R;
    frag_u.light_color[1] = LIGHT_COLOR_G;
    frag_u.light_color[2] = LIGHT_COLOR_B;
    frag_u.light_intensity = LIGHT_INTENSITY;
    SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

    SDL_GPUTextureSamplerBinding tex_binds[2];
    tex_binds[0] = (SDL_GPUTextureSamplerBinding){
        .texture = state->white_texture, .sampler = state->sampler };
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
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 30 \xe2\x80\x94 Planar Reflections",
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
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->linear_clamp = SDL_CreateGPUSampler(device, &si);
        if (!state->linear_clamp) {
            SDL_Log("Failed to create linear_clamp sampler: %s", SDL_GetError());
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

        SDL_snprintf(path, sizeof(path), "%s%s", base, BOAT_MODEL_PATH);
        if (!setup_model(device, &state->boat, path))
            goto init_fail;

        SDL_snprintf(path, sizeof(path), "%s%s", base, ROCKS_MODEL_PATH);
        if (!setup_model(device, &state->rocks, path))
            goto init_fail;

        /* Load cubemap. */
        SDL_snprintf(path, sizeof(path), "%s%s", base, SKYBOX_FACE_DIR);
        state->cubemap_texture = create_cubemap_texture(device, path);
        if (!state->cubemap_texture)
            goto init_fail;
    }

    /* ── Shadow pipeline (depth-only) ───────────────────────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            shadow_vert_spirv, sizeof(shadow_vert_spirv),
            shadow_vert_dxil, sizeof(shadow_vert_dxil),
        shadow_vert_msl, shadow_vert_msl_size, 0, 1);
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_frag_spirv, sizeof(shadow_frag_spirv),
            shadow_frag_dxil, sizeof(shadow_frag_dxil),
        shadow_frag_msl, shadow_frag_msl_size, 0, 0);
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
            scene_vert_dxil, sizeof(scene_vert_dxil),
        scene_vert_msl, scene_vert_msl_size, 0, 1);
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            scene_frag_spirv, sizeof(scene_frag_spirv),
            scene_frag_dxil, sizeof(scene_frag_dxil),
        scene_frag_msl, scene_frag_msl_size, 2, 1);
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
        if (!state->scene_pipeline) {
            SDL_ReleaseGPUShader(device, vert);
            SDL_ReleaseGPUShader(device, frag);
            SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
            goto init_fail;
        }

        /* Reflected variant: flip front face so back-face culling works
         * correctly when the reflection matrix reverses triangle winding. */
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
        state->scene_pipeline_refl = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->scene_pipeline_refl) {
            SDL_Log("Failed to create scene_pipeline_refl: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Skybox pipeline (depth test <= to draw at far plane) ────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            skybox_vert_spirv, sizeof(skybox_vert_spirv),
            skybox_vert_dxil, sizeof(skybox_vert_dxil),
        skybox_vert_msl, skybox_vert_msl_size, 0, 1);
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            skybox_frag_spirv, sizeof(skybox_frag_spirv),
            skybox_frag_dxil, sizeof(skybox_frag_dxil),
        skybox_frag_msl, skybox_frag_msl_size, 1, 0);
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
        if (!state->skybox_pipeline) {
            SDL_ReleaseGPUShader(device, vert);
            SDL_ReleaseGPUShader(device, frag);
            SDL_Log("Failed to create skybox pipeline: %s", SDL_GetError());
            goto init_fail;
        }

        /* Reflected variant: flip front face for correct culling in reflection. */
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
        state->skybox_pipeline_refl = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->skybox_pipeline_refl) {
            SDL_Log("Failed to create skybox_pipeline_refl: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Water pipeline (alpha blend, depth test ON, depth write OFF) ── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            water_vert_spirv, sizeof(water_vert_spirv),
            water_vert_dxil, sizeof(water_vert_dxil),
        water_vert_msl, water_vert_msl_size, 0, 1);
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            water_frag_spirv, sizeof(water_frag_spirv),
            water_frag_dxil, sizeof(water_frag_dxil),
        water_frag_msl, water_frag_msl_size, 1, 1);
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
        color_desc.blend_state.enable_blend = true;
        color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        color_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        color_desc.blend_state.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
            SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 3;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.target_info.has_depth_stencil_target   = true;

        state->water_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->water_pipeline) {
            SDL_Log("Failed to create water pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Floor geometry (XZ quad at FLOOR_Y) ────────────────────── */
    {
        ForgeGltfVertex verts[4] = {
            {{ -FLOOR_HALF_SIZE, FLOOR_Y, -FLOOR_HALF_SIZE }, { 0,1,0 }, { 0,0 }},
            {{  FLOOR_HALF_SIZE, FLOOR_Y, -FLOOR_HALF_SIZE }, { 0,1,0 }, { 1,0 }},
            {{  FLOOR_HALF_SIZE, FLOOR_Y,  FLOOR_HALF_SIZE }, { 0,1,0 }, { 1,1 }},
            {{ -FLOOR_HALF_SIZE, FLOOR_Y,  FLOOR_HALF_SIZE }, { 0,1,0 }, { 0,1 }},
        };
        Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };

        state->floor_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, verts, sizeof(verts));
        state->floor_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
        if (!state->floor_vb || !state->floor_ib) goto init_fail;
    }

    /* ── Water geometry (XZ quad at WATER_LEVEL) ────────────────── */
    {
        ForgeGltfVertex verts[4] = {
            {{ -WATER_HALF_SIZE, WATER_LEVEL, -WATER_HALF_SIZE }, { 0,1,0 }, { 0,0 }},
            {{  WATER_HALF_SIZE, WATER_LEVEL, -WATER_HALF_SIZE }, { 0,1,0 }, { 1,0 }},
            {{  WATER_HALF_SIZE, WATER_LEVEL,  WATER_HALF_SIZE }, { 0,1,0 }, { 1,1 }},
            {{ -WATER_HALF_SIZE, WATER_LEVEL,  WATER_HALF_SIZE }, { 0,1,0 }, { 0,1 }},
        };
        Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };

        state->water_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, verts, sizeof(verts));
        state->water_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
        if (!state->water_vb || !state->water_ib) goto init_fail;
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

    /* ── Reflection render targets ──────────────────────────────── */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type                = SDL_GPU_TEXTURETYPE_2D;
        ti.format              = swapchain_format;
        ti.width               = WINDOW_WIDTH;
        ti.height              = WINDOW_HEIGHT;
        ti.layer_count_or_depth = 1;
        ti.num_levels          = 1;
        ti.usage               = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                 SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->reflection_color = SDL_CreateGPUTexture(device, &ti);
        if (!state->reflection_color) {
            SDL_Log("Failed to create reflection_color: %s", SDL_GetError());
            goto init_fail;
        }

        ti.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        ti.usage  = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        state->reflection_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->reflection_depth) {
            SDL_Log("Failed to create reflection_depth: %s", SDL_GetError());
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

    /* ── Camera matrices ───────────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view   = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    mat4 proj   = mat4_perspective(
        (float)FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* ── Model placement matrices ──────────────────────────────────── */
    mat4 boat_placement = mat4_translate(
        vec3_create(BOAT_POS_X, BOAT_POS_Y, BOAT_POS_Z));
    mat4 rocks_placement = mat4_scale(
        vec3_create(ROCK_SCALE, ROCK_SCALE, ROCK_SCALE));

    /* ── Underwater guard ─────────────────────────────────────────── */
    /* Planar reflections are only valid when the camera is above the
     * water plane.  When the camera is submerged, the reflection matrix
     * mirrors it above the surface and the oblique clip plane clips the
     * wrong half-space, producing inverted/broken output.  The standard
     * approach is to skip the reflection and water passes entirely when
     * the camera is below the water level — the main scene pass still
     * renders correctly on its own.  A production engine would switch
     * to an underwater rendering mode (caustics, fog, tinted color). */
    bool camera_above_water = state->cam_position.y > WATER_LEVEL;

    /* ── Mirrored camera for reflection pass ───────────────────────── */
    /* Mirror the camera position across the water plane Y = WATER_LEVEL.
     * Only computed when the camera is above water. */
    vec3 reflected_cam = state->cam_position;
    mat4 reflected_view = view;
    if (camera_above_water) {
        reflected_cam.y = 2.0f * WATER_LEVEL - reflected_cam.y;

        /* Build the mirrored view matrix using reflection matrix. */
        mat4 reflect_mat = mat4_reflect(0.0f, 1.0f, 0.0f, -WATER_LEVEL);
        reflected_view = mat4_multiply(view, reflect_mat);
    }

    /* Apply oblique near-plane clipping: the clip plane is the water
     * surface in view space.  The water plane equation in world space
     * is (0, 1, 0, -WATER_LEVEL).  We transform it to view space.
     * Only needed when the camera is above water. */
    mat4 reflected_vp = mat4_multiply(proj, reflected_view);
    if (camera_above_water) {
        mat4 view_inv_transpose = mat4_transpose(mat4_inverse(reflected_view));
        float pw[4] = { 0.0f, 1.0f, 0.0f, -WATER_LEVEL };
        vec4 clip_plane_view;
        clip_plane_view.x = view_inv_transpose.m[0]  * pw[0] +
                            view_inv_transpose.m[4]  * pw[1] +
                            view_inv_transpose.m[8]  * pw[2] +
                            view_inv_transpose.m[12] * pw[3];
        clip_plane_view.y = view_inv_transpose.m[1]  * pw[0] +
                            view_inv_transpose.m[5]  * pw[1] +
                            view_inv_transpose.m[9]  * pw[2] +
                            view_inv_transpose.m[13] * pw[3];
        clip_plane_view.z = view_inv_transpose.m[2]  * pw[0] +
                            view_inv_transpose.m[6]  * pw[1] +
                            view_inv_transpose.m[10] * pw[2] +
                            view_inv_transpose.m[14] * pw[3];
        clip_plane_view.w = view_inv_transpose.m[3]  * pw[0] +
                            view_inv_transpose.m[7]  * pw[1] +
                            view_inv_transpose.m[11] * pw[2] +
                            view_inv_transpose.m[15] * pw[3];

        mat4 oblique_proj = mat4_oblique_near_plane(proj, clip_plane_view);
        reflected_vp = mat4_multiply(oblique_proj, reflected_view);
    }
    {

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

            draw_model_shadow(shadow_pass, cmd, &state->boat,
                              &boat_placement, &state->light_vp);
            draw_model_shadow(shadow_pass, cmd, &state->rocks,
                              &rocks_placement, &state->light_vp);

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

        /* ══ PASS 2: Reflection pass (skipped when camera is underwater) ═ */
        if (camera_above_water) {
            SDL_GPUColorTargetInfo refl_ct;
            SDL_zero(refl_ct);
            refl_ct.texture     = state->reflection_color;
            refl_ct.load_op     = SDL_GPU_LOADOP_CLEAR;
            refl_ct.store_op    = SDL_GPU_STOREOP_STORE;
            refl_ct.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

            SDL_GPUDepthStencilTargetInfo refl_dti;
            SDL_zero(refl_dti);
            refl_dti.texture     = state->reflection_depth;
            refl_dti.load_op     = SDL_GPU_LOADOP_CLEAR;
            refl_dti.store_op    = SDL_GPU_STOREOP_DONT_CARE;
            refl_dti.clear_depth = 1.0f;

            SDL_GPURenderPass *refl_pass = SDL_BeginGPURenderPass(
                cmd, &refl_ct, 1, &refl_dti);
            if (!refl_pass) {
                SDL_Log("SDL_BeginGPURenderPass (reflection) failed: %s",
                        SDL_GetError());
                if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                    SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
                }
                return SDL_APP_FAILURE;
            }

            /* Draw reflected scene: boat, rocks — NOT water, NOT floor.
             * Use the reflected-variant pipelines which flip the front face
             * to CLOCKWISE, compensating for the winding reversal caused by
             * the reflection matrix (det = -1). */
            SDL_BindGPUGraphicsPipeline(refl_pass, state->scene_pipeline_refl);

            draw_model_scene(refl_pass, cmd, &state->boat, state,
                             &boat_placement, &reflected_vp, &reflected_cam);
            draw_model_scene(refl_pass, cmd, &state->rocks, state,
                             &rocks_placement, &reflected_vp, &reflected_cam);

            /* Draw skybox in reflection (also needs flipped winding). */
            SDL_BindGPUGraphicsPipeline(refl_pass, state->skybox_pipeline_refl);
            draw_skybox(refl_pass, cmd, state, &reflected_view, &proj);

            SDL_EndGPURenderPass(refl_pass);
        }

        /* ══ PASS 3: Main scene pass (to swapchain) ═══════════════════ */
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
            draw_model_scene(main_pass, cmd, &state->boat, state,
                             &boat_placement, &cam_vp, &state->cam_position);
            draw_model_scene(main_pass, cmd, &state->rocks, state,
                             &rocks_placement, &cam_vp, &state->cam_position);

            /* Draw skybox. */
            SDL_BindGPUGraphicsPipeline(main_pass, state->skybox_pipeline);
            draw_skybox(main_pass, cmd, state, &view, &proj);

            SDL_EndGPURenderPass(main_pass);
        }

        /* ══ PASS 4: Water pass (skipped when camera is underwater) ════ */
        if (camera_above_water) {
            SDL_GPUColorTargetInfo water_ct;
            SDL_zero(water_ct);
            water_ct.texture  = swapchain_tex;
            water_ct.load_op  = SDL_GPU_LOADOP_LOAD;
            water_ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPUDepthStencilTargetInfo water_dti;
            SDL_zero(water_dti);
            water_dti.texture     = state->main_depth;
            water_dti.load_op     = SDL_GPU_LOADOP_LOAD;
            water_dti.store_op    = SDL_GPU_STOREOP_DONT_CARE;

            SDL_GPURenderPass *water_pass = SDL_BeginGPURenderPass(
                cmd, &water_ct, 1, &water_dti);
            if (!water_pass) {
                SDL_Log("SDL_BeginGPURenderPass (water) failed: %s",
                        SDL_GetError());
                if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                    SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
                }
                return SDL_APP_FAILURE;
            }

            SDL_BindGPUGraphicsPipeline(water_pass, state->water_pipeline);

            /* Water vertex uniforms. */
            mat4 water_model = mat4_identity();
            WaterVertUniforms water_vu;
            water_vu.mvp = mat4_multiply(cam_vp, water_model);
            SDL_PushGPUVertexUniformData(cmd, 0, &water_vu, sizeof(water_vu));

            /* Water fragment uniforms. */
            WaterFragUniforms water_fu;
            water_fu.eye_pos[0]   = state->cam_position.x;
            water_fu.eye_pos[1]   = state->cam_position.y;
            water_fu.eye_pos[2]   = state->cam_position.z;
            water_fu.water_level  = WATER_LEVEL;
            water_fu.water_tint[0] = WATER_TINT_R;
            water_fu.water_tint[1] = WATER_TINT_G;
            water_fu.water_tint[2] = WATER_TINT_B;
            water_fu.water_tint[3] = WATER_TINT_A;
            water_fu.fresnel_f0   = FRESNEL_F0;
            water_fu._pad[0] = 0.0f;
            water_fu._pad[1] = 0.0f;
            water_fu._pad[2] = 0.0f;
            SDL_PushGPUFragmentUniformData(cmd, 0, &water_fu, sizeof(water_fu));

            /* Bind reflection texture. */
            SDL_GPUTextureSamplerBinding water_tex = {
                .texture = state->reflection_color,
                .sampler = state->linear_clamp
            };
            SDL_BindGPUFragmentSamplers(water_pass, 0, &water_tex, 1);

            SDL_GPUBufferBinding vb = { state->water_vb, 0 };
            SDL_BindGPUVertexBuffers(water_pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib = { state->water_ib, 0 };
            SDL_BindGPUIndexBuffer(water_pass, &ib,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_DrawGPUIndexedPrimitives(water_pass, QUAD_INDEX_COUNT, 1, 0, 0, 0);

            SDL_EndGPURenderPass(water_pass);
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

    free_model_gpu(state->device, &state->boat);
    free_model_gpu(state->device, &state->rocks);

    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->scene_pipeline_refl)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline_refl);
    if (state->skybox_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->skybox_pipeline);
    if (state->skybox_pipeline_refl)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->skybox_pipeline_refl);
    if (state->water_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->water_pipeline);

    if (state->shadow_depth)
        SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->reflection_color)
        SDL_ReleaseGPUTexture(state->device, state->reflection_color);
    if (state->reflection_depth)
        SDL_ReleaseGPUTexture(state->device, state->reflection_depth);
    if (state->main_depth)
        SDL_ReleaseGPUTexture(state->device, state->main_depth);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
    if (state->cubemap_texture)
        SDL_ReleaseGPUTexture(state->device, state->cubemap_texture);

    if (state->sampler)
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    if (state->nearest_clamp)
        SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->linear_clamp)
        SDL_ReleaseGPUSampler(state->device, state->linear_clamp);
    if (state->cubemap_sampler)
        SDL_ReleaseGPUSampler(state->device, state->cubemap_sampler);

    if (state->floor_vb)  SDL_ReleaseGPUBuffer(state->device, state->floor_vb);
    if (state->floor_ib)  SDL_ReleaseGPUBuffer(state->device, state->floor_ib);
    if (state->water_vb)  SDL_ReleaseGPUBuffer(state->device, state->water_vb);
    if (state->water_ib)  SDL_ReleaseGPUBuffer(state->device, state->water_ib);
    if (state->skybox_vb) SDL_ReleaseGPUBuffer(state->device, state->skybox_vb);
    if (state->skybox_ib) SDL_ReleaseGPUBuffer(state->device, state->skybox_ib);

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
