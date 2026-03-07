/*
 * Lesson 34 — Portals & Outlines
 *
 * Demonstrates stencil buffer fundamentals through three techniques:
 * 1. Portal effect — render a different world through a masked opening
 * 2. Object outlines — highlight objects with colored borders
 * 3. Debug visualization — display stencil buffer contents as colors
 *
 * The stencil buffer is an 8-bit per-pixel integer buffer stored alongside
 * the depth buffer in a D24_UNORM_S8_UINT texture.  While depth controls
 * which fragments are closest, stencil provides general-purpose per-pixel
 * masking controlled entirely by the programmer.
 *
 * Key insight: stencil behavior is controlled by PIPELINE configuration,
 * not by shader code.  The shaders don't know about stencil at all —
 * it's a fixed-function feature configured on each graphics pipeline.
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

#include "shaders/compiled/outline_frag_spirv.h"
#include "shaders/compiled/outline_frag_dxil.h"

#include "shaders/compiled/debug_overlay_vert_spirv.h"
#include "shaders/compiled/debug_overlay_vert_dxil.h"
#include "shaders/compiled/debug_overlay_frag_spirv.h"
#include "shaders/compiled/debug_overlay_frag_dxil.h"

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

/* Portal dimensions and stencil reference values */
#define PORTAL_WIDTH       2.0f
#define PORTAL_HEIGHT      3.0f
#define PORTAL_THICKNESS   0.2f
#define OUTLINE_SCALE      1.04f
#define STENCIL_PORTAL     1
#define STENCIL_OUTLINE    2
#define PITCH_CLAMP        1.5f     /* max vertical look angle (radians, ~86°) */

/* Sphere tessellation resolution */
#define SPHERE_LAT_SEGS    20
#define SPHERE_LON_SEGS    20

/* Maximum geometry buffer sizes */
#define MAX_VERTICES       65536
#define MAX_INDICES         65536

/* Scene object counts */
#define CUBE_COUNT         4
#define PORTAL_SPHERE_COUNT 3
#define OUTLINED_CUBE_COUNT 2

/* ── Vertex layout ────────────────────────────────────────────────────── */

/* Position + normal vertex — used by cubes, spheres, portal frame.
 * UV coordinates are not needed since all objects use solid colors. */
typedef struct Vertex {
    vec3 position;   /* 12 bytes — world-space position */
    vec3 normal;     /* 12 bytes — outward surface normal */
} Vertex;            /* 24 bytes total */

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
 * The tint field adds color to the ambient term for the portal world,
 * creating a visually distinct atmosphere. */
typedef struct SceneFragUniforms {
    float base_color[4];    /* RGBA material color                  */
    float eye_pos[3];       /* camera world position                */
    float ambient;          /* ambient light intensity               */
    float light_dir[4];     /* xyz = directional light direction     */
    float light_color[3];   /* RGB light color                       */
    float light_intensity;  /* directional light brightness           */
    float shininess;        /* specular exponent                      */
    float specular_str;     /* specular strength multiplier           */
    float tint[3];          /* additive tint for portal world ambient */
    float _pad0;            /* 16-byte alignment padding              */
} SceneFragUniforms;        /* 80 bytes                               */

/* Grid floor vertex uniforms — VP + light VP for shadow mapping. */
typedef struct GridVertUniforms {
    mat4 vp;         /* camera view-projection                  */
    mat4 light_vp;   /* light view-projection for shadow coords */
} GridVertUniforms;  /* 128 bytes                                */

/* Grid floor fragment uniforms — grid pattern + lighting + tint.
 * tint_color multiplies the surface color, allowing the portal grid
 * to have a different color from the main world grid. */
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

/* Outline fragment uniforms — solid outline color. */
typedef struct OutlineFragUniforms {
    float outline_color[4];   /* RGBA outline color                 */
} OutlineFragUniforms;        /* 16 bytes                           */

/* ── Scene object description ─────────────────────────────────────────── */

typedef struct SceneObject {
    vec3  position;    /* world position                        */
    float scale;       /* uniform scale factor                  */
    float color[4];    /* RGBA material color                   */
    bool  outlined;    /* whether this object gets an outline   */
    float outline_r;   /* outline color red                     */
    float outline_g;   /* outline color green                   */
    float outline_b;   /* outline color blue                    */
} SceneObject;

/* ── Application state ────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;   /* OS window handle for rendering            */
    SDL_GPUDevice *device;   /* GPU device for all resource creation       */

    /* Pipelines — one for each stencil configuration.
     * This "pipeline explosion" is inherent to stencil effects:
     * each different stencil test/operation combination needs its
     * own pipeline because stencil state is baked into the PSO. */
    SDL_GPUGraphicsPipeline *shadow_pipeline;        /* depth-only shadow     */
    SDL_GPUGraphicsPipeline *mask_pipeline;           /* stencil write, no color */
    SDL_GPUGraphicsPipeline *portal_pipeline;         /* stencil == PORTAL     */
    SDL_GPUGraphicsPipeline *main_pipeline;           /* stencil != PORTAL     */
    SDL_GPUGraphicsPipeline *frame_pipeline;          /* stencil ALWAYS        */
    SDL_GPUGraphicsPipeline *outline_write_pipeline;  /* stencil REPLACE       */
    SDL_GPUGraphicsPipeline *outline_draw_pipeline;   /* stencil NOT_EQUAL     */
    SDL_GPUGraphicsPipeline *grid_pipeline;           /* grid, stencil != PORTAL */
    SDL_GPUGraphicsPipeline *grid_portal_pipeline;    /* grid, stencil == PORTAL */
    SDL_GPUGraphicsPipeline *debug_pipeline;          /* fullscreen debug overlay */

    /* Render targets */
    SDL_GPUTexture *shadow_depth;    /* D32_FLOAT 2048×2048 shadow map    */
    SDL_GPUTexture *main_depth;      /* D24_UNORM_S8_UINT window-sized    */
    SDL_GPUTextureFormat depth_stencil_fmt; /* negotiated DS format       */

    /* Samplers */
    SDL_GPUSampler *nearest_clamp;   /* shadow map + debug texture sampling */

    /* Geometry buffers */
    SDL_GPUBuffer *cube_vb;          /* cube vertex buffer                */
    SDL_GPUBuffer *cube_ib;          /* cube index buffer                 */
    SDL_GPUBuffer *sphere_vb;        /* sphere vertex buffer              */
    SDL_GPUBuffer *sphere_ib;        /* sphere index buffer               */
    SDL_GPUBuffer *portal_frame_vb;  /* portal frame vertices             */
    SDL_GPUBuffer *portal_frame_ib;  /* portal frame indices              */
    SDL_GPUBuffer *portal_mask_vb;   /* portal opening quad vertices      */
    SDL_GPUBuffer *portal_mask_ib;   /* portal opening quad indices        */
    SDL_GPUBuffer *grid_vb;          /* grid floor quad vertices           */
    SDL_GPUBuffer *grid_ib;          /* grid floor quad indices            */

    /* Index counts for draw calls */
    Uint32 cube_index_count;          /* number of indices in the cube mesh      */
    Uint32 sphere_index_count;        /* number of indices in the sphere mesh    */
    Uint32 portal_frame_index_count;  /* number of indices in the portal frame   */
    Uint32 portal_mask_index_count;   /* number of indices in the portal quad    */

    /* Debug overlay for stencil visualization */
    SDL_GPUTexture *debug_texture;   /* RGBA8 stencil visualization       */
    SDL_GPUSampler *debug_sampler;   /* nearest sampling for debug tex    */
    bool show_stencil_debug;         /* toggled with V key                */

    /* Scene objects */
    SceneObject cubes[CUBE_COUNT];                  /* main-world cubes (position, scale, color, outline) */
    SceneObject portal_spheres[PORTAL_SPHERE_COUNT]; /* alternate-world spheres visible through portal */

    /* Light */
    vec3 light_dir;                  /* normalized directional light direction */
    mat4 light_vp;                   /* light view-projection matrix      */

    SDL_GPUTextureFormat swapchain_format; /* window surface pixel format       */

    /* Camera — first-person with quaternion orientation */
    vec3  cam_position;  /* world-space camera position                    */
    float cam_yaw;       /* horizontal rotation in radians (0 = +Z)       */
    float cam_pitch;     /* vertical rotation in radians (clamped ±PITCH_CLAMP) */

    /* Timing & input */
    Uint64 last_ticks;   /* SDL_GetTicks() value from previous frame      */
    bool   mouse_captured; /* true when mouse is captured for FPS look    */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;                             /* screenshot/GIF capture state */
