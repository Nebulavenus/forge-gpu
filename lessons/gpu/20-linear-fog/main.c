/*
 * Lesson 20 — Linear Fog
 *
 * Lesson 15 arranged a milk truck and textured boxes in a ring formation.
 * This lesson reuses that scene and adds depth-based distance fog — the
 * first step toward atmospheric rendering.
 *
 * Distance fog simulates how light scatters in the atmosphere: objects
 * farther from the camera appear washed out and blend toward a uniform
 * "fog color."  By matching the fog color to the clear (background) color,
 * distant objects fade seamlessly into the horizon.
 *
 * Three fog modes are supported, toggled at runtime with keys 1/2/3:
 *
 *   1. Linear      — fog ramps linearly between a start and end distance
 *   2. Exponential  — smooth exponential decay (denser overall)
 *   3. Exp-squared  — holds clear near the camera, then drops sharply
 *
 * Both the scene objects and the grid floor apply the same fog parameters,
 * ensuring a consistent atmospheric effect across the entire scene.
 *
 * What's new compared to Lesson 15:
 *   - Fog uniforms added to both fragment cbuffers (96 → 128 bytes)
 *   - Three fog modes: linear, exponential, exponential-squared
 *   - Clear color = fog color for seamless horizon blending
 *   - Keys 1/2/3 toggle fog mode at runtime
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain      (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline              (Lesson 02)
 *   - Push uniforms for matrices + fragment data              (Lesson 03)
 *   - Texture + sampler binding, mipmaps                      (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize          (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time         (Lesson 07)
 *   - glTF parsing, GPU upload, material handling             (Lesson 09)
 *   - Blinn-Phong lighting, normal transformation             (Lesson 10)
 *   - Procedural grid floor                                   (Lesson 12)
 *   - Milk truck + box scene layout                           (Lesson 15)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   1 / 2 / 3          — switch fog mode (linear / exp / exp²)
 *   Escape             — release mouse / quit
 *
 * Models: CesiumMilkTruck and BoxTextured (from shared assets/models/).
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include "math/forge_math.h"
#include "gltf/forge_gltf.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

/* ── Frame capture (compile-time option) ─────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────── */

#include "shaders/compiled/fog_vert_spirv.h"
#include "shaders/compiled/fog_frag_spirv.h"
#include "shaders/compiled/fog_vert_dxil.h"
#include "shaders/compiled/fog_frag_dxil.h"
#include "shaders/compiled/grid_fog_vert_spirv.h"
#include "shaders/compiled/grid_fog_frag_spirv.h"
#include "shaders/compiled/grid_fog_vert_dxil.h"
#include "shaders/compiled/grid_fog_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 20 Linear Fog"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Medium gray fog / clear color.  The fog color is set to the same
 * value — this is essential for a seamless horizon.  Objects at the fog
 * end distance blend to this exact color, which is also the framebuffer
 * clear color, so there is no visible boundary between "fogged geometry"
 * and "empty background." */
#define CLEAR_R 0.5f
#define CLEAR_G 0.5f
#define CLEAR_B 0.5f
#define CLEAR_A 1.0f

#define FOG_R CLEAR_R
#define FOG_G CLEAR_G
#define FOG_B CLEAR_B

/* Depth buffer */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Texture constants */
#define BYTES_PER_PIXEL  4
#define WHITE_TEX_DIM    1
#define MAX_LOD          1000.0f

/* Scene model paths (relative to executable). */
#define TRUCK_MODEL_PATH "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH   "assets/models/BoxTextured/BoxTextured.gltf"
#define PATH_BUFFER_SIZE 512

/* Vertex attribute count: position (float3) + normal (float3) + uv (float2). */
#define NUM_VERTEX_ATTRIBUTES 3

/* ── Shader resource counts ──────────────────────────────────────────── */

/* Scene vertex shader: 0 samplers, 0 storage, 1 uniform (MVP + model) */
#define VS_NUM_SAMPLERS         0
#define VS_NUM_STORAGE_TEXTURES 0
#define VS_NUM_STORAGE_BUFFERS  0
#define VS_NUM_UNIFORM_BUFFERS  1

/* Scene fragment shader: 1 sampler (diffuse), 0 storage, 1 uniform */
#define FS_NUM_SAMPLERS         1
#define FS_NUM_STORAGE_TEXTURES 0
#define FS_NUM_STORAGE_BUFFERS  0
#define FS_NUM_UNIFORM_BUFFERS  1

/* Grid shader resource counts */
#define GRID_VS_NUM_SAMPLERS         0
#define GRID_VS_NUM_STORAGE_TEXTURES 0
#define GRID_VS_NUM_STORAGE_BUFFERS  0
#define GRID_VS_NUM_UNIFORM_BUFFERS  1

#define GRID_FS_NUM_SAMPLERS         0
#define GRID_FS_NUM_STORAGE_TEXTURES 0
#define GRID_FS_NUM_STORAGE_BUFFERS  0
#define GRID_FS_NUM_UNIFORM_BUFFERS  1

/* ── Camera parameters ───────────────────────────────────────────────── */

/* Start position: offset to the side and elevated, looking down at the
 * truck and box ring.  Yaw -40° and pitch -25° aim the camera toward
 * the scene center. */
#define CAM_START_X     -6.0f
#define CAM_START_Y      5.0f
#define CAM_START_Z      6.0f
#define CAM_START_YAW   -40.0f
#define CAM_START_PITCH -25.0f

#define MOVE_SPEED        5.0f
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG     89.0f

#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

#define MAX_DELTA_TIME 0.1f

/* ── Lighting parameters ─────────────────────────────────────────────── */

/* Directional light from upper-right-front. */
#define LIGHT_DIR_X 0.5f
#define LIGHT_DIR_Y 1.0f
#define LIGHT_DIR_Z 0.5f

/* Material defaults for Blinn-Phong (applied to glTF base colors). */
#define MAT_AMBIENT_SCALE   0.2f   /* ambient = base_color * this          */
#define MAT_DEFAULT_SPECULAR 0.3f  /* specular reflectance (rgb) [0..1]    */
#define MAT_DEFAULT_SHININESS 32.0f /* specular exponent (higher = tighter) */

