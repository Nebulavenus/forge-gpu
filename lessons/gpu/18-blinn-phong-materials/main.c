/*
 * Lesson 18 — Blinn-Phong with Materials
 *
 * Lesson 10 introduced Blinn-Phong lighting with a single set of global
 * parameters: one shininess, one ambient strength, one specular strength,
 * and always-white specular highlights.  Every surface in the scene used
 * the same material.
 *
 * This lesson extends that foundation with per-object material properties.
 * Each material defines three RGB colors — ambient, diffuse, and specular
 * reflectance — plus a shininess exponent.  Five Suzanne heads are
 * rendered side by side, each with a different material, demonstrating
 * how the same geometry looks dramatically different under the same light.
 *
 * The five materials come from the classic OpenGL material property tables
 * (originally published in the OpenGL Programming Guide).  They illustrate
 * key differences:
 *
 *   - Gold and Chrome have colored specular highlights (metallic)
 *   - Red Plastic has near-white specular (dielectric)
 *   - Jade has soft, wide highlights (low shininess)
 *   - Pearl has subtle, warm highlights
 *
 * What's new compared to Lesson 10:
 *   - Material struct: ambient, diffuse, specular colors + shininess
 *   - Per-object material uniforms pushed before each draw call
 *   - Multiple instances of the same model at different positions
 *   - Specular highlights are now full RGB (not always white)
 *   - Predefined material library based on classic OpenGL tables
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline             (Lesson 02)
 *   - Push uniforms for matrices + fragment data             (Lesson 03)
 *   - Texture + sampler binding, mipmaps                     (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize         (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time        (Lesson 07)
 *   - glTF parsing, GPU upload, material handling            (Lesson 09)
 *   - Blinn-Phong lighting, normal transformation            (Lesson 10)
 *   - Procedural grid floor                                  (Lesson 12)
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

#include "shaders/compiled/material_vert_spirv.h"
#include "shaders/compiled/material_frag_spirv.h"
#include "shaders/compiled/material_vert_dxil.h"
#include "shaders/compiled/material_frag_dxil.h"
#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 18 Blinn-Phong with Materials"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Dark background so the lit models stand out clearly. */
#define CLEAR_R 0.0099f
#define CLEAR_G 0.0099f
#define CLEAR_B 0.0267f
#define CLEAR_A 1.0f

/* Depth buffer */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Texture constants */
#define BYTES_PER_PIXEL  4
#define WHITE_TEX_DIM    1
#define MAX_LOD          1000.0f

/* Scene model path (relative to executable). */
#define GLTF_PATH        "assets/models/Suzanne/Suzanne.gltf"
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

/* Start position: pulled back and slightly elevated to see all five heads. */
#define CAM_START_X     0.0f
#define CAM_START_Y     2.0f
#define CAM_START_Z    12.0f
#define CAM_START_YAW   0.0f
#define CAM_START_PITCH 0.0f

#define MOVE_SPEED        5.0f
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG     89.0f

#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

#define MAX_DELTA_TIME 0.1f

/* ── Lighting parameters ─────────────────────────────────────────────── */

/* Directional light from upper-right-front — lights all five heads evenly. */
#define LIGHT_DIR_X 0.5f
#define LIGHT_DIR_Y 1.0f
#define LIGHT_DIR_Z 0.5f

/* ── Grid floor parameters ───────────────────────────────────────────── */

#define GRID_HALF_SIZE   50.0f
#define GRID_NUM_VERTS    4
#define GRID_NUM_INDICES  6
#define GRID_VERTEX_PITCH 12   /* 3 floats * 4 bytes */

/* Blue grid lines on dark background (linear sRGB, same as Lessons 12–17) */
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
#define GRID_AMBIENT      0.15f
#define GRID_SHININESS    32.0f
#define GRID_SPECULAR_STR 0.3f

/* ── Scene layout ────────────────────────────────────────────────────── */

/* Five Suzanne heads spaced evenly along the X axis. */
#define NUM_OBJECTS   5
#define OBJECT_SPACING 3.5f

/* Raise models above the grid floor so they sit on it naturally.
 * Suzanne's geometry extends slightly below y = 0 in model space. */
#define SCENE_Y_OFFSET 1.3f

