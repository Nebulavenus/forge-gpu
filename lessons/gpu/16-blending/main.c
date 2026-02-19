/*
 * Lesson 16 — Blending
 *
 * Teach alpha blending, alpha testing, and blend state configuration
 * in SDL GPU by loading the Khronos TransmissionOrderTest glTF model.
 * The model arranges objects in a 3×3 grid — each row uses a different
 * alpha mode (OPAQUE, MASK, BLEND) so the reader can directly compare:
 *
 *   Opaque      — standard depth-tested rendering, alpha channel ignored
 *   Alpha Test  — clip() discards fragments below a threshold (binary)
 *   Alpha Blend — smooth transparency via SrcAlpha / OneMinusSrcAlpha
 *
 * The model also includes blue glass boxes that use KHR_materials_transmission,
 * which our parser approximates as standard alpha blending.
 *
 * The lesson demonstrates:
 *   - SDL_GPUColorTargetBlendState configuration
 *   - Blend equations and factors (what each setting does)
 *   - Why alpha blending requires back-to-front sorting
 *   - Why alpha testing does NOT require sorting
 *   - Why transparent objects disable depth writes
 *   - The three glTF alpha modes: OPAQUE, MASK, BLEND
 *   - Loading a multi-material glTF with different blend pipelines
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain      (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline              (Lesson 02)
 *   - Push uniforms for per-primitive color and MVP           (Lesson 03)
 *   - Texture + sampler binding                               (Lesson 04)
 *   - Depth buffer, window resize                             (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time         (Lesson 07)
 *   - glTF loading with multi-material rendering              (Lesson 09)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   Escape             — release mouse / quit
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

#include "shaders/scene_vert_dxil.h"
#include "shaders/scene_vert_spirv.h"
#include "shaders/scene_frag_dxil.h"
#include "shaders/scene_frag_spirv.h"
#include "shaders/alpha_test_frag_dxil.h"
#include "shaders/alpha_test_frag_spirv.h"
#include "shaders/grid_vert_dxil.h"
#include "shaders/grid_vert_spirv.h"
#include "shaders/grid_frag_dxil.h"
#include "shaders/grid_frag_spirv.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_TITLE "Forge GPU - 16 Blending"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

/* Dark blue background — matches the grid scenes in Lessons 12–15. */
#define CLEAR_R 0.0099f
#define CLEAR_G 0.0099f
#define CLEAR_B 0.0267f
#define CLEAR_A 1.0f

/* Depth buffer */
#define DEPTH_CLEAR 1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Texture constants */
#define BYTES_PER_PIXEL  4
#define WHITE_TEX_DIM    1
#define MAX_LOD          1000.0f

/* Path to the glTF model (relative to executable). */
#define GLTF_PATH "assets/TransmissionOrderTest.gltf"
#define PATH_BUFFER_SIZE 512

/* The model's lowest geometry sits at about y = -0.85.  Shift the entire
 * scene upward so it rests on the grid floor (y = 0) with a small gap. */
#define SCENE_Y_OFFSET 0.9f

/* ── Camera parameters ───────────────────────────────────────────────── */

/* Position the camera to see the full 3×3 grid of the model.
 * After the Y offset the model spans roughly x=[-3.5, 3.5],
 * y=[0.05, 4.4], z=[-0.5, 2].  Camera is placed in front. */
#define CAM_START_X    0.0f
#define CAM_START_Y    2.1f
#define CAM_START_Z    5.5f
#define CAM_START_YAW  0.0f
#define CAM_START_PITCH 0.0f

#define MOVE_SPEED       5.0f
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG    89.0f

#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

#define MS_TO_SEC     1000.0f
#define MAX_DELTA_TIME 0.1f

/* ── Grid floor parameters ───────────────────────────────────────────── */

#define GRID_HALF_SIZE   50.0f
#define GRID_NUM_VERTS    4
#define GRID_NUM_INDICES  6
#define GRID_VERTEX_PITCH 12   /* 3 floats * 4 bytes */

/* Blue grid lines on a dark background (linear sRGB, same as Lesson 12) */
#define GRID_LINE_R       0.068f
#define GRID_LINE_G       0.534f
#define GRID_LINE_B       0.932f
#define GRID_LINE_A       1.0f

#define GRID_BG_R         0.014f
#define GRID_BG_G         0.014f
#define GRID_BG_B         0.045f
#define GRID_BG_A         1.0f