/* ── Grid floor parameters ───────────────────────────────────────────── */

#define GRID_HALF_SIZE   50.0f
#define GRID_NUM_VERTS    4
#define GRID_NUM_INDICES  6
#define GRID_VERTEX_PITCH 12   /* 3 floats * 4 bytes */

/* Neutral gray grid lines that read well against gray fog. */
#define GRID_LINE_R       0.35f
#define GRID_LINE_G       0.35f
#define GRID_LINE_B       0.35f
#define GRID_LINE_A       1.0f

#define GRID_BG_R         0.2f
#define GRID_BG_G         0.2f
#define GRID_BG_B         0.2f
#define GRID_BG_A         1.0f

#define GRID_SPACING      1.0f
#define GRID_LINE_WIDTH   0.02f
#define GRID_FADE_DIST    40.0f
#define GRID_AMBIENT      0.15f
#define GRID_SHININESS    32.0f
#define GRID_SPECULAR_STR 0.3f

/* ── Scene layout ────────────────────────────────────────────────────── */

/* Milk truck at the origin with 12 textured boxes arranged in a ring.
 * 8 ground-level boxes at radius 5, plus 4 stacked on top of every
 * other ground box.  This layout provides objects at varied distances
 * from the camera so the fog effect is clearly visible. */
#define BOX_GROUND_COUNT    8
#define BOX_STACK_COUNT     4
#define BOX_TOTAL_COUNT     (BOX_GROUND_COUNT + BOX_STACK_COUNT)
#define BOX_RING_RADIUS     5.0f
#define BOX_GROUND_Y        0.5f
#define BOX_STACK_Y         1.5f
#define BOX_GROUND_ROT_OFFSET 0.3f  /* per-box rotation increment (radians) */
#define BOX_STACK_ROT_OFFSET  0.5f  /* extra rotation for stacked boxes     */

/* ── Fog parameters ──────────────────────────────────────────────────── */

/* Fog mode identifiers (matching HLSL fog_mode uniform). */
#define FOG_MODE_LINEAR  0
#define FOG_MODE_EXP     1
#define FOG_MODE_EXP2    2

/* Fog parameters tuned for the scene layout.  The truck sits at the
 * origin; boxes orbit at radius 5.  With the camera at (-6, 5, 6),
 * distances range from ~5 (nearest box) to ~15 (farthest box).
 * A tighter fog range (2–18) and higher densities make the fog
 * prominently visible. */
#define FOG_START_DIST    2.0f    /* linear: fully visible before this  */
#define FOG_END_DIST     18.0f    /* linear: fully fogged beyond this   */
#define FOG_DENSITY_EXP   0.12f   /* exponential: fog density           */
#define FOG_DENSITY_EXP2  0.08f   /* exp-squared: fog density           */

/* ═══════════════════════════════════════════════════════════════════════
 * Uniform structures (must match HLSL cbuffer layouts exactly)
 * ═══════════════════════════════════════════════════════════════════════ */

/* Vertex uniforms: MVP + model matrix (128 bytes). */
typedef struct VertUniforms {
    mat4 mvp;   /* model-view-projection: transforms to clip space */
    mat4 model; /* model matrix: transforms to world space         */
} VertUniforms;

/* Fragment uniforms: material + lighting + fog (128 bytes).
 * Layout matches fog.frag.hlsl cbuffer:
 *   float4 mat_ambient     (16)
 *   float4 mat_diffuse     (16)
 *   float4 mat_specular    (16)  — rgb + shininess in w
 *   float4 light_dir       (16)
 *   float4 eye_pos         (16)
 *   uint   has_texture      (4)
 *   float  _pad[3]         (12)
 *   float4 fog_color       (16)
 *   float  fog_start        (4)
 *   float  fog_end          (4)
 *   float  fog_density      (4)
 *   uint   fog_mode         (4)
 *   Total: 128 bytes */
typedef struct FragUniforms {
    float mat_ambient[4];   /* material ambient color (rgb, w unused)         */
    float mat_diffuse[4];   /* material diffuse color (rgb, w unused)         */
    float mat_specular[4];  /* specular color (rgb), shininess exponent (w)   */
    float light_dir[4];     /* world-space light direction toward light (xyz) */
    float eye_pos[4];       /* world-space camera position (xyz)              */
    Uint32 has_texture;     /* non-zero = sample diffuse texture              */
    float _pad[3];          /* padding to 16-byte alignment                   */
    float fog_color[4];     /* fog color — must match clear color (rgb)       */
    float fog_start;        /* linear fog: distance where fog begins          */
    float fog_end;          /* linear fog: fully fogged beyond this distance  */
    float fog_density;      /* exp/exp2: fog density coefficient              */
    Uint32 fog_mode;        /* 0 = linear, 1 = exp, 2 = exp-squared          */
} FragUniforms;

/* Grid fragment uniforms: grid appearance + lighting + fog (128 bytes). */
typedef struct GridFragUniforms {
    float line_color[4];    /* grid line color in linear space (rgba)         */
    float bg_color[4];      /* background color between grid lines (rgba)    */
    float light_dir[4];     /* world-space light direction toward light (xyz) */
    float eye_pos[4];       /* world-space camera position (xyz)              */
    float grid_spacing;     /* world-space distance between grid lines        */
    float line_width;       /* grid line thickness in world units             */
    float fade_distance;    /* distance at which grid fades out               */
    float ambient;          /* ambient light intensity [0..1]                 */
    float shininess;        /* specular exponent (e.g. 32, 64, 128)          */
    float specular_str;     /* specular intensity [0..1]                      */
    float _pad0;            /* padding to 16-byte alignment                   */
    float _pad1;            /* padding to 16-byte alignment                   */
    float fog_color[4];     /* fog color — must match clear color (rgb)       */
    float fog_start;        /* linear fog: distance where fog begins          */
    float fog_end;          /* linear fog: fully fogged beyond this distance  */
    float fog_density;      /* exp/exp2: fog density coefficient              */
    Uint32 fog_mode;        /* 0 = linear, 1 = exp, 2 = exp-squared          */
} GridFragUniforms;