/* ═══════════════════════════════════════════════════════════════════════
 * Material System
 *
 * A Material groups the three reflectance colors of the Blinn-Phong
 * lighting model plus the specular exponent (shininess).
 *
 * These values define how the surface interacts with light:
 *   - ambient:  fraction of ambient light reflected (color in shadow)
 *   - diffuse:  fraction of direct light reflected (the "main color")
 *   - specular: fraction of light at the highlight angle (highlight color)
 *   - shininess: how tight the specular highlight is (higher = smaller)
 *
 * Metals have colored specular (gold highlights are golden); dielectrics
 * (plastics, stone) have near-white specular because their Fresnel
 * reflectance is roughly wavelength-independent at typical viewing angles.
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct Material {
    float ambient[4];    /* ambient reflectance (rgb, w unused)             */
    float diffuse[4];    /* diffuse reflectance (rgb, w unused)             */
    float specular[4];   /* specular reflectance (rgb), shininess (w)       */
} Material;

/* ── Classic material definitions ────────────────────────────────────── */
/* Values adapted from the OpenGL Programming Guide (Devernay tables).
 * These are physically motivated approximations — not measured BRDFs,
 * but they capture the essential character of each material. */

static const Material MATERIAL_GOLD = {
    { 0.24725f, 0.1995f,  0.0745f,  0.0f },
    { 0.75164f, 0.60648f, 0.22648f, 0.0f },
    { 0.628281f, 0.555802f, 0.366065f, 51.2f }
};

static const Material MATERIAL_RED_PLASTIC = {
    { 0.0f,  0.0f,  0.0f, 0.0f },
    { 0.5f,  0.0f,  0.0f, 0.0f },
    { 0.7f,  0.6f,  0.6f, 32.0f }
};

static const Material MATERIAL_JADE = {
    { 0.135f,    0.2225f,   0.1575f,   0.0f },
    { 0.54f,     0.89f,     0.63f,     0.0f },
    { 0.316228f, 0.316228f, 0.316228f, 12.8f }
};

static const Material MATERIAL_PEARL = {
    { 0.25f,     0.20725f,  0.20725f,  0.0f },
    { 1.0f,      0.829f,    0.829f,    0.0f },
    { 0.296648f, 0.296648f, 0.296648f, 11.264f }
};

static const Material MATERIAL_CHROME = {
    { 0.25f,     0.25f,     0.25f,     0.0f },
    { 0.4f,      0.4f,      0.4f,      0.0f },
    { 0.774597f, 0.774597f, 0.774597f, 76.8f }
};

/* ── Scene object definition ─────────────────────────────────────────── */
/* Each object pairs a material with a world-space position.  The same
 * model (Suzanne) is drawn once per object, each time with different
 * material uniforms — this is the core concept of the lesson. */

typedef struct SceneObject {
    const Material *material;
    vec3            position;
    const char     *name;  /* for logging — helps identify each head */
} SceneObject;

/* Compute the X position of each object so they're centered around X=0. */
#define OBJ_X(i) (((float)(i) - (float)(NUM_OBJECTS - 1) * 0.5f) * OBJECT_SPACING)

static const SceneObject scene_objects[NUM_OBJECTS] = {
    { &MATERIAL_GOLD,        { OBJ_X(0), SCENE_Y_OFFSET, 0.0f }, "Gold"        },
    { &MATERIAL_RED_PLASTIC, { OBJ_X(1), SCENE_Y_OFFSET, 0.0f }, "Red Plastic" },
    { &MATERIAL_JADE,        { OBJ_X(2), SCENE_Y_OFFSET, 0.0f }, "Jade"        },
    { &MATERIAL_PEARL,       { OBJ_X(3), SCENE_Y_OFFSET, 0.0f }, "Pearl"       },
    { &MATERIAL_CHROME,      { OBJ_X(4), SCENE_Y_OFFSET, 0.0f }, "Chrome"      },
};

/* ── Uniform structures (must match HLSL cbuffer layouts exactly) ────── */

/* Vertex uniforms: MVP + model matrix (128 bytes). */
typedef struct VertUniforms {
    mat4 mvp;                   /* 64 bytes */
    mat4 model;                 /* 64 bytes */
} VertUniforms;

/* Fragment uniforms: material + lighting (96 bytes).
 * Layout matches material.frag.hlsl cbuffer:
 *   float4 mat_ambient     (16)
 *   float4 mat_diffuse     (16)
 *   float4 mat_specular    (16)  — rgb + shininess in w
 *   float4 light_dir       (16)
 *   float4 eye_pos         (16)
 *   uint   has_texture      (4)
 *   float3 _pad            (12)
 *   Total: 96 bytes */