#define GRID_SPACING      1.0f   /* world units between grid lines   */
#define GRID_LINE_WIDTH   0.02f  /* line thickness in grid-space      */
#define GRID_FADE_DIST    40.0f  /* distance at which grid fades out  */

/* Grid shader resource counts (no samplers, 1 uniform each) */
#define GRID_VS_NUM_SAMPLERS         0
#define GRID_VS_NUM_STORAGE_TEXTURES 0
#define GRID_VS_NUM_STORAGE_BUFFERS  0
#define GRID_VS_NUM_UNIFORM_BUFFERS  1

#define GRID_FS_NUM_SAMPLERS         0
#define GRID_FS_NUM_STORAGE_TEXTURES 0
#define GRID_FS_NUM_STORAGE_BUFFERS  0
#define GRID_FS_NUM_UNIFORM_BUFFERS  1

/* ── Shader resource counts ──────────────────────────────────────────── */

/* Scene vertex shader: 0 samplers, 0 storage, 1 uniform (MVP) */
#define VS_NUM_SAMPLERS         0
#define VS_NUM_STORAGE_TEXTURES 0
#define VS_NUM_STORAGE_BUFFERS  0
#define VS_NUM_UNIFORM_BUFFERS  1

/* Scene fragment shaders: 1 sampler (diffuse), 0 storage, 1 uniform */
#define FS_NUM_SAMPLERS         1
#define FS_NUM_STORAGE_TEXTURES 0
#define FS_NUM_STORAGE_BUFFERS  0
#define FS_NUM_UNIFORM_BUFFERS  1

/* ── Blinn-Phong lighting parameters ─────────────────────────────────── */

/* Directional light from upper-right-front, same style as Lessons 10–15. */
#define LIGHT_DIR_X       0.3f
#define LIGHT_DIR_Y       0.8f
#define LIGHT_DIR_Z       0.5f

#define AMBIENT_INTENSITY  0.15f
#define SPECULAR_STRENGTH  0.4f
#define SHININESS          32.0f

/* ── Uniform structures (match HLSL cbuffers) ────────────────────────── */

typedef struct VertUniforms {
    mat4 mvp;                   /* 64 bytes */
    mat4 model;                 /* 64 bytes — total: 128 bytes */
} VertUniforms;

typedef struct FragUniforms {
    float base_color[4];        /* 16 bytes — RGBA multiplier              */
    float light_dir[4];         /* 16 bytes — world-space light direction  */
    float eye_pos[4];           /* 16 bytes — world-space camera position  */
    float alpha_cutoff;         /*  4 bytes — MASK discard threshold       */
    float has_texture;          /*  4 bytes — 1.0 = sample, 0.0 = skip    */
    float shininess;            /*  4 bytes — specular exponent            */
    float ambient;              /*  4 bytes — ambient intensity [0..1]     */
    float specular_str;         /*  4 bytes — specular intensity [0..1]    */
    float _pad0;
    float _pad1;
    float _pad2;                /* total: 80 bytes                         */
} FragUniforms;

/* ── Grid fragment uniforms (match grid.frag.hlsl cbuffer) ────────────── */

typedef struct GridFragUniforms {
    float line_color[4];    /* 16 bytes */
    float bg_color[4];      /* 16 bytes */
    float light_dir[4];     /* 16 bytes — world-space light direction  */
    float eye_pos[4];       /* 16 bytes — world-space camera position  */
    float grid_spacing;     /*  4 bytes */
    float line_width;       /*  4 bytes */
    float fade_distance;    /*  4 bytes */
    float ambient;          /*  4 bytes — ambient intensity [0..1]     */
    float shininess;        /*  4 bytes — specular exponent            */
    float specular_str;     /*  4 bytes — specular intensity [0..1]    */
    float _pad0;            /*  4 bytes */
    float _pad1;            /*  4 bytes — total: 96 bytes */
} GridFragUniforms;

/* ── GPU-side per-primitive data ─────────────────────────────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    Uint32         index_count;
    int            material_index;
    SDL_GPUIndexElementSize index_type;
    bool           has_uvs;
    vec3           aabb_min;    /* mesh-local bounding box (for sort)  */
    vec3           aabb_max;
} GpuPrimitive;

typedef struct GpuMaterial {
    float          base_color[4];
    SDL_GPUTexture *texture;    /* NULL = use white placeholder */
    bool           has_texture;
    ForgeGltfAlphaMode alpha_mode;
    float          alpha_cutoff;
    bool           double_sided;
} GpuMaterial;

/* ── Sortable draw for back-to-front transparency ────────────────────── */

