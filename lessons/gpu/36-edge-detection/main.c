/* Lesson 36 — Edge Detection
 *
 * Two post-processing outline techniques:
 * 1. Sobel edge detection on a G-buffer (depth + view-space normals via MRT)
 * 2. Stencil-based X-ray vision using depth_fail_op to reveal objects
 *    behind occluders as Fresnel ghost silhouettes
 *
 * Render pass architecture (5 passes):
 *   Pass 1: Shadow map (D32F, depth-only)
 *   Pass 2: G-buffer MRT (lit color + normals + D24S8)
 *   Pass 3: Edge composite (fullscreen Sobel → swapchain)
 *   Pass 4: X-ray mark (depth_fail_op=INCR, stencil only) [x-ray mode]
 *   Pass 5: X-ray ghost (Fresnel rim, stencil!=0, additive) [x-ray mode]
 *
 * Controls:
 *   1       — Edge detection mode (Sobel outlines)
 *   2       — X-ray mode (stencil ghost vision)
 *   E       — Cycle edge source: depth / normal / combined
 *   V       — Debug: show G-buffer (depth + normals)
 *   WASD    — Camera movement (horizontal)
 *   Space   — Move up
 *   Shift   — Move down
 *   Mouse   — Camera look (click to capture)
 *   Escape  — Release mouse cursor
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include <string.h>    /* memset   */
#include <math.h>      /* sinf, cosf for sphere generation */

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

#include "shaders/compiled/fullscreen_vert_spirv.h"
#include "shaders/compiled/fullscreen_vert_dxil.h"
#include "shaders/compiled/fullscreen_vert_msl.h"

#include "shaders/compiled/edge_detect_frag_spirv.h"
#include "shaders/compiled/edge_detect_frag_dxil.h"
#include "shaders/compiled/edge_detect_frag_msl.h"

#include "shaders/compiled/xray_mark_vert_spirv.h"
#include "shaders/compiled/xray_mark_vert_dxil.h"
#include "shaders/compiled/xray_mark_vert_msl.h"
#include "shaders/compiled/xray_mark_frag_spirv.h"
#include "shaders/compiled/xray_mark_frag_dxil.h"
#include "shaders/compiled/xray_mark_frag_msl.h"

#include "shaders/compiled/ghost_vert_spirv.h"
#include "shaders/compiled/ghost_vert_dxil.h"
#include "shaders/compiled/ghost_vert_msl.h"
#include "shaders/compiled/ghost_frag_spirv.h"
#include "shaders/compiled/ghost_frag_dxil.h"
#include "shaders/compiled/ghost_frag_msl.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "Lesson 36 — Edge Detection"

#define SHADOW_MAP_SIZE 2048

/* Camera defaults */
#define CAM_POS_X      0.0f
#define CAM_POS_Y      2.0f
#define CAM_POS_Z      5.0f
#define CAM_YAW        0.0f
#define CAM_PITCH     -0.15f
#define CAM_SPEED      4.0f
#define CAM_SENSITIVITY 0.003f
#define CAM_NEAR       0.1f
#define CAM_FAR        100.0f
#define CAM_FOV        1.0472f  /* 60 degrees in radians */

/* Edge detection thresholds */
#define DEPTH_THRESHOLD  0.002f
#define NORMAL_THRESHOLD 0.8f

/* Light direction (normalized in init) */
#define LIGHT_DIR_X     0.4f
#define LIGHT_DIR_Y    -0.8f
#define LIGHT_DIR_Z    -0.6f
#define LIGHT_INTENSITY 1.2f
#define AMBIENT_STRENGTH 0.15f

/* Scene layout */
#define NUM_FRONT_OBJECTS 6
#define NUM_XRAY_OBJECTS  2
#define NUM_SCENE_OBJECTS (NUM_FRONT_OBJECTS + 1 + NUM_XRAY_OBJECTS)  /* +1 for wall */

/* Grid settings */
#define GRID_HALF_SIZE  15.0f
#define GRID_SPACING     1.0f
#define GRID_LINE_WIDTH  0.02f
#define GRID_FADE_DIST  20.0f

/* Sphere generation — high tessellation to avoid faceted edges
 * in the normal-based Sobel filter, which detects per-triangle
 * normal discontinuities on coarse meshes. */
#define SPHERE_RINGS   48
#define SPHERE_SECTORS 72

/* Ghost settings */
#define GHOST_POWER      2.0f
#define GHOST_BRIGHTNESS 1.5f

/* Specular lighting */
#define SPECULAR_SHININESS 32.0f
#define SPECULAR_STRENGTH  0.5f

/* Shadow depth bias — prevents self-shadowing acne on curved surfaces */
#define SHADOW_DEPTH_BIAS_CONSTANT 1.5f
#define SHADOW_DEPTH_BIAS_SLOPE    2.0f

/* Delta-time clamp to prevent physics explosion after alt-tab */
#define MAX_DELTA_TIME     0.1f

/* Minimum velocity magnitude for camera movement */
#define VELOCITY_EPSILON   0.001f

/* Light position distance along negated light direction */
#define LIGHT_DISTANCE     20.0f

/* Edge threshold set very high to effectively disable edges in X-ray mode */
#define EDGE_THRESHOLD_DISABLED 1000.0f

/* Scene clear color (near-black with slight blue tint) */
#define CLEAR_COLOR_R 0.05f
#define CLEAR_COLOR_G 0.05f
#define CLEAR_COLOR_B 0.08f

/* Grid line color */
#define GRID_LINE_R   0.4f
#define GRID_LINE_G   0.4f
#define GRID_LINE_B   0.5f

/* Grid background color */
#define GRID_BG_R     0.08f
#define GRID_BG_G     0.08f
#define GRID_BG_B     0.1f

/* Shadow ortho projection size (covers scene) */
#define SHADOW_ORTHO_SIZE 15.0f
#define SHADOW_NEAR       0.1f
#define SHADOW_FAR        50.0f

/* Maximum geometry buffer sizes */
#define MAX_VERTICES     65536
#define MAX_INDICES      65536

/* Pitch clamp for camera look */
#define PITCH_CLAMP      1.5f

/* ── Enums ────────────────────────────────────────────────────────────── */

typedef enum RenderMode {
    RENDER_MODE_EDGE_DETECT = 0,
    RENDER_MODE_XRAY        = 1
} RenderMode;

typedef enum EdgeSource {
    EDGE_SOURCE_DEPTH    = 0,
    EDGE_SOURCE_NORMAL   = 1,
    EDGE_SOURCE_COMBINED = 2,
    EDGE_SOURCE_COUNT    = 3
} EdgeSource;

/* ── Vertex layout ────────────────────────────────────────────────────── */

/* Position + normal vertex — used by cubes, spheres, wall.
 * UV coordinates are not needed since all objects use solid colors. */
typedef struct Vertex {
    vec3 position;   /* 12 bytes — world-space position */
    vec3 normal;     /* 12 bytes — outward surface normal */
} Vertex;            /* 24 bytes total */

/* ── Scene object description ─────────────────────────────────────────── */

typedef struct SceneObject {
    vec3 position;        /* world position                          */
    vec3 scale;           /* per-axis scale factors                  */
    vec3 color;           /* RGB material color                      */
    bool is_sphere;       /* true = sphere mesh, false = cube mesh   */
    bool is_xray_target;  /* true = drawn in X-ray passes           */
    bool casts_shadow;    /* true = included in shadow pass          */
} SceneObject;

/* ── Uniform structures ───────────────────────────────────────────────── */

/* Scene vertex uniforms: MVP + model + light VP*model + model-view.
 * model_view is needed to transform normals into view space for the
 * G-buffer normal output used by Sobel edge detection. */
typedef struct SceneVertUniforms {
    mat4 mvp;            /* model-view-projection for clip space           */
    mat4 model;          /* model (world) matrix for lighting              */
    mat4 light_vp_model; /* light VP * model for shadow projection         */
    mat4 model_view;     /* view * model for view-space normals in G-buffer */
} SceneVertUniforms;     /* 256 bytes — 4 × 64                            */

/* Scene fragment uniforms for Blinn-Phong lighting with shadow map. */
typedef struct SceneFragUniforms {
    float base_color[4];    /* RGBA material color                    */
    float eye_pos[3];       /* camera world position                  */
    float ambient;          /* ambient light intensity                 */
    float light_dir[4];     /* xyz = directional light, w = unused    */
    float light_color[3];   /* RGB light color                         */
    float light_intensity;  /* directional light brightness             */
    float shininess;        /* specular exponent                        */
    float specular_str;     /* specular strength multiplier             */
    float _pad[2];          /* 16-byte alignment padding                */
} SceneFragUniforms;        /* 80 bytes                                 */