typedef struct FragUniforms {
    float mat_ambient[4];
    float mat_diffuse[4];
    float mat_specular[4];
    float light_dir[4];
    float eye_pos[4];
    Uint32 has_texture;
    float _pad[3];
} FragUniforms;

/* Grid fragment uniforms (same as Lessons 12–17). */
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

/* ── GPU-side per-primitive data ─────────────────────────────────────── */

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
    SDL_GPUTexture *texture;
    bool           has_texture;
} GpuMaterial;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Scene pipeline (material Blinn-Phong) */
    SDL_GPUGraphicsPipeline *scene_pipeline;

    /* Grid floor pipeline */
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUBuffer           *grid_vertex_buffer;
    SDL_GPUBuffer           *grid_index_buffer;

    /* Texture sampler (trilinear + repeat) */
    SDL_GPUSampler *sampler;

    /* 1x1 white placeholder for untextured materials */
    SDL_GPUTexture *white_texture;

    /* Loaded scene data (CPU side) */
    ForgeGltfScene scene;

    /* Uploaded GPU buffers (one per primitive) */
    GpuPrimitive   gpu_primitives[FORGE_GLTF_MAX_PRIMITIVES];
    int            gpu_primitive_count;

    /* Uploaded GPU materials */
    GpuMaterial    gpu_materials[FORGE_GLTF_MAX_MATERIALS];
    int            gpu_material_count;

    /* Loaded textures (for cleanup — avoids double-free) */
    SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
    int            loaded_texture_count;

    /* Depth buffer (recreated on resize) */
    SDL_GPUTexture *depth_texture;
    Uint32          depth_width;
    Uint32          depth_height;

    /* Camera state */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

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

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = data_size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &tbci);
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
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("SDL_BeginGPUCopyPass (texture) failed: %s", SDL_GetError());
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
    dst.w = w;
    dst.h = h;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

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

/* ── Upload parsed glTF scene to GPU ─────────────────────────────────── */