typedef struct BlendDraw {
    int   node_index;
    int   prim_index;       /* global index into gpu_primitives */
    float dist_to_cam;
} BlendDraw;

/* Maximum transparent draws per frame. */
#define MAX_BLEND_DRAWS 128

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Three pipelines: one per alpha mode. */
    SDL_GPUGraphicsPipeline *opaque_pipeline;
    SDL_GPUGraphicsPipeline *alpha_test_pipeline;
    SDL_GPUGraphicsPipeline *blend_pipeline;

    /* Grid floor (procedural anti-aliased grid from Lesson 12) */
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUBuffer           *grid_vertex_buffer;
    SDL_GPUBuffer           *grid_index_buffer;

    /* Texture sampler (trilinear + repeat) */
    SDL_GPUSampler *sampler;

    /* Placeholder 1×1 white texture for untextured materials. */
    SDL_GPUTexture *white_texture;

    /* Loaded scene data (CPU side) */
    ForgeGltfScene scene;

    /* Uploaded GPU buffers (one per primitive) */
    GpuPrimitive   gpu_primitives[FORGE_GLTF_MAX_PRIMITIVES];
    int            gpu_primitive_count;

    /* Uploaded GPU materials */
    GpuMaterial    gpu_materials[FORGE_GLTF_MAX_MATERIALS];
    int            gpu_material_count;

    /* Loaded textures (for cleanup) */
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

/* ══════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ══════════════════════════════════════════════════════════════════════ */

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
        SDL_Log("No supported shader format");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("SDL_CreateGPUShader failed: %s", SDL_GetError());
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

/* ── Load an image file to a GPU texture with mipmaps ────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        /* Try SDL_LoadSurface for PNG support. */
        surface = SDL_LoadSurface(path);
    }
    if (!surface) {
        SDL_Log("Failed to load texture %s: %s", path, SDL_GetError());
        return NULL;
    }

    /* Convert to RGBA8 for consistent GPU upload. */
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
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.width = w;
    tci.height = h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = mip_count;
    /* SAMPLER for fragment shader access, COLOR_TARGET is required by
     * SDL_GenerateMipmapsForGPUTexture to blit between mip levels. */
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
              | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tci);
    if (!texture) {
        SDL_Log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        SDL_DestroySurface(rgba);
        return NULL;
    }

    /* Upload mip 0. */
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

    /* Generate remaining mip levels. */
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

