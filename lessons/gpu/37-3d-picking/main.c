/*
 * Lesson 37 — 3D Picking
 *
 * Demonstrates two GPU-based object picking techniques:
 * 1. Color-ID picking — render each object with a unique flat color to an
 *    offscreen target, read back the pixel under the mouse to identify it
 * 2. Stencil-ID picking — write per-object stencil reference values during
 *    the scene pass, read back the stencil byte under the mouse
 *
 * Selected objects are highlighted with a stencil outline using the two-pass
 * silhouette expansion technique from Lesson 34.
 *
 * Key concepts:
 * - GPU-to-CPU data transfer with SDL_DownloadFromGPUTexture
 * - Transfer buffers for readback (DOWNLOAD usage)
 * - Synchronization with SDL_WaitForGPUIdle
 * - Offscreen render targets for picking
 * - Per-object stencil reference values
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include <stdio.h>     /* snprintf for window title */

#include "math/forge_math.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecode ─────────────────────────────────────────── */

#include "shaders/compiled/scene_vert_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_vert_msl.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_frag_dxil.h"
#include "shaders/compiled/scene_frag_msl.h"

#include "shaders/compiled/shadow_vert_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_vert_msl.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"
#include "shaders/compiled/shadow_frag_msl.h"

#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_vert_msl.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"
#include "shaders/compiled/grid_frag_msl.h"

#include "shaders/compiled/outline_frag_spirv.h"
#include "shaders/compiled/outline_frag_dxil.h"
#include "shaders/compiled/outline_frag_msl.h"

#include "shaders/compiled/id_pass_vert_spirv.h"
#include "shaders/compiled/id_pass_vert_dxil.h"
#include "shaders/compiled/id_pass_vert_msl.h"
#include "shaders/compiled/id_pass_frag_spirv.h"
#include "shaders/compiled/id_pass_frag_dxil.h"
#include "shaders/compiled/id_pass_frag_msl.h"

#include "shaders/compiled/crosshair_vert_spirv.h"
#include "shaders/compiled/crosshair_vert_dxil.h"
#include "shaders/compiled/crosshair_vert_msl.h"
#include "shaders/compiled/crosshair_frag_spirv.h"
#include "shaders/compiled/crosshair_frag_dxil.h"
#include "shaders/compiled/crosshair_frag_msl.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH       1280
#define WINDOW_HEIGHT      720
#define SHADOW_MAP_SIZE    2048
#define SHADOW_DEPTH_FMT   SDL_GPU_TEXTUREFORMAT_D32_FLOAT
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

/* Stencil reference for selection outline — high value to avoid
 * conflicting with stencil-ID picking values (1–10) */
#define STENCIL_OUTLINE    200
#define OUTLINE_SCALE      1.04f
#define OUTLINE_COLOR_R    1.0f     /* bright yellow selection outline */
#define OUTLINE_COLOR_G    0.85f
#define OUTLINE_COLOR_B    0.0f
#define PITCH_CLAMP        1.5f     /* max vertical look angle (radians, ~86°) */
#define MAX_DELTA_TIME     0.1f     /* cap delta to prevent huge jumps */

/* Blinn-Phong lighting defaults — shared between scene and outline passes */
#define SCENE_AMBIENT      0.12f    /* ambient light intensity */
#define SCENE_SHININESS    64.0f    /* specular exponent */
#define SCENE_SPECULAR_STR 0.4f     /* specular strength multiplier */

/* Grid floor appearance */
#define GRID_LINE_WIDTH    0.02f    /* line thickness [0..0.5] */
#define GRID_FADE_DISTANCE 40.0f    /* distance where grid fades out */
#define GRID_AMBIENT       0.15f    /* grid ambient intensity */

/* Sphere tessellation resolution */
#define SPHERE_LAT_SEGS    20
#define SPHERE_LON_SEGS    20

/* Maximum geometry buffer sizes */
#define MAX_VERTICES       65536
#define MAX_INDICES         65536

/* Scene object counts and shape types */
#define OBJECT_COUNT       10

/* Crosshair rendering — screen-center reticle for picking feedback */
#define CROSSHAIR_SIZE     0.02f    /* half-length in NDC */
#define CROSSHAIR_THICK    0.002f   /* half-thickness in NDC */
#define CROSSHAIR_VERTS    16       /* 4 quads × 4 verts */
#define CROSSHAIR_INDICES  24       /* 4 quads × 6 indices */

/* ── Shape type enum ──────────────────────────────────────────────────── */

typedef enum ShapeType {
    SHAPE_CUBE,
    SHAPE_SPHERE
} ShapeType;

/* ── Picking method enum ──────────────────────────────────────────────── */

/* Toggle between two GPU-based picking approaches.
 * Color-ID uses an offscreen render target with unique flat colors.
 * Stencil-ID writes per-object reference values into the stencil buffer. */
typedef enum PickingMethod {
    PICK_COLOR_ID,
    PICK_STENCIL_ID
} PickingMethod;

/* ── Vertex layout ────────────────────────────────────────────────────── */

/* Position + normal vertex — used by cubes, spheres, and all scene geometry.
 * UV coordinates are not needed since all objects use solid colors. */
typedef struct Vertex {
    vec3 position;   /* 12 bytes — world-space position */
    vec3 normal;     /* 12 bytes — outward surface normal */
} Vertex;            /* 24 bytes total */

/* Crosshair vertex — screen-space overlay with per-vertex color. */
typedef struct CrosshairVertex {
    vec2 position;    /* NDC coordinates */
    float color[4];   /* RGBA */
} CrosshairVertex;   /* 24 bytes total */

/* ── Uniform structures ───────────────────────────────────────────────── */

/* Vertex uniforms for scene objects: MVP + model + light VP matrices.
 * light_vp contains (lightVP * model) so the shader multiplies by
 * model-space positions to get light-clip coordinates. */
typedef struct SceneVertUniforms {
    mat4 mvp;        /* model-view-projection for clip space     */
    mat4 model;      /* model (world) matrix for lighting        */
    mat4 light_vp;   /* light VP * model for shadow projection   */
} SceneVertUniforms; /* 192 bytes — 3 × 64                      */

/* Fragment uniforms for Blinn-Phong lighting with shadow and tint.
 * The tint field adds color to the ambient term, creating a visually
 * distinct atmosphere when needed. */
typedef struct SceneFragUniforms {
    float base_color[4];    /* RGBA material color                  */
    float eye_pos[3];       /* camera world position                */
    float ambient;          /* ambient light intensity               */
    float light_dir[4];     /* xyz = directional light direction     */
    float light_color[3];   /* RGB light color                       */
    float light_intensity;  /* directional light brightness           */
    float shininess;        /* specular exponent                      */
    float specular_str;     /* specular strength multiplier           */
    float tint[3];          /* additive tint for ambient color        */
    float _pad0;            /* 16-byte alignment padding              */
} SceneFragUniforms;        /* 80 bytes                               */

/* Grid floor vertex uniforms — VP + light VP for shadow mapping. */
typedef struct GridVertUniforms {
    mat4 vp;         /* camera view-projection                  */
    mat4 light_vp;   /* light view-projection for shadow coords */
} GridVertUniforms;  /* 128 bytes                                */

/* Grid floor fragment uniforms — grid pattern + lighting + tint.
 * tint_color multiplies the surface color, allowing different grid
 * appearances in different rendering contexts. */
typedef struct GridFragUniforms {
    float line_color[4];      /* grid line color                    */
    float bg_color[4];        /* background surface color           */
    float light_dir[3];       /* directional light direction        */
    float light_intensity;    /* light brightness                   */
    float eye_pos[3];         /* camera world position              */
    float grid_spacing;       /* world units between grid lines     */
    float line_width;         /* line thickness [0..0.5]            */
    float fade_distance;      /* distance where grid fades out      */
    float ambient;            /* ambient light intensity             */
    float _pad;               /* alignment padding                  */
    float tint_color[4];      /* multiplicative tint (1,1,1 = none) */
} GridFragUniforms;           /* 96 bytes                           */

/* Outline fragment uniforms — solid outline color for selection highlight. */
typedef struct OutlineFragUniforms {
    float outline_color[4];   /* RGBA outline color                 */
} OutlineFragUniforms;        /* 16 bytes                           */

/* ID pass vertex uniforms — only the MVP matrix is needed since the
 * fragment shader outputs a flat color with no lighting. */
typedef struct IdVertUniforms {
    mat4 mvp;                 /* model-view-projection              */
} IdVertUniforms;             /* 64 bytes                           */

/* ID pass fragment uniforms — flat color encoding the object index.
 * Each object gets a unique color that maps back to its array index. */
typedef struct IdFragUniforms {
    float id_color[4];        /* RGBA encoding of object ID         */
} IdFragUniforms;             /* 16 bytes                           */

/* ── Scene object description ─────────────────────────────────────────── */

/* Describes a pickable object in the scene: its shape, placement,
 * and material color.  The name is shown in the window title on selection. */
typedef struct SceneObject {
    const char *name;     /* display name for window title */
    ShapeType   shape;    /* SHAPE_CUBE or SHAPE_SPHERE */
    vec3        position; /* world position */
    float       scale;    /* uniform scale factor */
    float       color[4]; /* RGBA material color */
} SceneObject;