/* ── GPU-side per-primitive data ─────────────────────────────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;          /* GPU vertex data (position, normal, uv) */
    SDL_GPUBuffer *index_buffer;           /* GPU index data for indexed drawing      */
    Uint32         index_count;            /* number of indices to draw               */
    int            material_index;         /* index into ModelData.materials (-1=none)*/
    SDL_GPUIndexElementSize index_type;    /* 16-bit or 32-bit indices                */
    bool           has_uvs;                /* whether vertices have texture coords    */
} GpuPrimitive;

typedef struct GpuMaterial {
    float          base_color[4]; /* glTF PBR base color factor (rgba, linear) */
    SDL_GPUTexture *texture;      /* diffuse texture (NULL if untextured)      */
    bool           has_texture;   /* whether to sample the diffuse texture     */
} GpuMaterial;

/* ── Per-model data ──────────────────────────────────────────────────── */

typedef struct ModelData {
    ForgeGltfScene scene;           /* parsed glTF scene (CPU-side)              */
    GpuPrimitive  *primitives;      /* GPU buffers per primitive (heap-allocated) */
    int            primitive_count;  /* number of primitives uploaded             */
    GpuMaterial   *materials;       /* GPU materials per glTF material (heap)    */
    int            material_count;  /* number of materials loaded                */
} ModelData;

/* ── Box placement ───────────────────────────────────────────────────── */

typedef struct BoxPlacement {
    vec3  position;   /* world-space center of the box          */
    float y_rotation; /* rotation around Y axis in radians      */
} BoxPlacement;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Scene pipeline (material Blinn-Phong + fog) */
    SDL_GPUGraphicsPipeline *scene_pipeline;

    /* Grid floor pipeline (procedural grid + fog) */
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUBuffer           *grid_vertex_buffer;
    SDL_GPUBuffer           *grid_index_buffer;

    /* Texture sampler (trilinear + repeat) */
    SDL_GPUSampler *sampler;

    /* 1x1 white placeholder for untextured materials */
    SDL_GPUTexture *white_texture;

    /* Two models loaded from glTF */
    ModelData truck;
    ModelData box;

    /* Pre-computed box placements (model matrices built each frame) */
    BoxPlacement box_placements[BOX_TOTAL_COUNT];
    int          box_count;

    /* Depth buffer (recreated on resize) */
    SDL_GPUTexture *depth_texture;
    Uint32          depth_width;
    Uint32          depth_height;

    /* Camera state */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Fog mode: 0 = linear, 1 = exponential, 2 = exp-squared */
    Uint32 fog_mode;

    /* Timing */
    Uint64 last_ticks;
    bool   mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;

/* ═══════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Create a shader from embedded bytecode ──────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code, size_t dxil_size,
    int num_samplers, int num_storage_textures,
    int num_storage_buffers, int num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage = stage;
    info.num_samplers = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers = num_storage_buffers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv_code;
        info.code_size = spirv_size;
        info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil_code;
        info.code_size = dxil_size;
        info.entrypoint = "main";
    } else {
        SDL_Log("No supported shader format (need SPIRV or DXIL)");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("SDL_CreateGPUShader (%s) failed: %s",
                stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
                SDL_GetError());
    }
    return shader;
}

/* ── Upload raw data to a GPU buffer ─────────────────────────────────── */

static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    const void *data, Uint32 size)
{
    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!transfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUBufferCreateInfo bci;
    SDL_zero(bci);
    bci.usage = usage;
    bci.size = size;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &bci);
    if (!buffer) {
        SDL_Log("SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("SDL_BeginGPUCopyPass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUBuffer(device, buffer);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return NULL;
    }

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = transfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size = size;

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

/* ── Load an image to a GPU texture with mipmaps ─────────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        surface = SDL_LoadSurface(path);
    }
    if (!surface) {
        SDL_Log("Failed to load texture %s: %s", path, SDL_GetError());
        return NULL;
    }

    /* Convert to RGBA8 (GPU R8G8B8A8 = SDL ABGR8888). */
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!rgba) {
        SDL_Log("Failed to convert surface: %s", SDL_GetError());
        return NULL;
    }

    Uint32 w = (Uint32)rgba->w;
    Uint32 h = (Uint32)rgba->h;

    /* Calculate mip count. */
    Uint32 max_dim = w > h ? w : h;
    Uint32 mip_count = 1;
    while (max_dim > 1) { max_dim >>= 1; mip_count++; }

    SDL_GPUTextureCreateInfo tci;
    SDL_zero(tci);
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tci.width = w;
    tci.height = h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = mip_count;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
              | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tci);
    if (!texture) {
        SDL_Log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        SDL_DestroySurface(rgba);
        return NULL;
    }

    Uint32 data_size = w * h * BYTES_PER_PIXEL;

    SDL_GPUTransferBufferCreateInfo transfer_bci;
    SDL_zero(transfer_bci);
    transfer_bci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_bci.size = data_size;
    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(device, &transfer_bci);
    if (!transfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(rgba);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(rgba);
        return NULL;
    }
    SDL_memcpy(mapped, rgba->pixels, data_size);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(rgba);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_Log("SDL_BeginGPUCopyPass (texture) failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = transfer;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = texture;
    tex_dst.w = w;
    tex_dst.h = h;
    tex_dst.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GenerateMipmapsForGPUTexture(cmd, texture);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer (texture) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return texture;
}

/* ── Create a 1x1 placeholder texture ────────────────────────────────── */