/* ── Create a 1×1 white placeholder texture ──────────────────────────── */

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device) {
    SDL_GPUTextureCreateInfo tci;
    SDL_zero(tci);
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.width = WHITE_TEX_DIM;
    tci.height = WHITE_TEX_DIM;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tci);
    if (!tex) {
        SDL_Log("SDL_CreateGPUTexture (white) failed: %s", SDL_GetError());
        return NULL;
    }

    Uint8 white[4] = { 255, 255, 255, 255 };

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(white);
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tb) {
        SDL_Log("SDL_CreateGPUTransferBuffer (white) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *p = SDL_MapGPUTransferBuffer(device, tb, false);
    if (!p) {
        SDL_Log("SDL_MapGPUTransferBuffer (white) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(p, white, sizeof(white));
    SDL_UnmapGPUTransferBuffer(device, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer (white) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = tb;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = WHITE_TEX_DIM;
    dst.h = WHITE_TEX_DIM;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer (white tex): %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, tb);
    return tex;
}

/* ── Upload the parsed glTF scene to GPU buffers and textures ────────── */

static bool upload_scene_to_gpu(SDL_GPUDevice *device, app_state *state) {
    ForgeGltfScene *scene = &state->scene;
    int i;

    /* ── Upload vertex and index buffers per primitive ──────────────── */
    for (i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *prim = &scene->primitives[i];
        GpuPrimitive *gpu = &state->gpu_primitives[i];

        gpu->material_index = prim->material_index;
        gpu->has_uvs = prim->has_uvs;
        gpu->index_count = (Uint32)prim->index_count;
        gpu->index_type = (prim->index_stride == 4)
            ? SDL_GPU_INDEXELEMENTSIZE_32BIT
            : SDL_GPU_INDEXELEMENTSIZE_16BIT;

        /* Compute mesh-local AABB for transparency sorting.
         * The nearest point on the world-space AABB gives a more accurate
         * sort distance than the node center — a 3D box's front face is
         * closer to the camera than its center, so it correctly sorts to
         * draw after interior objects like flat α planes. */
        if (prim->vertex_count > 0 && prim->vertices) {
            Uint32 v;
            gpu->aabb_min = prim->vertices[0].position;
            gpu->aabb_max = prim->vertices[0].position;
            for (v = 1; v < prim->vertex_count; v++) {
                vec3 p = prim->vertices[v].position;
                if (p.x < gpu->aabb_min.x) gpu->aabb_min.x = p.x;
                if (p.y < gpu->aabb_min.y) gpu->aabb_min.y = p.y;
                if (p.z < gpu->aabb_min.z) gpu->aabb_min.z = p.z;
                if (p.x > gpu->aabb_max.x) gpu->aabb_max.x = p.x;
                if (p.y > gpu->aabb_max.y) gpu->aabb_max.y = p.y;
                if (p.z > gpu->aabb_max.z) gpu->aabb_max.z = p.z;
            }
        } else {
            gpu->aabb_min = vec3_create(0.0f, 0.0f, 0.0f);
            gpu->aabb_max = vec3_create(0.0f, 0.0f, 0.0f);
        }

        /* Vertex buffer */
        Uint32 vb_size = (Uint32)(prim->vertex_count
                                  * sizeof(ForgeGltfVertex));
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

    /* ── Load textures with deduplication ───────────────────────────── */
    /* Multiple materials can reference the same image file.  Cache by
     * path so we only load each texture once. */
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
        dst->alpha_mode = src->alpha_mode;
        dst->alpha_cutoff = src->alpha_cutoff;
        dst->double_sided = src->double_sided;

        if (src->has_texture && src->texture_path[0] != '\0') {
            /* Check cache first. */
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
                    /* Track for cleanup. */
                    state->loaded_textures[state->loaded_texture_count++] =
                        dst->texture;
                    loaded_count++;
                } else {
                    SDL_Log("Texture load failed: %s — using base color",
                            src->texture_path);
                    dst->has_texture = false;
                }
            }
        }
    }
    state->gpu_material_count = scene->material_count;

    return true;
}

/* ── Transform mesh-local AABB to world space (Arvo's method) ────────── */
/* Instead of transforming all 8 corners, decompose the matrix into
 * per-axis contributions.  For each output axis i, sum the min/max
 * contributions from each input axis j scaled by matrix[column j][row i].
 * This handles rotation, scale, and translation correctly. */

static void transform_aabb(const mat4 *m, vec3 lmin, vec3 lmax,
                            vec3 *wmin, vec3 *wmax)
{
    float lo[3] = { lmin.x, lmin.y, lmin.z };
    float hi[3] = { lmax.x, lmax.y, lmax.z };
    float out_min[3], out_max[3];
    int i, j;

    for (i = 0; i < 3; i++) {
        out_min[i] = m->m[12 + i];   /* start with translation */
        out_max[i] = m->m[12 + i];
        for (j = 0; j < 3; j++) {
            float e = m->m[j * 4 + i] * lo[j];
            float f = m->m[j * 4 + i] * hi[j];
            if (e < f) {
                out_min[i] += e;
                out_max[i] += f;
            } else {
                out_min[i] += f;
                out_max[i] += e;
            }
        }
    }
    *wmin = vec3_create(out_min[0], out_min[1], out_min[2]);
    *wmax = vec3_create(out_max[0], out_max[1], out_max[2]);
}

/* ── Distance from a point to the nearest face of an AABB ────────────── */
/* Clamping the point to the AABB gives the nearest point ON the box.
 * A 3D box's nearest point is its front face (closest to camera); a flat
 * plane's nearest point equals its center.  Using this for sort distance
 * ensures enclosing objects (whose front face is closer) draw later. */

static float nearest_aabb_dist(vec3 pt, vec3 wmin, vec3 wmax)
{
    float nx = forge_clampf(pt.x, wmin.x, wmax.x);
    float ny = forge_clampf(pt.y, wmin.y, wmax.y);
    float nz = forge_clampf(pt.z, wmin.z, wmax.z);
    vec3 nearest = vec3_create(nx, ny, nz);
    return vec3_length(vec3_sub(nearest, pt));
}

/* ── Back-to-front sort comparison ───────────────────────────────────── */

static int compare_blend_draws(const void *a, const void *b) {
    const BlendDraw *da = (const BlendDraw *)a;
    const BlendDraw *db = (const BlendDraw *)b;
    if (da->dist_to_cam > db->dist_to_cam) return -1;
    if (da->dist_to_cam < db->dist_to_cam) return  1;
    return 0;
}

/* ── Draw one primitive ──────────────────────────────────────────────── */

static void draw_primitive(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const app_state *state,
    const mat4 *vp,
    int node_index,
    int prim_index)
{
    const ForgeGltfNode *node = &state->scene.nodes[node_index];
    const GpuPrimitive *prim = &state->gpu_primitives[prim_index];

    /* Vertex uniforms: MVP + model matrix for world-space lighting. */
    VertUniforms vu;
    vu.model = node->world_transform;
    vu.mvp = mat4_multiply(*vp, vu.model);
    SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

    /* Fragment uniforms: base color + lighting parameters. */
    FragUniforms fu;
    SDL_GPUTexture *tex = state->white_texture;

    if (prim->material_index >= 0 &&
        prim->material_index < state->gpu_material_count) {
        const GpuMaterial *mat = &state->gpu_materials[prim->material_index];
        fu.base_color[0] = mat->base_color[0];
        fu.base_color[1] = mat->base_color[1];
        fu.base_color[2] = mat->base_color[2];
        fu.base_color[3] = mat->base_color[3];
        fu.alpha_cutoff = mat->alpha_cutoff;
        fu.has_texture = (mat->has_texture && mat->texture) ? 1.0f : 0.0f;
        if (mat->texture) tex = mat->texture;
    } else {
        fu.base_color[0] = 1.0f;
        fu.base_color[1] = 1.0f;
        fu.base_color[2] = 1.0f;
        fu.base_color[3] = 1.0f;
        fu.alpha_cutoff = FORGE_GLTF_DEFAULT_ALPHA_CUTOFF;
        fu.has_texture = 0.0f;
    }

    /* Blinn-Phong lighting uniforms (same for every primitive). */
    {
        vec3 light = vec3_normalize(
            vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
        fu.light_dir[0] = light.x;
        fu.light_dir[1] = light.y;
        fu.light_dir[2] = light.z;
        fu.light_dir[3] = 0.0f;
    }
    fu.eye_pos[0] = state->cam_position.x;
    fu.eye_pos[1] = state->cam_position.y;
    fu.eye_pos[2] = state->cam_position.z;
    fu.eye_pos[3] = 0.0f;
    fu.shininess = SHININESS;
    fu.ambient = AMBIENT_INTENSITY;
    fu.specular_str = SPECULAR_STRENGTH;
    fu._pad0 = 0.0f;
    fu._pad1 = 0.0f;
    fu._pad2 = 0.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

    /* Bind texture + sampler. */
    SDL_GPUTextureSamplerBinding tsb;
    SDL_zero(tsb);
    tsb.texture = tex;
    tsb.sampler = state->sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);

    /* Bind vertex + index buffers. */
    SDL_GPUBufferBinding vbb;
    SDL_zero(vbb);
    vbb.buffer = prim->vertex_buffer;
    SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);

    SDL_GPUBufferBinding ibb;
    SDL_zero(ibb);
    ibb.buffer = prim->index_buffer;
    SDL_BindGPUIndexBuffer(pass, &ibb, prim->index_type);

    SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
}