/* Grid vertex uniforms — VP + light VP + view for shadow + normals. */
typedef struct GridVertUniforms {
    mat4 vp;         /* camera view-projection                    */
    mat4 light_vp;   /* light view-projection for shadow coords   */
    mat4 view;       /* camera view matrix for view-space normals */
} GridVertUniforms;  /* 192 bytes — 3 × 64                        */

/* Grid fragment uniforms — grid pattern + lighting. */
typedef struct GridFragUniforms {
    float line_color[4];      /* grid line color                      */
    float bg_color[4];        /* background surface color             */
    float light_dir[3];       /* directional light direction          */
    float light_intensity;    /* light brightness                     */
    float eye_pos[3];         /* camera world position                */
    float grid_spacing;       /* world units between grid lines       */
    float line_width;         /* line thickness [0..0.5]              */
    float fade_distance;      /* distance where grid fades out        */
    float ambient;            /* ambient light intensity               */
    float _pad;               /* alignment padding                    */
} GridFragUniforms;           /* 80 bytes                             */

/* Edge detection fragment uniforms — thresholds and mode selection.
 * The float2 texel_size must come first to match the HLSL cbuffer layout. */
typedef struct EdgeDetectUniforms {
    float texel_size[2];      /* 1.0 / render target dimensions         */
    float depth_threshold;    /* sensitivity for depth discontinuities   */
    float normal_threshold;   /* sensitivity for normal discontinuities  */
    int   edge_source;        /* 0=depth, 1=normal, 2=combined           */
    int   show_debug;         /* 1 = show G-buffer debug view            */
    float _pad[2];            /* 16-byte alignment padding               */
} EdgeDetectUniforms;         /* 32 bytes                                */

/* X-ray mark vertex uniforms — only MVP needed for stencil writing. */
typedef struct MarkVertUniforms {
    mat4 mvp;                 /* model-view-projection for clip space   */
} MarkVertUniforms;           /* 64 bytes                               */

/* Ghost vertex uniforms — MVP + model-view for Fresnel calculation. */
typedef struct GhostVertUniforms {
    mat4 mvp;                 /* model-view-projection for clip space   */
    mat4 model_view;          /* model * view for view-space normals    */
} GhostVertUniforms;          /* 128 bytes — 2 × 64                    */

/* Ghost fragment uniforms — Fresnel parameters and ghost color.
 * Must match the HLSL cbuffer layout: float3, float, float, float3 pad. */
typedef struct GhostFragUniforms {
    float ghost_color[3];     /* RGB ghost tint                          */
    float ghost_power;        /* Fresnel exponent for rim falloff        */
    float ghost_brightness;   /* overall ghost brightness multiplier     */
    float _pad[3];            /* 16-byte alignment padding               */
} GhostFragUniforms;          /* 32 bytes                                */