static SDL_GPUTexture *create_1x1_texture(SDL_GPUDevice *device,
                                           Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_GPUTextureCreateInfo tci;
    SDL_zero(tci);
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tci.width = 1;
    tci.height = 1;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tci);
    if (!tex) {
        SDL_Log("SDL_CreateGPUTexture (1x1) failed: %s", SDL_GetError());
        return NULL;
    }

    Uint8 pixels[4] = { r, g, b, a };

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(pixels);
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tb) {
        SDL_Log("SDL_CreateGPUTransferBuffer (1x1) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *p = SDL_MapGPUTransferBuffer(device, tb, false);
    if (!p) {
        SDL_Log("SDL_MapGPUTransferBuffer (1x1) failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(p, pixels, sizeof(pixels));
    SDL_UnmapGPUTransferBuffer(device, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer (1x1) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("SDL_BeginGPUCopyPass (1x1) failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = tb;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = 1;
    dst.h = 1;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer (1x1 tex): %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, tb);
    return tex;
}

/* ── Free GPU resources for a model ──────────────────────────────────── */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model)
{
    if (model->primitives) {
        int i;
        for (i = 0; i < model->primitive_count; i++) {
            if (model->primitives[i].vertex_buffer)
                SDL_ReleaseGPUBuffer(device,
                    model->primitives[i].vertex_buffer);
            if (model->primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device,
                    model->primitives[i].index_buffer);
        }
        SDL_free(model->primitives);
        model->primitives = NULL;
    }

    if (model->materials) {
        SDL_GPUTexture *released[FORGE_GLTF_MAX_IMAGES];
        int released_count = 0;
        int i;
        SDL_memset(released, 0, sizeof(released));

        for (i = 0; i < model->material_count; i++) {
            SDL_GPUTexture *tex = model->materials[i].texture;
            int j;
            bool already;
            if (!tex) continue;

            already = false;
            for (j = 0; j < released_count; j++) {
                if (released[j] == tex) {
                    already = true;
                    break;
                }
            }
            if (!already && released_count < FORGE_GLTF_MAX_IMAGES) {
                SDL_ReleaseGPUTexture(device, tex);
                released[released_count++] = tex;
            }
        }
        SDL_free(model->materials);
        model->materials = NULL;
    }
}

/* ── Upload parsed glTF scene to GPU ─────────────────────────────────── */

static bool upload_model_to_gpu(SDL_GPUDevice *device, ModelData *model)
{
    ForgeGltfScene *scene = &model->scene;
    int i;

    /* ── Upload vertex + index buffers per primitive ──────────────── */
    model->primitive_count = scene->primitive_count;
    model->primitives = (GpuPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(GpuPrimitive));
    if (!model->primitives) {
        SDL_Log("Failed to allocate GPU primitives");
        return false;
    }

    for (i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *prim = &scene->primitives[i];
        GpuPrimitive *gpu = &model->primitives[i];

        gpu->material_index = prim->material_index;
        gpu->has_uvs = prim->has_uvs;
        gpu->index_count = (Uint32)prim->index_count;
        gpu->index_type = (prim->index_stride == 2)
            ? SDL_GPU_INDEXELEMENTSIZE_16BIT
            : SDL_GPU_INDEXELEMENTSIZE_32BIT;

        /* Vertex buffer */
        if (prim->vertices && prim->vertex_count > 0) {
            Uint32 vb_size =
                (Uint32)(prim->vertex_count * sizeof(ForgeGltfVertex));
            gpu->vertex_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_VERTEX,
                prim->vertices, vb_size);
            if (!gpu->vertex_buffer) {
                SDL_Log("Failed to upload vertex buffer for primitive %d", i);
                free_model_gpu(device, model);
                return false;
            }
        }

        /* Index buffer */
        if (prim->indices && prim->index_count > 0) {
            Uint32 ib_size =
                (Uint32)(prim->index_count * prim->index_stride);
            gpu->index_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_INDEX,
                prim->indices, ib_size);
            if (!gpu->index_buffer) {
                SDL_Log("Failed to upload index buffer for primitive %d", i);
                free_model_gpu(device, model);
                return false;
            }
        }
    }

    /* ── Load material textures with deduplication ────────────────── */
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
        const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
        SDL_GPUTexture *loaded_tex[FORGE_GLTF_MAX_IMAGES];
        int loaded_count = 0;
        SDL_memset(loaded_tex, 0, sizeof(loaded_tex));
        SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

        for (i = 0; i < scene->material_count; i++) {
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
                int j;
                for (j = 0; j < loaded_count; j++) {
                    if (loaded_paths[j] &&
                        SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
                        dst->texture = loaded_tex[j];
                        found = true;
                        break;
                    }
                }

                if (!found && loaded_count < FORGE_GLTF_MAX_IMAGES) {
                    dst->texture = load_texture(device, src->texture_path);
                    if (dst->texture) {
                        loaded_paths[loaded_count] = src->texture_path;
                        loaded_tex[loaded_count] = dst->texture;
                        loaded_count++;
                    } else {
                        dst->has_texture = false;
                    }
                }
            }

            SDL_Log("  Material %d: '%s' color=(%.2f,%.2f,%.2f) tex=%s",
                    i, src->name,
                    dst->base_color[0], dst->base_color[1],
                    dst->base_color[2],
                    dst->has_texture ? "yes" : "no");
        }
    }

    return true;
}

/* ── Load and set up one model ───────────────────────────────────────── */

static bool setup_model(
    SDL_GPUDevice *device,
    ModelData *model,
    const char *gltf_path,
    const char *name)
{
    SDL_Log("Loading %s from '%s'...", name, gltf_path);

    if (!forge_gltf_load(gltf_path, &model->scene)) {
        SDL_Log("Failed to load %s from '%s'", name, gltf_path);
        return false;
    }

    SDL_Log("%s scene: %d nodes, %d meshes, %d primitives, %d materials",
            name,
            model->scene.node_count,
            model->scene.mesh_count,
            model->scene.primitive_count,
            model->scene.material_count);

    if (!upload_model_to_gpu(device, model)) {
        SDL_Log("Failed to upload %s to GPU", name);
        forge_gltf_free(&model->scene);
        return false;
    }

    return true;
}

/* ── Generate box placements ─────────────────────────────────────────── */
/* 8 boxes in a ring around the origin + 4 stacked on selected boxes. */