/* ── Determine the alpha mode for a primitive ────────────────────────── */

static ForgeGltfAlphaMode prim_alpha_mode(
    const app_state *state, int prim_index)
{
    int mi = state->gpu_primitives[prim_index].material_index;
    if (mi >= 0 && mi < state->gpu_material_count)
        return state->gpu_materials[mi].alpha_mode;
    return FORGE_GLTF_ALPHA_OPAQUE;
}

/* ══════════════════════════════════════════════════════════════════════
 * SDL Application Callbacks
 * ══════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppInit ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
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

    /* ── 3. Create window ───────────────────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Claim the window for GPU rendering ──────────────────────── */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 5. Set up sRGB swapchain ───────────────────────────────────── */
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

    /* ── 7. Load the glTF model ─────────────────────────────────────── */
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

        /* Raise the scene above the grid floor.  Apply a Y translation to
         * every node's world_transform so the model sits on y = 0. */
        {
            mat4 lift = mat4_translate(
                vec3_create(0.0f, SCENE_Y_OFFSET, 0.0f));
            int ni;
            for (ni = 0; ni < state->scene.node_count; ni++) {
                state->scene.nodes[ni].world_transform = mat4_multiply(
                    lift, state->scene.nodes[ni].world_transform);
            }
        }
    }

    /* ── 8. Create shaders ──────────────────────────────────────────── */
    SDL_GPUShader *scene_vs = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil),
        VS_NUM_SAMPLERS, VS_NUM_STORAGE_TEXTURES,
        VS_NUM_STORAGE_BUFFERS, VS_NUM_UNIFORM_BUFFERS);
    if (!scene_vs) goto fail;

    SDL_GPUShader *scene_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_STORAGE_TEXTURES,
        FS_NUM_STORAGE_BUFFERS, FS_NUM_UNIFORM_BUFFERS);
    if (!scene_fs) { SDL_ReleaseGPUShader(device, scene_vs); goto fail; }

    SDL_GPUShader *alpha_test_fs = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        alpha_test_frag_spirv, sizeof(alpha_test_frag_spirv),
        alpha_test_frag_dxil, sizeof(alpha_test_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_STORAGE_TEXTURES,
        FS_NUM_STORAGE_BUFFERS, FS_NUM_UNIFORM_BUFFERS);
    if (!alpha_test_fs) {
        SDL_ReleaseGPUShader(device, scene_fs);
        SDL_ReleaseGPUShader(device, scene_vs);
        goto fail;
    }

    /* ── 9. Define vertex layout for ForgeGltfVertex ────────────────── */
    SDL_GPUVertexBufferDescription vb_desc;
    SDL_zero(vb_desc);
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(ForgeGltfVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute attrs[3];
    SDL_zero(attrs);

    /* Location 0: position (float3) */
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = offsetof(ForgeGltfVertex, position);

    /* Location 1: normal (float3) */
    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = offsetof(ForgeGltfVertex, normal);

    /* Location 2: uv (float2) */
    attrs[2].location = 2;
    attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset = offsetof(ForgeGltfVertex, uv);

    /* ── 10. Create OPAQUE pipeline ─────────────────────────────────── */
    /* Standard depth-tested rendering.  No blend state — fragments
     * replace whatever is in the framebuffer.  Back-face culling OFF
     * because some materials in the model are double-sided. */
    {
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
        pipe.vertex_input_state.num_vertex_attributes = 3;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->opaque_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->opaque_pipeline) {
            SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
            SDL_ReleaseGPUShader(device, alpha_test_fs);
            SDL_ReleaseGPUShader(device, scene_fs);
            SDL_ReleaseGPUShader(device, scene_vs);
            goto fail;
        }
    }

    /* ── 11. Create ALPHA TEST (MASK) pipeline ──────────────────────── */
    /* Same as opaque except the fragment shader uses clip() to discard
     * fragments below the alpha cutoff.  Depth write stays ON because
     * surviving fragments are fully opaque.  This is glTF "MASK". */
    {
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = scene_vs;
        pipe.fragment_shader = alpha_test_fs;
        pipe.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = attrs;
        pipe.vertex_input_state.num_vertex_attributes = 3;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->alpha_test_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->alpha_test_pipeline) {
            SDL_Log("Failed to create alpha test pipeline: %s",
                    SDL_GetError());
            SDL_ReleaseGPUShader(device, alpha_test_fs);
            SDL_ReleaseGPUShader(device, scene_fs);
            SDL_ReleaseGPUShader(device, scene_vs);
            goto fail;
        }
    }

    /* ── 12. Create ALPHA BLEND pipeline ────────────────────────────── */
    /* The key differences from opaque:
     *  1. Blend state enabled  — src * SRC_ALPHA + dst * ONE_MINUS_SRC_ALPHA
     *  2. Depth write OFF      — transparent surface must not block what's behind
     *  3. Depth test stays ON  — transparent surfaces occlude behind opaque ones
     *
     * This pipeline is used for both glTF "BLEND" materials and our
     * approximation of KHR_materials_transmission (glass). */
    {
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;
        ctd.blend_state.enable_blend = true;

        /* Color: src * srcAlpha + dst * (1 - srcAlpha) */
        ctd.blend_state.src_color_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ctd.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;

        /* Alpha: src * 1 + dst * (1 - srcAlpha) */
        ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ctd.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        ctd.blend_state.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
            SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = scene_vs;
        pipe.fragment_shader = scene_fs;
        pipe.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = attrs;
        pipe.vertex_input_state.num_vertex_attributes = 3;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        /* CRITICAL: depth write OFF for transparency. */
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = false;
        pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->blend_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->blend_pipeline) {
            SDL_Log("Failed to create blend pipeline: %s", SDL_GetError());
            SDL_ReleaseGPUShader(device, alpha_test_fs);
            SDL_ReleaseGPUShader(device, scene_fs);
            SDL_ReleaseGPUShader(device, scene_vs);
            goto fail;
        }
    }

    /* Shaders are baked into pipelines — safe to release now. */
    SDL_ReleaseGPUShader(device, alpha_test_fs);
    SDL_ReleaseGPUShader(device, scene_fs);
    SDL_ReleaseGPUShader(device, scene_vs);

    /* ── 13. Create GRID pipeline ──────────────────────────────────── */
    /* Position-only vertex format, no samplers, no culling (visible from
     * below), depth write ON so the grid occludes correctly. */
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

    /* ── 14. Upload grid geometry ──────────────────────────────────── */
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

    /* ── 15. Create sampler ─────────────────────────────────────────── */
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

    /* ── 16. Create white placeholder texture ───────────────────────── */
    state->white_texture = create_white_texture(device);
    if (!state->white_texture) {
        SDL_Log("Failed to create white placeholder texture");
        goto fail;
    }

    /* ── 17. Upload scene to GPU ────────────────────────────────────── */
    if (!upload_scene_to_gpu(device, state)) {
        SDL_Log("Failed to upload scene to GPU");
        goto fail;
    }

    /* ── 18. Create depth texture ───────────────────────────────────── */
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

    /* ── 19. Camera initial state ───────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_START_YAW * (FORGE_PI / 180.0f);
    state->cam_pitch = CAM_START_PITCH * (FORGE_PI / 180.0f);
    state->last_ticks = SDL_GetPerformanceCounter();
    state->mouse_captured = false;

    /* ── 20. Capture mouse ──────────────────────────────────────────── */
    if (SDL_SetWindowRelativeMouseMode(window, true)) {
        state->mouse_captured = true;
    }

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            goto fail;
        }
    }