/* ── Application state ────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;   /* OS window handle for rendering            */
    SDL_GPUDevice *device;   /* GPU device for all resource creation       */

    /* Pipelines — each stencil/blend configuration requires its own PSO.
     * scene_pipeline renders without stencil (color-ID picking mode).
     * scene_stencil_pipeline writes per-object stencil refs (stencil-ID mode). */
    SDL_GPUGraphicsPipeline *shadow_pipeline;         /* depth-only shadow map pass         */
    SDL_GPUGraphicsPipeline *scene_pipeline;          /* Blinn-Phong, no stencil            */
    SDL_GPUGraphicsPipeline *scene_stencil_pipeline;  /* Blinn-Phong + stencil REPLACE      */
    SDL_GPUGraphicsPipeline *grid_pipeline;           /* procedural grid floor              */
    SDL_GPUGraphicsPipeline *id_pipeline;             /* flat color-ID offscreen pass       */
    SDL_GPUGraphicsPipeline *crosshair_pipeline;      /* screen-space overlay, no depth     */
    SDL_GPUGraphicsPipeline *outline_write_pipeline;  /* stencil REPLACE (ref=200)          */
    SDL_GPUGraphicsPipeline *outline_draw_pipeline;   /* stencil NOT_EQUAL outline color    */

    /* Render targets */
    SDL_GPUTexture *shadow_depth;       /* D32_FLOAT shadow map              */
    SDL_GPUTexture *main_depth;         /* D24S8 window-sized depth+stencil  */
    SDL_GPUTexture *id_texture;         /* R8G8B8A8 offscreen for color-ID   */
    SDL_GPUTexture *id_depth;           /* D24S8 for ID pass depth testing   */
    SDL_GPUTextureFormat depth_stencil_fmt; /* negotiated DS format          */

    /* Samplers */
    SDL_GPUSampler *nearest_clamp;      /* shadow map sampling               */

    /* Geometry buffers */
    SDL_GPUBuffer *cube_vb;             /* unit cube vertex buffer           */
    SDL_GPUBuffer *cube_ib;             /* unit cube index buffer            */
    SDL_GPUBuffer *sphere_vb;           /* UV sphere vertex buffer           */
    SDL_GPUBuffer *sphere_ib;           /* UV sphere index buffer            */
    SDL_GPUBuffer *grid_vb;             /* ground quad vertex buffer         */
    SDL_GPUBuffer *grid_ib;             /* ground quad index buffer          */
    SDL_GPUBuffer *crosshair_vb;        /* crosshair overlay vertices        */
    SDL_GPUBuffer *crosshair_ib;        /* crosshair overlay indices         */

    /* Index counts for draw calls */
    Uint32 cube_index_count;            /* number of indices in cube mesh    */
    Uint32 sphere_index_count;          /* number of indices in sphere mesh  */

    /* Readback — reusable transfer buffer for GPU-to-CPU pixel download.
     * Sized to hold one RGBA pixel (4 bytes), used each frame a pick
     * is requested. */
    SDL_GPUTransferBuffer *pick_readback;

    /* Scene */
    SceneObject objects[OBJECT_COUNT]; /* pickable objects in the scene     */

    /* Picking state */
    PickingMethod picking_method;      /* COLOR_ID or STENCIL_ID            */
    int  selected_object;     /* -1 = none */
    bool pick_pending;        /* true on click frame */
    int  pick_x, pick_y;     /* mouse position at click time */

    /* Light */
    vec3 light_dir;                  /* normalized directional light direction */
    mat4 light_vp;                   /* light view-projection matrix          */

    SDL_GPUTextureFormat swapchain_format; /* window surface pixel format     */

    /* Camera — first-person with yaw/pitch orientation */
    vec3  cam_position;  /* world-space camera position                    */
    float cam_yaw;       /* horizontal rotation in radians (0 = +Z)       */
    float cam_pitch;     /* vertical rotation in radians (clamped ±PITCH_CLAMP) */

    /* Timing & input */
    Uint64 last_ticks;   /* SDL_GetTicks() value from previous frame      */
    bool   mouse_captured; /* true when mouse is captured for FPS look    */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;                  /* screenshot/GIF capture state  */
#endif
} app_state;

/* ── Helper: create_shader ────────────────────────────────────────────── */

/* Create a GPU shader from pre-compiled SPIRV/DXIL bytecode or MSL source.
 * Automatically selects the correct format based on the GPU backend. */
static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
    const char *msl_code, unsigned int msl_size,
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

    /* Select bytecode format based on GPU backend capabilities */
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil_code;
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
        SDL_Log("ERROR: SDL_CreateGPUShader failed: %s", SDL_GetError());
    }
    return shader;
}

/* ── Helper: upload_gpu_buffer ────────────────────────────────────────── */

/* Upload CPU data to a GPU buffer via a transfer buffer.
 * Returns the created GPU buffer, or NULL on failure. */