#endif
} app_state;

/* ── Helper: create_shader ────────────────────────────────────────────── */

/* Create a GPU shader from pre-compiled SPIRV and DXIL bytecode.
 * Automatically selects the correct format based on the GPU backend. */
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
        float sin_t = sinf(theta);
        float cos_t = cosf(theta);

        for (int j = 0; j <= lon; j++) {
            float phi = (float)j * 2.0f * FORGE_PI / (float)lon;
            float sin_p = sinf(phi);
            float cos_p = cosf(phi);

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

/* ── Geometry: generate_portal_frame ──────────────────────────────────── */

/* Build the portal doorway frame from 4 box sections:
 * left pillar, right pillar, top beam, and floor threshold.
 * The portal opening is PORTAL_WIDTH × PORTAL_HEIGHT, centered at origin,
 * bottom at y=0.  The frame has PORTAL_THICKNESS depth. */
static void generate_portal_frame(Vertex *verts, Uint32 *vert_count,
                                   Uint16 *indices, Uint32 *idx_count)
{
    *vert_count = 0;
    *idx_count = 0;

    float hw = PORTAL_WIDTH / 2.0f;    /* half-width of the opening */
    float h  = PORTAL_HEIGHT;           /* full height               */
    float t  = PORTAL_THICKNESS;        /* frame bar thickness       */
    float ht = t / 2.0f;               /* half-thickness             */
    float d  = t;                       /* frame depth (Z direction) */
    float hd = d / 2.0f;               /* half-depth                */

    /* Left pillar: from y=0 to y=h, at x = -(hw + ht) */
    add_box(-(hw + ht), h / 2.0f, 0.0f,
            ht, h / 2.0f, hd,
            verts, vert_count, indices, idx_count);

    /* Right pillar: from y=0 to y=h, at x = +(hw + ht) */
    add_box(hw + ht, h / 2.0f, 0.0f,
            ht, h / 2.0f, hd,
            verts, vert_count, indices, idx_count);

    /* Top beam: spans the full width including pillars */
    add_box(0.0f, h + ht, 0.0f,
            hw + t, ht, hd,
            verts, vert_count, indices, idx_count);

    /* Floor threshold: thin strip at the base */
    add_box(0.0f, ht * 0.5f, 0.0f,
            hw + t, ht * 0.5f, hd,
            verts, vert_count, indices, idx_count);
}

/* ── Geometry: generate_portal_mask_quad ───────────────────────────────── */

/* Generate an invisible quad that fills the portal opening.
 * This quad writes to the stencil buffer but not to color or depth.
 * 4 vertices + 6 indices, facing +Z. */
static void generate_portal_mask_quad(Vertex *verts, Uint32 *vert_count,
                                       Uint16 *indices, Uint32 *idx_count)
{
    float hw = PORTAL_WIDTH / 2.0f;
    float h  = PORTAL_HEIGHT;

    /* Quad corners — bottom-left to top-right, facing +Z */
    verts[0].position = vec3_create(-hw, 0.0f, 0.0f);
    verts[0].normal   = vec3_create(0.0f, 0.0f, 1.0f);

    verts[1].position = vec3_create( hw, 0.0f, 0.0f);
    verts[1].normal   = vec3_create(0.0f, 0.0f, 1.0f);

    verts[2].position = vec3_create( hw, h, 0.0f);
    verts[2].normal   = vec3_create(0.0f, 0.0f, 1.0f);

    verts[3].position = vec3_create(-hw, h, 0.0f);
    verts[3].normal   = vec3_create(0.0f, 0.0f, 1.0f);

    /* Two triangles */
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    indices[3] = 0; indices[4] = 2; indices[5] = 3;

    *vert_count = 4;
    *idx_count = 6;
}

/* ── End of Part A ────────────────────────────────────────────────────── */
/* ── Part B: SDL_AppInit ──────────────────────────────────────────────── */

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
        "Lesson 34 — Portals & Outlines",
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("ERROR: SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("ERROR: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->device = device;
    state->window = window;
    state->swapchain_format = swapchain_format;

    /* Set appstate early so SDL_AppQuit can clean up on partial init failure.
     * Since state was calloc'd, all pointers are NULL and SDL_AppQuit's
     * NULL checks will skip resources that haven't been created yet. */
    *appstate = state;

    /* ── 2. Depth-stencil format negotiation ─────────────────────────── */
    /* Prefer D24_UNORM_S8_UINT because it uses less memory (4 bytes per
     * pixel vs. 8 for D32_FLOAT_S8_UINT).  Not all GPUs support it, so
     * we fall back to D32_FLOAT_S8_UINT which is universally available. */

    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        SDL_Log("Depth-stencil format: D24_UNORM_S8_UINT");
    } else {
        state->depth_stencil_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        SDL_Log("Depth-stencil format: D32_FLOAT_S8_UINT (fallback)");
    }

    /* ── 3. Depth-stencil textures ───────────────────────────────────── */

    /* Main depth-stencil target — window-sized, contains the 8-bit stencil
     * channel used for portal masking and object outlines. */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = state->depth_stencil_fmt;
        ti.width = WINDOW_WIDTH;
        ti.height = WINDOW_HEIGHT;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        state->main_depth = SDL_CreateGPUTexture(device, &ti);
        if (!state->main_depth) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (main_depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Shadow map depth texture — larger resolution for crisp shadows.
     * Uses D32_FLOAT (no stencil) since shadow mapping only needs depth. */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = SHADOW_DEPTH_FMT;
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

    /* ── 4. Samplers ─────────────────────────────────────────────────── */

    /* Nearest-clamp sampler — used for shadow map depth comparison and
     * debug texture visualization.  Nearest filtering avoids interpolation
     * artifacts in the shadow map and preserves stencil debug values. */
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
            SDL_Log("ERROR: SDL_CreateGPUSampler failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 5. Shaders ──────────────────────────────────────────────────── */

    /* Scene vertex shader — transforms position by MVP, computes world
     * position for lighting, and projects into light space for shadows. */
    SDL_GPUShader *scene_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil),
        0, 0, 1);

    /* Scene fragment shader — Blinn-Phong with shadow mapping and tint. */
    SDL_GPUShader *scene_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil),
        1, 0, 1);

    /* Shadow vertex shader — only outputs clip position for depth pass. */
    SDL_GPUShader *shadow_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil, sizeof(shadow_vert_dxil),
        0, 0, 1);

    /* Shadow fragment shader — empty, the GPU writes depth automatically. */
    SDL_GPUShader *shadow_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil, sizeof(shadow_frag_dxil),
        0, 0, 0);

    /* Grid vertex shader — passes position to frag for grid pattern. */
    SDL_GPUShader *grid_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil, sizeof(grid_vert_dxil),
        0, 0, 1);

    /* Grid fragment shader — procedural grid lines + shadow mapping. */
    SDL_GPUShader *grid_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil, sizeof(grid_frag_dxil),
        1, 0, 1);

    /* Outline fragment shader — solid flat color for stencil outlines. */
    SDL_GPUShader *outline_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        outline_frag_spirv, sizeof(outline_frag_spirv),
        outline_frag_dxil, sizeof(outline_frag_dxil),
        0, 0, 1);

    /* Debug overlay vertex shader — generates fullscreen triangle from
     * SV_VertexID, no vertex buffers needed. */
    SDL_GPUShader *debug_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        debug_overlay_vert_spirv, sizeof(debug_overlay_vert_spirv),
        debug_overlay_vert_dxil, sizeof(debug_overlay_vert_dxil),
        0, 0, 0);

    /* Debug overlay fragment shader — maps stencil values to colors. */
    SDL_GPUShader *debug_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        debug_overlay_frag_spirv, sizeof(debug_overlay_frag_spirv),
        debug_overlay_frag_dxil, sizeof(debug_overlay_frag_dxil),
        1, 0, 0);

    if (!scene_vert || !scene_frag || !shadow_vert || !shadow_frag ||
        !grid_vert || !grid_frag || !outline_frag ||
        !debug_vert || !debug_frag) {
        SDL_Log("ERROR: One or more shaders failed to compile");
        return SDL_APP_FAILURE;
    }

    /* ── 6. Vertex input state (scene geometry) ──────────────────────── */
    /* All scene meshes (cubes, spheres, portal) share the same Vertex
     * layout: position (float3) + normal (float3) = 24 bytes per vertex. */

    SDL_GPUVertexBufferDescription scene_vb_desc;
    SDL_zero(scene_vb_desc);
    scene_vb_desc.slot = 0;
    scene_vb_desc.pitch = sizeof(Vertex);
    scene_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

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

    SDL_GPUVertexInputState scene_vertex_input;
    SDL_zero(scene_vertex_input);
    scene_vertex_input.vertex_buffer_descriptions = &scene_vb_desc;
    scene_vertex_input.num_vertex_buffers = 1;
    scene_vertex_input.vertex_attributes = scene_attrs;
    scene_vertex_input.num_vertex_attributes = 2;

    /* ── 7. Pipelines ────────────────────────────────────────────────── */
    /* Stencil testing requires many pipelines because stencil state is
     * baked into the pipeline state object (PSO).  Each different
     * combination of compare_op, pass_op, and write_mask needs its own
     * pipeline.  This is the cost of per-pixel masking. */

    SDL_GPUColorTargetDescription color_target;
    SDL_zero(color_target);
    color_target.format = swapchain_format;

    /* ── Pipeline 1: shadow_pipeline ─────────────────────────────────
     * Renders scene geometry into the shadow map depth buffer.
     * No color output, no stencil — shadow mapping only needs depth. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = shadow_vert;
        pi.fragment_shader = shadow_frag;
        pi.vertex_input_state = scene_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* Depth-only: write depth, test with LESS */
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        /* No color targets for shadow pass */
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = SHADOW_DEPTH_FMT;
        pi.target_info.has_depth_stencil_target = true;

        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 2: mask_pipeline ───────────────────────────────────
     * Draws the portal opening quad to write STENCIL_PORTAL into the
     * stencil buffer.  Color writes and depth writes are DISABLED —
     * this pass ONLY marks pixels in the stencil buffer.
     *
     * Stencil config: ALWAYS pass, REPLACE with reference value.
     * The reference value (STENCIL_PORTAL) is set at draw time via
     * SDL_SetStencilReference(). */
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

        /* Depth test enabled to respect scene depth ordering, but depth
         * write disabled so the mask quad does not pollute the depth
         * buffer — portal content will write its own depth later. */
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_write = false;

        /* Stencil: ALWAYS write the reference value into every pixel
         * covered by the portal quad. */
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_REPLACE;
        pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0xFF;

        /* Suppress all color output — mask is invisible */
        color_target.blend_state.color_write_mask = 0;
        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->mask_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);

        /* Restore color write mask for subsequent pipelines */
        color_target.blend_state.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
            SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    }

    /* ── Pipeline 3: portal_pipeline ─────────────────────────────────
     * Draws objects that exist INSIDE the portal world.  Only pixels
     * whose stencil value EQUALS STENCIL_PORTAL pass the test.
     *
     * This is the key insight of portal rendering: we draw a completely
     * different scene, but it only appears where the mask was written. */
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

        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_write = true;

        /* Stencil: only draw where stencil == reference (STENCIL_PORTAL).
         * Do NOT modify the stencil buffer — keep the mask intact. */
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
        pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0x00;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->portal_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 4: main_pipeline ───────────────────────────────────
     * Draws the main world objects.  Main cubes render BEFORE the portal
     * mask so they write depth first.  The mask's depth test then
     * prevents stencil from being written where a closer cube exists,
     * ensuring cubes in front of the portal correctly occlude it.
     *
     * No stencil test is needed — draw order handles the masking. */
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

        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->main_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 5: frame_pipeline ──────────────────────────────────
     * Draws the portal doorway frame (pillars + beam).  This geometry
     * is always visible regardless of stencil — it sits on top of
     * both worlds to frame the portal opening. */
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

        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->frame_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 6: outline_write_pipeline ──────────────────────────
     * First pass of object outlining: draw the object normally while
     * writing STENCIL_OUTLINE into the stencil buffer for every pixel
     * the object covers.  This creates a stencil silhouette of the
     * object that the outline pass will use as a mask.
     *
     * Uses LESS_OR_EQUAL because the cube was already drawn in the
     * main cube phase at the same depth.  LESS would fail on equal
     * depth, preventing the stencil write entirely. */
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

        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_write = true;

        /* Stencil: stamp STENCIL_OUTLINE onto every visible pixel */
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_REPLACE;
        pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0xFF;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->outline_write_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 7: outline_draw_pipeline ───────────────────────────
     * Second pass of object outlining: draw the same object scaled up
     * by OUTLINE_SCALE with a flat outline color.  The stencil test
     * rejects pixels where stencil == STENCIL_OUTLINE (the original
     * object silhouette), so only the expanded border remains visible.
     *
     * Depth test is DISABLED so the outline is visible even when the
     * scaled geometry is occluded.  Cull mode is NONE because the
     * scale transformation may flip triangle winding. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = scene_vert;
        pi.fragment_shader = outline_frag;
        pi.vertex_input_state = scene_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* Depth disabled — outline draws over everything */
        pi.depth_stencil_state.enable_depth_test = false;
        pi.depth_stencil_state.enable_depth_write = false;

        /* Stencil: only draw where stencil != STENCIL_OUTLINE, so the
         * outline appears only around the object's edges */
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_NOT_EQUAL;
        pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0x00;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->outline_draw_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Grid vertex input ───────────────────────────────────────────
     * The grid floor uses position-only vertices (vec3, 12 bytes).
     * No normal attribute — the grid shader computes lighting from
     * the known Y=0 ground plane normal. */

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

    /* ── Pipeline 8: grid_pipeline ───────────────────────────────────
     * Draws the main world grid floor.  Stencil test excludes pixels
     * marked as STENCIL_PORTAL so the main grid does not bleed into
     * the portal area. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = grid_vert;
        pi.fragment_shader = grid_frag;
        pi.vertex_input_state = grid_vertex_input;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* NONE cull — the floor quad is visible from both sides */
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* LESS_OR_EQUAL so the grid coplanar with other geometry
         * resolves correctly in the depth buffer */
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_write = true;

        /* Stencil: exclude portal area (stencil != STENCIL_PORTAL) */
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_NOT_EQUAL;
        pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0x00;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 9: grid_portal_pipeline ────────────────────────────
     * Draws the portal world grid floor.  Stencil test ONLY passes
     * where stencil == STENCIL_PORTAL, so this grid is confined to
     * the portal opening.  Uses EQUAL instead of NOT_EQUAL. */
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

        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_write = true;

        /* Stencil: only inside portal area (stencil == STENCIL_PORTAL) */
        pi.depth_stencil_state.enable_stencil_test = true;
        pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
        pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        pi.depth_stencil_state.compare_mask = 0xFF;
        pi.depth_stencil_state.write_mask = 0x00;

        pi.target_info.color_target_descriptions = &color_target;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = state->depth_stencil_fmt;
        pi.target_info.has_depth_stencil_target = true;

        state->grid_portal_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 10: debug_pipeline ─────────────────────────────────
     * Fullscreen overlay that visualizes stencil buffer contents as
     * colors.  No vertex input — the vertex shader generates a
     * fullscreen triangle from SV_VertexID.  Alpha blending lets the
     * scene show through. */
    {
        SDL_GPUColorTargetDescription debug_ct;
        SDL_zero(debug_ct);
        debug_ct.format = swapchain_format;
        debug_ct.blend_state.enable_blend = true;
        debug_ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        debug_ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        debug_ct.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        debug_ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        debug_ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        debug_ct.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        debug_ct.blend_state.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
            SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = debug_vert;
        pi.fragment_shader = debug_frag;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* No vertex input — fullscreen triangle from vertex ID */
        SDL_GPUVertexInputState empty_input;
        SDL_zero(empty_input);
        pi.vertex_input_state = empty_input;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* No depth or stencil — overlay draws on top unconditionally */
        pi.depth_stencil_state.enable_depth_test = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = false;

        pi.target_info.color_target_descriptions = &debug_ct;
        pi.target_info.num_color_targets = 1;
        pi.target_info.has_depth_stencil_target = false;

        state->debug_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* Verify all pipelines were created successfully */
    if (!state->shadow_pipeline || !state->mask_pipeline ||
        !state->portal_pipeline || !state->main_pipeline ||
        !state->frame_pipeline || !state->outline_write_pipeline ||
        !state->outline_draw_pipeline || !state->grid_pipeline ||
        !state->grid_portal_pipeline || !state->debug_pipeline) {
        SDL_Log("ERROR: One or more pipelines failed to create: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 8. Release shaders ──────────────────────────────────────────
     * Shader objects are only needed during pipeline creation.  Once
     * the pipelines are built, the compiled code lives inside the PSO
     * and the shader handles can be freed. */

    SDL_ReleaseGPUShader(device, scene_vert);
    SDL_ReleaseGPUShader(device, scene_frag);
    SDL_ReleaseGPUShader(device, shadow_vert);
    SDL_ReleaseGPUShader(device, shadow_frag);
    SDL_ReleaseGPUShader(device, grid_vert);
    SDL_ReleaseGPUShader(device, grid_frag);
    SDL_ReleaseGPUShader(device, outline_frag);
    SDL_ReleaseGPUShader(device, debug_vert);
    SDL_ReleaseGPUShader(device, debug_frag);

    /* ── 9. Generate and upload geometry ─────────────────────────────── */

    static Vertex temp_verts[MAX_VERTICES];
    static Uint16 temp_indices[MAX_INDICES];
    Uint32 vcount = 0;
    Uint32 icount = 0;

    /* Cube: 24 vertices, 36 indices */
    generate_cube(0.5f, temp_verts, &vcount, temp_indices, &icount);
    state->cube_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
        temp_verts, vcount * sizeof(Vertex));
    state->cube_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
        temp_indices, icount * sizeof(Uint16));
    state->cube_index_count = icount;

    /* Sphere: UV sphere for portal world objects */
    vcount = 0; icount = 0;
    generate_sphere(1.0f, temp_verts, &vcount, temp_indices, &icount);
    state->sphere_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
        temp_verts, vcount * sizeof(Vertex));
    state->sphere_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
        temp_indices, icount * sizeof(Uint16));
    state->sphere_index_count = icount;

    /* Portal frame: pillars + beam surrounding the portal opening */
    vcount = 0; icount = 0;
    generate_portal_frame(temp_verts, &vcount, temp_indices, &icount);
    state->portal_frame_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
        temp_verts, vcount * sizeof(Vertex));
    state->portal_frame_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
        temp_indices, icount * sizeof(Uint16));
    state->portal_frame_index_count = icount;

    /* Portal mask: invisible quad that fills the portal opening */
    vcount = 0; icount = 0;
    generate_portal_mask_quad(temp_verts, &vcount, temp_indices, &icount);
    state->portal_mask_vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
        temp_verts, vcount * sizeof(Vertex));
    state->portal_mask_ib = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
        temp_indices, icount * sizeof(Uint16));
    state->portal_mask_index_count = icount;

    /* Grid floor: large quad on XZ plane at Y=0 */
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
    }

    /* Verify all geometry buffers */
    if (!state->cube_vb || !state->cube_ib ||
        !state->sphere_vb || !state->sphere_ib ||
        !state->portal_frame_vb || !state->portal_frame_ib ||
        !state->portal_mask_vb || !state->portal_mask_ib ||
        !state->grid_vb || !state->grid_ib) {
        SDL_Log("ERROR: Failed to upload one or more geometry buffers");
        return SDL_APP_FAILURE;
    }

    /* ── 10. Scene objects ───────────────────────────────────────────── */

    /* Main world cubes — positioned around the scene.
     * Two cubes have outlines enabled to demonstrate the stencil
     * outline technique. */
    state->cubes[0] = (SceneObject){
        vec3_create(2.0f, 0.5f, -3.0f), 1.0f,
        {0.8f, 0.2f, 0.2f, 1.0f}, false, 0.0f, 0.0f, 0.0f
    };
    state->cubes[1] = (SceneObject){
        vec3_create(-1.0f, 0.5f, -2.0f), 1.0f,
        {0.2f, 0.3f, 0.8f, 1.0f}, true, 1.0f, 1.0f, 0.0f
    };
    state->cubes[2] = (SceneObject){
        vec3_create(-1.0f, 1.5f, -2.0f), 0.8f,
        {0.2f, 0.8f, 0.8f, 1.0f}, false, 0.0f, 0.0f, 0.0f
    };
    state->cubes[3] = (SceneObject){
        vec3_create(3.0f, 0.5f, 1.0f), 1.2f,
        {0.2f, 0.7f, 0.3f, 1.0f}, true, 0.0f, 1.0f, 0.3f
    };

    /* Portal world spheres — visible only through the portal opening */
    state->portal_spheres[0] = (SceneObject){
        vec3_create(0.0f, 1.0f, -7.0f), 0.8f,
        {1.0f, 0.8f, 0.2f, 1.0f}, false, 0.0f, 0.0f, 0.0f
    };
    state->portal_spheres[1] = (SceneObject){
        vec3_create(-1.5f, 0.6f, -6.0f), 0.6f,
        {0.8f, 0.2f, 0.8f, 1.0f}, false, 0.0f, 0.0f, 0.0f
    };
    state->portal_spheres[2] = (SceneObject){
        vec3_create(1.2f, 0.5f, -8.0f), 0.5f,
        {0.2f, 0.8f, 0.8f, 1.0f}, false, 0.0f, 0.0f, 0.0f
    };

    /* ── 11. Light setup ─────────────────────────────────────────────── */
    /* Directional light coming from upper-right-front.  The orthographic
     * projection captures a large area for shadow mapping. */

    state->light_dir = vec3_normalize(vec3_create(0.4f, -0.8f, -0.6f));
    vec3 light_pos = vec3_scale(state->light_dir, -30.0f);
    mat4 light_view = mat4_look_at(light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 60.0f);
    state->light_vp = mat4_multiply(light_proj, light_view);

    /* ── 12. Camera ──────────────────────────────────────────────────── */

    state->cam_position = vec3_create(1.0f, 2.5f, 5.0f);
    state->cam_yaw = -0.15f;
    state->cam_pitch = -0.25f;
    state->last_ticks = SDL_GetTicks();

    /* ── 13. Mouse capture ───────────────────────────────────────────── */
    /* Lock the mouse to the window for first-person camera control. */

    if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
        SDL_Log("WARN: SDL_SetWindowRelativeMouseMode failed: %s",
                SDL_GetError());
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

    /* ── 14. Done ────────────────────────────────────────────────────── */

    SDL_Log("Lesson 34 initialized: %d pipelines, %s depth-stencil",
            10, (state->depth_stencil_fmt == SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT)
            ? "D24S8" : "D32S8");
    return SDL_APP_CONTINUE;
}