#endif

    *appstate = state;
    SDL_Log("Initialization complete — 4 pipelines, %d textures loaded",
            state->loaded_texture_count);
    return SDL_APP_CONTINUE;

fail:
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->blend_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->blend_pipeline);
    if (state->alpha_test_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->alpha_test_pipeline);
    if (state->opaque_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->opaque_pipeline);
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

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
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
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            float max_pitch = MAX_PITCH_DEG * (FORGE_PI / 180.0f);
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

SDL_AppResult SDL_AppIterate(void *appstate) {
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
        FOV_DEG * (FORGE_PI / 180.0f), aspect, NEAR_PLANE, FAR_PLANE);
    mat4 vp = mat4_multiply(proj, view);

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

    /* ── Render grid floor ───────────────────────────────────────────
     * Draw the procedural grid first.  It writes to the depth buffer,
     * so scene objects that sit on the floor occlude it correctly. */
    {
        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

        /* Vertex uniform: VP matrix (no model — grid is at origin) */
        SDL_PushGPUVertexUniformData(cmd, 0, &vp, sizeof(vp));

        /* Fragment uniform: grid appearance + lighting parameters */
        GridFragUniforms gfu;
        gfu.line_color[0] = GRID_LINE_R;
        gfu.line_color[1] = GRID_LINE_G;
        gfu.line_color[2] = GRID_LINE_B;
        gfu.line_color[3] = GRID_LINE_A;
        gfu.bg_color[0]   = GRID_BG_R;
        gfu.bg_color[1]   = GRID_BG_G;
        gfu.bg_color[2]   = GRID_BG_B;
        gfu.bg_color[3]   = GRID_BG_A;
        {
            vec3 light = vec3_normalize(
                vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
            gfu.light_dir[0] = light.x;
            gfu.light_dir[1] = light.y;
            gfu.light_dir[2] = light.z;
            gfu.light_dir[3] = 0.0f;
        }
        gfu.eye_pos[0]     = state->cam_position.x;
        gfu.eye_pos[1]     = state->cam_position.y;
        gfu.eye_pos[2]     = state->cam_position.z;
        gfu.eye_pos[3]     = 0.0f;
        gfu.grid_spacing   = GRID_SPACING;
        gfu.line_width     = GRID_LINE_WIDTH;
        gfu.fade_distance  = GRID_FADE_DIST;
        gfu.ambient        = AMBIENT_INTENSITY;
        gfu.shininess      = SHININESS;
        gfu.specular_str   = SPECULAR_STRENGTH;
        gfu._pad0          = 0.0f;
        gfu._pad1          = 0.0f;
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

    /* ── Render pass 1: OPAQUE primitives ────────────────────────────
     * Draw all OPAQUE materials first to fill the depth buffer.  Draw
     * order does not matter — depth testing handles occlusion. */
    {
        SDL_BindGPUGraphicsPipeline(pass, state->opaque_pipeline);
        int ni;
        for (ni = 0; ni < state->scene.node_count; ni++) {
            const ForgeGltfNode *node = &state->scene.nodes[ni];
            if (node->mesh_index < 0) continue;

            const ForgeGltfMesh *mesh =
                &state->scene.meshes[node->mesh_index];
            int pi;
            for (pi = 0; pi < mesh->primitive_count; pi++) {
                int gi = mesh->first_primitive + pi;
                if (prim_alpha_mode(state, gi) == FORGE_GLTF_ALPHA_OPAQUE) {
                    draw_primitive(pass, cmd, state, &vp, ni, gi);
                }
            }
        }
    }

    /* ── Render pass 2: ALPHA TEST (MASK) primitives ─────────────────
     * Draw after opaque.  Surviving fragments write depth normally, so
     * draw order does not matter.  No sorting needed. */
    {
        SDL_BindGPUGraphicsPipeline(pass, state->alpha_test_pipeline);
        int ni;
        for (ni = 0; ni < state->scene.node_count; ni++) {
            const ForgeGltfNode *node = &state->scene.nodes[ni];
            if (node->mesh_index < 0) continue;

            const ForgeGltfMesh *mesh =
                &state->scene.meshes[node->mesh_index];
            int pi;
            for (pi = 0; pi < mesh->primitive_count; pi++) {
                int gi = mesh->first_primitive + pi;
                if (prim_alpha_mode(state, gi) == FORGE_GLTF_ALPHA_MASK) {
                    draw_primitive(pass, cmd, state, &vp, ni, gi);
                }
            }
        }
    }

    /* ── Render pass 3: ALPHA BLEND primitives (sorted back-to-front) ─
     * Collect all transparent draws, sort by distance from camera
     * (farthest first = painter's algorithm), then draw in order. */
    {
        BlendDraw draws[MAX_BLEND_DRAWS];
        int draw_count = 0;

        bool draw_limit_hit = false;
        int ni;
        for (ni = 0; ni < state->scene.node_count && !draw_limit_hit; ni++) {
            const ForgeGltfNode *node = &state->scene.nodes[ni];
            if (node->mesh_index < 0) continue;

            const ForgeGltfMesh *mesh =
                &state->scene.meshes[node->mesh_index];
            int pi;
            for (pi = 0; pi < mesh->primitive_count; pi++) {
                int gi = mesh->first_primitive + pi;
                if (prim_alpha_mode(state, gi) != FORGE_GLTF_ALPHA_BLEND)
                    continue;
                if (draw_count >= MAX_BLEND_DRAWS) {
                    SDL_Log("Warning: transparent draw limit reached "
                            "(%d / %d), remaining primitives skipped",
                            draw_count, MAX_BLEND_DRAWS);
                    draw_limit_hit = true;
                    break;
                }

                /* Compute world-space AABB and sort by nearest-point
                 * distance.  Using the AABB's nearest face instead of
                 * the node center handles objects at the same position:
                 * a 3D glass box's front face is closer to the camera
                 * than a flat plane inside it, so the box draws later
                 * and blends on top — making the plane visible through
                 * the glass. */
                {
                    const GpuPrimitive *gprim =
                        &state->gpu_primitives[gi];
                    vec3 w_min, w_max;
                    transform_aabb(&node->world_transform,
                                   gprim->aabb_min, gprim->aabb_max,
                                   &w_min, &w_max);

                    draws[draw_count].node_index = ni;
                    draws[draw_count].prim_index = gi;
                    draws[draw_count].dist_to_cam =
                        nearest_aabb_dist(state->cam_position,
                                          w_min, w_max);
                    draw_count++;
                }
            }
        }

        /* Sort back-to-front: farthest first. */
        if (draw_count > 1) {
            SDL_qsort(draws, (size_t)draw_count, sizeof(BlendDraw),
                       compare_blend_draws);
        }

        SDL_BindGPUGraphicsPipeline(pass, state->blend_pipeline);
        int di;
        for (di = 0; di < draw_count; di++) {
            draw_primitive(pass, cmd, state, &vp,
                           draws[di].node_index, draws[di].prim_index);
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

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
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
    if (state->blend_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->blend_pipeline);
    if (state->alpha_test_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->alpha_test_pipeline);
    if (state->opaque_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->opaque_pipeline);

    forge_gltf_free(&state->scene);

    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
}