static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    const void *data,
    Uint32 size)
{
    /* Create the GPU-side buffer */
    SDL_GPUBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = usage;
    buf_info.size = size;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
    if (!buffer) {
        SDL_Log("ERROR: SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        return NULL;
    }

    /* Create a transfer buffer to stage the data */
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

    /* Map, copy, unmap */
    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("ERROR: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    /* Upload via a copy pass */
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
    src.offset = 0;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.offset = 0;
    dst.size = size;

    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }
    SDL_ReleaseGPUTransferBuffer(device, xfer);

    return buffer;
}

/* ── Geometry: add a box to vertex/index arrays ───────────────────────── */

/* Append a box (axis-aligned) to existing vertex/index arrays.
 * The box is centered at (cx, cy, cz) with half-extents (hx, hy, hz).
 * Each face has 4 vertices with outward normals and 6 indices. */
static void add_box(
    float cx, float cy, float cz,
    float hx, float hy, float hz,
    Vertex *verts, Uint32 *vert_count,
    Uint16 *indices, Uint32 *idx_count)
{
    Uint16 base = (Uint16)*vert_count;
    Uint32 v = *vert_count;
    Uint32 i = *idx_count;

    /* Face data: normal direction and 4 corner offsets */
    const float faces[6][4][3] = {
        /* +Z front face */
        {{ -hx, -hy, hz }, { hx, -hy, hz }, { hx, hy, hz }, { -hx, hy, hz }},
        /* -Z back face */
        {{ hx, -hy, -hz }, { -hx, -hy, -hz }, { -hx, hy, -hz }, { hx, hy, -hz }},
        /* +X right face */
        {{ hx, -hy, hz }, { hx, -hy, -hz }, { hx, hy, -hz }, { hx, hy, hz }},
        /* -X left face */
        {{ -hx, -hy, -hz }, { -hx, -hy, hz }, { -hx, hy, hz }, { -hx, hy, -hz }},
        /* +Y top face */
        {{ -hx, hy, hz }, { hx, hy, hz }, { hx, hy, -hz }, { -hx, hy, -hz }},
        /* -Y bottom face */
        {{ -hx, -hy, -hz }, { hx, -hy, -hz }, { hx, -hy, hz }, { -hx, -hy, hz }},
    };
    const float normals[6][3] = {
        { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 },
        { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 },
    };

    for (int f = 0; f < 6; f++) {
        vec3 n = vec3_create(normals[f][0], normals[f][1], normals[f][2]);
        for (int c = 0; c < 4; c++) {
            verts[v].position = vec3_create(
                cx + faces[f][c][0],
                cy + faces[f][c][1],
                cz + faces[f][c][2]);
            verts[v].normal = n;
            v++;
        }
        /* Two triangles per face: 0-1-2 and 0-2-3 */
        Uint16 fb = base + (Uint16)(f * 4);
        indices[i++] = fb + 0;
        indices[i++] = fb + 1;
        indices[i++] = fb + 2;
        indices[i++] = fb + 0;
        indices[i++] = fb + 2;
        indices[i++] = fb + 3;
    }

    *vert_count = v;
    *idx_count = i;
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
    add_box(0.0f, 0.0f, 0.0f, half_size, half_size, half_size,
            verts, vert_count, indices, idx_count);
}

/* ── Geometry: generate_sphere ────────────────────────────────────────── */

/* Generate a UV sphere with given radius, latitude and longitude segments.
 * Uses standard spherical coordinate parameterization. */
static void generate_sphere(float radius,
                             Vertex *verts, Uint32 *vert_count,
                             Uint16 *indices, Uint32 *idx_count)
{
    Uint32 v = 0;
    Uint32 idx = 0;
    const int lat = SPHERE_LAT_SEGS;
    const int lon = SPHERE_LON_SEGS;

    /* Generate vertices row by row from top pole to bottom pole */
    for (int i = 0; i <= lat; i++) {
        float theta = (float)i * FORGE_PI / (float)lat;
        float sin_t = SDL_sinf(theta);
        float cos_t = SDL_cosf(theta);

        for (int j = 0; j <= lon; j++) {
            float phi = (float)j * 2.0f * FORGE_PI / (float)lon;
            float sin_p = SDL_sinf(phi);
            float cos_p = SDL_cosf(phi);

            /* Position on unit sphere, then scale by radius */
            float x = cos_p * sin_t;
            float y = cos_t;
            float z = sin_p * sin_t;

            verts[v].position = vec3_create(x * radius, y * radius, z * radius);
            /* Normal is the normalized position (unit sphere direction) */
            verts[v].normal = vec3_create(x, y, z);
            v++;
        }
    }

    /* Generate indices — each quad between two latitude rows becomes
     * two triangles.  Skip degenerate triangles at the poles. */
    for (int i = 0; i < lat; i++) {
        for (int j = 0; j < lon; j++) {
            Uint16 a = (Uint16)(i * (lon + 1) + j);
            Uint16 b = (Uint16)(a + (lon + 1));

            /* Skip degenerate triangles at the top pole */
            if (i != 0) {
                indices[idx++] = a;
                indices[idx++] = b;
                indices[idx++] = (Uint16)(a + 1);
            }
            /* Skip degenerate triangles at the bottom pole */
            if (i != lat - 1) {
                indices[idx++] = (Uint16)(a + 1);
                indices[idx++] = b;
                indices[idx++] = (Uint16)(b + 1);
            }
        }
    }

    *vert_count = v;
    *idx_count = idx;
}

/* ── Picking helpers ──────────────────────────────────────────────────── */

/* Convert object index to a unique RGB color for color-ID picking.
 * Index 0 maps to ID 1 (so that background color (0,0,0) means "no object").
 * The ID is packed into R (low 8 bits) and G (high 8 bits), supporting
 * up to 65535 objects. */
static void index_to_color(int index, float *r, float *g, float *b)
{
    int id = index + 1;  /* 1-based so 0 means "nothing" */
    *r = (float)((id >> 0) & 0xFF) / 255.0f;
    *g = (float)((id >> 8) & 0xFF) / 255.0f;
    *b = 0.0f;
}

/* Decode a read-back pixel's RGB values back to an object index.
 * Returns -1 if the pixel is background (no object). */
static int color_to_index(Uint8 r, Uint8 g, Uint8 b)
{
    (void)b;
    int id = (int)r | ((int)g << 8);
    if (id == 0) return -1;
    return id - 1;
}

/* ── End of Chunk A ──────────────────────────────────────────────────── */
/* ── Part B: SDL_AppInit ──────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── 1. SDL init ─────────────────────────────────────────────────── */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. GPU device ───────────────────────────────────────────────── */

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
        true,   /* debug mode */
        NULL);  /* no preferred backend */
    if (!device) {
        SDL_Log("ERROR: SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 3. Window ───────────────────────────────────────────────────── */

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 37 \xe2\x80\x94 3D Picking",
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

    /* ── 4. Allocate app state — set *appstate early so SDL_AppQuit
     *      can free resources even if init fails partway through ──── */

    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    *appstate = state;
    state->window = window;
    state->device = device;
    state->swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 5. Depth-stencil format negotiation ─────────────────────────
     *
     * We need a combined depth+stencil format for both the main depth
     * buffer (stencil outline) and the ID depth buffer.  Prefer D24S8
     * for efficiency; fall back to D32S8 if the driver doesn't support it. */

    SDL_GPUTextureFormat ds_candidates[] = {
        SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
    };
    state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_INVALID;
    for (int i = 0; i < 2; i++) {
        if (SDL_GPUTextureSupportsFormat(
                device, ds_candidates[i],
                SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            state->depth_stencil_fmt = ds_candidates[i];
            SDL_Log("Depth-stencil format: %s",
                    (i == 0) ? "D24_UNORM_S8_UINT" : "D32_FLOAT_S8_UINT");
            break;
        }
    }
    if (state->depth_stencil_fmt == SDL_GPU_TEXTUREFORMAT_INVALID) {
        SDL_Log("ERROR: No depth-stencil format supported");
        return SDL_APP_FAILURE;
    }

    /* ── 6. Texture creation ─────────────────────────────────────────── */

    /* Main depth+stencil — window-sized, used for scene depth testing
     * and stencil outline rendering */
    {
        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type   = SDL_GPU_TEXTURETYPE_2D;
        ci.format = state->depth_stencil_fmt;
        ci.width  = WINDOW_WIDTH;
        ci.height = WINDOW_HEIGHT;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage  = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        state->main_depth = SDL_CreateGPUTexture(device, &ci);
        if (!state->main_depth) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (main_depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Shadow map depth — D32_FLOAT, used for shadow mapping.
     * Needs SAMPLER usage so the scene shader can sample from it. */
    {
        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type   = SDL_GPU_TEXTURETYPE_2D;
        ci.format = SHADOW_DEPTH_FMT;
        ci.width  = SHADOW_MAP_SIZE;
        ci.height = SHADOW_MAP_SIZE;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage  = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
                  | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->shadow_depth = SDL_CreateGPUTexture(device, &ci);
        if (!state->shadow_depth) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (shadow_depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Color-ID offscreen target — R8G8B8A8 so we can encode object IDs
     * as flat colors and read them back.  Verify format support first. */
    {
        if (!SDL_GPUTextureSupportsFormat(
                device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_COLOR_TARGET)) {
            SDL_Log("ERROR: R8G8B8A8_UNORM not supported as COLOR_TARGET");
            return SDL_APP_FAILURE;
        }

        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type   = SDL_GPU_TEXTURETYPE_2D;
        ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ci.width  = WINDOW_WIDTH;
        ci.height = WINDOW_HEIGHT;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage  = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        state->id_texture = SDL_CreateGPUTexture(device, &ci);
        if (!state->id_texture) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (id_texture) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ID pass depth — separate D24S8 buffer for the color-ID render pass.
     * The ID pass needs its own depth testing independent of the main pass. */
    {
        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type   = SDL_GPU_TEXTURETYPE_2D;
        ci.format = state->depth_stencil_fmt;
        ci.width  = WINDOW_WIDTH;
        ci.height = WINDOW_HEIGHT;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage  = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        state->id_depth = SDL_CreateGPUTexture(device, &ci);
        if (!state->id_depth) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (id_depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 7. Transfer buffer for readback ─────────────────────────────
     *
     * A small DOWNLOAD transfer buffer, reused every frame a pick is
     * requested.  Holds exactly one RGBA pixel (4 bytes). */
    {
        SDL_GPUTransferBufferCreateInfo xfer_ci;
        SDL_zero(xfer_ci);
        xfer_ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        xfer_ci.size  = 4;  /* 1 pixel x 4 bytes (RGBA8) */
        state->pick_readback = SDL_CreateGPUTransferBuffer(device, &xfer_ci);
        if (!state->pick_readback) {
            SDL_Log("ERROR: SDL_CreateGPUTransferBuffer (pick_readback) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 8. Samplers ─────────────────────────────────────────────────
     *
     * Nearest-clamp sampler for shadow map — nearest filtering avoids
     * blurring depth comparisons, clamp prevents wrapping artifacts. */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter    = SDL_GPU_FILTER_NEAREST;
        si.mag_filter    = SDL_GPU_FILTER_NEAREST;
        si.mipmap_mode   = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->nearest_clamp = SDL_CreateGPUSampler(device, &si);
        if (!state->nearest_clamp) {
            SDL_Log("ERROR: SDL_CreateGPUSampler failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 9. Shader creation ──────────────────────────────────────────
     *
     * All 11 shaders compiled from HLSL to SPIRV + DXIL + MSL.  Resource
     * counts must match the shader declarations exactly. */

    SDL_GPUShader *scene_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil,  sizeof(scene_vert_dxil),
        scene_vert_msl,   scene_vert_msl_size,
        0, 0, 1);

    SDL_GPUShader *scene_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil,  sizeof(scene_frag_dxil),
        scene_frag_msl,   scene_frag_msl_size,
        1, 0, 1);

    SDL_GPUShader *shadow_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil,  sizeof(shadow_vert_dxil),
        shadow_vert_msl,   shadow_vert_msl_size,
        0, 0, 1);

    SDL_GPUShader *shadow_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl,   shadow_frag_msl_size,
        0, 0, 0);

    SDL_GPUShader *grid_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil,  sizeof(grid_vert_dxil),
        grid_vert_msl,   grid_vert_msl_size,
        0, 0, 1);

    SDL_GPUShader *grid_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil,  sizeof(grid_frag_dxil),
        grid_frag_msl,   grid_frag_msl_size,
        1, 0, 1);

    SDL_GPUShader *outline_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        outline_frag_spirv, sizeof(outline_frag_spirv),
        outline_frag_dxil,  sizeof(outline_frag_dxil),
        outline_frag_msl,   outline_frag_msl_size,
        0, 0, 1);

    SDL_GPUShader *id_pass_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        id_pass_vert_spirv, sizeof(id_pass_vert_spirv),
        id_pass_vert_dxil,  sizeof(id_pass_vert_dxil),
        id_pass_vert_msl,   id_pass_vert_msl_size,
        0, 0, 1);

    SDL_GPUShader *id_pass_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        id_pass_frag_spirv, sizeof(id_pass_frag_spirv),
        id_pass_frag_dxil,  sizeof(id_pass_frag_dxil),
        id_pass_frag_msl,   id_pass_frag_msl_size,
        0, 0, 1);

    SDL_GPUShader *crosshair_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        crosshair_vert_spirv, sizeof(crosshair_vert_spirv),
        crosshair_vert_dxil,  sizeof(crosshair_vert_dxil),
        crosshair_vert_msl,   crosshair_vert_msl_size,
        0, 0, 0);

    SDL_GPUShader *crosshair_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        crosshair_frag_spirv, sizeof(crosshair_frag_spirv),
        crosshair_frag_dxil,  sizeof(crosshair_frag_dxil),
        crosshair_frag_msl,   crosshair_frag_msl_size,
        0, 0, 0);

    /* Verify all shaders loaded successfully.  Release any that succeeded
     * before returning — shaders are local variables, not in app_state,
     * so SDL_AppQuit cannot clean them up. */
    if (!scene_vert || !scene_frag || !shadow_vert || !shadow_frag ||
        !grid_vert || !grid_frag || !outline_frag ||
        !id_pass_vert || !id_pass_frag ||
        !crosshair_vert || !crosshair_frag) {
        SDL_Log("ERROR: One or more shaders failed to create");
        if (scene_vert)     SDL_ReleaseGPUShader(device, scene_vert);
        if (scene_frag)     SDL_ReleaseGPUShader(device, scene_frag);
        if (shadow_vert)    SDL_ReleaseGPUShader(device, shadow_vert);
        if (shadow_frag)    SDL_ReleaseGPUShader(device, shadow_frag);
        if (grid_vert)      SDL_ReleaseGPUShader(device, grid_vert);
        if (grid_frag)      SDL_ReleaseGPUShader(device, grid_frag);
        if (outline_frag)   SDL_ReleaseGPUShader(device, outline_frag);
        if (id_pass_vert)   SDL_ReleaseGPUShader(device, id_pass_vert);
        if (id_pass_frag)   SDL_ReleaseGPUShader(device, id_pass_frag);
        if (crosshair_vert) SDL_ReleaseGPUShader(device, crosshair_vert);
        if (crosshair_frag) SDL_ReleaseGPUShader(device, crosshair_frag);
        return SDL_APP_FAILURE;
    }

    /* ── 10. Vertex input states ─────────────────────────────────────
     *
     * Three different vertex layouts for the different geometry types. */

    /* Scene vertex input: position (vec3) + normal (vec3) = 24 bytes */
    SDL_GPUVertexBufferDescription scene_vbuf_desc;
    SDL_zero(scene_vbuf_desc);
    scene_vbuf_desc.slot  = 0;
    scene_vbuf_desc.pitch = sizeof(Vertex);
    scene_vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    scene_vbuf_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute scene_attrs[2];
    SDL_zero(scene_attrs);
    scene_attrs[0].location = 0;
    scene_attrs[0].buffer_slot = 0;
    scene_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    scene_attrs[0].offset = 0;
    scene_attrs[1].location = 1;
    scene_attrs[1].buffer_slot = 0;
    scene_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    scene_attrs[1].offset = offsetof(Vertex, normal);

    SDL_GPUVertexInputState scene_vis;
    SDL_zero(scene_vis);
    scene_vis.vertex_buffer_descriptions = &scene_vbuf_desc;
    scene_vis.num_vertex_buffers = 1;
    scene_vis.vertex_attributes = scene_attrs;
    scene_vis.num_vertex_attributes = 2;

    /* Grid vertex input: position only (vec3) = 12 bytes */
    SDL_GPUVertexBufferDescription grid_vbuf_desc;
    SDL_zero(grid_vbuf_desc);
    grid_vbuf_desc.slot  = 0;
    grid_vbuf_desc.pitch = sizeof(vec3);
    grid_vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    grid_vbuf_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute grid_attrs[1];
    SDL_zero(grid_attrs);
    grid_attrs[0].location = 0;
    grid_attrs[0].buffer_slot = 0;
    grid_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    grid_attrs[0].offset = 0;

    SDL_GPUVertexInputState grid_vis;
    SDL_zero(grid_vis);
    grid_vis.vertex_buffer_descriptions = &grid_vbuf_desc;
    grid_vis.num_vertex_buffers = 1;
    grid_vis.vertex_attributes = grid_attrs;
    grid_vis.num_vertex_attributes = 1;

    /* Crosshair vertex input: position (vec2) + color (vec4) = 24 bytes */
    SDL_GPUVertexBufferDescription ch_vbuf_desc;
    SDL_zero(ch_vbuf_desc);
    ch_vbuf_desc.slot  = 0;
    ch_vbuf_desc.pitch = sizeof(CrosshairVertex);
    ch_vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    ch_vbuf_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute ch_attrs[2];
    SDL_zero(ch_attrs);
    ch_attrs[0].location = 0;
    ch_attrs[0].buffer_slot = 0;
    ch_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    ch_attrs[0].offset = 0;
    ch_attrs[1].location = 1;
    ch_attrs[1].buffer_slot = 0;
    ch_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    ch_attrs[1].offset = offsetof(CrosshairVertex, color);

    SDL_GPUVertexInputState ch_vis;
    SDL_zero(ch_vis);
    ch_vis.vertex_buffer_descriptions = &ch_vbuf_desc;
    ch_vis.num_vertex_buffers = 1;
    ch_vis.vertex_attributes = ch_attrs;
    ch_vis.num_vertex_attributes = 2;

    /* ── 11. Pipeline creation ───────────────────────────────────────
     *
     * Eight pipelines, each with a specific combination of shaders,
     * depth/stencil state, and blend configuration. */

    /* ── 11a. Shadow pipeline ────────────────────────────────────────
     * Depth-only pass — no color targets, back-face culling.
     * Renders scene geometry into the shadow map. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = shadow_vert;
        pi.fragment_shader = shadow_frag;
        pi.vertex_input_state = scene_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* Depth testing for shadow map generation */
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        /* No color targets — depth-only */
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = SHADOW_DEPTH_FMT;
        pi.target_info.has_depth_stencil_target = true;

        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->shadow_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (shadow) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11b. Scene pipeline (no stencil) ────────────────────────────
     * Standard Blinn-Phong lighting with shadow mapping.
     * Used when picking method is COLOR_ID (no per-object stencil needed). */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = state->swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = scene_vert;
        pi.fragment_shader = scene_frag;
        pi.vertex_input_state = scene_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->scene_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (scene) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11c. Scene stencil pipeline ─────────────────────────────────
     * Same as scene pipeline but writes per-object stencil reference
     * values using ALWAYS/REPLACE.  Used in stencil-ID picking mode —
     * each object sets its own reference value (1-based index), then
     * we read back the stencil byte under the mouse to identify it. */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = state->swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = scene_vert;
        pi.fragment_shader = scene_frag;
        pi.vertex_input_state = scene_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = true;

        /* Stencil: always pass, replace stencil with reference value.
         * The reference value is set per-draw via SDL_SetGPUStencilReference. */
        pi.depth_stencil_state.front_stencil_state.fail_op       = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.pass_op       = SDL_GPU_STENCILOP_REPLACE;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.compare_op    = SDL_GPU_COMPAREOP_ALWAYS;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.write_mask   = 0xFF;
        pi.depth_stencil_state.compare_mask = 0xFF;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->scene_stencil_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->scene_stencil_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (scene_stencil) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11d. Grid pipeline ──────────────────────────────────────────
     * Infinite grid on the XZ plane.  CULL_NONE because the grid quad
     * should be visible from both sides.  LESS_OR_EQUAL depth to coexist
     * with objects resting on y=0. */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = state->swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = grid_vert;
        pi.fragment_shader = grid_frag;
        pi.vertex_input_state = grid_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->grid_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (grid) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11e. Color-ID pipeline ──────────────────────────────────────
     * Renders each object with a unique flat color to an offscreen
     * R8G8B8A8 target.  No lighting, no blending — just a solid
     * per-object color encoding its array index. */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = id_pass_vert;
        pi.fragment_shader = id_pass_frag;
        pi.vertex_input_state = scene_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->id_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->id_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (id) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11f. Crosshair pipeline ─────────────────────────────────────
     * Screen-space overlay — no depth testing, alpha blending so the
     * crosshair is semi-transparent.  CULL_NONE for 2D quads. */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = state->swapchain_format;

        /* Alpha blending: src * srcAlpha + dst * (1 - srcAlpha) */
        ct.blend_state.enable_blend = true;
        ct.blend_state.src_color_blendfactor   = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ct.blend_state.dst_color_blendfactor   = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ct.blend_state.color_blend_op          = SDL_GPU_BLENDOP_ADD;
        ct.blend_state.src_alpha_blendfactor   = SDL_GPU_BLENDFACTOR_ONE;
        ct.blend_state.dst_alpha_blendfactor   = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ct.blend_state.alpha_blend_op          = SDL_GPU_BLENDOP_ADD;
        ct.blend_state.color_write_mask        = SDL_GPU_COLORCOMPONENT_R
                                               | SDL_GPU_COLORCOMPONENT_G
                                               | SDL_GPU_COLORCOMPONENT_B
                                               | SDL_GPU_COLORCOMPONENT_A;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = crosshair_vert;
        pi.fragment_shader = crosshair_frag;
        pi.vertex_input_state = ch_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* No depth testing — crosshair always on top */
        pi.depth_stencil_state.enable_depth_test  = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        /* Must match the main scene render pass which has a depth-stencil
         * attachment, even though the crosshair does not use depth testing. */
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->crosshair_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->crosshair_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (crosshair) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11g. Outline write pipeline ─────────────────────────────────
     * First pass of the two-pass stencil outline technique: render the
     * selected object normally while writing a marker value (STENCIL_OUTLINE)
     * into the stencil buffer. */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = state->swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = scene_vert;
        pi.fragment_shader = scene_frag;
        pi.vertex_input_state = scene_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = true;

        /* Stencil: always pass, replace with STENCIL_OUTLINE.
         * This marks exactly the pixels covered by the selected object. */
        pi.depth_stencil_state.front_stencil_state.fail_op       = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.pass_op       = SDL_GPU_STENCILOP_REPLACE;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.compare_op    = SDL_GPU_COMPAREOP_ALWAYS;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.write_mask   = 0xFF;
        pi.depth_stencil_state.compare_mask = 0xFF;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->outline_write_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->outline_write_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (outline_write) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 11h. Outline draw pipeline ──────────────────────────────────
     * Second pass: render a slightly enlarged version of the selected
     * object with the outline shader.  Only pixels that DON'T have the
     * stencil marker pass (NOT_EQUAL), producing a visible outline
     * around the silhouette edges.  No depth test — the outline should
     * always be visible even if occluded. */
    {
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = state->swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = scene_vert;
        pi.fragment_shader = outline_frag;
        pi.vertex_input_state = scene_vis;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* No depth test — outline draws on top */
        pi.depth_stencil_state.enable_depth_test  = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = true;

        /* Stencil: pass only where stencil != STENCIL_OUTLINE.
         * write_mask = 0 so the outline pass doesn't modify stencil. */
        pi.depth_stencil_state.front_stencil_state.fail_op       = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.pass_op       = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.compare_op    = SDL_GPU_COMPAREOP_NOT_EQUAL;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.write_mask   = 0x00;
        pi.depth_stencil_state.compare_mask = 0xFF;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->outline_draw_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->outline_draw_pipeline) {
            SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline (outline_draw) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 12. Release shaders — pipelines retain their own copies ──── */

    SDL_ReleaseGPUShader(device, scene_vert);
    SDL_ReleaseGPUShader(device, scene_frag);
    SDL_ReleaseGPUShader(device, shadow_vert);
    SDL_ReleaseGPUShader(device, shadow_frag);
    SDL_ReleaseGPUShader(device, grid_vert);
    SDL_ReleaseGPUShader(device, grid_frag);
    SDL_ReleaseGPUShader(device, outline_frag);
    SDL_ReleaseGPUShader(device, id_pass_vert);
    SDL_ReleaseGPUShader(device, id_pass_frag);
    SDL_ReleaseGPUShader(device, crosshair_vert);
    SDL_ReleaseGPUShader(device, crosshair_frag);

    /* ── 13. Geometry generation + upload ─────────────────────────── */

    /* Cube geometry — 24 vertices (4 per face), 36 indices */
    {
        Vertex cube_verts[24];     /* 6 faces × 4 verts */
        Uint16 cube_indices[36];   /* 6 faces × 6 indices */
        Uint32 cube_vc = 0, cube_ic = 0;
        generate_cube(0.5f, cube_verts, &cube_vc, cube_indices, &cube_ic);
        state->cube_index_count = cube_ic;
        state->cube_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
                                           cube_verts, cube_vc * sizeof(Vertex));
        state->cube_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
                                           cube_indices, cube_ic * sizeof(Uint16));
        if (!state->cube_vb || !state->cube_ib) {
            SDL_Log("ERROR: Failed to upload cube geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* Sphere geometry — UV sphere with configurable tessellation.
     * Vertex count = (lat+1)*(lon+1), index count = lat*lon*6 */
    {
        Vertex sphere_verts[(SPHERE_LAT_SEGS + 1) * (SPHERE_LON_SEGS + 1)];
        Uint16 sphere_indices[SPHERE_LAT_SEGS * SPHERE_LON_SEGS * 6];
        Uint32 sphere_vc = 0, sphere_ic = 0;
        generate_sphere(0.5f, sphere_verts, &sphere_vc,
                        sphere_indices, &sphere_ic);
        state->sphere_index_count = sphere_ic;
        state->sphere_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
                                             sphere_verts, sphere_vc * sizeof(Vertex));
        state->sphere_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
                                             sphere_indices, sphere_ic * sizeof(Uint16));
        if (!state->sphere_vb || !state->sphere_ib) {
            SDL_Log("ERROR: Failed to upload sphere geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* Grid geometry — single quad on the XZ plane, spanning the scene */
    {
        vec3 grid_verts[4];
        grid_verts[0] = vec3_create(-GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE);
        grid_verts[1] = vec3_create( GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE);
        grid_verts[2] = vec3_create( GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE);
        grid_verts[3] = vec3_create(-GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE);
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

    /* Crosshair geometry — 4 thin quads forming a + reticle at screen center.
     * A small gap in the center prevents overlap at the exact crosshair point.
     * Coordinates are in NDC (-1..1). */
    {
        CrosshairVertex ch_verts[CROSSHAIR_VERTS];
        Uint16 ch_indices[CROSSHAIR_INDICES];

        float sz   = CROSSHAIR_SIZE;
        float th   = CROSSHAIR_THICK;
        float gap  = CROSSHAIR_THICK * 2.0f;  /* gap between arms and center */
        float col[4] = { 1.0f, 1.0f, 1.0f, 0.8f };

        int v = 0;
        int idx = 0;

        /* Helper macro — set crosshair vertex position + color */
#define CH_VERT(px, py) do { \
    ch_verts[v].position = vec2_create(px, py); \
    ch_verts[v].color[0] = col[0]; \
    ch_verts[v].color[1] = col[1]; \
    ch_verts[v].color[2] = col[2]; \
    ch_verts[v].color[3] = col[3]; \
    v++; \
} while (0)

        /* Quad 0 — left arm: extends from -sz to -gap horizontally */
        CH_VERT(-sz, -th); CH_VERT(-gap, -th); CH_VERT(-gap, th); CH_VERT(-sz, th);
        ch_indices[idx++] = 0; ch_indices[idx++] = 1; ch_indices[idx++] = 2;
        ch_indices[idx++] = 0; ch_indices[idx++] = 2; ch_indices[idx++] = 3;

        /* Quad 1 — right arm: extends from gap to sz horizontally */
        CH_VERT(gap, -th); CH_VERT(sz, -th); CH_VERT(sz, th); CH_VERT(gap, th);
        ch_indices[idx++] = 4; ch_indices[idx++] = 5; ch_indices[idx++] = 6;
        ch_indices[idx++] = 4; ch_indices[idx++] = 6; ch_indices[idx++] = 7;

        /* Quad 2 — top arm: extends from gap to sz vertically */
        CH_VERT(-th, gap); CH_VERT(th, gap); CH_VERT(th, sz); CH_VERT(-th, sz);
        ch_indices[idx++] = 8;  ch_indices[idx++] = 9;  ch_indices[idx++] = 10;
        ch_indices[idx++] = 8;  ch_indices[idx++] = 10; ch_indices[idx++] = 11;

        /* Quad 3 — bottom arm: extends from -sz to -gap vertically */
        CH_VERT(-th, -sz); CH_VERT(th, -sz); CH_VERT(th, -gap); CH_VERT(-th, -gap);

#undef CH_VERT
        ch_indices[idx++] = 12; ch_indices[idx++] = 13; ch_indices[idx++] = 14;
        ch_indices[idx++] = 12; ch_indices[idx++] = 14; ch_indices[idx++] = 15;

        state->crosshair_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
                                                ch_verts, (Uint32)(v * sizeof(CrosshairVertex)));
        state->crosshair_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
                                                ch_indices, (Uint32)(idx * sizeof(Uint16)));
        if (!state->crosshair_vb || !state->crosshair_ib) {
            SDL_Log("ERROR: Failed to upload crosshair geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* ── 14. Scene object initialization ─────────────────────────────
     *
     * 10 objects scattered across the ground plane — a mix of cubes and
     * spheres with distinct colors for visual identification. */
    {
        /* Initialize objects using assignment — MSVC C99 does not support
         * compound literals with nested struct members in array initializers. */
#define OBJ(i, nm, sh, px, py, pz, sc, cr, cg, cb) do { \
    state->objects[i].name     = nm; \
    state->objects[i].shape    = sh; \
    state->objects[i].position = vec3_create(px, py, pz); \
    state->objects[i].scale    = sc; \
    state->objects[i].color[0] = cr; \
    state->objects[i].color[1] = cg; \
    state->objects[i].color[2] = cb; \
    state->objects[i].color[3] = 1.0f; \
} while (0)
        OBJ(0, "Red Cube",      SHAPE_CUBE,   -3.0f, 0.5f, -2.0f, 1.0f, 0.85f, 0.20f, 0.15f);
        OBJ(1, "Blue Cube",     SHAPE_CUBE,    2.0f, 0.5f, -1.0f, 0.8f, 0.15f, 0.40f, 0.85f);
        OBJ(2, "Green Cube",    SHAPE_CUBE,    0.0f, 0.75f,-4.0f, 1.5f, 0.20f, 0.75f, 0.25f);
        OBJ(3, "Yellow Cube",   SHAPE_CUBE,   -1.5f, 0.5f, -5.0f, 0.7f, 0.85f, 0.80f, 0.15f);
        OBJ(4, "Purple Sphere", SHAPE_SPHERE,  3.0f, 0.6f, -3.0f, 0.6f, 0.60f, 0.25f, 0.75f);
        OBJ(5, "Orange Sphere", SHAPE_SPHERE, -2.0f, 0.4f, -3.5f, 0.4f, 0.90f, 0.50f, 0.15f);
        OBJ(6, "Cyan Cube",     SHAPE_CUBE,    1.0f, 1.0f, -2.5f, 0.6f, 0.15f, 0.80f, 0.80f);
        OBJ(7, "Pink Sphere",   SHAPE_SPHERE, -0.5f, 0.5f, -1.5f, 0.5f, 0.90f, 0.40f, 0.60f);
        OBJ(8, "White Cube",    SHAPE_CUBE,    2.5f, 0.4f, -5.0f, 0.8f, 0.85f, 0.85f, 0.85f);
        OBJ(9, "Teal Sphere",   SHAPE_SPHERE, -3.0f, 0.6f, -4.5f, 0.6f, 0.20f, 0.70f, 0.65f);
#undef OBJ
    }

    /* ── 15. Camera & state initialization ───────────────────────────── */

    state->cam_position = vec3_create(0.0f, 2.0f, 3.0f);
    state->cam_yaw   = -0.15f;    /* face roughly toward -Z (objects) */
    state->cam_pitch = -0.25f;    /* look slightly downward at the scene */
    state->last_ticks = SDL_GetTicks();
    state->selected_object = -1;   /* no object selected initially */
    state->picking_method  = PICK_COLOR_ID;
    state->pick_pending    = false;

    /* Light direction — upper-right, slightly behind the scene.
     * Normalized so the Blinn-Phong calculation works correctly. */
    state->light_dir = vec3_normalize(vec3_create(-0.4f, -0.8f, -0.3f));

    /* Light view-projection matrix for shadow mapping.
     * The orthographic projection covers the scene from the light's
     * perspective, centered roughly on the object cluster. */
    {
        vec3 light_pos = vec3_scale(state->light_dir, -30.0f);
        mat4 light_view = mat4_look_at(
            light_pos,
            vec3_create(0.0f, 0.0f, -3.0f),
            vec3_create(0.0f, 1.0f, 0.0f));
        mat4 light_proj = mat4_orthographic(-12.0f, 12.0f, -12.0f, 12.0f, 0.1f, 60.0f);
        state->light_vp = mat4_multiply(light_proj, light_view);
    }

    /* ── 16. Capture initialization ──────────────────────────────────── */

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, state->window)) {
            SDL_Log("Failed to initialise capture");
            return SDL_APP_FAILURE;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    /* Capture the mouse for FPS-style look controls.
     * This hides the cursor and provides relative mouse motion events. */
    if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
        SDL_Log("WARNING: SDL_SetWindowRelativeMouseMode failed: %s",
                SDL_GetError());
    }
    state->mouse_captured = true;

    return SDL_APP_CONTINUE;
}

/* ── End of Chunk B ──────────────────────────────────────────────────── */
/* ── Chunk C: Event handling and render loop ─────────────────────────── */

/* ────────────────────────────────────────────────────────────────────── */
/*  SDL_AppEvent                                                         */
/*                                                                       */
/*  Handles input for camera control, picking, and method toggling.      */
/*  Escape toggles mouse capture (free cursor for clicking vs FPS        */
/*  camera).  Tab switches between color-ID and stencil-ID picking.      */
/*  Left click initiates a pick at the click position (free cursor)      */
/*  or screen center (captured cursor).                                  */
/* ────────────────────────────────────────────────────────────────────── */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        switch (event->key.key) {
        case SDLK_ESCAPE:
            /* Toggle mouse capture — release for clicking objects,
             * recapture for FPS-style camera control */
            if (state->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                    SDL_Log("WARNING: SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                }
                state->mouse_captured = false;
            } else {
                if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
                    SDL_Log("WARNING: SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                }
                state->mouse_captured = true;
            }
            break;

        case SDLK_TAB:
            /* Toggle between color-ID and stencil-ID picking methods.
             * Both produce the same result — the lesson demonstrates two
             * approaches so the reader can compare their trade-offs. */
            state->picking_method = (state->picking_method == PICK_COLOR_ID)
                                  ? PICK_STENCIL_ID : PICK_COLOR_ID;

            /* Update window title to reflect the active method */
            {
                char title[256];
                const char *method = (state->picking_method == PICK_COLOR_ID)
                                   ? "Color-ID" : "Stencil-ID";
                const char *sel = (state->selected_object >= 0)
                                ? state->objects[state->selected_object].name
                                : "None";
                SDL_snprintf(title, sizeof(title),
                    "Lesson 37 — 3D Picking | Method: %s | Selected: %s",
                    method, sel);
                SDL_SetWindowTitle(state->window, title);
            }
            break;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (!state->mouse_captured) {
                /* Free cursor mode — use click position for picking */
                state->pick_pending = true;
                state->pick_x = (int)event->button.x;
                state->pick_y = (int)event->button.y;
            } else {
                /* FPS camera mode — pick at screen center (crosshair) */
                state->pick_pending = true;
                state->pick_x = WINDOW_WIDTH / 2;
                state->pick_y = WINDOW_HEIGHT / 2;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        /* Only rotate camera when mouse is captured (FPS mode) */
        if (state->mouse_captured) {
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;

            /* Clamp pitch to prevent camera flipping at the poles */
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
        }
        break;
    }

    return SDL_APP_CONTINUE;
}

/* ────────────────────────────────────────────────────────────────────── */
/*  SDL_AppIterate                                                       */
/*                                                                       */
/*  The render loop has eight phases:                                     */
/*    1. Timing & camera update                                          */
/*    2. Acquire swapchain + command buffer                               */
/*    3. Shadow pass (depth-only from light's perspective)                */
/*    4. Main scene pass (lit objects + grid + crosshair)                 */
/*    5. ID pass (color-ID: flat-color objects to offscreen texture)      */
/*    6. Copy pass (download the picked pixel to a readback buffer)       */
/*    7. Outline pass (stencil-based selection highlight)                 */
/*    8. Submit + readback decode (resolve which object was picked)       */
/* ────────────────────────────────────────────────────────────────────── */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── Phase 1: Timing & camera ──────────────────────────────────── */

    Uint64 now = SDL_GetTicks();
    float dt = (float)(now - state->last_ticks) / 1000.0f;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;  /* Cap delta for alt-tab / debugger pauses */
    state->last_ticks = now;

    /* Derive camera orientation from yaw/pitch using the math library's
     * quaternion helpers — never hand-roll sin/cos for camera math. */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(cam_orient);
    vec3 right   = quat_right(cam_orient);

    /* WASD + Space/Shift movement — standard FPS controls */
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

    /* Build view and projection matrices using the quaternion view helper */
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    mat4 proj = mat4_perspective(
        FOV_DEG * FORGE_DEG2RAD,
        (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT,
        NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* ── Phase 2: Acquire swapchain + command buffer ───────────────── */

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("ERROR: SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError());
        return SDL_APP_CONTINUE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw_w, sw_h;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                        &swapchain_tex, &sw_w, &sw_h)) {
        SDL_Log("ERROR: SDL_AcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }
    if (!swapchain_tex) {
        /* Window is minimized or otherwise unavailable — skip this frame */
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    /* ── Phase 3: Shadow pass ──────────────────────────────────────── */
    /* Render all objects from the light's perspective into the shadow
     * depth map.  This texture is sampled during the main scene pass
     * to determine which fragments are in shadow. */
    {
        vec3 light_dir = state->light_dir;

        SDL_GPUDepthStencilTargetInfo ds_info;
        SDL_zero(ds_info);
        ds_info.texture     = state->shadow_depth;
        ds_info.clear_depth = 1.0f;
        ds_info.load_op     = SDL_GPU_LOADOP_CLEAR;
        ds_info.store_op    = SDL_GPU_STOREOP_STORE;
        /* SDL_zero sets stencil_load_op to 0 = LOAD, which conflicts
         * with cycle=true.  Set to DONT_CARE since D32_FLOAT has no stencil. */
        ds_info.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
        ds_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        ds_info.cycle            = true;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &ds_info);
        SDL_BindGPUGraphicsPipeline(pass, state->shadow_pipeline);

        for (int i = 0; i < OBJECT_COUNT; i++) {
            /* Each object's model matrix: translate to position, then
             * scale uniformly.  Multiply by light VP to get the
             * light-space MVP for shadow depth rendering. */
            mat4 model = mat4_multiply(
                mat4_translate(state->objects[i].position),
                mat4_scale_uniform(state->objects[i].scale));
            mat4 lmvp = mat4_multiply(state->light_vp, model);
            SDL_PushGPUVertexUniformData(cmd, 0, &lmvp, sizeof(lmvp));

            /* Bind geometry matching this object's shape type */
            SDL_GPUBufferBinding vb = {
                .buffer = (state->objects[i].shape == SHAPE_CUBE)
                        ? state->cube_vb : state->sphere_vb,
                .offset = 0
            };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib = {
                .buffer = (state->objects[i].shape == SHAPE_CUBE)
                        ? state->cube_ib : state->sphere_ib,
                .offset = 0
            };
            SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            Uint32 idx_count = (state->objects[i].shape == SHAPE_CUBE)
                             ? state->cube_index_count
                             : state->sphere_index_count;
            SDL_DrawGPUIndexedPrimitives(pass, idx_count, 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    /* ── Phase 4: Main scene pass ──────────────────────────────────── */
    /* Render the lit scene to the swapchain.  In stencil-ID mode, each
     * object also writes its unique index to the stencil buffer so we
     * can identify it on click without an extra render pass. */
    {
        vec3 light_dir = state->light_dir;

        SDL_GPUColorTargetInfo color_info;
        SDL_zero(color_info);
        color_info.texture  = swapchain_tex;
        color_info.load_op  = SDL_GPU_LOADOP_CLEAR;
        color_info.store_op = SDL_GPU_STOREOP_STORE;
        color_info.clear_color.r = CLEAR_R;
        color_info.clear_color.g = CLEAR_G;
        color_info.clear_color.b = CLEAR_B;
        color_info.clear_color.a = 1.0f;

        SDL_GPUDepthStencilTargetInfo ds_info;
        SDL_zero(ds_info);
        ds_info.texture          = state->main_depth;
        ds_info.clear_depth      = 1.0f;
        ds_info.clear_stencil    = 0;
        ds_info.load_op          = SDL_GPU_LOADOP_CLEAR;
        ds_info.store_op         = SDL_GPU_STOREOP_STORE;
        ds_info.stencil_load_op  = SDL_GPU_LOADOP_CLEAR;
        ds_info.stencil_store_op = SDL_GPU_STOREOP_STORE;
        ds_info.cycle            = true;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_info, 1, &ds_info);

        /* Choose pipeline based on picking method.  The stencil variant
         * has stencil-write enabled so each object stamps its ID. */
        bool use_stencil = (state->picking_method == PICK_STENCIL_ID);
        SDL_BindGPUGraphicsPipeline(pass,
            use_stencil ? state->scene_stencil_pipeline
                        : state->scene_pipeline);

        /* Bind shadow map for shadow sampling in the fragment shader */
        SDL_GPUTextureSamplerBinding shadow_bind = {
            .texture = state->shadow_depth,
            .sampler = state->nearest_clamp,
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

        /* ── Draw all objects ──────────────────────────────────────── */
        for (int i = 0; i < OBJECT_COUNT; i++) {
            /* In stencil-ID mode, set the stencil reference to (i + 1).
             * We use i+1 because stencil 0 is reserved for "no object"
             * (the cleared background value). */
            if (use_stencil) {
                SDL_SetGPUStencilReference(pass, (Uint8)(i + 1));
            }

            /* Build per-object transform matrices */
            mat4 model = mat4_multiply(
                mat4_translate(state->objects[i].position),
                mat4_scale_uniform(state->objects[i].scale));
            mat4 mvp  = mat4_multiply(cam_vp, model);
            mat4 lmvp = mat4_multiply(state->light_vp, model);

            /* Vertex uniforms: MVP for screen projection, model for
             * world-space normals, light_vp for shadow coordinates */
            SceneVertUniforms vert_u;
            vert_u.mvp      = mvp;
            vert_u.model    = model;
            vert_u.light_vp = lmvp;
            SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

            /* Fragment uniforms: Blinn-Phong material + lighting params */
            SceneFragUniforms frag_u;
            SDL_zero(frag_u);
            frag_u.base_color[0] = state->objects[i].color[0];
            frag_u.base_color[1] = state->objects[i].color[1];
            frag_u.base_color[2] = state->objects[i].color[2];
            frag_u.base_color[3] = state->objects[i].color[3];
            frag_u.eye_pos[0]    = state->cam_position.x;
            frag_u.eye_pos[1]    = state->cam_position.y;
            frag_u.eye_pos[2]    = state->cam_position.z;
            frag_u.ambient       = SCENE_AMBIENT;
            frag_u.light_dir[0]  = light_dir.x;
            frag_u.light_dir[1]  = light_dir.y;
            frag_u.light_dir[2]  = light_dir.z;
            frag_u.light_dir[3]  = 0.0f;
            frag_u.light_color[0]  = 1.0f;
            frag_u.light_color[1]  = 1.0f;
            frag_u.light_color[2]  = 1.0f;
            frag_u.light_intensity = 1.0f;
            frag_u.shininess       = SCENE_SHININESS;
            frag_u.specular_str    = SCENE_SPECULAR_STR;
            frag_u.tint[0] = 0.0f;
            frag_u.tint[1] = 0.0f;
            frag_u.tint[2] = 0.0f;
            SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

            /* Bind geometry for this object's shape */
            SDL_GPUBufferBinding vb = {
                .buffer = (state->objects[i].shape == SHAPE_CUBE)
                        ? state->cube_vb : state->sphere_vb,
                .offset = 0
            };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib_bind = {
                .buffer = (state->objects[i].shape == SHAPE_CUBE)
                        ? state->cube_ib : state->sphere_ib,
                .offset = 0
            };
            SDL_BindGPUIndexBuffer(pass, &ib_bind,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);

            Uint32 idx_count = (state->objects[i].shape == SHAPE_CUBE)
                             ? state->cube_index_count
                             : state->sphere_index_count;
            SDL_DrawGPUIndexedPrimitives(pass, idx_count, 1, 0, 0, 0);
        }

        /* ── Grid floor ────────────────────────────────────────────── */
        /* A large quad with procedural grid lines — provides spatial
         * reference and receives shadows from the scene objects. */
        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);
        SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

        GridVertUniforms grid_vert_u;
        grid_vert_u.vp       = cam_vp;
        grid_vert_u.light_vp = state->light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &grid_vert_u,
                                     sizeof(grid_vert_u));

        GridFragUniforms grid_frag_u;
        SDL_zero(grid_frag_u);
        grid_frag_u.line_color[0] = 0.35f;
        grid_frag_u.line_color[1] = 0.35f;
        grid_frag_u.line_color[2] = 0.40f;
        grid_frag_u.line_color[3] = 1.0f;
        grid_frag_u.bg_color[0]   = 0.08f;
        grid_frag_u.bg_color[1]   = 0.08f;
        grid_frag_u.bg_color[2]   = 0.10f;
        grid_frag_u.bg_color[3]   = 1.0f;
        grid_frag_u.light_dir[0]  = light_dir.x;
        grid_frag_u.light_dir[1]  = light_dir.y;
        grid_frag_u.light_dir[2]  = light_dir.z;
        grid_frag_u.light_intensity = 1.0f;
        grid_frag_u.eye_pos[0]    = state->cam_position.x;
        grid_frag_u.eye_pos[1]    = state->cam_position.y;
        grid_frag_u.eye_pos[2]    = state->cam_position.z;
        grid_frag_u.grid_spacing  = 1.0f;
        grid_frag_u.line_width    = GRID_LINE_WIDTH;
        grid_frag_u.fade_distance = GRID_FADE_DISTANCE;
        grid_frag_u.ambient       = GRID_AMBIENT;
        grid_frag_u.tint_color[0] = 1.0f;
        grid_frag_u.tint_color[1] = 1.0f;
        grid_frag_u.tint_color[2] = 1.0f;
        grid_frag_u.tint_color[3] = 1.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &grid_frag_u,
                                       sizeof(grid_frag_u));

        SDL_GPUBufferBinding grid_vb = { .buffer = state->grid_vb, .offset = 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &grid_vb, 1);
        SDL_GPUBufferBinding grid_ib = { .buffer = state->grid_ib, .offset = 0 };
        SDL_BindGPUIndexBuffer(pass, &grid_ib,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, GRID_INDEX_COUNT, 1, 0, 0, 0);

        /* ── Crosshair ─────────────────────────────────────────────── */
        /* A small + overlay at screen center to indicate the pick
         * target when the mouse is captured in FPS mode. */
        SDL_BindGPUGraphicsPipeline(pass, state->crosshair_pipeline);

        SDL_GPUBufferBinding ch_vb = {
            .buffer = state->crosshair_vb, .offset = 0
        };
        SDL_BindGPUVertexBuffers(pass, 0, &ch_vb, 1);

        SDL_GPUBufferBinding ch_ib = {
            .buffer = state->crosshair_ib, .offset = 0
        };
        SDL_BindGPUIndexBuffer(pass, &ch_ib,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, CROSSHAIR_INDICES, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    /* ── Phase 5: Color-ID pass ────────────────────────────────────── */
    /* When the user clicks and color-ID picking is active, render every
     * object as a flat color to an offscreen R8G8B8A8 texture.  Each
     * object gets a unique RGB value derived from its index, so we can
     * read back the pixel under the cursor and decode which object was
     * hit.  This pass only runs on click frames to avoid wasting GPU
     * time every frame. */
    if (state->pick_pending && state->picking_method == PICK_COLOR_ID) {
        SDL_GPUColorTargetInfo id_color;
        SDL_zero(id_color);
        id_color.texture  = state->id_texture;
        id_color.load_op  = SDL_GPU_LOADOP_CLEAR;
        id_color.store_op = SDL_GPU_STOREOP_STORE;
        /* Clear to (0,0,0,0) — background pixels decode to "no object" */
        id_color.clear_color.r = 0.0f;
        id_color.clear_color.g = 0.0f;
        id_color.clear_color.b = 0.0f;
        id_color.clear_color.a = 0.0f;

        SDL_GPUDepthStencilTargetInfo id_ds;
        SDL_zero(id_ds);
        id_ds.texture          = state->id_depth;
        id_ds.clear_depth      = 1.0f;
        id_ds.load_op          = SDL_GPU_LOADOP_CLEAR;
        id_ds.store_op         = SDL_GPU_STOREOP_DONT_CARE;
        id_ds.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
        id_ds.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        id_ds.cycle            = true;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &id_color, 1, &id_ds);
        SDL_BindGPUGraphicsPipeline(pass, state->id_pipeline);

        for (int i = 0; i < OBJECT_COUNT; i++) {
            mat4 model = mat4_multiply(
                mat4_translate(state->objects[i].position),
                mat4_scale_uniform(state->objects[i].scale));
            mat4 mvp = mat4_multiply(cam_vp, model);

            /* Vertex uniform: only need MVP (no lighting in ID pass) */
            IdVertUniforms id_vert_u;
            id_vert_u.mvp = mvp;
            SDL_PushGPUVertexUniformData(cmd, 0, &id_vert_u,
                                         sizeof(id_vert_u));

            /* Fragment uniform: encode object index as an RGB color.
             * index_to_color distributes indices across the color space
             * so each object gets a visually distinct, decodable color. */
            IdFragUniforms id_frag_u;
            float id_r, id_g, id_b;
            index_to_color(i, &id_r, &id_g, &id_b);
            id_frag_u.id_color[0] = id_r;
            id_frag_u.id_color[1] = id_g;
            id_frag_u.id_color[2] = id_b;
            id_frag_u.id_color[3] = 1.0f;
            SDL_PushGPUFragmentUniformData(cmd, 0, &id_frag_u,
                                           sizeof(id_frag_u));

            /* Bind geometry for this object's shape */
            SDL_GPUBufferBinding vb = {
                .buffer = (state->objects[i].shape == SHAPE_CUBE)
                        ? state->cube_vb : state->sphere_vb,
                .offset = 0
            };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib_bind = {
                .buffer = (state->objects[i].shape == SHAPE_CUBE)
                        ? state->cube_ib : state->sphere_ib,
                .offset = 0
            };
            SDL_BindGPUIndexBuffer(pass, &ib_bind,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);

            Uint32 idx_count = (state->objects[i].shape == SHAPE_CUBE)
                             ? state->cube_index_count
                             : state->sphere_index_count;
            SDL_DrawGPUIndexedPrimitives(pass, idx_count, 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    /* ── Phase 6: Copy pass + readback ─────────────────────────────── */
    /* Download the single pixel under the cursor from either the
     * color-ID texture or the depth-stencil texture (for stencil-ID).
     * We copy into a transfer buffer that the CPU can read after
     * GPU submission completes. */
    if (state->pick_pending) {
        /* Clamp pick coordinates to valid texture bounds */
        int px = state->pick_x;
        int py = state->pick_y;
        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px >= WINDOW_WIDTH)  px = WINDOW_WIDTH - 1;
        if (py >= WINDOW_HEIGHT) py = WINDOW_HEIGHT - 1;

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTextureRegion src_region;
        SDL_zero(src_region);
        src_region.x = (Uint32)px;
        src_region.y = (Uint32)py;
        src_region.w = 1;
        src_region.h = 1;
        src_region.d = 1;

        /* Choose source texture based on the active picking method:
         * - Color-ID: read from the offscreen ID texture (RGBA8)
         * - Stencil-ID: read from the main depth-stencil (D24S8) */
        if (state->picking_method == PICK_COLOR_ID) {
            src_region.texture = state->id_texture;
        } else {
            src_region.texture = state->main_depth;
        }

        SDL_GPUTextureTransferInfo dst_info;
        SDL_zero(dst_info);
        dst_info.transfer_buffer = state->pick_readback;
        dst_info.offset = 0;

        SDL_DownloadFromGPUTexture(copy, &src_region, &dst_info);
        SDL_EndGPUCopyPass(copy);
    }

    /* ── Phase 7: Selection outline ────────────────────────────────── */
    /* When an object is selected, draw a bright outline around it using
     * the two-pass stencil technique from Lesson 34:
     *   Pass 1: Redraw the object, writing a marker value to stencil
     *   Pass 2: Draw a slightly scaled-up version — only the border
     *           ring passes stencil because the interior was already
     *           marked in pass 1 */
    if (state->selected_object >= 0) {
        vec3 light_dir = state->light_dir;

        /* Begin a new render pass that LOADs the existing color and
         * depth from the main scene pass.  We CLEAR the stencil to 0
         * because we need a fresh stencil for the outline technique
         * (the main pass stencil was used for picking). */
        SDL_GPUColorTargetInfo outline_color;
        SDL_zero(outline_color);
        outline_color.texture  = swapchain_tex;
        outline_color.load_op  = SDL_GPU_LOADOP_LOAD;
        outline_color.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo outline_ds;
        SDL_zero(outline_ds);
        outline_ds.texture          = state->main_depth;
        outline_ds.load_op          = SDL_GPU_LOADOP_LOAD;
        outline_ds.store_op         = SDL_GPU_STOREOP_DONT_CARE;
        outline_ds.stencil_load_op  = SDL_GPU_LOADOP_CLEAR;
        outline_ds.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        outline_ds.clear_stencil    = 0;
        outline_ds.cycle            = false; /* must not cycle — depth is LOADed */

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &outline_color, 1, &outline_ds);

        int sel = state->selected_object;

        /* Shadow map binding for the outline write pass (which uses
         * the full scene shader to render the object normally) */
        SDL_GPUTextureSamplerBinding shadow_bind = {
            .texture = state->shadow_depth,
            .sampler = state->nearest_clamp,
        };

        /* Step 1: Draw the selected object normally, writing
         * STENCIL_OUTLINE to the stencil buffer wherever it passes
         * the depth test.  This marks the object's silhouette. */
        SDL_BindGPUGraphicsPipeline(pass, state->outline_write_pipeline);
        SDL_SetGPUStencilReference(pass, STENCIL_OUTLINE);
        SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

        mat4 model = mat4_multiply(
            mat4_translate(state->objects[sel].position),
            mat4_scale_uniform(state->objects[sel].scale));
        mat4 mvp  = mat4_multiply(cam_vp, model);
        mat4 lmvp = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        frag_u.base_color[0]   = state->objects[sel].color[0];
        frag_u.base_color[1]   = state->objects[sel].color[1];
        frag_u.base_color[2]   = state->objects[sel].color[2];
        frag_u.base_color[3]   = state->objects[sel].color[3];
        frag_u.eye_pos[0]      = state->cam_position.x;
        frag_u.eye_pos[1]      = state->cam_position.y;
        frag_u.eye_pos[2]      = state->cam_position.z;
        frag_u.ambient         = 0.12f;
        frag_u.light_dir[0]    = light_dir.x;
        frag_u.light_dir[1]    = light_dir.y;
        frag_u.light_dir[2]    = light_dir.z;
        frag_u.light_dir[3]    = 0.0f;
        frag_u.light_color[0]  = 1.0f;
        frag_u.light_color[1]  = 1.0f;
        frag_u.light_color[2]  = 1.0f;
        frag_u.light_intensity = 1.0f;
        frag_u.shininess       = SCENE_SHININESS;
        frag_u.specular_str    = SCENE_SPECULAR_STR;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        /* Bind geometry for the selected object */
        SDL_GPUBufferBinding vb = {
            .buffer = (state->objects[sel].shape == SHAPE_CUBE)
                    ? state->cube_vb : state->sphere_vb,
            .offset = 0
        };
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

        SDL_GPUBufferBinding ib_bind = {
            .buffer = (state->objects[sel].shape == SHAPE_CUBE)
                    ? state->cube_ib : state->sphere_ib,
            .offset = 0
        };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);

        Uint32 idx_count = (state->objects[sel].shape == SHAPE_CUBE)
                         ? state->cube_index_count
                         : state->sphere_index_count;
        SDL_DrawGPUIndexedPrimitives(pass, idx_count, 1, 0, 0, 0);

        /* Step 2: Draw the selected object again, scaled up by
         * OUTLINE_SCALE.  The outline_draw_pipeline uses stencil
         * NOT_EQUAL to STENCIL_OUTLINE — so only fragments outside
         * the original silhouette pass, creating the outline border. */
        SDL_BindGPUGraphicsPipeline(pass, state->outline_draw_pipeline);
        SDL_SetGPUStencilReference(pass, STENCIL_OUTLINE);

        mat4 outline_model = mat4_multiply(
            mat4_translate(state->objects[sel].position),
            mat4_scale_uniform(state->objects[sel].scale * OUTLINE_SCALE));
        mat4 outline_mvp  = mat4_multiply(cam_vp, outline_model);
        mat4 outline_lmvp = mat4_multiply(state->light_vp, outline_model);

        /* Reuse the scene vertex layout for the scaled-up mesh */
        SceneVertUniforms outline_vert_u;
        outline_vert_u.mvp      = outline_mvp;
        outline_vert_u.model    = outline_model;
        outline_vert_u.light_vp = outline_lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &outline_vert_u,
                                     sizeof(outline_vert_u));

        /* Bright yellow outline color — high visibility for selection */
        OutlineFragUniforms outline_frag_u;
        outline_frag_u.outline_color[0] = OUTLINE_COLOR_R;
        outline_frag_u.outline_color[1] = OUTLINE_COLOR_G;
        outline_frag_u.outline_color[2] = OUTLINE_COLOR_B;
        outline_frag_u.outline_color[3] = 1.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &outline_frag_u,
                                       sizeof(outline_frag_u));

        /* Bind the same geometry — the scale change is in the matrix */
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, idx_count, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    /* ── Phase 8: Submit + readback decode ─────────────────────────── */
    /* Submit the command buffer to the GPU.  If capture is active,
     * use the capture helper which handles screenshot/GIF recording
     * before submission. */
#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd,
                                        swapchain_tex)) {
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

    /* After submission, if a pick was pending, wait for the GPU to
     * finish and read back the pixel data.  This stalls the CPU but
     * only happens on click frames — an acceptable cost for
     * interactive object selection. */
    if (state->pick_pending) {
        if (!SDL_WaitForGPUIdle(state->device)) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }

        void *pixel_data = SDL_MapGPUTransferBuffer(
            state->device, state->pick_readback, false);
        if (pixel_data) {
            Uint8 *bytes = (Uint8 *)pixel_data;

            if (state->picking_method == PICK_COLOR_ID) {
                /* Color-ID method: decode the RGB value back to an
                 * object index using the inverse of index_to_color.
                 * Background pixels are (0,0,0) which decode to -1. */
                int picked = color_to_index(bytes[0], bytes[1], bytes[2]);
                state->selected_object =
                    (picked >= 0 && picked < OBJECT_COUNT) ? picked : -1;
            } else {
                /* Stencil-ID method: extract the stencil byte from the
                 * D24_UNORM_S8_UINT pixel.  The byte layout varies by
                 * GPU vendor — most pack depth in the first 3 bytes
                 * and stencil in the 4th, but some reverse this.
                 * We try bytes[3] first, then bytes[0] as fallback. */
                Uint8 stencil = bytes[3];
                int picked = (int)stencil - 1;
                if (picked < 0 || picked >= OBJECT_COUNT) {
                    /* Fallback: some GPUs pack stencil in byte 0 */
                    stencil = bytes[0];
                    picked = (int)stencil - 1;
                }
                state->selected_object =
                    (picked >= 0 && picked < OBJECT_COUNT) ? picked : -1;
            }

            SDL_UnmapGPUTransferBuffer(state->device,
                                       state->pick_readback);
        }

        state->pick_pending = false;

        /* Update window title with the selection result so the user
         * can see which object was picked without any UI overlay */
        {
            char title[256];
            const char *method = (state->picking_method == PICK_COLOR_ID)
                               ? "Color-ID" : "Stencil-ID";
            const char *sel = (state->selected_object >= 0)
                            ? state->objects[state->selected_object].name
                            : "None";
            SDL_snprintf(title, sizeof(title),
                "Lesson 37 — 3D Picking | Method: %s | Selected: %s",
                method, sel);
            SDL_SetWindowTitle(state->window, title);
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── End of Chunk C ──────────────────────────────────────────────────── */
/* ── Part D: SDL_AppQuit ──────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    /* Wait for all GPU work to finish before releasing resources */
    if (!SDL_WaitForGPUIdle(state->device)) {
        SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

    /* Release pipelines */
    if (state->shadow_pipeline)        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->scene_pipeline)         SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->scene_stencil_pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_stencil_pipeline);
    if (state->grid_pipeline)          SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->id_pipeline)            SDL_ReleaseGPUGraphicsPipeline(state->device, state->id_pipeline);
    if (state->crosshair_pipeline)     SDL_ReleaseGPUGraphicsPipeline(state->device, state->crosshair_pipeline);
    if (state->outline_write_pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, state->outline_write_pipeline);
    if (state->outline_draw_pipeline)  SDL_ReleaseGPUGraphicsPipeline(state->device, state->outline_draw_pipeline);

    /* Release geometry buffers */
    if (state->cube_vb)      SDL_ReleaseGPUBuffer(state->device, state->cube_vb);
    if (state->cube_ib)      SDL_ReleaseGPUBuffer(state->device, state->cube_ib);
    if (state->sphere_vb)    SDL_ReleaseGPUBuffer(state->device, state->sphere_vb);
    if (state->sphere_ib)    SDL_ReleaseGPUBuffer(state->device, state->sphere_ib);
    if (state->grid_vb)      SDL_ReleaseGPUBuffer(state->device, state->grid_vb);
    if (state->grid_ib)      SDL_ReleaseGPUBuffer(state->device, state->grid_ib);
    if (state->crosshair_vb) SDL_ReleaseGPUBuffer(state->device, state->crosshair_vb);
    if (state->crosshair_ib) SDL_ReleaseGPUBuffer(state->device, state->crosshair_ib);

    /* Release transfer buffer */
    if (state->pick_readback) SDL_ReleaseGPUTransferBuffer(state->device, state->pick_readback);

    /* Release samplers */
    if (state->nearest_clamp) SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);

    /* Release textures */
    if (state->shadow_depth) SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->main_depth)   SDL_ReleaseGPUTexture(state->device, state->main_depth);
    if (state->id_texture)   SDL_ReleaseGPUTexture(state->device, state->id_texture);
    if (state->id_depth)     SDL_ReleaseGPUTexture(state->device, state->id_depth);

    /* Destroy window and device */
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