static void generate_box_placements(app_state *state)
{
    int idx = 0;
    int i;

    /* Ground-level ring of boxes */
    for (i = 0; i < BOX_GROUND_COUNT; i++) {
        float angle =
            (float)i * (2.0f * FORGE_PI / (float)BOX_GROUND_COUNT);
        state->box_placements[idx].position = vec3_create(
            cosf(angle) * BOX_RING_RADIUS,
            BOX_GROUND_Y,
            sinf(angle) * BOX_RING_RADIUS);
        state->box_placements[idx].y_rotation =
            angle + BOX_GROUND_ROT_OFFSET * (float)i;
        idx++;
    }

    /* Stacked boxes on top of every other ground box */
    for (i = 0; i < BOX_STACK_COUNT; i++) {
        int base = i * 2; /* stack on boxes 0, 2, 4, 6 */
        state->box_placements[idx].position = vec3_create(
            state->box_placements[base].position.x,
            BOX_STACK_Y,
            state->box_placements[base].position.z);
        state->box_placements[idx].y_rotation =
            state->box_placements[base].y_rotation + BOX_STACK_ROT_OFFSET;
        idx++;
    }

    state->box_count = idx;
}

/* ── Draw a model with fog ───────────────────────────────────────────── */
/* Renders all primitives of a model into the current render pass with
 * Blinn-Phong lighting and fog.  The placement matrix positions the
 * object in the scene; each node's world_transform handles the glTF
 * hierarchy (so multi-node models like the truck assemble correctly). */