/* ── Application state ────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;   /* OS window handle for rendering              */
    SDL_GPUDevice *device;   /* GPU device for all resource creation         */

    /* Pipelines — 6 total:
     * shadow, scene (G-buffer MRT), grid, edge_detect, xray_mark, ghost */
    SDL_GPUGraphicsPipeline *shadow_pipeline;       /* depth-only shadow map       */
    SDL_GPUGraphicsPipeline *scene_pipeline;         /* G-buffer MRT (color+normal) */
    SDL_GPUGraphicsPipeline *grid_pipeline;          /* grid floor with MRT         */
    SDL_GPUGraphicsPipeline *edge_detect_pipeline;   /* fullscreen Sobel composite  */
    SDL_GPUGraphicsPipeline *xray_mark_pipeline;     /* depth_fail stencil marking  */
    SDL_GPUGraphicsPipeline *ghost_pipeline;          /* Fresnel ghost rendering     */

    /* Render targets — G-buffer textures */
    SDL_GPUTexture *shadow_depth;       /* D32_FLOAT 2048×2048 shadow map          */
    SDL_GPUTexture *scene_color;        /* RGBA8 window-sized lit color output      */
    SDL_GPUTexture *scene_normal;       /* RGBA16F window-sized view-space normals  */
    SDL_GPUTexture *scene_depth_stencil;/* D24S8 window-sized depth+stencil         */

    /* Samplers */
    SDL_GPUSampler *nearest_clamp;      /* shadow map + G-buffer depth sampling     */
    SDL_GPUSampler *linear_clamp;       /* G-buffer color + normal sampling         */

    /* Geometry buffers */
    SDL_GPUBuffer *cube_vb;             /* cube vertex buffer                       */
    SDL_GPUBuffer *cube_ib;             /* cube index buffer                        */
    SDL_GPUBuffer *sphere_vb;           /* sphere vertex buffer                     */
    SDL_GPUBuffer *sphere_ib;           /* sphere index buffer                      */
    SDL_GPUBuffer *grid_vb;             /* grid floor quad vertices                 */
    SDL_GPUBuffer *grid_ib;             /* grid floor quad indices                  */

    /* Index counts for draw calls */
    Uint32 cube_index_count;            /* number of indices in the cube mesh       */
    Uint32 sphere_index_count;          /* number of indices in the sphere mesh     */

    /* Scene objects — front objects, wall, and X-ray targets */
    SceneObject scene_objects[NUM_SCENE_OBJECTS];

    /* Light */
    vec3 light_dir;                     /* normalized directional light direction   */
    mat4 light_vp;                      /* light view-projection matrix             */

    /* Texture formats (negotiated at init) */
    SDL_GPUTextureFormat depth_stencil_fmt; /* D24S8 or D32S8 format              */
    SDL_GPUTextureFormat swapchain_format;  /* window surface pixel format          */

    /* Camera — first-person with yaw/pitch */
    vec3  cam_position;                 /* world-space camera position              */
    float cam_yaw;                      /* horizontal rotation in radians           */
    float cam_pitch;                    /* vertical rotation in radians             */

    /* Render mode and edge source */
    RenderMode render_mode;             /* current visualization mode               */
    EdgeSource edge_source;             /* which G-buffer channels drive edges      */
    bool       show_debug;              /* true = show G-buffer debug view          */

    /* Keyboard state for movement */
    bool key_w;              /* W held — move forward           */
    bool key_a;              /* A held — strafe left            */
    bool key_s;              /* S held — move backward          */
    bool key_d;              /* D held — strafe right           */
    bool key_space;          /* Space held — move up            */
    bool key_shift;          /* Left Shift held — move down     */

    /* Timing & input */
    Uint64 last_ticks;                  /* SDL_GetTicks() value from previous frame */
    bool   mouse_captured;              /* true when mouse is captured for FPS look */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;               /* screenshot/GIF capture state             */
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
    Uint32 num_uniform_buffers)
{
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage = stage;
    info.num_samplers = num_samplers;
    info.num_storage_buffers = 0;
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
    if (!copy) {
        SDL_Log("ERROR: SDL_BeginGPUCopyPass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

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
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
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

/* Generate a unit cube centered at origin with half-size 0.5.
 * 24 vertices (4 per face) + 36 indices. */
static void generate_cube(Vertex *verts, Uint32 *vert_count,
                           Uint16 *indices, Uint32 *idx_count)
{
    *vert_count = 0;
    *idx_count = 0;
    add_box(0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f,
            verts, vert_count, indices, idx_count);
}

/* ── Geometry: generate_sphere ────────────────────────────────────────── */

/* Generate a UV sphere with unit radius using SPHERE_RINGS latitude bands
 * and SPHERE_SECTORS longitude segments.  Uses standard spherical coordinate
 * parameterization: theta sweeps pole-to-pole, phi sweeps around the equator. */
static void generate_sphere(Vertex *verts, Uint32 *vert_count,
                             Uint16 *indices, Uint32 *idx_count)
{
    Uint32 v = 0;
    Uint32 idx = 0;
    const int lat = SPHERE_RINGS;
    const int lon = SPHERE_SECTORS;

    /* Generate vertices row by row from top pole to bottom pole */
    for (int i = 0; i <= lat; i++) {
        float theta = (float)i * FORGE_PI / (float)lat;
        float sin_t = sinf(theta);
        float cos_t = cosf(theta);

        for (int j = 0; j <= lon; j++) {
            float phi = (float)j * 2.0f * FORGE_PI / (float)lon;
            float sin_p = sinf(phi);
            float cos_p = cosf(phi);

            /* Position on unit sphere */
            float x = cos_p * sin_t;
            float y = cos_t;
            float z = sin_p * sin_t;

            verts[v].position = vec3_create(x, y, z);
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

            /* Skip degenerate triangles at the top pole.
             * Winding: (a, a+1, b) is CCW when viewed from outside
             * the sphere, matching the cube's outward-facing convention. */
            if (i != 0) {
                indices[idx++] = a;
                indices[idx++] = (Uint16)(a + 1);
                indices[idx++] = b;
            }
            /* Skip degenerate triangles at the bottom pole */
            if (i != lat - 1) {
                indices[idx++] = (Uint16)(a + 1);
                indices[idx++] = (Uint16)(b + 1);
                indices[idx++] = b;
            }
        }
    }

    *vert_count = v;
    *idx_count = idx;
}
/* ── Part B: SDL_AppInit ──────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
#ifndef FORGE_CAPTURE
    (void)argc;
    (void)argv;
#endif

    /* ── 1. Allocate app_state ──────────────────────────────────────── */

    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    /* ── 2. SDL_Init + device + window ──────────────────────────────── */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
        true, NULL);
    if (!device) {
        SDL_Log("ERROR: SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

    state->device = device;
    state->window = window;

    /* ── 3. Swapchain format + sRGB setup ───────────────────────────── */
    /* Try SDR_LINEAR for correct gamma: the GPU will auto-convert
     * linear fragment output to sRGB on write, giving physically
     * correct lighting without manual pow(1/2.2) in shaders. */

    if (SDL_WindowSupportsGPUSwapchainComposition(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("WARNING: SDL_SetGPUSwapchainParameters (SDR_LINEAR) failed: %s",
                    SDL_GetError());
        }
    }

    state->swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 4. Depth format negotiation ────────────────────────────────── */

    /* Shadow depth: D32_FLOAT with SAMPLER usage (sampled in scene pass
     * for shadow mapping).  No stencil needed for shadows. */
    SDL_GPUTextureFormat shadow_depth_fmt;

    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        shadow_depth_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        SDL_Log("Shadow depth format: D32_FLOAT");
    } else if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        shadow_depth_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        SDL_Log("Shadow depth format: D16_UNORM (fallback)");
    } else {
        SDL_Log("ERROR: No shadow depth format supports DEPTH_STENCIL_TARGET | SAMPLER");
        return SDL_APP_FAILURE;
    }

    /* Scene depth-stencil: needs 8-bit stencil for X-ray marking, plus
     * SAMPLER usage so the edge detection pass can read depth values.
     * Prefer D24_UNORM_S8_UINT (4 bytes/pixel) over D32_FLOAT_S8_UINT
     * (8 bytes/pixel) for memory efficiency. */
    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        SDL_Log("Depth-stencil format: D24_UNORM_S8_UINT (with SAMPLER)");
    } else if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        SDL_Log("Depth-stencil format: D32_FLOAT_S8_UINT (fallback, with SAMPLER)");
    } else {
        SDL_Log("ERROR: No depth-stencil format supports DEPTH_STENCIL_TARGET | SAMPLER");
        return SDL_APP_FAILURE;
    }

    /* ── 5. Texture creation (4 render targets) ─────────────────────── */

    /* Shadow depth — 2048x2048, D32_FLOAT, sampled in scene fragment
     * shader for percentage-closer shadow mapping. */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = shadow_depth_fmt;
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

    /* Scene color — window-sized, R8G8B8A8_UNORM.  Receives the lit
     * scene color from the G-buffer MRT pass.  Sampled by the edge
     * detection shader to composite outlines over the scene. */
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
        state->scene_color = SDL_CreateGPUTexture(device, &ti);
        if (!state->scene_color) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (scene_color) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Scene normal — R16G16B16A16_FLOAT for high-precision view-space
     * normals.  16-bit float avoids quantization artifacts that would
     * cause false edges in the Sobel normal-based edge detector. */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
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

    /* Scene depth-stencil — window-sized, D24S8 or D32S8.
     * Depth channel: sampled by edge detection for depth-based outlines.
     * Stencil channel: written by X-ray mark pass (depth_fail_op=INCR),
     * read by ghost pass (compare_op=NOT_EQUAL ref 0) to render hidden objects. */
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
        state->scene_depth_stencil = SDL_CreateGPUTexture(device, &ti);
        if (!state->scene_depth_stencil) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (scene_depth_stencil) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 6. Sampler creation ────────────────────────────────────────── */

    /* Nearest-clamp — used for shadow map, depth buffer, and normal
     * buffer sampling.  Nearest filtering avoids interpolation across
     * depth/normal discontinuities which would blur edges. */
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
            SDL_Log("ERROR: SDL_CreateGPUSampler (nearest) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Linear-clamp — used for scene color sampling in the edge detection
     * composite pass.  Linear filtering produces smoother color blending
     * when the edge shader reads neighboring texels. */
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
            SDL_Log("ERROR: SDL_CreateGPUSampler (linear) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 7. Shader creation (12 shaders — 6 pairs) ──────────────────── */

    /* Scene vertex shader — transforms position by MVP, computes world
     * position for lighting, projects into light space for shadows, and
     * transforms normals into view space for the G-buffer. */
    SDL_GPUShader *scene_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil),
        scene_vert_msl,   scene_vert_msl_size,
        0, 1);

    /* Scene fragment shader — Blinn-Phong with shadow mapping.
     * Outputs lit color to MRT[0] and view-space normals to MRT[1]. */
    SDL_GPUShader *scene_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil),
        scene_frag_msl,   scene_frag_msl_size,
        1, 1);

    /* Shadow vertex shader — only outputs clip position for the
     * depth-only shadow map pass. */
    SDL_GPUShader *shadow_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil, sizeof(shadow_vert_dxil),
        shadow_vert_msl,   shadow_vert_msl_size,
        0, 1);

    /* Shadow fragment shader — empty body, the GPU writes depth
     * automatically during rasterization. */
    SDL_GPUShader *shadow_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil, sizeof(shadow_frag_dxil),
        shadow_frag_msl,   shadow_frag_msl_size,
        0, 0);

    /* Grid vertex shader — passes world position to the fragment shader
     * for procedural grid pattern and shadow coordinate computation. */
    SDL_GPUShader *grid_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil, sizeof(grid_vert_dxil),
        grid_vert_msl,   grid_vert_msl_size,
        0, 1);

    /* Grid fragment shader — procedural grid lines with shadow mapping.
     * Samples the shadow map to darken grid lines in shadow. */
    SDL_GPUShader *grid_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil, sizeof(grid_frag_dxil),
        grid_frag_msl,   grid_frag_msl_size,
        1, 1);

    /* Fullscreen vertex shader — generates a fullscreen triangle from
     * SV_VertexID (0,1,2) with no vertex buffer needed. */
    SDL_GPUShader *fullscreen_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil),
        fullscreen_vert_msl,   fullscreen_vert_msl_size,
        0, 0);

    /* Edge detection fragment shader — applies a 3x3 Sobel operator to
     * the G-buffer depth and/or normals, then composites outlines over
     * the scene color.  3 samplers: depth, normal, color. */
    SDL_GPUShader *edge_detect_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        edge_detect_frag_spirv, sizeof(edge_detect_frag_spirv),
        edge_detect_frag_dxil, sizeof(edge_detect_frag_dxil),
        edge_detect_frag_msl,   edge_detect_frag_msl_size,
        3, 1);

    /* X-ray mark vertex shader — transforms the X-ray target geometry
     * by MVP so the stencil mark covers the correct screen pixels. */
    SDL_GPUShader *xray_mark_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        xray_mark_vert_spirv, sizeof(xray_mark_vert_spirv),
        xray_mark_vert_dxil, sizeof(xray_mark_vert_dxil),
        xray_mark_vert_msl,   xray_mark_vert_msl_size,
        0, 1);

    /* X-ray mark fragment shader — empty body.  No color or depth output;
     * only the stencil depth_fail_op matters in this pass. */
    SDL_GPUShader *xray_mark_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        xray_mark_frag_spirv, sizeof(xray_mark_frag_spirv),
        xray_mark_frag_dxil, sizeof(xray_mark_frag_dxil),
        xray_mark_frag_msl,   xray_mark_frag_msl_size,
        0, 0);

    /* Ghost vertex shader — transforms hidden object geometry and
     * computes view-space normals for Fresnel rim calculation. */
    SDL_GPUShader *ghost_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        ghost_vert_spirv, sizeof(ghost_vert_spirv),
        ghost_vert_dxil, sizeof(ghost_vert_dxil),
        ghost_vert_msl,   ghost_vert_msl_size,
        0, 1);

    /* Ghost fragment shader — computes Fresnel rim intensity to make
     * hidden objects glow at their silhouette edges. */
    SDL_GPUShader *ghost_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        ghost_frag_spirv, sizeof(ghost_frag_spirv),
        ghost_frag_dxil, sizeof(ghost_frag_dxil),
        ghost_frag_msl,   ghost_frag_msl_size,
        0, 1);

    if (!scene_vert || !scene_frag || !shadow_vert || !shadow_frag ||
        !grid_vert || !grid_frag || !fullscreen_vert || !edge_detect_frag ||
        !xray_mark_vert || !xray_mark_frag || !ghost_vert || !ghost_frag) {
        SDL_Log("ERROR: One or more shaders failed to compile");
        /* Release any shaders that were successfully created */
        if (scene_vert)      SDL_ReleaseGPUShader(device, scene_vert);
        if (scene_frag)      SDL_ReleaseGPUShader(device, scene_frag);
        if (shadow_vert)     SDL_ReleaseGPUShader(device, shadow_vert);
        if (shadow_frag)     SDL_ReleaseGPUShader(device, shadow_frag);
        if (grid_vert)       SDL_ReleaseGPUShader(device, grid_vert);
        if (grid_frag)       SDL_ReleaseGPUShader(device, grid_frag);
        if (fullscreen_vert) SDL_ReleaseGPUShader(device, fullscreen_vert);
        if (edge_detect_frag) SDL_ReleaseGPUShader(device, edge_detect_frag);
        if (xray_mark_vert)  SDL_ReleaseGPUShader(device, xray_mark_vert);
        if (xray_mark_frag)  SDL_ReleaseGPUShader(device, xray_mark_frag);
        if (ghost_vert)      SDL_ReleaseGPUShader(device, ghost_vert);
        if (ghost_frag)      SDL_ReleaseGPUShader(device, ghost_frag);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Vertex input state descriptions ─────────────────────────── */

    /* Full vertex input (position + normal) — used by scene and ghost
     * pipelines.  Both need normals: scene for lighting and G-buffer,
     * ghost for Fresnel rim calculation. */
    SDL_GPUVertexBufferDescription vb_full;
    SDL_zero(vb_full);
    vb_full.slot = 0;
    vb_full.pitch = sizeof(Vertex);
    vb_full.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute full_attrs[2];
    SDL_zero(full_attrs);
    full_attrs[0].location = 0;
    full_attrs[0].buffer_slot = 0;
    full_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    full_attrs[0].offset = offsetof(Vertex, position);

    full_attrs[1].location = 1;
    full_attrs[1].buffer_slot = 0;
    full_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    full_attrs[1].offset = offsetof(Vertex, normal);

    SDL_GPUVertexInputState full_vertex_input;
    SDL_zero(full_vertex_input);
    full_vertex_input.vertex_buffer_descriptions = &vb_full;
    full_vertex_input.num_vertex_buffers = 1;
    full_vertex_input.vertex_attributes = full_attrs;
    full_vertex_input.num_vertex_attributes = 2;

    /* Position-only vertex input — used by shadow and X-ray mark
     * pipelines.  These passes only need clip-space position; normals
     * are irrelevant for depth-only and stencil-only rendering. */
    SDL_GPUVertexBufferDescription vb_pos;
    SDL_zero(vb_pos);
    vb_pos.slot = 0;
    vb_pos.pitch = sizeof(Vertex);  /* same stride, fewer attributes */
    vb_pos.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute pos_attr;
    SDL_zero(pos_attr);
    pos_attr.location = 0;
    pos_attr.buffer_slot = 0;
    pos_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    pos_attr.offset = offsetof(Vertex, position);

    SDL_GPUVertexInputState pos_vertex_input;
    SDL_zero(pos_vertex_input);
    pos_vertex_input.vertex_buffer_descriptions = &vb_pos;
    pos_vertex_input.num_vertex_buffers = 1;
    pos_vertex_input.vertex_attributes = &pos_attr;
    pos_vertex_input.num_vertex_attributes = 1;

    /* ── 9. Pipeline creation (6 pipelines) ─────────────────────────── */

    /* Scene pass writes to two color targets (MRT):
     *   [0] R8G8B8A8_UNORM  — lit scene color for compositing
     *   [1] R16G16B16A16_FLOAT — view-space normals for edge detection */
    SDL_GPUColorTargetDescription scene_color_targets[2];
    SDL_zero(scene_color_targets);
    scene_color_targets[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    scene_color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

    /* ── Pipeline 1: shadow_pipeline ─────────────────────────────────
     * Renders all shadow-casting geometry into the 2048x2048 depth
     * buffer from the light's point of view.  No color output — the
     * depth buffer IS the shadow map. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = shadow_vert;
        pi.fragment_shader = shadow_frag;
        pi.vertex_input_state = pos_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        /* No culling in the shadow pass ensures both front and back faces
         * are rendered.  Front faces win the LESS depth test, giving the
         * tightest shadow boundary.  Slope-scaled depth bias prevents
         * self-shadow acne on curved surfaces (spheres) where a single
         * shadow map texel spans a steep depth gradient.  The shader-side
         * SHADOW_BIAS (0.005) in scene.frag.hlsl handles remaining
         * precision issues at shadow edges. */
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor = SHADOW_DEPTH_BIAS_CONSTANT;
        pi.rasterizer_state.depth_bias_slope_factor = SHADOW_DEPTH_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = shadow_depth_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 2: scene_pipeline (MRT) ────────────────────────────
     * Renders lit scene objects to a G-buffer with two color targets:
     * MRT[0] = lit color (Blinn-Phong + shadow), MRT[1] = view-space
     * normals.  Both are later read by the edge detection pass. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = scene_vert;
        pi.fragment_shader = scene_frag;
        pi.vertex_input_state = full_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions = scene_color_targets;
        pi.target_info.num_color_targets = 2;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 3: grid_pipeline (MRT) ─────────────────────────────
     * Renders the procedural grid floor into the same G-buffer targets.
     * Uses LESS_OR_EQUAL depth to avoid z-fighting at y=0.
     * Cull NONE because the grid is a single-sided quad viewed from
     * either side depending on camera position. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = grid_vert;
        pi.fragment_shader = grid_frag;
        pi.vertex_input_state = pos_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions = scene_color_targets;
        pi.target_info.num_color_targets = 2;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 4: edge_detect_pipeline ────────────────────────────
     * Fullscreen Sobel edge detection composite.  Reads the G-buffer
     * (depth, normals, color) as input textures and writes the final
     * outlined image to the swapchain.  No vertex input — the vertex
     * shader generates a fullscreen triangle from SV_VertexID. */
    {
        SDL_GPUColorTargetDescription edge_color_desc;
        SDL_zero(edge_color_desc);
        edge_color_desc.format = state->swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = fullscreen_vert;
        pi.fragment_shader = edge_detect_frag;
        /* No vertex input — fullscreen triangle from SV_VertexID */
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.enable_depth_test = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions = &edge_color_desc;
        pi.target_info.num_color_targets = 1;
        pi.target_info.has_depth_stencil_target = false;
        state->edge_detect_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 5: xray_mark_pipeline ──────────────────────────────
     * The key technique: writes to the stencil buffer using
     * depth_fail_op = INCREMENT_AND_WRAP.  When an X-ray target
     * fragment fails the depth test (is behind an occluder), the
     * stencil value at that pixel is incremented.  This marks exactly
     * the screen pixels where hidden geometry exists behind visible
     * geometry.
     *
     * No color output and no depth write — this pass only modifies
     * the stencil buffer.  Cull NONE so both front and back faces
     * contribute to the stencil count. */
    {
        SDL_GPUStencilOpState mark_stencil;
        SDL_zero(mark_stencil);
        mark_stencil.fail_op       = SDL_GPU_STENCILOP_KEEP;
        mark_stencil.depth_fail_op = SDL_GPU_STENCILOP_INCREMENT_AND_WRAP;
        mark_stencil.pass_op       = SDL_GPU_STENCILOP_KEEP;
        mark_stencil.compare_op    = SDL_GPU_COMPAREOP_ALWAYS;

        SDL_GPUColorTargetDescription mark_color_desc;
        SDL_zero(mark_color_desc);
        mark_color_desc.format = state->swapchain_format;
        /* All color write mask bits off — no color output */

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = xray_mark_vert;
        pi.fragment_shader = xray_mark_frag;
        pi.vertex_input_state = pos_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state = mark_stencil;
        pi.depth_stencil_state.back_stencil_state = mark_stencil;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0xFF;
        pi.target_info.color_target_descriptions = &mark_color_desc;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->xray_mark_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 6: ghost_pipeline ──────────────────────────────────
     * Renders hidden objects as Fresnel-edge ghost silhouettes wherever
     * the stencil buffer was marked (stencil > 0) by the X-ray mark
     * pass.  Uses additive blending (ONE + ONE) to let the ghost glow
     * accumulate over the scene color.
     *
     * No depth test — the ghost should render regardless of scene depth
     * since the stencil mask already gates visibility.
     * Stencil is read-only (write_mask=0x00) with NOT_EQUAL comparison
     * against reference 0 — only pixels where stencil was incremented
     * above 0 will pass.  NOT_EQUAL is used because SDL GPU compare ops
     * follow Vulkan convention: ref OP stencil.  GREATER would test
     * ref > stencil (0 > N), which never passes for positive values. */
    {
        SDL_GPUStencilOpState ghost_stencil;
        SDL_zero(ghost_stencil);
        ghost_stencil.fail_op       = SDL_GPU_STENCILOP_KEEP;
        ghost_stencil.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        ghost_stencil.pass_op       = SDL_GPU_STENCILOP_KEEP;
        ghost_stencil.compare_op    = SDL_GPU_COMPAREOP_NOT_EQUAL;

        SDL_GPUColorTargetDescription ghost_color_desc;
        SDL_zero(ghost_color_desc);
        ghost_color_desc.format = state->swapchain_format;
        /* Additive blend: ONE + ONE — ghost glow accumulates */
        ghost_color_desc.blend_state.enable_blend = true;
        ghost_color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ghost_color_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ghost_color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        ghost_color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ghost_color_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ghost_color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = ghost_vert;
        pi.fragment_shader = ghost_frag;
        pi.vertex_input_state = full_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.enable_depth_test = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state = ghost_stencil;
        pi.depth_stencil_state.back_stencil_state = ghost_stencil;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0x00;
        pi.target_info.color_target_descriptions = &ghost_color_desc;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->ghost_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── 10. Release all 12 shaders (pipeline keeps its own copies) ── */

    SDL_ReleaseGPUShader(device, scene_vert);
    SDL_ReleaseGPUShader(device, scene_frag);
    SDL_ReleaseGPUShader(device, shadow_vert);
    SDL_ReleaseGPUShader(device, shadow_frag);
    SDL_ReleaseGPUShader(device, grid_vert);
    SDL_ReleaseGPUShader(device, grid_frag);
    SDL_ReleaseGPUShader(device, fullscreen_vert);
    SDL_ReleaseGPUShader(device, edge_detect_frag);
    SDL_ReleaseGPUShader(device, xray_mark_vert);
    SDL_ReleaseGPUShader(device, xray_mark_frag);
    SDL_ReleaseGPUShader(device, ghost_vert);
    SDL_ReleaseGPUShader(device, ghost_frag);

    /* Verify all pipelines were created successfully */
    if (!state->shadow_pipeline || !state->scene_pipeline ||
        !state->grid_pipeline || !state->edge_detect_pipeline ||
        !state->xray_mark_pipeline || !state->ghost_pipeline) {
        SDL_Log("ERROR: One or more pipelines failed to create: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 11. Geometry generation + GPU upload ───────────────────────── */

    /* Cube — unit cube (half-size 0.5) with per-face normals.
     * Used for scene cubes, the wall, and X-ray target cubes.
     * Scale is applied per-object via the model matrix. */
    {
        Vertex cube_verts[24];
        Uint16 cube_indices[36];
        Uint32 cube_vert_count = 0, cube_index_count = 0;
        generate_cube(cube_verts, &cube_vert_count,
                      cube_indices, &cube_index_count);

        state->cube_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
            cube_verts, cube_vert_count * (Uint32)sizeof(Vertex));
        state->cube_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
            cube_indices, cube_index_count * (Uint32)sizeof(Uint16));
        state->cube_index_count = cube_index_count;

        if (!state->cube_vb || !state->cube_ib) {
            SDL_Log("ERROR: Failed to upload cube geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* Sphere — unit-radius UV sphere with SPHERE_RINGS latitude bands
     * and SPHERE_SECTORS longitude segments.  Heap-allocated because
     * the vertex count can be large: (48+1)*(72+1) = 3577 vertices. */
    {
        Uint32 max_verts = (SPHERE_RINGS + 1) * (SPHERE_SECTORS + 1);
        Uint32 max_indices = SPHERE_RINGS * SPHERE_SECTORS * 6;
        Vertex *sphere_verts = SDL_calloc(max_verts, sizeof(Vertex));
        Uint16 *sphere_indices = SDL_calloc(max_indices, sizeof(Uint16));
        if (!sphere_verts || !sphere_indices) {
            SDL_Log("ERROR: Failed to allocate sphere geometry arrays");
            SDL_free(sphere_verts);
            SDL_free(sphere_indices);
            return SDL_APP_FAILURE;
        }

        Uint32 sphere_vert_count = 0, sphere_index_count = 0;
        generate_sphere(sphere_verts, &sphere_vert_count,
                        sphere_indices, &sphere_index_count);

        state->sphere_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
            sphere_verts, sphere_vert_count * (Uint32)sizeof(Vertex));
        state->sphere_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
            sphere_indices, sphere_index_count * (Uint32)sizeof(Uint16));
        state->sphere_index_count = sphere_index_count;

        SDL_free(sphere_verts);
        SDL_free(sphere_indices);

        if (!state->sphere_vb || !state->sphere_ib) {
            SDL_Log("ERROR: Failed to upload sphere geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* Grid — single large XZ quad for the procedural grid floor.
     * The grid pattern is generated in the fragment shader using
     * world-space coordinates, so the quad just needs to be large
     * enough to cover the visible area. */
    {
        Vertex grid_verts[4] = {
            {{ -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE }, { 0.0f, 1.0f, 0.0f }},
            {{  GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE }, { 0.0f, 1.0f, 0.0f }},
            {{  GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE }, { 0.0f, 1.0f, 0.0f }},
            {{ -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE }, { 0.0f, 1.0f, 0.0f }},
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

    /* ── 12. Scene object initialization ────────────────────────────── */
    /* 9 objects total:
     *   [0-3] Front cubes — visible, different colors and positions
     *   [4-5] Front spheres — visible, different colors
     *   [6]   Wall — large opaque occluder hiding the X-ray targets
     *   [7]   X-ray cube — behind the wall, revealed by stencil
     *   [8]   X-ray sphere — behind the wall, revealed by stencil */

    /* Front cubes */
    state->scene_objects[0] = (SceneObject){
        { 2.0f, 0.5f, -2.0f},      /* position   */
        {1.0f, 1.0f, 1.0f},        /* scale      */
        {0.6f, 0.6f, 0.6f},        /* color: grey */
        false, false, true          /* cube, not xray, casts shadow */
    };
    state->scene_objects[1] = (SceneObject){
        {-1.5f, 0.5f, -1.0f},
        {1.0f, 1.0f, 1.0f},
        {0.3f, 0.4f, 0.8f},        /* color: blue */
        false, false, true
    };
    state->scene_objects[2] = (SceneObject){
        { 0.0f, 0.5f, -3.0f},
        {0.8f, 0.8f, 0.8f},
        {0.8f, 0.3f, 0.3f},        /* color: red */
        false, false, true
    };
    state->scene_objects[3] = (SceneObject){
        { 3.0f, 0.8f,  0.0f},
        {1.2f, 1.2f, 1.2f},
        {0.3f, 0.7f, 0.4f},        /* color: green */
        false, false, true
    };

    /* Front spheres */
    state->scene_objects[4] = (SceneObject){
        {-2.5f, 0.7f, -3.5f},
        {0.7f, 0.7f, 0.7f},
        {0.9f, 0.5f, 0.2f},        /* color: orange */
        true, false, true           /* sphere, not xray, casts shadow */
    };
    state->scene_objects[5] = (SceneObject){
        { 1.0f, 0.6f,  1.0f},
        {0.6f, 0.6f, 0.6f},
        {0.2f, 0.7f, 0.7f},        /* color: teal */
        true, false, true
    };

    /* Wall — large flat box that occludes the X-ray targets */
    state->scene_objects[6] = (SceneObject){
        { 0.0f, 1.5f, -6.0f},
        {6.0f, 3.0f, 0.3f},
        {0.5f, 0.45f, 0.4f},       /* color: stone */
        false, false, true
    };

    /* X-ray targets — hidden behind the wall, revealed by stencil
     * ghost rendering.  Cast shadows like all other opaque geometry. */
    state->scene_objects[7] = (SceneObject){
        {-1.5f, 0.8f, -8.0f},
        {1.0f, 1.0f, 1.0f},
        {0.15f, 0.7f, 1.0f},       /* color: bright cyan */
        false, true, true           /* cube, is xray target, casts shadow */
    };
    state->scene_objects[8] = (SceneObject){
        { 1.5f, 1.0f, -8.5f},
        {0.8f, 0.8f, 0.8f},
        {0.15f, 0.7f, 1.0f},       /* color: bright cyan */
        true, true, true            /* sphere, is xray target, casts shadow */
    };

    /* ── 13. Camera + mode initialization ───────────────────────────── */

    state->cam_position = vec3_create(CAM_POS_X, CAM_POS_Y, CAM_POS_Z);
    state->cam_yaw = CAM_YAW;
    state->cam_pitch = CAM_PITCH;
    state->render_mode = RENDER_MODE_EDGE_DETECT;
    state->edge_source = EDGE_SOURCE_COMBINED;
    state->show_debug = false;
    state->last_ticks = SDL_GetTicks();

    /* ── 14. Light setup ────────────────────────────────────────────── */

    state->light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));

    /* Build light view-projection matrix for shadow mapping.
     * The light "camera" looks from a position along the negated light
     * direction toward the origin, using an orthographic projection
     * sized to cover the scene. */
    vec3 light_pos = vec3_scale(state->light_dir, -LIGHT_DISTANCE);
    mat4 light_view = mat4_look_at(light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
        -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
        SHADOW_NEAR, SHADOW_FAR);
    state->light_vp = mat4_multiply(light_proj, light_view);

    /* ── 15. Mouse capture ──────────────────────────────────────────── */

    if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
        SDL_Log("WARNING: SDL_SetWindowRelativeMouseMode failed: %s",
                SDL_GetError());
    }
    state->mouse_captured = true;

    /* ── 16. Capture init ───────────────────────────────────────────── */

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, state->device, state->window)) {
            SDL_Log("Failed to initialise capture");
            return SDL_APP_FAILURE;
        }
    }
#endif

    /* ── 17. Done ───────────────────────────────────────────────────── */

    SDL_Log("Lesson 36 initialized — press 1/2 to switch modes, E for edge source");
    return SDL_APP_CONTINUE;
}
/* ── Part C: Event handling, rendering, and cleanup ───────────────────── */

/* ── Helper: draw_object ─────────────────────────────────────────────── */

/* Bind and draw the correct mesh (cube or sphere) for a scene object.
 * This avoids duplicating buffer-binding code in every render pass. */
static void draw_object(SDL_GPURenderPass *pass, const app_state *state,
                         const SceneObject *obj)
{
    if (obj->is_sphere) {
        SDL_GPUBufferBinding vb = { state->sphere_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_GPUBufferBinding ib = { state->sphere_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->sphere_index_count,
                                     1, 0, 0, 0);
    } else {
        SDL_GPUBufferBinding vb = { state->cube_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_GPUBufferBinding ib = { state->cube_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->cube_index_count,
                                     1, 0, 0, 0);
    }
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat) break;  /* ignore auto-repeat */

        if (event->key.key == SDLK_1) {
            state->render_mode = RENDER_MODE_EDGE_DETECT;
            SDL_Log("Mode: Edge Detection (Sobel outlines)");
        } else if (event->key.key == SDLK_2) {
            state->render_mode = RENDER_MODE_XRAY;
            SDL_Log("Mode: X-ray vision (stencil ghost)");
        } else if (event->key.key == SDLK_E) {
            state->edge_source = (EdgeSource)((state->edge_source + 1) %
                                               EDGE_SOURCE_COUNT);
            const char *names[] = { "Depth", "Normal", "Combined" };
            SDL_Log("Edge source: %s", names[state->edge_source]);
        } else if (event->key.key == SDLK_V) {
            state->show_debug = !state->show_debug;
            SDL_Log("Debug view: %s", state->show_debug ? "ON" : "OFF");
        } else if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
            if (state->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                    SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                } else {
                    state->mouse_captured = false;
                }
            }
        } else if (event->key.scancode == SDL_SCANCODE_W) {
            state->key_w = true;
        } else if (event->key.scancode == SDL_SCANCODE_A) {
            state->key_a = true;
        } else if (event->key.scancode == SDL_SCANCODE_S) {
            state->key_s = true;
        } else if (event->key.scancode == SDL_SCANCODE_D) {
            state->key_d = true;
        } else if (event->key.scancode == SDL_SCANCODE_SPACE) {
            state->key_space = true;
        } else if (event->key.scancode == SDL_SCANCODE_LSHIFT) {
            state->key_shift = true;
        }
        break;

    case SDL_EVENT_KEY_UP:
        if (event->key.scancode == SDL_SCANCODE_W) {
            state->key_w = false;
        } else if (event->key.scancode == SDL_SCANCODE_A) {
            state->key_a = false;
        } else if (event->key.scancode == SDL_SCANCODE_S) {
            state->key_s = false;
        } else if (event->key.scancode == SDL_SCANCODE_D) {
            state->key_d = false;
        } else if (event->key.scancode == SDL_SCANCODE_SPACE) {
            state->key_space = false;
        } else if (event->key.scancode == SDL_SCANCODE_LSHIFT) {
            state->key_shift = false;
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

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw   -= event->motion.xrel * CAM_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * CAM_SENSITIVITY;
            /* Clamp pitch to avoid gimbal-lock flipping */
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
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
    SDL_GPUDevice *device = state->device;

    /* ── 1. Delta time ────────────────────────────────────────────── */

    Uint64 now = SDL_GetTicks();
    float dt = (float)(now - state->last_ticks) / 1000.0f;
    state->last_ticks = now;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── 2. Camera movement ───────────────────────────────────────── */
    /* Compute forward and right vectors from yaw (horizontal rotation).
     * Pitch only affects view direction, not movement — this keeps the
     * camera moving along the XZ plane regardless of look angle. */

    float sin_yaw = sinf(state->cam_yaw);
    float cos_yaw = cosf(state->cam_yaw);
    vec3 forward = vec3_create(-sin_yaw, 0.0f, -cos_yaw);
    vec3 world_up = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 right = vec3_normalize(
        vec3_cross(forward, world_up));

    vec3 velocity = vec3_create(0.0f, 0.0f, 0.0f);
    if (state->key_w) velocity = vec3_add(velocity, forward);
    if (state->key_s) velocity = vec3_sub(velocity, forward);
    if (state->key_d) velocity = vec3_add(velocity, right);
    if (state->key_a) velocity = vec3_sub(velocity, right);
    if (state->key_space) velocity = vec3_add(velocity, world_up);
    if (state->key_shift) velocity = vec3_sub(velocity, world_up);

    float vel_len = vec3_length(velocity);
    if (vel_len > VELOCITY_EPSILON) {
        velocity = vec3_scale(
            vec3_normalize(velocity), CAM_SPEED * dt);
        state->cam_position = vec3_add(state->cam_position,
                                                    velocity);
    }

    /* ── 3. Matrix computation ────────────────────────────────────── */

    /* View matrix from camera position + yaw/pitch look direction */
    float sin_pitch = sinf(state->cam_pitch);
    float cos_pitch = cosf(state->cam_pitch);
    vec3 look_dir = vec3_create(-sin_yaw * cos_pitch,
                                      sin_pitch,
                                     -cos_yaw * cos_pitch);
    vec3 target = vec3_add(state->cam_position, look_dir);
    mat4 view = mat4_look_at(state->cam_position, target, world_up);
    mat4 proj = mat4_perspective(
        CAM_FOV, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT,
        CAM_NEAR, CAM_FAR);
    mat4 vp = mat4_multiply(proj, view);

    /* Light matrices for shadow mapping (light_dir computed once in init) */
    vec3 light_pos = vec3_scale(state->light_dir, -LIGHT_DISTANCE);
    mat4 light_view = mat4_look_at(
        light_pos, vec3_create(0.0f, 0.0f, 0.0f), world_up);
    mat4 light_proj = mat4_orthographic(
        -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
        -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
        SHADOW_NEAR, SHADOW_FAR);
    mat4 light_vp = mat4_multiply(light_proj, light_view);

    /* ── 4. Acquire command buffer and swapchain ──────────────────── */

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("ERROR: SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError());
        return SDL_APP_CONTINUE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw, sh;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain_tex, &sw, &sh)) {
        SDL_Log("ERROR: SDL_WaitAndAcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }
    if (!swapchain_tex) {
        /* Window minimized or not ready — skip this frame */
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    /* ── 5. Pass 1: Shadow map (depth-only) ───────────────────────── */
    /* Render all shadow-casting objects into the shadow depth texture.
     * No color target — only the depth buffer is written. */

    SDL_GPUDepthStencilTargetInfo shadow_ds;
    SDL_zero(shadow_ds);
    shadow_ds.texture   = state->shadow_depth;
    shadow_ds.load_op   = SDL_GPU_LOADOP_CLEAR;
    shadow_ds.store_op  = SDL_GPU_STOREOP_STORE;
    shadow_ds.clear_depth = 1.0f;

    SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
        cmd, NULL, 0, &shadow_ds);
    if (!shadow_pass) {
        SDL_Log("ERROR: SDL_BeginGPURenderPass (shadow) failed: %s",
                SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }
    SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

    for (int i = 0; i < NUM_SCENE_OBJECTS; i++) {
        if (!state->scene_objects[i].casts_shadow) continue;

        mat4 model = mat4_multiply(
            mat4_translate(state->scene_objects[i].position),
            mat4_scale(state->scene_objects[i].scale));
        mat4 shadow_mvp = mat4_multiply(light_vp, model);
        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_mvp, sizeof(shadow_mvp));
        draw_object(shadow_pass, state, &state->scene_objects[i]);
    }

    SDL_EndGPURenderPass(shadow_pass);

    /* ── 6. Pass 2: G-buffer MRT (color + normals + depth-stencil) ── */
    /* Two color targets:
     *   target 0 = scene_color (RGBA8, lit Blinn-Phong output)
     *   target 1 = scene_normal (RGBA16F, view-space normals for Sobel)
     * Plus a depth-stencil target with stencil for the X-ray pass. */

    SDL_GPUColorTargetInfo gbuf_targets[2];
    SDL_zero(gbuf_targets);

    gbuf_targets[0].texture    = state->scene_color;
    gbuf_targets[0].load_op    = SDL_GPU_LOADOP_CLEAR;
    gbuf_targets[0].store_op   = SDL_GPU_STOREOP_STORE;
    gbuf_targets[0].clear_color = (SDL_FColor){ CLEAR_COLOR_R, CLEAR_COLOR_G, CLEAR_COLOR_B, 1.0f };

    gbuf_targets[1].texture    = state->scene_normal;
    gbuf_targets[1].load_op    = SDL_GPU_LOADOP_CLEAR;
    gbuf_targets[1].store_op   = SDL_GPU_STOREOP_STORE;
    /* Default +Y up normal stored directly in RGBA16F [-1,1] range */
    gbuf_targets[1].clear_color = (SDL_FColor){ 0.0f, 1.0f, 0.0f, 1.0f };

    SDL_GPUDepthStencilTargetInfo scene_ds;
    SDL_zero(scene_ds);
    scene_ds.texture           = state->scene_depth_stencil;
    scene_ds.load_op           = SDL_GPU_LOADOP_CLEAR;
    scene_ds.store_op          = SDL_GPU_STOREOP_STORE;
    scene_ds.clear_depth       = 1.0f;
    scene_ds.stencil_load_op   = SDL_GPU_LOADOP_CLEAR;
    scene_ds.stencil_store_op  = SDL_GPU_STOREOP_STORE;
    scene_ds.clear_stencil     = 0;

    SDL_GPURenderPass *gbuf_pass = SDL_BeginGPURenderPass(
        cmd, gbuf_targets, 2, &scene_ds);
    if (!gbuf_pass) {
        SDL_Log("ERROR: SDL_BeginGPURenderPass (gbuffer) failed: %s",
                SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    /* Bind scene pipeline for all solid objects */
    SDL_BindGPUGraphicsPipeline(gbuf_pass, state->scene_pipeline);

    /* Bind shadow map as fragment sampler for shadow testing */
    SDL_GPUTextureSamplerBinding shadow_bind;
    shadow_bind.texture = state->shadow_depth;
    shadow_bind.sampler = state->nearest_clamp;
    SDL_BindGPUFragmentSamplers(gbuf_pass, 0, &shadow_bind, 1);

    /* Draw all scene objects with Blinn-Phong lighting */
    for (int i = 0; i < NUM_SCENE_OBJECTS; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(state->scene_objects[i].position),
            mat4_scale(state->scene_objects[i].scale));
        mat4 mvp = mat4_multiply(vp, model);
        mat4 light_vp_model = mat4_multiply(light_vp, model);
        mat4 model_view_mat = mat4_multiply(view, model);

        SceneVertUniforms svu;
        svu.mvp           = mvp;
        svu.model         = model;
        svu.light_vp_model = light_vp_model;
        svu.model_view    = model_view_mat;
        SDL_PushGPUVertexUniformData(cmd, 0, &svu, sizeof(svu));

        SceneFragUniforms sfu;
        SDL_zero(sfu);
        sfu.base_color[0] = state->scene_objects[i].color.x;
        sfu.base_color[1] = state->scene_objects[i].color.y;
        sfu.base_color[2] = state->scene_objects[i].color.z;
        sfu.base_color[3] = 1.0f;
        sfu.eye_pos[0]    = state->cam_position.x;
        sfu.eye_pos[1]    = state->cam_position.y;
        sfu.eye_pos[2]    = state->cam_position.z;
        sfu.ambient        = AMBIENT_STRENGTH;
        sfu.light_dir[0]  = state->light_dir.x;
        sfu.light_dir[1]  = state->light_dir.y;
        sfu.light_dir[2]  = state->light_dir.z;
        sfu.light_dir[3]  = 0.0f;
        sfu.light_color[0] = 1.0f;
        sfu.light_color[1] = 1.0f;
        sfu.light_color[2] = 1.0f;
        sfu.light_intensity = LIGHT_INTENSITY;
        sfu.shininess      = SPECULAR_SHININESS;
        sfu.specular_str   = SPECULAR_STRENGTH;
        SDL_PushGPUFragmentUniformData(cmd, 0, &sfu, sizeof(sfu));

        draw_object(gbuf_pass, state, &state->scene_objects[i]);
    }

    /* Draw grid floor — uses its own pipeline with position-only vertices */
    SDL_BindGPUGraphicsPipeline(gbuf_pass, state->grid_pipeline);

    /* Re-bind shadow map for grid fragment sampling */
    SDL_GPUTextureSamplerBinding grid_shadow_bind;
    grid_shadow_bind.texture = state->shadow_depth;
    grid_shadow_bind.sampler = state->nearest_clamp;
    SDL_BindGPUFragmentSamplers(gbuf_pass, 0, &grid_shadow_bind, 1);

    GridVertUniforms gvu;
    gvu.vp       = vp;
    gvu.light_vp = light_vp;
    gvu.view     = view;
    SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));

    GridFragUniforms gfu;
    SDL_zero(gfu);
    gfu.line_color[0]   = GRID_LINE_R;
    gfu.line_color[1]   = GRID_LINE_G;
    gfu.line_color[2]   = GRID_LINE_B;
    gfu.line_color[3]   = 1.0f;
    gfu.bg_color[0]     = GRID_BG_R;
    gfu.bg_color[1]     = GRID_BG_G;
    gfu.bg_color[2]     = GRID_BG_B;
    gfu.bg_color[3]     = 1.0f;
    gfu.light_dir[0]    = state->light_dir.x;
    gfu.light_dir[1]    = state->light_dir.y;
    gfu.light_dir[2]    = state->light_dir.z;
    gfu.light_intensity = LIGHT_INTENSITY;
    gfu.eye_pos[0]      = state->cam_position.x;
    gfu.eye_pos[1]      = state->cam_position.y;
    gfu.eye_pos[2]      = state->cam_position.z;
    gfu.grid_spacing    = GRID_SPACING;
    gfu.line_width      = GRID_LINE_WIDTH;
    gfu.fade_distance   = GRID_FADE_DIST;
    gfu.ambient         = AMBIENT_STRENGTH;
    SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

    SDL_GPUBufferBinding grid_vb_bind = { state->grid_vb, 0 };
    SDL_BindGPUVertexBuffers(gbuf_pass, 0, &grid_vb_bind, 1);
    SDL_GPUBufferBinding grid_ib_bind = { state->grid_ib, 0 };
    SDL_BindGPUIndexBuffer(gbuf_pass, &grid_ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(gbuf_pass, 6, 1, 0, 0, 0);

    SDL_EndGPURenderPass(gbuf_pass);

    /* ── 7. Pass 3: Edge composite (fullscreen Sobel to swapchain) ── */
    /* Sample the G-buffer textures (depth, normals, color) and apply
     * Sobel edge detection.  The fullscreen triangle is generated in
     * the vertex shader from SV_VertexID — no vertex buffer needed. */

    SDL_GPUColorTargetInfo edge_target;
    SDL_zero(edge_target);
    edge_target.texture    = swapchain_tex;
    edge_target.load_op    = SDL_GPU_LOADOP_CLEAR;
    edge_target.store_op   = SDL_GPU_STOREOP_STORE;
    edge_target.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };

    SDL_GPURenderPass *edge_pass = SDL_BeginGPURenderPass(
        cmd, &edge_target, 1, NULL);
    if (!edge_pass) {
        SDL_Log("ERROR: SDL_BeginGPURenderPass (edge) failed: %s",
                SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }
    SDL_BindGPUGraphicsPipeline(edge_pass, state->edge_detect_pipeline);

    /* Bind G-buffer textures: slot 0 = depth, slot 1 = normals,
     * slot 2 = scene color */
    SDL_GPUTextureSamplerBinding edge_samplers[3];
    edge_samplers[0].texture = state->scene_depth_stencil;
    edge_samplers[0].sampler = state->nearest_clamp;
    edge_samplers[1].texture = state->scene_normal;
    edge_samplers[1].sampler = state->nearest_clamp;
    edge_samplers[2].texture = state->scene_color;
    edge_samplers[2].sampler = state->linear_clamp;
    SDL_BindGPUFragmentSamplers(edge_pass, 0, edge_samplers, 3);

    EdgeDetectUniforms edu;
    SDL_zero(edu);
    edu.texel_size[0] = 1.0f / (float)WINDOW_WIDTH;
    edu.texel_size[1] = 1.0f / (float)WINDOW_HEIGHT;

    if (state->render_mode == RENDER_MODE_XRAY) {
        /* In X-ray mode, disable edge detection by setting very high
         * thresholds so the composite pass simply copies scene color */
        edu.depth_threshold  = EDGE_THRESHOLD_DISABLED;
        edu.normal_threshold = EDGE_THRESHOLD_DISABLED;
        edu.edge_source      = EDGE_SOURCE_COMBINED;
    } else {
        edu.depth_threshold  = DEPTH_THRESHOLD;
        edu.normal_threshold = NORMAL_THRESHOLD;
        edu.edge_source      = (int)state->edge_source;
    }
    edu.show_debug = state->show_debug ? 1 : 0;

    SDL_PushGPUFragmentUniformData(cmd, 0, &edu, sizeof(edu));

    /* Fullscreen triangle: 3 vertices generated in vertex shader */
    SDL_DrawGPUPrimitives(edge_pass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(edge_pass);

    /* ── 8. Pass 4: X-ray mark (conditional) ──────────────────────── */
    /* When X-ray mode is active, re-draw X-ray target objects using
     * depth_fail_op = INCREMENT to mark stencil where the target is
     * occluded by closer geometry.  The mark pass loads the existing
     * depth buffer from the G-buffer pass so the depth test uses the
     * scene's depth values. */

    if (state->render_mode == RENDER_MODE_XRAY) {

        SDL_GPUColorTargetInfo mark_target;
        SDL_zero(mark_target);
        mark_target.texture  = swapchain_tex;
        mark_target.load_op  = SDL_GPU_LOADOP_LOAD;  /* preserve edge output */
        mark_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo mark_ds;
        SDL_zero(mark_ds);
        mark_ds.texture           = state->scene_depth_stencil;
        mark_ds.load_op           = SDL_GPU_LOADOP_LOAD;   /* keep G-buffer depth */
        mark_ds.store_op          = SDL_GPU_STOREOP_STORE;
        mark_ds.stencil_load_op   = SDL_GPU_LOADOP_CLEAR;  /* clear stencil to 0 */
        mark_ds.stencil_store_op  = SDL_GPU_STOREOP_STORE;
        mark_ds.clear_stencil     = 0;

        SDL_GPURenderPass *mark_pass = SDL_BeginGPURenderPass(
            cmd, &mark_target, 1, &mark_ds);
        if (!mark_pass) {
            SDL_Log("ERROR: SDL_BeginGPURenderPass (xray mark) failed: %s",
                    SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmd);
            return SDL_APP_CONTINUE;
        }
        SDL_BindGPUGraphicsPipeline(mark_pass, state->xray_mark_pipeline);

        for (int i = 0; i < NUM_SCENE_OBJECTS; i++) {
            if (!state->scene_objects[i].is_xray_target) continue;

            mat4 model = mat4_multiply(
                mat4_translate(state->scene_objects[i].position),
                mat4_scale(state->scene_objects[i].scale));
            mat4 mark_mvp = mat4_multiply(vp, model);

            MarkVertUniforms mvu;
            mvu.mvp = mark_mvp;
            SDL_PushGPUVertexUniformData(cmd, 0, &mvu, sizeof(mvu));
            draw_object(mark_pass, state, &state->scene_objects[i]);
        }

        SDL_EndGPURenderPass(mark_pass);

        /* ── 9. Pass 5: Ghost (Fresnel rim where stencil > 0) ────── */
        /* Draw X-ray target objects with a Fresnel rim shader.  The
         * stencil test passes only where stencil > 0 (i.e., where the
         * mark pass detected occlusion), creating a ghost silhouette
         * that reveals objects behind walls and other occluders. */

        SDL_GPUColorTargetInfo ghost_target;
        SDL_zero(ghost_target);
        ghost_target.texture  = swapchain_tex;
        ghost_target.load_op  = SDL_GPU_LOADOP_LOAD;
        ghost_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo ghost_ds;
        SDL_zero(ghost_ds);
        ghost_ds.texture          = state->scene_depth_stencil;
        ghost_ds.load_op          = SDL_GPU_LOADOP_LOAD;
        ghost_ds.store_op         = SDL_GPU_STOREOP_DONT_CARE;
        ghost_ds.stencil_load_op  = SDL_GPU_LOADOP_LOAD;
        ghost_ds.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass *ghost_pass = SDL_BeginGPURenderPass(
            cmd, &ghost_target, 1, &ghost_ds);
        if (!ghost_pass) {
            SDL_Log("ERROR: SDL_BeginGPURenderPass (ghost) failed: %s",
                    SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmd);
            return SDL_APP_CONTINUE;
        }
        SDL_BindGPUGraphicsPipeline(ghost_pass, state->ghost_pipeline);

        /* Stencil reference 0 with NOT_EQUAL compare — passes where
         * stencil != 0 (the mark pass incremented these pixels) */
        SDL_SetGPUStencilReference(ghost_pass, 0);

        for (int i = 0; i < NUM_SCENE_OBJECTS; i++) {
            if (!state->scene_objects[i].is_xray_target) continue;

            mat4 model = mat4_multiply(
                mat4_translate(state->scene_objects[i].position),
                mat4_scale(state->scene_objects[i].scale));
            mat4 ghost_mvp = mat4_multiply(vp, model);
            mat4 ghost_mv  = mat4_multiply(view, model);

            GhostVertUniforms ghost_vu;
            ghost_vu.mvp        = ghost_mvp;
            ghost_vu.model_view = ghost_mv;
            SDL_PushGPUVertexUniformData(cmd, 0, &ghost_vu,
                                          sizeof(ghost_vu));

            GhostFragUniforms ghost_fu;
            SDL_zero(ghost_fu);
            ghost_fu.ghost_color[0]  = state->scene_objects[i].color.x;
            ghost_fu.ghost_color[1]  = state->scene_objects[i].color.y;
            ghost_fu.ghost_color[2]  = state->scene_objects[i].color.z;
            ghost_fu.ghost_power     = GHOST_POWER;
            ghost_fu.ghost_brightness = GHOST_BRIGHTNESS;
            SDL_PushGPUFragmentUniformData(cmd, 0, &ghost_fu,
                                            sizeof(ghost_fu));

            draw_object(ghost_pass, state, &state->scene_objects[i]);
        }

        SDL_EndGPURenderPass(ghost_pass);
    }

    /* ── 10. Submit command buffer + capture ──────────────────────── */

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

/* ── SDL_AppQuit ─────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    /* Wait for all in-flight GPU work to complete before releasing */
    if (!SDL_WaitForGPUIdle(state->device)) {
        SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

    /* Release pipelines (6 total) */
    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->edge_detect_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->edge_detect_pipeline);
    if (state->xray_mark_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->xray_mark_pipeline);
    if (state->ghost_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->ghost_pipeline);

    /* Release render target textures */
    if (state->shadow_depth)
        SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->scene_color)
        SDL_ReleaseGPUTexture(state->device, state->scene_color);
    if (state->scene_normal)
        SDL_ReleaseGPUTexture(state->device, state->scene_normal);
    if (state->scene_depth_stencil)
        SDL_ReleaseGPUTexture(state->device, state->scene_depth_stencil);

    /* Release samplers */
    if (state->nearest_clamp)
        SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->linear_clamp)
        SDL_ReleaseGPUSampler(state->device, state->linear_clamp);

    /* Release geometry buffers */
    if (state->cube_vb)   SDL_ReleaseGPUBuffer(state->device, state->cube_vb);
    if (state->cube_ib)   SDL_ReleaseGPUBuffer(state->device, state->cube_ib);
    if (state->sphere_vb) SDL_ReleaseGPUBuffer(state->device, state->sphere_vb);
    if (state->sphere_ib) SDL_ReleaseGPUBuffer(state->device, state->sphere_ib);
    if (state->grid_vb)   SDL_ReleaseGPUBuffer(state->device, state->grid_vb);
    if (state->grid_ib)   SDL_ReleaseGPUBuffer(state->device, state->grid_ib);

    /* Release window from GPU device, then destroy window and device */
    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