/* ── End of Part B ────────────────────────────────────────────────────── */
/* ── Part C: Event handling, rendering, and cleanup ───────────────────── */

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
            /* Toggle mouse capture so the user can interact with the OS */
            state->mouse_captured = !state->mouse_captured;
            if (!SDL_SetWindowRelativeMouseMode(state->window, state->mouse_captured)) {
                SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            }
        } else if (event->key.scancode == SDL_SCANCODE_V) {
            /* Toggle stencil debug overlay */
            state->show_stencil_debug = !state->show_stencil_debug;
            SDL_Log("Stencil debug: %s",
                    state->show_stencil_debug ? "ON" : "OFF");
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            /* Clamp pitch to avoid gimbal-lock flipping */
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            state->mouse_captured = true;
            if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
                SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            }
        }
        break;

    default:
        break;
    }

#ifdef FORGE_CAPTURE
    /* capture has no event handler in this API version */
#endif

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
    if (dt > 0.1f) dt = 0.1f;  /* cap delta for alt-tab pauses */

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

    /* Shared scene constants */
    vec3 portal_pos = vec3_create(0.0f, 0.0f, -5.0f);
    vec3 light_dir  = state->light_dir;

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
        /* Window minimized or not ready — skip this frame */
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ── 5. Shadow pass ────────────────────────────────────────────── */
    /* Render main-world cubes and the portal frame into the shadow map.
     * No stencil is needed — this is depth-only for shadow mapping. */

    SDL_GPUDepthStencilTargetInfo shadow_ds;
    SDL_zero(shadow_ds);
    shadow_ds.texture       = state->shadow_depth;
    shadow_ds.load_op       = SDL_GPU_LOADOP_CLEAR;
    shadow_ds.store_op      = SDL_GPU_STOREOP_STORE;
    shadow_ds.clear_depth   = 1.0f;

    SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
        cmd, NULL, 0, &shadow_ds);
    SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

    /* Shadow pass: main-world cubes */
    for (int i = 0; i < CUBE_COUNT; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(state->cubes[i].position),
            mat4_scale_uniform(state->cubes[i].scale));
        mat4 shadow_mvp = mat4_multiply(state->light_vp, model);

        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_mvp, sizeof(shadow_mvp));

        SDL_GPUBufferBinding vb_bind = { state->cube_vb, 0 };
        SDL_BindGPUVertexBuffers(shadow_pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->cube_ib, 0 };
        SDL_BindGPUIndexBuffer(shadow_pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(shadow_pass, state->cube_index_count,
                                     1, 0, 0, 0);
    }

    /* Shadow pass: portal frame (it casts shadows too) */
    {
        mat4 model = mat4_translate(portal_pos);
        mat4 shadow_mvp = mat4_multiply(state->light_vp, model);

        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_mvp, sizeof(shadow_mvp));

        SDL_GPUBufferBinding vb_bind = { state->portal_frame_vb, 0 };
        SDL_BindGPUVertexBuffers(shadow_pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->portal_frame_ib, 0 };
        SDL_BindGPUIndexBuffer(shadow_pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(shadow_pass,
                                     state->portal_frame_index_count,
                                     1, 0, 0, 0);
    }

    SDL_EndGPURenderPass(shadow_pass);

    /* ── 6. Main scene render pass ─────────────────────────────────── */
    /* This single render pass contains all stencil phases.  The stencil
     * buffer starts cleared to 0.  Each phase sets its own pipeline,
     * which embeds a different stencil test/operation configuration. */

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture    = swapchain_tex;
    color_target.load_op    = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op   = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

    SDL_GPUDepthStencilTargetInfo ds_target;
    SDL_zero(ds_target);
    ds_target.texture           = state->main_depth;
    ds_target.load_op           = SDL_GPU_LOADOP_CLEAR;
    ds_target.store_op          = SDL_GPU_STOREOP_STORE;
    ds_target.clear_depth       = 1.0f;
    ds_target.stencil_load_op   = SDL_GPU_LOADOP_CLEAR;
    ds_target.stencil_store_op  = SDL_GPU_STOREOP_STORE;
    ds_target.clear_stencil     = 0;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
        cmd, &color_target, 1, &ds_target);

    /* ── Phase A: Main world cubes ────────────────────────────────
     * Draw main-world cubes FIRST so they write depth before the
     * portal mask.  This ensures cubes in front of the portal
     * correctly occlude it — the mask's depth test will fail where
     * a closer cube already wrote, preventing stencil from being
     * written there.  No stencil test is needed at this stage. */

    SDL_GPUTextureSamplerBinding shadow_bind;
    shadow_bind.texture = state->shadow_depth;
    shadow_bind.sampler = state->nearest_clamp;

    SDL_BindGPUGraphicsPipeline(pass, state->main_pipeline);

    /* Bind shadow map for main-world fragment sampling */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

    for (int i = 0; i < CUBE_COUNT; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(state->cubes[i].position),
            mat4_scale_uniform(state->cubes[i].scale));
        mat4 mvp  = mat4_multiply(cam_vp, model);
        mat4 lmvp = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        frag_u.base_color[0] = state->cubes[i].color[0];
        frag_u.base_color[1] = state->cubes[i].color[1];
        frag_u.base_color[2] = state->cubes[i].color[2];
        frag_u.base_color[3] = state->cubes[i].color[3];
        frag_u.eye_pos[0] = state->cam_position.x;
        frag_u.eye_pos[1] = state->cam_position.y;
        frag_u.eye_pos[2] = state->cam_position.z;
        frag_u.ambient       = 0.12f;
        frag_u.light_dir[0]  = light_dir.x;
        frag_u.light_dir[1]  = light_dir.y;
        frag_u.light_dir[2]  = light_dir.z;
        frag_u.light_dir[3]  = 0.0f;
        frag_u.light_color[0] = 1.0f;
        frag_u.light_color[1] = 1.0f;
        frag_u.light_color[2] = 1.0f;
        frag_u.light_intensity = 1.0f;
        frag_u.shininess     = 64.0f;
        frag_u.specular_str  = 0.4f;
        /* No tint for the main world */
        frag_u.tint[0] = 0.0f;
        frag_u.tint[1] = 0.0f;
        frag_u.tint[2] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        SDL_GPUBufferBinding vb_bind = { state->cube_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->cube_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->cube_index_count,
                                     1, 0, 0, 0);
    }

    /* ── Phase B: Portal mask write ────────────────────────────────
     * Draw an invisible quad at the portal opening.  The mask pipeline
     * writes STENCIL_PORTAL to the stencil buffer but disables color
     * writes and depth writes.  The mask uses depth LESS_OR_EQUAL so
     * it only writes stencil where no closer main-world geometry
     * already wrote depth.  After this phase, the portal region
     * in the stencil buffer contains STENCIL_PORTAL (1) only where
     * the portal opening is unoccluded. */

    SDL_BindGPUGraphicsPipeline(pass, state->mask_pipeline);
    SDL_SetGPUStencilReference(pass, STENCIL_PORTAL);

    /* Bind shadow map sampler — the mask pipeline uses scene_frag which
     * declares a sampler slot, even though color output is masked. */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

    {
        mat4 model = mat4_translate(portal_pos);
        mat4 mvp   = mat4_multiply(cam_vp, model);
        mat4 lmvp  = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        /* Fragment uniforms are required by the pipeline layout even though
         * color output is masked — push zeroed data to satisfy the binding */
        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        SDL_GPUBufferBinding vb_bind = { state->portal_mask_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->portal_mask_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->portal_mask_index_count,
                                     1, 0, 0, 0);
    }

    /* ── Phase C: Portal world (stencil == PORTAL) ─────────────────
     * Draw objects that should appear "inside" the portal.  The portal
     * pipeline passes only where stencil == STENCIL_PORTAL, so these
     * objects are visible only through the portal opening.  Portal
     * objects depth-test correctly against each other. */

    SDL_BindGPUGraphicsPipeline(pass, state->portal_pipeline);
    SDL_SetGPUStencilReference(pass, STENCIL_PORTAL);

    /* Re-bind shadow map for portal world fragment sampling */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

    for (int i = 0; i < PORTAL_SPHERE_COUNT; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(state->portal_spheres[i].position),
            mat4_scale_uniform(state->portal_spheres[i].scale));
        mat4 mvp  = mat4_multiply(cam_vp, model);
        mat4 lmvp = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        /* Warm-tinted lighting to distinguish the portal world */
        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        frag_u.base_color[0] = state->portal_spheres[i].color[0];
        frag_u.base_color[1] = state->portal_spheres[i].color[1];
        frag_u.base_color[2] = state->portal_spheres[i].color[2];
        frag_u.base_color[3] = state->portal_spheres[i].color[3];
        frag_u.eye_pos[0] = state->cam_position.x;
        frag_u.eye_pos[1] = state->cam_position.y;
        frag_u.eye_pos[2] = state->cam_position.z;
        frag_u.ambient       = 0.15f;
        frag_u.light_dir[0]  = light_dir.x;
        frag_u.light_dir[1]  = light_dir.y;
        frag_u.light_dir[2]  = light_dir.z;
        frag_u.light_dir[3]  = 0.0f;
        frag_u.light_color[0] = 1.0f;
        frag_u.light_color[1] = 0.95f;
        frag_u.light_color[2] = 0.85f;
        frag_u.light_intensity = 1.2f;
        frag_u.shininess     = 32.0f;
        frag_u.specular_str  = 0.5f;
        /* Warm orange tint for portal world ambient */
        frag_u.tint[0] = 0.3f;
        frag_u.tint[1] = 0.15f;
        frag_u.tint[2] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        SDL_GPUBufferBinding vb_bind = { state->sphere_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->sphere_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->sphere_index_count,
                                     1, 0, 0, 0);
    }

    /* ── Phase D: Grid floor ───────────────────────────────────────
     * The grid is drawn twice: once for the main world (stencil != PORTAL)
     * and once for the portal world (stencil == PORTAL) with a warm tint.
     * This shows how stencil can partition a single piece of geometry
     * into two visual regions rendered with different materials. */

    /* Shared grid vertex uniforms */
    GridVertUniforms grid_vert_u;
    grid_vert_u.vp       = cam_vp;
    grid_vert_u.light_vp = state->light_vp;

    /* Main world grid (stencil != PORTAL) — cool neutral color */
    SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);
    SDL_SetGPUStencilReference(pass, STENCIL_PORTAL);

    /* Bind shadow map for grid shadow sampling */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

    SDL_PushGPUVertexUniformData(cmd, 0, &grid_vert_u, sizeof(grid_vert_u));

    GridFragUniforms grid_frag_u;
    SDL_zero(grid_frag_u);
    grid_frag_u.line_color[0]  = 0.35f;
    grid_frag_u.line_color[1]  = 0.35f;
    grid_frag_u.line_color[2]  = 0.4f;
    grid_frag_u.line_color[3]  = 1.0f;
    grid_frag_u.bg_color[0]    = 0.08f;
    grid_frag_u.bg_color[1]    = 0.08f;
    grid_frag_u.bg_color[2]    = 0.1f;
    grid_frag_u.bg_color[3]    = 1.0f;
    grid_frag_u.light_dir[0]   = light_dir.x;
    grid_frag_u.light_dir[1]   = light_dir.y;
    grid_frag_u.light_dir[2]   = light_dir.z;
    grid_frag_u.light_intensity = 0.6f;
    grid_frag_u.eye_pos[0]     = state->cam_position.x;
    grid_frag_u.eye_pos[1]     = state->cam_position.y;
    grid_frag_u.eye_pos[2]     = state->cam_position.z;
    grid_frag_u.grid_spacing   = 1.0f;
    grid_frag_u.line_width     = 0.02f;
    grid_frag_u.fade_distance  = 40.0f;
    grid_frag_u.ambient        = 0.15f;
    /* No tint for main world grid */
    grid_frag_u.tint_color[0]  = 1.0f;
    grid_frag_u.tint_color[1]  = 1.0f;
    grid_frag_u.tint_color[2]  = 1.0f;
    grid_frag_u.tint_color[3]  = 1.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &grid_frag_u, sizeof(grid_frag_u));

    {
        SDL_GPUBufferBinding vb_bind = { state->grid_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->grid_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, GRID_INDEX_COUNT, 1, 0, 0, 0);
    }

    /* Portal world grid (stencil == PORTAL) — warm orange tint */
    SDL_BindGPUGraphicsPipeline(pass, state->grid_portal_pipeline);
    SDL_SetGPUStencilReference(pass, STENCIL_PORTAL);

    /* Re-bind shadow map and push vertex uniforms for portal grid */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);
    SDL_PushGPUVertexUniformData(cmd, 0, &grid_vert_u, sizeof(grid_vert_u));

    GridFragUniforms portal_grid_frag_u;
    SDL_memcpy(&portal_grid_frag_u, &grid_frag_u, sizeof(GridFragUniforms));
    /* Warm tint distinguishes the portal world floor */
    portal_grid_frag_u.tint_color[0] = 1.3f;
    portal_grid_frag_u.tint_color[1] = 0.9f;
    portal_grid_frag_u.tint_color[2] = 0.6f;
    portal_grid_frag_u.tint_color[3] = 1.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &portal_grid_frag_u,
                                    sizeof(portal_grid_frag_u));

    {
        SDL_GPUBufferBinding vb_bind = { state->grid_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->grid_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, GRID_INDEX_COUNT, 1, 0, 0, 0);
    }

    /* ── Phase E: Portal frame ─────────────────────────────────────
     * The frame is drawn with stencil ALWAYS so it appears on top of
     * both the main world and the portal world.  It provides a visible
     * border around the portal opening and anchors the effect visually. */

    SDL_BindGPUGraphicsPipeline(pass, state->frame_pipeline);

    /* Re-bind shadow map for frame lighting */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

    {
        mat4 model = mat4_translate(portal_pos);
        mat4 mvp   = mat4_multiply(cam_vp, model);
        mat4 lmvp  = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        /* Neutral grey color for the portal frame */
        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        frag_u.base_color[0] = 0.5f;
        frag_u.base_color[1] = 0.5f;
        frag_u.base_color[2] = 0.5f;
        frag_u.base_color[3] = 1.0f;
        frag_u.eye_pos[0] = state->cam_position.x;
        frag_u.eye_pos[1] = state->cam_position.y;
        frag_u.eye_pos[2] = state->cam_position.z;
        frag_u.ambient       = 0.15f;
        frag_u.light_dir[0]  = light_dir.x;
        frag_u.light_dir[1]  = light_dir.y;
        frag_u.light_dir[2]  = light_dir.z;
        frag_u.light_dir[3]  = 0.0f;
        frag_u.light_color[0] = 1.0f;
        frag_u.light_color[1] = 1.0f;
        frag_u.light_color[2] = 1.0f;
        frag_u.light_intensity = 1.0f;
        frag_u.shininess     = 16.0f;
        frag_u.specular_str  = 0.3f;
        frag_u.tint[0] = 0.0f;
        frag_u.tint[1] = 0.0f;
        frag_u.tint[2] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        SDL_GPUBufferBinding vb_bind = { state->portal_frame_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->portal_frame_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->portal_frame_index_count,
                                     1, 0, 0, 0);
    }

    /* ── Phase F: Outline pass ─────────────────────────────────────
     * Object outlines use a two-step stencil technique:
     * Step 1 — Draw the cube normally with outline_write_pipeline.
     *          This pipeline uses stencil REPLACE to write
     *          STENCIL_OUTLINE wherever the cube's fragments pass.
     * Step 2 — Draw the cube again, slightly scaled up, with
     *          outline_draw_pipeline.  This pipeline uses stencil
     *          NOT_EQUAL against STENCIL_OUTLINE, so the enlarged
     *          cube is visible ONLY in the thin border region that
     *          extends beyond the original cube's silhouette.
     *          The outline_frag shader outputs a solid color. */

    /* Re-bind shadow map for outline base pass lighting */
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

    for (int i = 0; i < CUBE_COUNT; i++) {
        if (!state->cubes[i].outlined) continue;

        /* Step 1: Draw the cube, writing STENCIL_OUTLINE to stencil */
        SDL_BindGPUGraphicsPipeline(pass, state->outline_write_pipeline);
        SDL_SetGPUStencilReference(pass, STENCIL_OUTLINE);

        mat4 model = mat4_multiply(
            mat4_translate(state->cubes[i].position),
            mat4_scale_uniform(state->cubes[i].scale));
        mat4 mvp  = mat4_multiply(cam_vp, model);
        mat4 lmvp = mat4_multiply(state->light_vp, model);

        SceneVertUniforms vert_u;
        vert_u.mvp      = mvp;
        vert_u.model    = model;
        vert_u.light_vp = lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        /* Use the cube's own color for the base draw */
        SceneFragUniforms frag_u;
        SDL_zero(frag_u);
        frag_u.base_color[0] = state->cubes[i].color[0];
        frag_u.base_color[1] = state->cubes[i].color[1];
        frag_u.base_color[2] = state->cubes[i].color[2];
        frag_u.base_color[3] = state->cubes[i].color[3];
        frag_u.eye_pos[0] = state->cam_position.x;
        frag_u.eye_pos[1] = state->cam_position.y;
        frag_u.eye_pos[2] = state->cam_position.z;
        frag_u.ambient       = 0.12f;
        frag_u.light_dir[0]  = light_dir.x;
        frag_u.light_dir[1]  = light_dir.y;
        frag_u.light_dir[2]  = light_dir.z;
        frag_u.light_dir[3]  = 0.0f;
        frag_u.light_color[0] = 1.0f;
        frag_u.light_color[1] = 1.0f;
        frag_u.light_color[2] = 1.0f;
        frag_u.light_intensity = 1.0f;
        frag_u.shininess     = 64.0f;
        frag_u.specular_str  = 0.4f;
        frag_u.tint[0] = 0.0f;
        frag_u.tint[1] = 0.0f;
        frag_u.tint[2] = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

        SDL_GPUBufferBinding vb_bind = { state->cube_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_GPUBufferBinding ib_bind = { state->cube_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->cube_index_count,
                                     1, 0, 0, 0);

        /* Step 2: Draw the cube scaled up — only the border ring passes
         * because the interior was already marked with STENCIL_OUTLINE */
        SDL_BindGPUGraphicsPipeline(pass, state->outline_draw_pipeline);
        SDL_SetGPUStencilReference(pass, STENCIL_OUTLINE);

        /* Scale the cube slightly larger to create the outline border.
         * The scale is applied uniformly around the cube's center position. */
        mat4 outline_model = mat4_multiply(
            mat4_translate(state->cubes[i].position),
            mat4_multiply(
                mat4_scale_uniform(state->cubes[i].scale * OUTLINE_SCALE),
                mat4_identity()));
        mat4 outline_mvp  = mat4_multiply(cam_vp, outline_model);
        mat4 outline_lmvp = mat4_multiply(state->light_vp, outline_model);

        SceneVertUniforms outline_vert_u;
        outline_vert_u.mvp      = outline_mvp;
        outline_vert_u.model    = outline_model;
        outline_vert_u.light_vp = outline_lmvp;
        SDL_PushGPUVertexUniformData(cmd, 0, &outline_vert_u,
                                      sizeof(outline_vert_u));

        /* Solid outline color from the object's configuration */
        OutlineFragUniforms outline_frag_u;
        outline_frag_u.outline_color[0] = state->cubes[i].outline_r;
        outline_frag_u.outline_color[1] = state->cubes[i].outline_g;
        outline_frag_u.outline_color[2] = state->cubes[i].outline_b;
        outline_frag_u.outline_color[3] = 1.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &outline_frag_u,
                                        sizeof(outline_frag_u));

        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);
        SDL_BindGPUIndexBuffer(pass, &ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, state->cube_index_count,
                                     1, 0, 0, 0);
    }

    SDL_EndGPURenderPass(pass);

    /* ── 7. Debug overlay ──────────────────────────────────────────
     * When enabled with 'V', this re-renders key stencil-contributing
     * geometry with flat diagnostic colors so the user can see which
     * regions of the screen have stencil values written.
     *
     * A full GPU stencil readback would require a blit from the
     * depth-stencil texture to a staging buffer, which is complex
     * and not supported on all backends.  Instead, we approximate
     * by re-drawing the portal mask and outlined objects with
     * translucent flat colors on a second pass that loads (preserves)
     * the existing framebuffer content.
     *
     * This approach is deliberately simple — a production engine would
     * use compute shaders or platform-specific readback.  See the
     * README exercises for ideas on extending this. */
    if (state->show_stencil_debug) {
        SDL_GPUColorTargetInfo dbg_color;
        SDL_zero(dbg_color);
        dbg_color.texture  = swapchain_tex;
        dbg_color.load_op  = SDL_GPU_LOADOP_LOAD;
        dbg_color.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *dbg_pass = SDL_BeginGPURenderPass(
            cmd, &dbg_color, 1, NULL);

        /* Bind the debug overlay pipeline (fullscreen alpha-blended) */
        SDL_BindGPUGraphicsPipeline(dbg_pass, state->debug_pipeline);

        /* Re-draw portal mask area with translucent red to indicate
         * where STENCIL_PORTAL was written */
        {
            mat4 model = mat4_translate(portal_pos);
            mat4 mvp   = mat4_multiply(cam_vp, model);

            SDL_PushGPUVertexUniformData(cmd, 0, &mvp, sizeof(mvp));

            /* Push debug tint color: translucent red for portal region */
            float dbg_tint[4] = { 0.8f, 0.1f, 0.1f, 0.35f };
            SDL_PushGPUFragmentUniformData(cmd, 0, dbg_tint, sizeof(dbg_tint));

            SDL_GPUBufferBinding vb_bind = { state->portal_mask_vb, 0 };
            SDL_BindGPUVertexBuffers(dbg_pass, 0, &vb_bind, 1);
            SDL_GPUBufferBinding ib_bind = { state->portal_mask_ib, 0 };
            SDL_BindGPUIndexBuffer(dbg_pass, &ib_bind,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);
            SDL_DrawGPUIndexedPrimitives(dbg_pass,
                                         state->portal_mask_index_count,
                                         1, 0, 0, 0);
        }

        /* Re-draw outlined cubes with translucent green to indicate
         * where STENCIL_OUTLINE was written */
        for (int i = 0; i < CUBE_COUNT; i++) {
            if (!state->cubes[i].outlined) continue;

            mat4 model = mat4_multiply(
                mat4_translate(state->cubes[i].position),
                mat4_scale_uniform(state->cubes[i].scale));
            mat4 mvp = mat4_multiply(cam_vp, model);

            SDL_PushGPUVertexUniformData(cmd, 0, &mvp, sizeof(mvp));

            float dbg_tint[4] = { 0.1f, 0.8f, 0.2f, 0.35f };
            SDL_PushGPUFragmentUniformData(cmd, 0, dbg_tint, sizeof(dbg_tint));

            SDL_GPUBufferBinding vb_bind = { state->cube_vb, 0 };
            SDL_BindGPUVertexBuffers(dbg_pass, 0, &vb_bind, 1);
            SDL_GPUBufferBinding ib_bind = { state->cube_ib, 0 };
            SDL_BindGPUIndexBuffer(dbg_pass, &ib_bind,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);
            SDL_DrawGPUIndexedPrimitives(dbg_pass, state->cube_index_count,
                                         1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(dbg_pass);
    }

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

/* ── SDL_AppQuit ─────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    /* Wait for all GPU work to finish before releasing resources */
    SDL_WaitForGPUIdle(state->device);

    /* Release pipelines (10 total — one per stencil configuration) */
    if (state->shadow_pipeline)       SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->mask_pipeline)         SDL_ReleaseGPUGraphicsPipeline(state->device, state->mask_pipeline);
    if (state->portal_pipeline)       SDL_ReleaseGPUGraphicsPipeline(state->device, state->portal_pipeline);
    if (state->main_pipeline)         SDL_ReleaseGPUGraphicsPipeline(state->device, state->main_pipeline);
    if (state->frame_pipeline)        SDL_ReleaseGPUGraphicsPipeline(state->device, state->frame_pipeline);
    if (state->outline_write_pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, state->outline_write_pipeline);
    if (state->outline_draw_pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, state->outline_draw_pipeline);
    if (state->grid_pipeline)         SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->grid_portal_pipeline)  SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_portal_pipeline);
    if (state->debug_pipeline)        SDL_ReleaseGPUGraphicsPipeline(state->device, state->debug_pipeline);

    /* Release geometry buffers (vertex + index for each mesh) */
    if (state->cube_vb)          SDL_ReleaseGPUBuffer(state->device, state->cube_vb);
    if (state->cube_ib)          SDL_ReleaseGPUBuffer(state->device, state->cube_ib);
    if (state->sphere_vb)        SDL_ReleaseGPUBuffer(state->device, state->sphere_vb);
    if (state->sphere_ib)        SDL_ReleaseGPUBuffer(state->device, state->sphere_ib);
    if (state->portal_frame_vb)  SDL_ReleaseGPUBuffer(state->device, state->portal_frame_vb);
    if (state->portal_frame_ib)  SDL_ReleaseGPUBuffer(state->device, state->portal_frame_ib);
    if (state->portal_mask_vb)   SDL_ReleaseGPUBuffer(state->device, state->portal_mask_vb);
    if (state->portal_mask_ib)   SDL_ReleaseGPUBuffer(state->device, state->portal_mask_ib);
    if (state->grid_vb)          SDL_ReleaseGPUBuffer(state->device, state->grid_vb);
    if (state->grid_ib)          SDL_ReleaseGPUBuffer(state->device, state->grid_ib);

    /* Release samplers */
    if (state->nearest_clamp)    SDL_ReleaseGPUSampler(state->device, state->nearest_clamp);
    if (state->debug_sampler)    SDL_ReleaseGPUSampler(state->device, state->debug_sampler);

    /* Release render target textures */
    if (state->shadow_depth)     SDL_ReleaseGPUTexture(state->device, state->shadow_depth);
    if (state->main_depth)       SDL_ReleaseGPUTexture(state->device, state->main_depth);
    if (state->debug_texture)    SDL_ReleaseGPUTexture(state->device, state->debug_texture);

    /* Destroy window and device */
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