static void draw_model(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    mat4 placement,
    mat4 cam_vp,
    vec3 light_dir,
    float fog_density)
{
    const ForgeGltfScene *scene = &model->scene;
    int ni;

    for (ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        int pi;
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        {
            /* Per-node model matrix: placement * node's own hierarchy
             * transform.  Critical for multi-node models like
             * CesiumMilkTruck where each part (body, wheels, tank) has
             * its own transform. */
            mat4 model_matrix =
                mat4_multiply(placement, node->world_transform);
            mat4 mvp = mat4_multiply(cam_vp, model_matrix);

            /* Push vertex uniforms: MVP + model matrix. */
            VertUniforms vu;
            vu.mvp = mvp;
            vu.model = model_matrix;
            SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

            const ForgeGltfMesh *mesh =
                &scene->meshes[node->mesh_index];
            for (pi = 0; pi < mesh->primitive_count; pi++) {
                int prim_idx = mesh->first_primitive + pi;
                const GpuPrimitive *prim = &model->primitives[prim_idx];
                SDL_GPUTexture *tex;
                FragUniforms fu;
                SDL_GPUTextureSamplerBinding tsb;
                SDL_GPUBufferBinding vbb;
                SDL_GPUBufferBinding ibb;

                if (!prim->vertex_buffer || !prim->index_buffer) continue;

                /* Set up fragment uniforms from model material */
                tex = state->white_texture;

                if (prim->material_index >= 0 &&
                    prim->material_index < model->material_count) {
                    const GpuMaterial *mat =
                        &model->materials[prim->material_index];
                    /* Use base_color as both ambient and diffuse */
                    fu.mat_ambient[0] = mat->base_color[0] * MAT_AMBIENT_SCALE;
                    fu.mat_ambient[1] = mat->base_color[1] * MAT_AMBIENT_SCALE;
                    fu.mat_ambient[2] = mat->base_color[2] * MAT_AMBIENT_SCALE;
                    fu.mat_ambient[3] = 0.0f;
                    fu.mat_diffuse[0] = mat->base_color[0];
                    fu.mat_diffuse[1] = mat->base_color[1];
                    fu.mat_diffuse[2] = mat->base_color[2];
                    fu.mat_diffuse[3] = 0.0f;
                    fu.mat_specular[0] = MAT_DEFAULT_SPECULAR;
                    fu.mat_specular[1] = MAT_DEFAULT_SPECULAR;
                    fu.mat_specular[2] = MAT_DEFAULT_SPECULAR;
                    fu.mat_specular[3] = MAT_DEFAULT_SHININESS;
                    fu.has_texture = mat->has_texture ? 1 : 0;
                    if (mat->texture)
                        tex = mat->texture;
                } else {
                    fu.mat_ambient[0] = MAT_AMBIENT_SCALE;
                    fu.mat_ambient[1] = MAT_AMBIENT_SCALE;
                    fu.mat_ambient[2] = MAT_AMBIENT_SCALE;
                    fu.mat_ambient[3] = 0.0f;
                    fu.mat_diffuse[0] = 1.0f;
                    fu.mat_diffuse[1] = 1.0f;
                    fu.mat_diffuse[2] = 1.0f;
                    fu.mat_diffuse[3] = 0.0f;
                    fu.mat_specular[0] = MAT_DEFAULT_SPECULAR;
                    fu.mat_specular[1] = MAT_DEFAULT_SPECULAR;
                    fu.mat_specular[2] = MAT_DEFAULT_SPECULAR;
                    fu.mat_specular[3] = MAT_DEFAULT_SHININESS;
                    fu.has_texture = 0;
                }

                fu.light_dir[0] = light_dir.x;
                fu.light_dir[1] = light_dir.y;
                fu.light_dir[2] = light_dir.z;
                fu.light_dir[3] = 0.0f;
                fu.eye_pos[0]   = state->cam_position.x;
                fu.eye_pos[1]   = state->cam_position.y;
                fu.eye_pos[2]   = state->cam_position.z;
                fu.eye_pos[3]   = 0.0f;
                fu._pad[0]      = 0.0f;
                fu._pad[1]      = 0.0f;
                fu._pad[2]      = 0.0f;
                fu.fog_color[0] = FOG_R;
                fu.fog_color[1] = FOG_G;
                fu.fog_color[2] = FOG_B;
                fu.fog_color[3] = 1.0f;
                fu.fog_start    = FOG_START_DIST;
                fu.fog_end      = FOG_END_DIST;
                fu.fog_density  = fog_density;
                fu.fog_mode     = state->fog_mode;
                SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

                /* Bind texture + sampler */
                SDL_zero(tsb);
                tsb.texture = tex;
                tsb.sampler = state->sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);

                /* Bind vertex + index buffers and draw */
                SDL_zero(vbb);
                vbb.buffer = prim->vertex_buffer;
                SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);

                SDL_zero(ibb);
                ibb.buffer = prim->index_buffer;
                SDL_BindGPUIndexBuffer(pass, &ibb, prim->index_type);

                SDL_DrawGPUIndexedPrimitives(
                    pass, prim->index_count, 1, 0, 0, 0);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SDL Application Callbacks
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppInit ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── 1. Initialise SDL ──────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ───────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true,  /* debug mode */
        NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create window ───────────────────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Claim window for GPU rendering ──────────────────────────── */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 5. Request sRGB swapchain ──────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 6. Allocate application state ──────────────────────────────── */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window = window;
    state->device = device;
    state->fog_mode = FOG_MODE_LINEAR;

    /* ── 7. Create sampler ─────────────────────────────────────────── */
    {
        SDL_GPUSamplerCreateInfo sci;
        SDL_zero(sci);
        sci.min_filter = SDL_GPU_FILTER_LINEAR;
        sci.mag_filter = SDL_GPU_FILTER_LINEAR;
        sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sci.max_lod = MAX_LOD;

        state->sampler = SDL_CreateGPUSampler(device, &sci);
        if (!state->sampler) {
            SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
            goto fail;
        }
    }

    /* ── 8. Create placeholder textures ──────────────────────────────── */
    state->white_texture = create_1x1_texture(device, 255, 255, 255, 255);
    if (!state->white_texture) {
        SDL_Log("Failed to create white placeholder texture");
        goto fail;
    }

    /* ── 9. Load both glTF models ────────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            goto fail;
        }
        char truck_path[PATH_BUFFER_SIZE];
        char box_path[PATH_BUFFER_SIZE];
        int len;

        len = SDL_snprintf(truck_path, sizeof(truck_path),
                           "%s%s", base, TRUCK_MODEL_PATH);
        if (len < 0 || (size_t)len >= sizeof(truck_path)) {
            SDL_Log("Truck model path too long");
            goto fail;
        }

        len = SDL_snprintf(box_path, sizeof(box_path),
                           "%s%s", base, BOX_MODEL_PATH);
        if (len < 0 || (size_t)len >= sizeof(box_path)) {
            SDL_Log("Box model path too long");
            goto fail;
        }

        if (!setup_model(device, &state->truck, truck_path,
                         "CesiumMilkTruck")) {
            goto fail;
        }

        if (!setup_model(device, &state->box, box_path,
                         "BoxTextured")) {
            free_model_gpu(device, &state->truck);
            forge_gltf_free(&state->truck.scene);
            goto fail;
        }
    }

    /* Generate box placement data */
    generate_box_placements(state);

    /* ── 10. Create shaders ─────────────────────────────────────────── */
    SDL_GPUShader *scene_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        fog_vert_spirv, sizeof(fog_vert_spirv),
        fog_vert_dxil, sizeof(fog_vert_dxil),
        VS_NUM_SAMPLERS, VS_NUM_STORAGE_TEXTURES,
        VS_NUM_STORAGE_BUFFERS, VS_NUM_UNIFORM_BUFFERS);
    if (!scene_vs) goto fail;

    SDL_GPUShader *scene_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        fog_frag_spirv, sizeof(fog_frag_spirv),
        fog_frag_dxil, sizeof(fog_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_STORAGE_TEXTURES,
        FS_NUM_STORAGE_BUFFERS, FS_NUM_UNIFORM_BUFFERS);
    if (!scene_fs) { SDL_ReleaseGPUShader(device, scene_vs); goto fail; }

    /* ── 11. Define vertex layout ───────────────────────────────────── */
    {
        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot = 0;
        vb_desc.pitch = sizeof(ForgeGltfVertex);
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_desc.instance_step_rate = 0;

        SDL_GPUVertexAttribute attrs[NUM_VERTEX_ATTRIBUTES];
        SDL_zero(attrs);

        /* Location 0: position (float3) — maps to HLSL TEXCOORD0 */
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = offsetof(ForgeGltfVertex, position);

        /* Location 1: normal (float3) — maps to HLSL TEXCOORD1 */
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset = offsetof(ForgeGltfVertex, normal);

        /* Location 2: uv (float2) — maps to HLSL TEXCOORD2 */
        attrs[2].location = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset = offsetof(ForgeGltfVertex, uv);

        /* ── 12. Create scene pipeline ─────────────────────────────── */
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = scene_vs;
        pipe.fragment_shader = scene_fs;
        pipe.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = attrs;
        pipe.vertex_input_state.num_vertex_attributes = NUM_VERTEX_ATTRIBUTES;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* Back-face culling — glTF faces are counter-clockwise. */
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* Depth testing for correct draw order. */
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->scene_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->scene_pipeline) {
            SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
            SDL_ReleaseGPUShader(device, scene_fs);
            SDL_ReleaseGPUShader(device, scene_vs);
            goto fail;
        }
    }

    /* Shaders are baked into the pipeline — safe to release now. */
    SDL_ReleaseGPUShader(device, scene_fs);
    SDL_ReleaseGPUShader(device, scene_vs);

    /* ── 13. Create grid pipeline ────────────────────────────────────── */
    {
        SDL_GPUShader *grid_vs = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_fog_vert_spirv, sizeof(grid_fog_vert_spirv),
            grid_fog_vert_dxil, sizeof(grid_fog_vert_dxil),
            GRID_VS_NUM_SAMPLERS, GRID_VS_NUM_STORAGE_TEXTURES,
            GRID_VS_NUM_STORAGE_BUFFERS, GRID_VS_NUM_UNIFORM_BUFFERS);
        if (!grid_vs) goto fail;

        SDL_GPUShader *grid_fs = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_fog_frag_spirv, sizeof(grid_fog_frag_spirv),
            grid_fog_frag_dxil, sizeof(grid_fog_frag_dxil),
            GRID_FS_NUM_SAMPLERS, GRID_FS_NUM_STORAGE_TEXTURES,
            GRID_FS_NUM_STORAGE_BUFFERS, GRID_FS_NUM_UNIFORM_BUFFERS);
        if (!grid_fs) {
            SDL_ReleaseGPUShader(device, grid_vs);
            goto fail;
        }

        SDL_GPUVertexBufferDescription grid_vb_desc;
        SDL_zero(grid_vb_desc);
        grid_vb_desc.slot = 0;
        grid_vb_desc.pitch = GRID_VERTEX_PITCH;
        grid_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        grid_vb_desc.instance_step_rate = 0;

        SDL_GPUVertexAttribute grid_attr;
        SDL_zero(grid_attr);
        grid_attr.location = 0;
        grid_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        grid_attr.offset = 0;

        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = grid_vs;
        pipe.fragment_shader = grid_fs;
        pipe.vertex_input_state.vertex_buffer_descriptions = &grid_vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = &grid_attr;
        pipe.vertex_input_state.num_vertex_attributes = 1;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->grid_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        SDL_ReleaseGPUShader(device, grid_fs);
        SDL_ReleaseGPUShader(device, grid_vs);
        if (!state->grid_pipeline) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto fail;
        }
    }

    /* ── 14. Upload grid geometry ────────────────────────────────────── */
    {
        float grid_verts[GRID_NUM_VERTS * 3] = {
            -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
            -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
        };
        Uint16 grid_indices[GRID_NUM_INDICES] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vertex_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_VERTEX,
            grid_verts, sizeof(grid_verts));
        if (!state->grid_vertex_buffer) goto fail;

        state->grid_index_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_INDEX,
            grid_indices, sizeof(grid_indices));
        if (!state->grid_index_buffer) goto fail;
    }

    /* ── 15. Create depth texture ────────────────────────────────────── */
    {
        int win_w, win_h;
        if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
            SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
            goto fail;
        }

        SDL_GPUTextureCreateInfo dci;
        SDL_zero(dci);
        dci.type = SDL_GPU_TEXTURETYPE_2D;
        dci.format = DEPTH_FORMAT;
        dci.width = (Uint32)win_w;
        dci.height = (Uint32)win_h;
        dci.layer_count_or_depth = 1;
        dci.num_levels = 1;
        dci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->depth_texture = SDL_CreateGPUTexture(device, &dci);
        if (!state->depth_texture) {
            SDL_Log("SDL_CreateGPUTexture (depth) failed: %s",
                    SDL_GetError());
            goto fail;
        }
        state->depth_width = (Uint32)win_w;
        state->depth_height = (Uint32)win_h;
    }

    /* ── 16. Camera initial state ────────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_START_YAW * FORGE_DEG2RAD;
    state->cam_pitch = CAM_START_PITCH * FORGE_DEG2RAD;
    state->last_ticks = SDL_GetPerformanceCounter();
    state->mouse_captured = false;

    /* ── 17. Capture mouse ───────────────────────────────────────────── */