static bool upload_scene_to_gpu(SDL_GPUDevice *device, app_state *state)
{
    ForgeGltfScene *scene = &state->scene;
    int i;

    /* ── Upload vertex + index buffers per primitive ──────────────── */
    for (i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *prim = &scene->primitives[i];
        GpuPrimitive *gpu = &state->gpu_primitives[i];

        gpu->material_index = prim->material_index;
        gpu->has_uvs = prim->has_uvs;
        gpu->index_count = (Uint32)prim->index_count;
        gpu->index_type = (prim->index_stride == 4)
            ? SDL_GPU_INDEXELEMENTSIZE_32BIT
            : SDL_GPU_INDEXELEMENTSIZE_16BIT;

        /* Vertex buffer */
        Uint32 vb_size = (Uint32)(prim->vertex_count * sizeof(ForgeGltfVertex));
        gpu->vertex_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_VERTEX,
            prim->vertices, vb_size);
        if (!gpu->vertex_buffer) {
            SDL_Log("Failed to upload vertex buffer for primitive %d", i);
            state->gpu_primitive_count = i;
            return false;
        }

        /* Index buffer */
        Uint32 ib_size = (Uint32)(prim->index_count * prim->index_stride);
        gpu->index_buffer = upload_gpu_buffer(
            device, SDL_GPU_BUFFERUSAGE_INDEX,
            prim->indices, ib_size);
        if (!gpu->index_buffer) {
            SDL_Log("Failed to upload index buffer for primitive %d", i);
            SDL_ReleaseGPUBuffer(device, gpu->vertex_buffer);
            gpu->vertex_buffer = NULL;
            state->gpu_primitive_count = i;
            return false;
        }

        state->gpu_primitive_count = i + 1;
    }

    /* ── Load material textures with deduplication ────────────────── */
    const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
    SDL_GPUTexture *loaded_tex[FORGE_GLTF_MAX_IMAGES];
    int loaded_count = 0;

    for (i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &state->gpu_materials[i];

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
                if (SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
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
                    state->loaded_textures[state->loaded_texture_count++] =
                        dst->texture;
                    loaded_count++;
                } else {
                    dst->has_texture = false;
                }
            }
        }
    }
    state->gpu_material_count = scene->material_count;

    return true;
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

    /* ── 7. Load Suzanne glTF model ─────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            SDL_free(state);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
        char gltf_path[PATH_BUFFER_SIZE];
        SDL_snprintf(gltf_path, sizeof(gltf_path), "%s%s", base, GLTF_PATH);

        if (!forge_gltf_load(gltf_path, &state->scene)) {
            SDL_Log("Failed to load glTF: %s", gltf_path);
            SDL_free(state);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }

        SDL_Log("Loaded glTF: %d nodes, %d meshes, %d primitives, "
                "%d materials",
                state->scene.node_count, state->scene.mesh_count,
                state->scene.primitive_count, state->scene.material_count);
    }

    /* ── 8. Create shaders ──────────────────────────────────────────── */
    SDL_GPUShader *scene_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        material_vert_spirv, sizeof(material_vert_spirv),
        material_vert_dxil, sizeof(material_vert_dxil),
        VS_NUM_SAMPLERS, VS_NUM_STORAGE_TEXTURES,
        VS_NUM_STORAGE_BUFFERS, VS_NUM_UNIFORM_BUFFERS);
    if (!scene_vs) goto fail;

    SDL_GPUShader *scene_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        material_frag_spirv, sizeof(material_frag_spirv),
        material_frag_dxil, sizeof(material_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_STORAGE_TEXTURES,
        FS_NUM_STORAGE_BUFFERS, FS_NUM_UNIFORM_BUFFERS);
    if (!scene_fs) { SDL_ReleaseGPUShader(device, scene_vs); goto fail; }

    /* ── 9. Define vertex layout ────────────────────────────────────── */
    /* ForgeGltfVertex: position (float3), normal (float3), uv (float2). */
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

        /* ── 10. Create scene pipeline ──────────────────────────────── */
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

        /* Back-face culling — Suzanne faces are counter-clockwise. */
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

    /* ── 11. Create grid pipeline ───────────────────────────────────── */
    {
        SDL_GPUShader *grid_vs = create_shader(
            device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_vert_spirv, sizeof(grid_vert_spirv),
            grid_vert_dxil, sizeof(grid_vert_dxil),
            GRID_VS_NUM_SAMPLERS, GRID_VS_NUM_STORAGE_TEXTURES,
            GRID_VS_NUM_STORAGE_BUFFERS, GRID_VS_NUM_UNIFORM_BUFFERS);
        if (!grid_vs) goto fail;

        SDL_GPUShader *grid_fs = create_shader(
            device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_frag_spirv, sizeof(grid_frag_spirv),
            grid_frag_dxil, sizeof(grid_frag_dxil),
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

    /* ── 12. Upload grid geometry ───────────────────────────────────── */
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

    /* ── 13. Create sampler ─────────────────────────────────────────── */
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

    /* ── 14. Create placeholder textures ─────────────────────────────── */
    state->white_texture = create_1x1_texture(device, 255, 255, 255, 255);
    if (!state->white_texture) {
        SDL_Log("Failed to create white placeholder texture");
        goto fail;
    }

    /* ── 15. Upload scene to GPU ────────────────────────────────────── */
    if (!upload_scene_to_gpu(device, state)) {
        SDL_Log("Failed to upload scene to GPU");
        goto fail;
    }

    /* ── 16. Create depth texture ───────────────────────────────────── */
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

    /* ── 17. Camera initial state ───────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_START_YAW * FORGE_DEG2RAD;
    state->cam_pitch = CAM_START_PITCH * FORGE_DEG2RAD;
    state->last_ticks = SDL_GetPerformanceCounter();
    state->mouse_captured = false;

    /* ── 18. Capture mouse ──────────────────────────────────────────── */
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

    /* Log the material lineup for quick reference. */
    {
        int i;
        SDL_Log("Materials:");
        for (i = 0; i < NUM_OBJECTS; i++) {
            SDL_Log("  %d. %s (shininess=%.1f)",
                    i + 1, scene_objects[i].name,
                    scene_objects[i].material->specular[3]);
        }
    }
    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");

    return SDL_APP_CONTINUE;

fail:
    /* Centralised cleanup on init failure. */
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
    {
        int ci;
        for (ci = 0; ci < state->loaded_texture_count; ci++)
            SDL_ReleaseGPUTexture(device, state->loaded_textures[ci]);
        for (ci = 0; ci < state->gpu_primitive_count; ci++) {
            if (state->gpu_primitives[ci].vertex_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[ci].vertex_buffer);
            if (state->gpu_primitives[ci].index_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[ci].index_buffer);
        }
    }
    forge_gltf_free(&state->scene);
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

        /* Fragment uniform: grid appearance + lighting */
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
    /* This is the core of the lesson: the same model is drawn once per
     * scene object, each time with different material uniforms.  The
     * model matrix translates each copy to its position. */
    {
        int obj_i;

        SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);

        for (obj_i = 0; obj_i < NUM_OBJECTS; obj_i++) {
            const SceneObject *obj = &scene_objects[obj_i];
            const Material *mat = obj->material;

            /* The model matrix positions each Suzanne head.  We compose
             * with the glTF node's world_transform in case the model has
             * non-identity transforms (Suzanne's node is at the origin,
             * but this pattern works for any model). */
            int ni;
            for (ni = 0; ni < state->scene.node_count; ni++) {
                const ForgeGltfNode *node = &state->scene.nodes[ni];
                if (node->mesh_index < 0) continue;

                /* Model matrix: place at object position, then apply the
                 * node's own transform (rotation, scale, etc.) */
                mat4 translate = mat4_translate(obj->position);
                mat4 model = mat4_multiply(translate, node->world_transform);
                mat4 mvp = mat4_multiply(vp, model);

                /* Push vertex uniforms: MVP + model matrix. */
                VertUniforms vu;
                vu.mvp = mvp;
                vu.model = model;
                SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

                /* Push fragment uniforms: material + lighting.
                 * The material colors are copied directly from the
                 * predefined Material struct — this is where each object
                 * gets its distinct appearance. */
                FragUniforms fu;
                SDL_memcpy(fu.mat_ambient,  mat->ambient,  sizeof(fu.mat_ambient));
                SDL_memcpy(fu.mat_diffuse,  mat->diffuse,  sizeof(fu.mat_diffuse));
                SDL_memcpy(fu.mat_specular, mat->specular, sizeof(fu.mat_specular));
                fu.light_dir[0] = light_dir.x;
                fu.light_dir[1] = light_dir.y;
                fu.light_dir[2] = light_dir.z;
                fu.light_dir[3] = 0.0f;
                fu.eye_pos[0]   = state->cam_position.x;
                fu.eye_pos[1]   = state->cam_position.y;
                fu.eye_pos[2]   = state->cam_position.z;
                fu.eye_pos[3]   = 0.0f;
                fu.has_texture  = 0; /* Use material colors, not texture */
                fu._pad[0]      = 0.0f;
                fu._pad[1]      = 0.0f;
                fu._pad[2]      = 0.0f;
                SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

                /* Bind the white placeholder texture — the shader still
                 * requires a valid texture binding even when has_texture=0. */
                SDL_GPUTextureSamplerBinding tsb;
                SDL_zero(tsb);
                tsb.texture = state->white_texture;
                tsb.sampler = state->sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);

                /* Draw all primitives of this mesh. */
                const ForgeGltfMesh *mesh =
                    &state->scene.meshes[node->mesh_index];
                int pi;
                for (pi = 0; pi < mesh->primitive_count; pi++) {
                    int gi = mesh->first_primitive + pi;
                    const GpuPrimitive *prim = &state->gpu_primitives[gi];

                    if (!prim->vertex_buffer || !prim->index_buffer) continue;

                    SDL_GPUBufferBinding vbb;
                    SDL_zero(vbb);
                    vbb.buffer = prim->vertex_buffer;
                    SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);

                    SDL_GPUBufferBinding ibb;
                    SDL_zero(ibb);
                    ibb.buffer = prim->index_buffer;
                    SDL_BindGPUIndexBuffer(pass, &ibb, prim->index_type);

                    SDL_DrawGPUIndexedPrimitives(
                        pass, prim->index_count, 1, 0, 0, 0);
                }
            }
        }
    }

    /* ── End render pass ─────────────────────────────────────────────── */
    SDL_EndGPURenderPass(pass);

#ifdef FORGE_CAPTURE
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

    /* Release in reverse order of creation. */
    {
        int i;
        for (i = 0; i < state->gpu_primitive_count; i++) {
            if (state->gpu_primitives[i].vertex_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[i].vertex_buffer);
            if (state->gpu_primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device,
                    state->gpu_primitives[i].index_buffer);
        }
        for (i = 0; i < state->loaded_texture_count; i++)
            SDL_ReleaseGPUTexture(device, state->loaded_textures[i]);
    }

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

    forge_gltf_free(&state->scene);

    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
}