#ifndef FORGE_CAPTURE
    if (SDL_SetWindowRelativeMouseMode(window, true)) {
        state->mouse_captured = true;
    } else {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    }
#endif

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            goto fail;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    *appstate = state;

    SDL_Log("Scene: milk truck at origin, %d boxes in ring (radius=%.1f)",
            state->box_count, BOX_RING_RADIUS);
    SDL_Log("Fog mode: Linear (press 1/2/3 to switch)");
    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");

    return SDL_APP_CONTINUE;

fail:
    /* Centralised cleanup on init failure. */
    free_model_gpu(device, &state->box);
    forge_gltf_free(&state->box.scene);
    free_model_gpu(device, &state->truck);
    forge_gltf_free(&state->truck.scene);
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->scene_pipeline);
    if (state->sampler) SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────── */

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
                }
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        /* Fog mode toggle: keys 1, 2, 3 */
        if (event->key.key == SDLK_1) {
            state->fog_mode = FOG_MODE_LINEAR;
            SDL_Log("Fog mode: Linear (start=%.0f, end=%.0f)",
                    FOG_START_DIST, FOG_END_DIST);
        }
        if (event->key.key == SDLK_2) {
            state->fog_mode = FOG_MODE_EXP;
            SDL_Log("Fog mode: Exponential (density=%.3f)",
                    FOG_DENSITY_EXP);
        }
        if (event->key.key == SDLK_3) {
            state->fog_mode = FOG_MODE_EXP2;
            SDL_Log("Fog mode: Exp-squared (density=%.3f)",
                    FOG_DENSITY_EXP2);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            if (SDL_SetWindowRelativeMouseMode(state->window, true)) {
                state->mouse_captured = true;
            } else {
                SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            float max_pitch = MAX_PITCH_DEG * FORGE_DEG2RAD;
            if (state->cam_pitch > max_pitch) state->cam_pitch = max_pitch;
            if (state->cam_pitch < -max_pitch) state->cam_pitch = -max_pitch;
        }
        break;

    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Delta time ──────────────────────────────────────────────────── */
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - state->last_ticks) /
               (float)SDL_GetPerformanceFrequency();
    state->last_ticks = now;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── Camera movement ─────────────────────────────────────────────── */
    {
        quat orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
        vec3 forward = quat_forward(orient);
        vec3 right   = quat_right(orient);
        vec3 up      = vec3_create(0.0f, 1.0f, 0.0f);

        const bool *keys = SDL_GetKeyboardState(NULL);
        float speed = MOVE_SPEED * dt;

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_SPACE])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(up, speed));
        if (keys[SDL_SCANCODE_LSHIFT])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(up, speed));
    }

    /* ── Acquire swapchain texture ───────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
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

    /* ── Resize depth buffer if needed ───────────────────────────────── */
    if (sw_w != state->depth_width || sw_h != state->depth_height) {
        if (state->depth_texture)
            SDL_ReleaseGPUTexture(device, state->depth_texture);

        SDL_GPUTextureCreateInfo dci;
        SDL_zero(dci);
        dci.type = SDL_GPU_TEXTURETYPE_2D;
        dci.format = DEPTH_FORMAT;
        dci.width = sw_w;
        dci.height = sw_h;
        dci.layer_count_or_depth = 1;
        dci.num_levels = 1;
        dci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->depth_texture = SDL_CreateGPUTexture(device, &dci);
        if (!state->depth_texture) {
            SDL_Log("SDL_CreateGPUTexture (depth resize) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }
        state->depth_width = sw_w;
        state->depth_height = sw_h;
    }

    /* ── Build camera matrices ───────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)sw_w / (float)sw_h;
    mat4 proj = mat4_perspective(
        FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 vp = mat4_multiply(proj, view);

    /* Pre-compute normalized light direction (constant for all draws). */
    vec3 light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));

    /* Select fog density based on current mode. */
    float fog_density = (state->fog_mode == FOG_MODE_EXP2)
                      ? FOG_DENSITY_EXP2 : FOG_DENSITY_EXP;

    /* ── Begin render pass ───────────────────────────────────────────── */
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = swapchain_tex;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color.r = CLEAR_R;
    color_target.clear_color.g = CLEAR_G;
    color_target.clear_color.b = CLEAR_B;
    color_target.clear_color.a = CLEAR_A;

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture = state->depth_texture;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.clear_depth = DEPTH_CLEAR;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
        cmd, &color_target, 1, &depth_target);
    if (!pass) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    SDL_SetGPUViewport(pass, &(SDL_GPUViewport){
        0, 0, (float)sw_w, (float)sw_h, 0.0f, 1.0f });
    SDL_SetGPUScissor(pass, &(SDL_Rect){ 0, 0, (int)sw_w, (int)sw_h });

    /* ── Render grid floor ───────────────────────────────────────────── */
    {
        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

        /* Vertex uniform: VP matrix (no model — grid sits at the origin) */
        SDL_PushGPUVertexUniformData(cmd, 0, &vp, sizeof(vp));

        /* Fragment uniform: grid appearance + lighting + fog */
        GridFragUniforms gfu;
        gfu.line_color[0] = GRID_LINE_R;
        gfu.line_color[1] = GRID_LINE_G;
        gfu.line_color[2] = GRID_LINE_B;
        gfu.line_color[3] = GRID_LINE_A;
        gfu.bg_color[0]   = GRID_BG_R;
        gfu.bg_color[1]   = GRID_BG_G;
        gfu.bg_color[2]   = GRID_BG_B;
        gfu.bg_color[3]   = GRID_BG_A;
        gfu.light_dir[0]  = light_dir.x;
        gfu.light_dir[1]  = light_dir.y;
        gfu.light_dir[2]  = light_dir.z;
        gfu.light_dir[3]  = 0.0f;
        gfu.eye_pos[0]    = state->cam_position.x;
        gfu.eye_pos[1]    = state->cam_position.y;
        gfu.eye_pos[2]    = state->cam_position.z;
        gfu.eye_pos[3]    = 0.0f;
        gfu.grid_spacing  = GRID_SPACING;
        gfu.line_width    = GRID_LINE_WIDTH;
        gfu.fade_distance = GRID_FADE_DIST;
        gfu.ambient       = GRID_AMBIENT;
        gfu.shininess     = GRID_SHININESS;
        gfu.specular_str  = GRID_SPECULAR_STR;
        gfu._pad0         = 0.0f;
        gfu._pad1         = 0.0f;
        gfu.fog_color[0]  = FOG_R;
        gfu.fog_color[1]  = FOG_G;
        gfu.fog_color[2]  = FOG_B;
        gfu.fog_color[3]  = 1.0f;
        gfu.fog_start     = FOG_START_DIST;
        gfu.fog_end       = FOG_END_DIST;
        gfu.fog_density   = fog_density;
        gfu.fog_mode      = state->fog_mode;
        SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

        SDL_GPUBufferBinding gvb;
        SDL_zero(gvb);
        gvb.buffer = state->grid_vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &gvb, 1);

        SDL_GPUBufferBinding gib;
        SDL_zero(gib);
        gib.buffer = state->grid_index_buffer;
        SDL_BindGPUIndexBuffer(pass, &gib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES, 1, 0, 0, 0);
    }

    /* ── Render scene objects ────────────────────────────────────────── */
    {
        int bi;

        SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);

        /* Draw the truck at the origin */
        draw_model(pass, cmd, &state->truck, state,
                   mat4_identity(), vp, light_dir, fog_density);

        /* Draw all boxes at their pre-computed placements */
        for (bi = 0; bi < state->box_count; bi++) {
            mat4 t = mat4_translate(state->box_placements[bi].position);
            mat4 r = mat4_rotate_y(state->box_placements[bi].y_rotation);
            mat4 box_placement = mat4_multiply(t, r);

            draw_model(pass, cmd, &state->box, state,
                       box_placement, vp, light_dir, fog_density);
        }
    }

    /* ── End render pass ─────────────────────────────────────────────── */
    SDL_EndGPURenderPass(pass);

#ifdef FORGE_CAPTURE
    /* forge_capture_finish_frame submits the command buffer internally
     * when it returns true (it uses SDL_SubmitGPUCommandBufferAndAcquireFence).
     * The caller must NOT call SDL_SubmitGPUCommandBuffer again — return
     * early in both the "quit after capture" and "continue" cases. */
    if (state->capture.mode != FORGE_CAPTURE_NONE && swapchain_tex) {
        if (forge_capture_finish_frame(&state->capture, cmd, swapchain_tex)) {
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

/* ── SDL_AppQuit ──────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    app_state *state = (app_state *)appstate;
    (void)result;

    if (!state) return;

    SDL_GPUDevice *device = state->device;

    /* Wait for GPU to finish all pending work before releasing. */
    if (!SDL_WaitForGPUIdle(device)) {
        SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* Release models (handles both primitives and material textures). */
    free_model_gpu(device, &state->box);
    forge_gltf_free(&state->box.scene);
    free_model_gpu(device, &state->truck);
    forge_gltf_free(&state->truck.scene);

    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->sampler)
        SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->scene_pipeline);

    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
}
