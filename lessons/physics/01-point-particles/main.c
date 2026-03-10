/*
 * Physics Lesson 01 — Point Particles
 *
 * Demonstrates: Symplectic Euler integration, gravity, drag, velocity damping,
 * sphere-plane collision with restitution, fixed timestep with accumulator.
 *
 * 20 spheres drop from random heights, bounce on a ground plane with varying
 * restitution. Sphere color maps to velocity magnitude — red at rest, blue
 * at high speed. The simulation uses a fixed 60 Hz timestep decoupled from
 * rendering, with interpolation for smooth visuals.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around
 *   Space             — pause / resume
 *   R                 — reset simulation
 *   T                 — toggle slow motion
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>
#include <string.h>

#include "math/forge_math.h"
#include "physics/forge_physics.h"
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecode ─────────────────────────────────────── */

#include "shaders/compiled/scene_vert_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_frag_dxil.h"
#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"
#include "shaders/compiled/shadow_vert_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────── */

#define WINDOW_WIDTH   1280
#define WINDOW_HEIGHT  720
#define WINDOW_TITLE   "Physics Lesson 01 — Point Particles"

/* Physics simulation */
#define PHYSICS_DT        (1.0f / 60.0f)
#define NUM_PARTICLES     20
#define GRAVITY_Y        -9.81f
#define DRAG_COEFF        0.02f
#define GROUND_Y          0.0f
#define SPHERE_RADIUS     0.3f
#define SPAWN_HEIGHT_MIN  3.0f
#define SPAWN_HEIGHT_MAX  12.0f
#define SPAWN_SPREAD      8.0f
#define RESTITUTION_MIN   0.3f
#define RESTITUTION_MAX   0.9f

/* Shadow map */
#define SHADOW_MAP_SIZE   2048
#define SHADOW_DEPTH_BIAS_CONSTANT  2.0f
#define SHADOW_DEPTH_BIAS_SLOPE     2.0f

/* Camera */
#define CAM_START_X       0.0f
#define CAM_START_Y       4.0f
#define CAM_START_Z       12.0f
#define CAM_YAW           0.0f
#define CAM_PITCH        -0.2f
#define CAM_SPEED         5.0f
#define MOUSE_SENSITIVITY 0.003f
#define PITCH_CLAMP       1.5f
#define FOV_DEG           60
#define NEAR_PLANE        0.1f
#define FAR_PLANE         200.0f

/* Light direction (world space, normalized at init) */
#define LIGHT_DIR_X       0.4f
#define LIGHT_DIR_Y      -0.8f
#define LIGHT_DIR_Z      -0.6f
#define LIGHT_INTENSITY   1.2f
#define AMBIENT_STRENGTH  0.15f

/* Grid floor */
#define GRID_HALF_SIZE   20.0f
#define GRID_SPACING      1.0f
#define GRID_LINE_WIDTH   0.02f
#define GRID_FADE_DIST   30.0f

/* Shadow map light orthographic projection bounds */
#define SHADOW_ORTHO_SIZE 15.0f
#define SHADOW_NEAR        0.1f
#define SHADOW_FAR        50.0f
#define SHADOW_HEIGHT     20.0f

/* Delta-time clamp to prevent physics explosion after alt-tab */
#define MAX_DELTA_TIME     0.1f

/* Minimum velocity magnitude for movement detection */
#define VELOCITY_EPSILON   0.001f

/* Specular lighting */
#define SPECULAR_SHININESS 32.0f
#define SPECULAR_STRENGTH  0.5f

/* Velocity-to-color mapping */
#define VELOCITY_COLOR_MAX  10.0f   /* speed (m/s) mapped to "max" color */

/* Particle defaults */
#define PARTICLE_MASS       1.0f    /* kg */
#define PARTICLE_DAMPING    0.01f   /* velocity damping per step [0..1] */

/* Sphere mesh resolution */
#define SPHERE_SLICES       32
#define SPHERE_STACKS       16

/* Slow motion */
#define SLOW_MOTION_FACTOR  0.5f    /* dt multiplier when slow motion is on (half speed) */

/* Pseudo-random number generation */
#define RNG_PRECISION       10000   /* modulus for float conversion */

/* Fallback aspect ratio when window size is unavailable */
#define DEFAULT_ASPECT      (16.0f / 9.0f)

/* Light color (warm white) */
#define LIGHT_COLOR_R  1.0f
#define LIGHT_COLOR_G  0.95f
#define LIGHT_COLOR_B  0.9f

/* Scene clear color (near-black with slight blue tint) */
#define CLEAR_COLOR_R 0.05f
#define CLEAR_COLOR_G 0.05f
#define CLEAR_COLOR_B 0.08f

/* Grid colors */
#define GRID_LINE_R   0.4f
#define GRID_LINE_G   0.4f
#define GRID_LINE_B   0.5f
#define GRID_BG_R     0.08f
#define GRID_BG_G     0.08f
#define GRID_BG_B     0.1f

/* ── Types ────────────────────────────────────────────────────────── */

/* Interleaved vertex for scene geometry (position + normal). */
typedef struct Vertex {
    vec3 position;  /* object-space vertex position */
    vec3 normal;    /* object-space vertex normal   */
} Vertex;

/* Vertex shader uniforms — 3 matrices, 192 bytes.
 * Must match scene.vert.hlsl cbuffer SceneVertUniforms. */
typedef struct SceneVertUniforms {
    mat4 mvp;       /* model-view-projection */
    mat4 model;     /* world transform       */
    mat4 light_vp;  /* light VP * model      */
} SceneVertUniforms;

/* Fragment shader uniforms — must match scene.frag.hlsl SceneFragUniforms.
 * HLSL packing: float4-aligned. Total 80 bytes. */
typedef struct SceneFragUniforms {
    float base_color[4];     /* material RGBA                        */
    float eye_pos[3];        /* camera position                      */
    float ambient;           /* ambient intensity                    */
    float light_dir[4];      /* directional light direction          */
    float light_color[3];    /* light RGB                            */
    float light_intensity;   /* light brightness                     */
    float shininess;         /* specular exponent                    */
    float specular_str;      /* specular strength                    */
    float _pad[2];           /* align to 16 bytes                    */
} SceneFragUniforms;

/* Grid vertex shader uniforms — 2 matrices, 128 bytes.
 * Must match grid.vert.hlsl cbuffer GridVertUniforms. */
typedef struct GridVertUniforms {
    mat4 vp;        /* view-projection       */
    mat4 light_vp;  /* light view-projection */
} GridVertUniforms;

/* Grid fragment shader uniforms — must match grid.frag.hlsl.
 * Total 80 bytes. */
typedef struct GridFragUniforms {
    float line_color[4];     /* grid line color                      */
    float bg_color[4];       /* background surface color             */
    float light_dir[3];      /* directional light direction          */
    float light_intensity;   /* light brightness                     */
    float eye_pos[3];        /* camera position                      */
    float grid_spacing;      /* world units between lines            */
    float line_width;        /* line thickness [0..0.5]              */
    float fade_distance;     /* distance where grid fades            */
    float ambient;           /* ambient intensity                    */
    float _pad;              /* align to 16 bytes                    */
} GridFragUniforms;

/* Shadow pass vertex uniforms — 1 matrix, 64 bytes.
 * Must match shadow.vert.hlsl cbuffer ShadowUniforms. */
typedef struct ShadowVertUniforms {
    mat4 light_vp;  /* light VP * model */
} ShadowVertUniforms;

/* Application state — persists across all SDL callbacks. */
typedef struct app_state {
    SDL_Window    *window;   /* main application window                */
    SDL_GPUDevice *device;   /* GPU device for all rendering           */

    /* Pipelines */
    SDL_GPUGraphicsPipeline *scene_pipeline;  /* Blinn-Phong lit scene   */
    SDL_GPUGraphicsPipeline *grid_pipeline;   /* procedural grid floor   */
    SDL_GPUGraphicsPipeline *shadow_pipeline; /* depth-only shadow pass  */

    /* GPU resources */
    SDL_GPUBuffer  *sphere_vb;       /* sphere vertex buffer (pos+normal)  */
    SDL_GPUBuffer  *sphere_ib;       /* sphere index buffer (uint16)       */
    SDL_GPUBuffer  *grid_vb;         /* grid quad vertex buffer            */
    SDL_GPUBuffer  *grid_ib;         /* grid quad index buffer             */
    SDL_GPUTexture *depth_tex;       /* scene depth buffer (D32_FLOAT)     */
    Uint32          depth_width;     /* current depth texture width         */
    Uint32          depth_height;    /* current depth texture height        */
    SDL_GPUTextureFormat depth_fmt;  /* scene depth format for recreation   */
    SDL_GPUTexture *shadow_map;      /* directional shadow depth texture   */
    SDL_GPUSampler *shadow_sampler;  /* comparison sampler for PCF shadows */
    int             sphere_index_count; /* number of indices for sphere draw */

    /* Camera */
    vec3  cam_position;    /* world-space camera position             */
    float cam_yaw;         /* horizontal rotation (radians, 0 = +Z)  */
    float cam_pitch;       /* vertical rotation (radians, ±PITCH_CLAMP) */
    bool  mouse_captured;  /* true when mouse is in relative mode     */

    /* Timing */
    Uint64 last_ticks;     /* SDL ticks at start of previous frame    */
    float  accumulator;    /* leftover time for fixed-step physics (s) */
    float  sim_time;       /* total simulated time (s)                */
    bool   paused;         /* true = physics frozen, camera still works */
    bool   slow_motion;    /* true = physics runs at SLOW_MOTION_FACTOR */

    /* Light */
    vec3 light_dir;        /* normalized directional light direction  */
    mat4 light_vp;         /* precomputed light view-projection       */

    /* Physics particles */
    ForgePhysicsParticle particles[NUM_PARTICLES];         /* active state */
    ForgePhysicsParticle initial_particles[NUM_PARTICLES]; /* for reset    */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;  /* screenshot / GIF capture state          */
#endif
} app_state;

/* ── Helper: create_shader ────────────────────────────────────────── */

/* Create a GPU shader from pre-compiled SPIRV and DXIL bytecode.
 * Automatically selects the correct format based on the GPU backend. */
static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
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

/* ── Helper: upload_gpu_buffer ────────────────────────────────────── */

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

/* ── Helper: velocity_to_color ────────────────────────────────────── */

/* Map velocity magnitude to a color gradient.
 * Rest (0 m/s) → warm red, mid speed (5 m/s) → yellow,
 * high speed (10+ m/s) → cool blue. */
static void velocity_to_color(float speed, float out_color[4])
{
    /* Normalize speed to [0, 1] range */
    float t = speed / VELOCITY_COLOR_MAX;
    if (t > 1.0f) t = 1.0f;

    if (t < 0.5f) {
        /* Red → Yellow */
        float s = t * 2.0f;
        out_color[0] = 1.0f;
        out_color[1] = s;
        out_color[2] = 0.0f;
    } else {
        /* Yellow → Blue */
        float s = (t - 0.5f) * 2.0f;
        out_color[0] = 1.0f - s;
        out_color[1] = 1.0f - s;
        out_color[2] = s;
    }
    out_color[3] = 1.0f;
}

/* ── Helper: init_particles ───────────────────────────────────────── */

/* Initialize particles with spread-out positions and varying restitution.
 * Uses a simple LCG seeded from the particle index for deterministic
 * placement — no rand() dependency. */
static void init_particles(ForgePhysicsParticle *particles,
                           ForgePhysicsParticle *initial)
{
    for (int i = 0; i < NUM_PARTICLES; i++) {
        /* Deterministic pseudo-random from index */
        unsigned int seed = (unsigned int)(i * 2654435761u);
        float fx = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float fz = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float fy = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION);
        seed = seed * 1664525u + 1013904223u;
        float fr = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION);

        float x = fx * SPAWN_SPREAD;
        float y = SPAWN_HEIGHT_MIN + fy * (SPAWN_HEIGHT_MAX - SPAWN_HEIGHT_MIN);
        float z = fz * SPAWN_SPREAD;
        float restitution = RESTITUTION_MIN + fr * (RESTITUTION_MAX - RESTITUTION_MIN);

        particles[i] = forge_physics_particle_create(
            vec3_create(x, y, z),
            PARTICLE_MASS,      /* mass: 1 kg */
            PARTICLE_DAMPING,   /* velocity damping */
            restitution,
            SPHERE_RADIUS);

        initial[i] = particles[i];  /* save for reset */
    }
}
/* ── SDL_AppInit ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
#ifndef FORGE_CAPTURE
    (void)argc;
    (void)argv;
#endif

    /* ── 1. Allocate and zero app_state ────────────────────────────── */

    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        return SDL_APP_FAILURE;
    }

    /* Assign early so SDL_AppQuit handles cleanup on any failure path.
     * SDL guarantees SDL_AppQuit is called even when SDL_AppInit returns
     * SDL_APP_FAILURE, and our quit function NULL-checks every resource. */
    *appstate = state;

    /* ── 2. Create window ──────────────────────────────────────────── */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_Log("ERROR: SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    state->window = window;

    /* ── 3. Create GPU device ──────────────────────────────────────── */

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true, NULL);
    if (!device) {
        SDL_Log("ERROR: SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    state->device = device;

    /* ── 4. Claim window ───────────────────────────────────────────── */

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("ERROR: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 5. Swapchain: SDR_LINEAR for correct gamma ────────────────── */

    if (SDL_WindowSupportsGPUSwapchainComposition(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("ERROR: SDL_SetGPUSwapchainParameters (SDR_LINEAR) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_fmt =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 6. Shadow depth format negotiation ────────────────────────── */

    /* Shadow depth: D32_FLOAT with SAMPLER usage — sampled in scene
     * and grid fragment shaders for percentage-closer shadow mapping. */
    SDL_GPUTextureFormat shadow_map_fmt;

    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        shadow_map_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        SDL_Log("Shadow depth format: D32_FLOAT");
    } else if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        shadow_map_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        SDL_Log("Shadow depth format: D16_UNORM (fallback)");
    } else {
        SDL_Log("ERROR: No shadow depth format supports DEPTH_STENCIL_TARGET | SAMPLER");
        return SDL_APP_FAILURE;
    }

    /* Scene depth: D32_FLOAT (depth-only, no stencil needed for this
     * lesson).  Only used as a DEPTH_STENCIL_TARGET, not sampled. */
    SDL_GPUTextureFormat scene_depth_fmt;

    if (SDL_GPUTextureSupportsFormat(device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        scene_depth_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    } else {
        SDL_Log("ERROR: D32_FLOAT not supported for DEPTH_STENCIL_TARGET");
        return SDL_APP_FAILURE;
    }

    /* ── 7. Load shaders (6 total: 3 vertex + 3 fragment) ─────────── */

    /* Scene vertex: transforms position by MVP, computes world position
     * for lighting, projects into light space for shadow mapping.
     * 0 samplers, 0 storage, 1 uniform buffer (192 bytes: 3 mat4s). */
    SDL_GPUShader *scene_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil),
        0, 1);

    /* Scene fragment: Blinn-Phong lighting with shadow map sampling.
     * 1 sampler (shadow map), 0 storage, 1 uniform buffer. */
    SDL_GPUShader *scene_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil),
        1, 1);

    /* Shadow vertex: only outputs clip position for depth-only pass.
     * 0 samplers, 0 storage, 1 uniform buffer (64 bytes: 1 mat4). */
    SDL_GPUShader *shadow_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil, sizeof(shadow_vert_dxil),
        0, 1);

    /* Shadow fragment: empty body — GPU writes depth automatically.
     * 0 samplers, 0 storage, 0 uniform buffers. */
    SDL_GPUShader *shadow_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil, sizeof(shadow_frag_dxil),
        0, 0);

    /* Grid vertex: transforms world-space quad to clip space, passes
     * world position for procedural grid pattern and shadow coordinates.
     * 0 samplers, 0 storage, 1 uniform buffer (128 bytes: 2 mat4s). */
    SDL_GPUShader *grid_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil, sizeof(grid_vert_dxil),
        0, 1);

    /* Grid fragment: procedural grid lines with shadow map sampling.
     * 1 sampler (shadow map), 0 storage, 1 uniform buffer (80 bytes). */
    SDL_GPUShader *grid_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil, sizeof(grid_frag_dxil),
        1, 1);

    /* Verify all shaders loaded successfully */
    if (!scene_vert || !scene_frag || !shadow_vert || !shadow_frag ||
        !grid_vert || !grid_frag) {
        SDL_Log("ERROR: One or more shaders failed to compile");
        if (scene_vert)  SDL_ReleaseGPUShader(device, scene_vert);
        if (scene_frag)  SDL_ReleaseGPUShader(device, scene_frag);
        if (shadow_vert) SDL_ReleaseGPUShader(device, shadow_vert);
        if (shadow_frag) SDL_ReleaseGPUShader(device, shadow_frag);
        if (grid_vert)   SDL_ReleaseGPUShader(device, grid_vert);
        if (grid_frag)   SDL_ReleaseGPUShader(device, grid_frag);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Vertex input state descriptions ────────────────────────── */

    /* Full vertex input (position + normal) — used by the scene pipeline
     * for Blinn-Phong lighting which requires surface normals. */
    SDL_GPUVertexBufferDescription vb_full;
    SDL_zero(vb_full);
    vb_full.slot = 0;
    vb_full.pitch = sizeof(Vertex);
    vb_full.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute full_attrs[2];
    SDL_zero(full_attrs);
    full_attrs[0].location = 0;       /* position */
    full_attrs[0].buffer_slot = 0;
    full_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    full_attrs[0].offset = offsetof(Vertex, position);

    full_attrs[1].location = 1;       /* normal */
    full_attrs[1].buffer_slot = 0;
    full_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    full_attrs[1].offset = offsetof(Vertex, normal);

    SDL_GPUVertexInputState full_vertex_input;
    SDL_zero(full_vertex_input);
    full_vertex_input.vertex_buffer_descriptions = &vb_full;
    full_vertex_input.num_vertex_buffers = 1;
    full_vertex_input.vertex_attributes = full_attrs;
    full_vertex_input.num_vertex_attributes = 2;

    /* Position-only vertex input — used by shadow and grid pipelines.
     * Same stride as full (sizeof(Vertex)) but only reads position;
     * the normal bytes are skipped by the GPU. */
    SDL_GPUVertexBufferDescription vb_pos;
    SDL_zero(vb_pos);
    vb_pos.slot = 0;
    vb_pos.pitch = sizeof(Vertex);
    vb_pos.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute pos_attr;
    SDL_zero(pos_attr);
    pos_attr.location = 0;            /* position only */
    pos_attr.buffer_slot = 0;
    pos_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    pos_attr.offset = offsetof(Vertex, position);

    SDL_GPUVertexInputState pos_vertex_input;
    SDL_zero(pos_vertex_input);
    pos_vertex_input.vertex_buffer_descriptions = &vb_pos;
    pos_vertex_input.num_vertex_buffers = 1;
    pos_vertex_input.vertex_attributes = &pos_attr;
    pos_vertex_input.num_vertex_attributes = 1;

    /* ── 9. Pipeline creation (3 pipelines) ────────────────────────── */

    /* ── Pipeline 1: shadow_pipeline ─────────────────────────────────
     * Renders all shadow-casting geometry into the 2048x2048 depth
     * buffer from the light's point of view.  No color output — the
     * depth buffer IS the shadow map.  CULL_NONE ensures both front
     * and back faces contribute depth, giving the tightest shadow
     * boundary.  Slope-scaled depth bias prevents self-shadow acne
     * on curved surfaces (spheres). */
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
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor = SHADOW_DEPTH_BIAS_CONSTANT;
        pi.rasterizer_state.depth_bias_slope_factor = SHADOW_DEPTH_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = shadow_map_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 2: scene_pipeline ──────────────────────────────────
     * Renders lit scene objects (particles as spheres) with Blinn-Phong
     * lighting and shadow mapping.  Single color target output to the
     * swapchain format. */
    {
        SDL_GPUColorTargetDescription scene_color_desc;
        SDL_zero(scene_color_desc);
        scene_color_desc.format = swapchain_fmt;

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
        pi.target_info.color_target_descriptions = &scene_color_desc;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = scene_depth_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── Pipeline 3: grid_pipeline ───────────────────────────────────
     * Renders the procedural grid floor.  CULL_NONE because the grid
     * quad is single-sided but can be viewed from below.
     * LESS_OR_EQUAL depth prevents z-fighting at Y=0.
     * Alpha blending (SRC_ALPHA / ONE_MINUS_SRC_ALPHA) enables the
     * grid's distance fade effect — far grid lines fade to transparent. */
    {
        SDL_GPUColorTargetDescription grid_color_desc;
        SDL_zero(grid_color_desc);
        grid_color_desc.format = swapchain_fmt;
        grid_color_desc.blend_state.enable_blend = true;
        grid_color_desc.blend_state.src_color_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        grid_color_desc.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        grid_color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        grid_color_desc.blend_state.src_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        grid_color_desc.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        grid_color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

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
        pi.target_info.color_target_descriptions = &grid_color_desc;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = scene_depth_fmt;
        pi.target_info.has_depth_stencil_target = true;
        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    }

    /* ── 10. Release all 6 shaders (pipelines keep internal copies) ─ */

    SDL_ReleaseGPUShader(device, scene_vert);
    SDL_ReleaseGPUShader(device, scene_frag);
    SDL_ReleaseGPUShader(device, shadow_vert);
    SDL_ReleaseGPUShader(device, shadow_frag);
    SDL_ReleaseGPUShader(device, grid_vert);
    SDL_ReleaseGPUShader(device, grid_frag);

    /* Verify all pipelines were created */
    if (!state->shadow_pipeline || !state->scene_pipeline ||
        !state->grid_pipeline) {
        SDL_Log("ERROR: One or more pipelines failed to create: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 11. Create depth texture (window-sized, scene depth buffer) ─ */
    /* Query actual pixel dimensions for HiDPI correctness — never use
     * the logical WINDOW_WIDTH/HEIGHT for GPU texture creation. */

    state->depth_fmt = scene_depth_fmt;
    {
        int init_w = 0, init_h = 0;
        if (!SDL_GetWindowSizeInPixels(window, &init_w, &init_h)) {
            SDL_Log("WARNING: SDL_GetWindowSizeInPixels failed: %s",
                    SDL_GetError());
        }
        if (init_w <= 0 || init_h <= 0) {
            init_w = WINDOW_WIDTH;
            init_h = WINDOW_HEIGHT;
        }
        state->depth_width  = (Uint32)init_w;
        state->depth_height = (Uint32)init_h;

        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = scene_depth_fmt;
        ti.width = state->depth_width;
        ti.height = state->depth_height;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        state->depth_tex = SDL_CreateGPUTexture(device, &ti);
        if (!state->depth_tex) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (depth) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 12. Create shadow map texture (2048x2048, sampled by shaders) */

    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = shadow_map_fmt;
        ti.width = SHADOW_MAP_SIZE;
        ti.height = SHADOW_MAP_SIZE;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                   SDL_GPU_TEXTUREUSAGE_SAMPLER;
        state->shadow_map = SDL_CreateGPUTexture(device, &ti);
        if (!state->shadow_map) {
            SDL_Log("ERROR: SDL_CreateGPUTexture (shadow_map) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 13. Create shadow sampler (nearest, clamp to edge) ────────── */

    /* Nearest filtering avoids interpolation across depth discontinuities.
     * Clamp-to-edge prevents sampling beyond the shadow map boundary. */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter = SDL_GPU_FILTER_NEAREST;
        si.mag_filter = SDL_GPU_FILTER_NEAREST;
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->shadow_sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->shadow_sampler) {
            SDL_Log("ERROR: SDL_CreateGPUSampler (shadow) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 14. Generate sphere geometry (forge_shapes library) ───────── */

    /* Generate a UV sphere with 32 slices and 16 stacks.  The ForgeShape
     * uses struct-of-arrays layout (separate positions[] and normals[]),
     * so we interleave into our Vertex (array-of-structs) layout. */
    {
        ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
        if (sphere.vertex_count == 0) {
            SDL_Log("ERROR: forge_shapes_sphere failed to allocate");
            return SDL_APP_FAILURE;
        }

        /* Interleave positions + normals into Vertex array */
        Vertex *sphere_verts = SDL_calloc((size_t)sphere.vertex_count,
                                          sizeof(Vertex));
        if (!sphere_verts) {
            SDL_Log("ERROR: Failed to allocate sphere vertex array");
            forge_shapes_free(&sphere);
            return SDL_APP_FAILURE;
        }

        for (int i = 0; i < sphere.vertex_count; i++) {
            sphere_verts[i].position = sphere.positions[i];
            sphere_verts[i].normal   = sphere.normals[i];
        }

        /* ForgeShape indices are uint32_t — upload as 32-bit index buffer */
        state->sphere_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX,
            sphere_verts,
            (Uint32)sphere.vertex_count * (Uint32)sizeof(Vertex));
        state->sphere_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX,
            sphere.indices,
            (Uint32)sphere.index_count * (Uint32)sizeof(uint32_t));
        state->sphere_index_count = (Uint32)sphere.index_count;

        SDL_free(sphere_verts);
        forge_shapes_free(&sphere);

        if (!state->sphere_vb || !state->sphere_ib) {
            SDL_Log("ERROR: Failed to upload sphere geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* ── 15. Generate grid quad (4 vertices, 6 indices) ────────────── */

    /* A large XZ plane at Y=0.  The procedural grid pattern is computed
     * in the fragment shader using world-space coordinates — this quad
     * just needs to be large enough to cover the visible floor area. */
    {
        Vertex grid_verts[4];
        grid_verts[0].position = vec3_create(-GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE);
        grid_verts[0].normal   = vec3_create(0.0f, 1.0f, 0.0f);
        grid_verts[1].position = vec3_create( GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE);
        grid_verts[1].normal   = vec3_create(0.0f, 1.0f, 0.0f);
        grid_verts[2].position = vec3_create( GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE);
        grid_verts[2].normal   = vec3_create(0.0f, 1.0f, 0.0f);
        grid_verts[3].position = vec3_create(-GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE);
        grid_verts[3].normal   = vec3_create(0.0f, 1.0f, 0.0f);

        Uint16 grid_indices[6] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vb = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX,
            grid_verts, sizeof(grid_verts));
        state->grid_ib = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX,
            grid_indices, sizeof(grid_indices));

        if (!state->grid_vb || !state->grid_ib) {
            SDL_Log("ERROR: Failed to upload grid geometry");
            return SDL_APP_FAILURE;
        }
    }

    /* ── 16. Verify all GPU resources ──────────────────────────────── */

    if (!state->shadow_pipeline || !state->scene_pipeline ||
        !state->grid_pipeline || !state->depth_tex ||
        !state->shadow_map || !state->shadow_sampler ||
        !state->sphere_vb || !state->sphere_ib ||
        !state->grid_vb || !state->grid_ib) {
        SDL_Log("ERROR: One or more GPU resources are NULL");
        return SDL_APP_FAILURE;
    }

    /* ── 17. Initialize light direction and VP matrix ──────────────── */

    state->light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));

    /* Orthographic projection from the light's point of view.
     * The light "sits" at SHADOW_HEIGHT units along the negated light
     * direction and looks at the origin — covering the scene area
     * defined by SHADOW_ORTHO_SIZE. */
    vec3 light_pos = vec3_scale(state->light_dir, -SHADOW_HEIGHT);
    mat4 light_view = mat4_look_at(
        light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
        -SHADOW_ORTHO_SIZE, SHADOW_ORTHO_SIZE,
        SHADOW_NEAR, SHADOW_FAR);
    state->light_vp = mat4_multiply(light_proj, light_view);

    /* ── 18. Initialize camera state ───────────────────────────────── */

    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_YAW;
    state->cam_pitch = CAM_PITCH;
    state->mouse_captured = false;

    /* ── 19. Initialize timing ─────────────────────────────────────── */

    state->last_ticks = SDL_GetPerformanceCounter();
    state->accumulator = 0.0f;
    state->sim_time = 0.0f;
    state->paused = false;
    state->slow_motion = false;

    /* ── 20. Initialize particles (copies to initial_particles too) ── */

    init_particles(state->particles, state->initial_particles);

    /* ── 21. Capture init ──────────────────────────────────────────── */

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, state->device, state->window)) {
            SDL_Log("ERROR: Failed to initialise capture");
            return SDL_APP_FAILURE;
        }
    }
#endif

    /* ── 22. Set appstate and return ───────────────────────────────── */

    *appstate = state;
    return SDL_APP_CONTINUE;
}
/* ── Part 3: Event handling and rendering ──────────────────────────── */

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat) break;  /* ignore auto-repeat */

        if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
            if (state->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                    SDL_Log("ERROR: SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                } else {
                    state->mouse_captured = false;
                }
            } else {
                return SDL_APP_SUCCESS;
            }
        } else if (event->key.scancode == SDL_SCANCODE_R) {
            /* Reset simulation — restore initial particle state */
            for (int i = 0; i < NUM_PARTICLES; i++) {
                state->particles[i] = state->initial_particles[i];
            }
            state->accumulator = 0.0f;
            state->sim_time = 0.0f;
            SDL_Log("Simulation reset");
        } else if (event->key.scancode == SDL_SCANCODE_SPACE) {
            state->paused = !state->paused;
            SDL_Log("Simulation %s", state->paused ? "paused" : "resumed");
        } else if (event->key.scancode == SDL_SCANCODE_T) {
            state->slow_motion = !state->slow_motion;
            SDL_Log("Slow motion %s", state->slow_motion ? "ON" : "OFF");
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
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
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

    /* ── 1. Delta time ────────────────────────────────────────────── */

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - state->last_ticks)
             / (float)SDL_GetPerformanceFrequency();
    state->last_ticks = now;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;  /* prevent spiral of death */

    /* ── 2. Camera movement (quaternion FPS camera) ───────────────── */
    /* Build orientation quaternion from yaw and pitch, then extract
     * forward and right vectors for WASD movement. E/Q for vertical. */

    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(cam_orient);
    vec3 right   = quat_right(cam_orient);

    const bool *keys = SDL_GetKeyboardState(NULL);
    vec3 move = vec3_create(0.0f, 0.0f, 0.0f);

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
        move = vec3_add(move, forward);
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
        move = vec3_sub(move, forward);
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
        move = vec3_add(move, right);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
        move = vec3_sub(move, right);
    if (keys[SDL_SCANCODE_E])      move.y += 1.0f;  /* fly up   */
    if (keys[SDL_SCANCODE_Q])      move.y -= 1.0f;  /* fly down */

    if (vec3_length(move) > VELOCITY_EPSILON) {
        move = vec3_scale(vec3_normalize(move), CAM_SPEED * dt);
        state->cam_position = vec3_add(state->cam_position, move);
    }

    /* ── 3. Fixed-timestep physics ────────────────────────────────── */
    /* Accumulate frame time and step physics in fixed increments.
     * This decouples rendering from simulation — the physics always
     * runs at PHYSICS_DT regardless of frame rate. */

    if (!state->paused) {
        float sim_dt = state->slow_motion ? dt * SLOW_MOTION_FACTOR : dt;
        state->accumulator += sim_dt;

        while (state->accumulator >= PHYSICS_DT) {
            /* Apply forces to all particles */
            vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
            for (int i = 0; i < NUM_PARTICLES; i++) {
                forge_physics_apply_gravity(&state->particles[i], gravity);
                forge_physics_apply_drag(&state->particles[i], DRAG_COEFF);
            }

            /* Integrate — symplectic Euler advances velocity then position */
            for (int i = 0; i < NUM_PARTICLES; i++) {
                forge_physics_integrate(&state->particles[i], PHYSICS_DT);
            }

            /* Collide with ground plane (y = GROUND_Y, normal = up) */
            vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);
            for (int i = 0; i < NUM_PARTICLES; i++) {
                forge_physics_collide_plane(&state->particles[i],
                                            ground_normal, GROUND_Y);
            }

            state->accumulator -= PHYSICS_DT;
            state->sim_time += PHYSICS_DT;
        }
    }

    /* Interpolation factor for smooth rendering between physics steps.
     * alpha blends from prev_position (alpha=0) to position (alpha=1). */
    float alpha = state->accumulator / PHYSICS_DT;

    /* ── 4. Camera matrices ───────────────────────────────────────── */

    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("WARNING: SDL_GetWindowSizeInPixels failed: %s",
                SDL_GetError());
    }
    float aspect = (w > 0 && h > 0) ? (float)w / (float)h : DEFAULT_ASPECT;
    mat4 proj = mat4_perspective(
        FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* ── 5. Acquire command buffer and swapchain ──────────────────── */

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("ERROR: SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError());
        return SDL_APP_CONTINUE;
    }

    SDL_GPUTexture *swapchain = NULL;
    Uint32 sw, sh;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain, &sw, &sh)) {
        SDL_Log("ERROR: SDL_WaitAndAcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }
    if (!swapchain) {
        /* Window minimized or not ready — skip this frame */
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    /* ── 5b. Recreate depth texture if swapchain dimensions changed ─ */
    /* HiDPI or window resize can cause the swapchain to differ from
     * the depth texture.  Recreate to keep them matched. */
    if (sw != state->depth_width || sh != state->depth_height) {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = state->depth_fmt;
        ti.width = sw;
        ti.height = sh;
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        SDL_GPUTexture *new_depth = SDL_CreateGPUTexture(state->device, &ti);
        if (new_depth) {
            SDL_ReleaseGPUTexture(state->device, state->depth_tex);
            state->depth_tex    = new_depth;
            state->depth_width  = sw;
            state->depth_height = sh;
        } else {
            SDL_Log("WARNING: failed to recreate depth texture: %s",
                    SDL_GetError());
        }
    }

    /* ── 6. Pass 1: Shadow map (depth-only) ───────────────────────── */
    /* Render each particle sphere into the shadow depth texture from
     * the light's perspective. No color target — only depth is stored. */
    {
        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture    = state->shadow_map;
        depth_target.clear_depth = 1.0f;
        depth_target.load_op    = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op   = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &depth_target);
        if (!pass) {
            SDL_Log("ERROR: SDL_BeginGPURenderPass (shadow) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            return SDL_APP_CONTINUE;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->shadow_pipeline);

        /* Set shadow map viewport */
        SDL_GPUViewport shadow_vp = {
            0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0.0f, 1.0f
        };
        SDL_SetGPUViewport(pass, &shadow_vp);

        /* Bind sphere geometry */
        SDL_GPUBufferBinding sphere_vb_bind = { state->sphere_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &sphere_vb_bind, 1);
        SDL_GPUBufferBinding sphere_ib_bind = { state->sphere_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &sphere_ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_32BIT);

        /* Draw each particle as a sphere */
        for (int i = 0; i < NUM_PARTICLES; i++) {
            /* Interpolate position for smooth shadow rendering */
            vec3 render_pos = vec3_lerp(
                state->particles[i].prev_position,
                state->particles[i].position, alpha);

            mat4 model = mat4_multiply(
                mat4_translate(render_pos),
                mat4_scale_uniform(SPHERE_RADIUS));

            ShadowVertUniforms shadow_uni;
            shadow_uni.light_vp = mat4_multiply(state->light_vp, model);

            SDL_PushGPUVertexUniformData(cmd, 0,
                &shadow_uni, sizeof(shadow_uni));
            SDL_DrawGPUIndexedPrimitives(pass,
                state->sphere_index_count, 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    /* ── 7. Pass 2: Scene (spheres + grid) ────────────────────────── */
    /* Render the full scene to the swapchain with Blinn-Phong lighting
     * and shadow mapping. Each sphere's color is mapped from velocity. */
    {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture    = swapchain;
        color_target.clear_color = (SDL_FColor){
            CLEAR_COLOR_R, CLEAR_COLOR_G, CLEAR_COLOR_B, 1.0f
        };
        color_target.load_op    = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op   = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture    = state->depth_tex;
        depth_target.clear_depth = 1.0f;
        depth_target.load_op    = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op   = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, &depth_target);
        if (!pass) {
            SDL_Log("ERROR: SDL_BeginGPURenderPass (scene) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("ERROR: SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            return SDL_APP_CONTINUE;
        }

        /* Set viewport and scissor to swapchain size */
        SDL_GPUViewport scene_vp = {
            0, 0, (float)sw, (float)sh, 0.0f, 1.0f
        };
        SDL_SetGPUViewport(pass, &scene_vp);
        SDL_SetGPUScissor(pass, &(SDL_Rect){ 0, 0, (int)sw, (int)sh });

        /* ── Draw spheres ─────────────────────────────────────── */

        SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);

        /* Bind shadow map as fragment sampler for shadow testing */
        SDL_GPUTextureSamplerBinding shadow_binding = {
            state->shadow_map, state->shadow_sampler
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &shadow_binding, 1);

        /* Bind sphere geometry */
        SDL_GPUBufferBinding sphere_vb_bind = { state->sphere_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &sphere_vb_bind, 1);
        SDL_GPUBufferBinding sphere_ib_bind = { state->sphere_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &sphere_ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_32BIT);

        for (int i = 0; i < NUM_PARTICLES; i++) {
            /* Interpolate position between physics steps */
            vec3 render_pos = vec3_lerp(
                state->particles[i].prev_position,
                state->particles[i].position, alpha);

            mat4 model = mat4_multiply(
                mat4_translate(render_pos),
                mat4_scale_uniform(SPHERE_RADIUS));

            /* Vertex uniforms: MVP, model, light VP */
            SceneVertUniforms vu;
            vu.mvp      = mat4_multiply(cam_vp, model);
            vu.model    = model;
            vu.light_vp = mat4_multiply(state->light_vp, model);
            SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof(vu));

            /* Fragment uniforms: velocity-mapped color + lighting */
            float speed = vec3_length(state->particles[i].velocity);

            SceneFragUniforms fu;
            memset(&fu, 0, sizeof(fu));
            velocity_to_color(speed, fu.base_color);
            fu.eye_pos[0]     = state->cam_position.x;
            fu.eye_pos[1]     = state->cam_position.y;
            fu.eye_pos[2]     = state->cam_position.z;
            fu.ambient        = AMBIENT_STRENGTH;
            fu.light_dir[0]   = state->light_dir.x;
            fu.light_dir[1]   = state->light_dir.y;
            fu.light_dir[2]   = state->light_dir.z;
            fu.light_dir[3]   = 0.0f;
            fu.light_color[0] = LIGHT_COLOR_R;
            fu.light_color[1] = LIGHT_COLOR_G;
            fu.light_color[2] = LIGHT_COLOR_B;
            fu.light_intensity = LIGHT_INTENSITY;
            fu.shininess      = SPECULAR_SHININESS;
            fu.specular_str   = SPECULAR_STRENGTH;
            SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

            SDL_DrawGPUIndexedPrimitives(pass,
                state->sphere_index_count, 1, 0, 0, 0);
        }

        /* ── Draw grid floor ──────────────────────────────────── */

        SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

        /* Re-bind shadow map for grid fragment sampling */
        SDL_BindGPUFragmentSamplers(pass, 0, &shadow_binding, 1);

        /* Bind grid geometry */
        SDL_GPUBufferBinding grid_vb_bind = { state->grid_vb, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &grid_vb_bind, 1);
        SDL_GPUBufferBinding grid_ib_bind = { state->grid_ib, 0 };
        SDL_BindGPUIndexBuffer(pass, &grid_ib_bind,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);

        /* Grid vertex uniforms */
        GridVertUniforms gvu;
        gvu.vp       = cam_vp;
        gvu.light_vp = state->light_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));

        /* Grid fragment uniforms */
        GridFragUniforms gfu;
        memset(&gfu, 0, sizeof(gfu));
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

        SDL_DrawGPUIndexedPrimitives(pass, 6, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    /* ── 8. Capture and submit ────────────────────────────────────── */

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
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
            return SDL_APP_FAILURE;
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

    /* If device was never created, nothing to release — just free state */
    if (!state->device) {
        if (state->window)
            SDL_DestroyWindow(state->window);
        SDL_free(state);
        return;
    }

    /* Wait for all in-flight GPU work to complete before releasing */
    if (!SDL_WaitForGPUIdle(state->device)) {
        SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

    /* Release pipelines */
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);

    /* Release geometry buffers */
    if (state->sphere_vb)
        SDL_ReleaseGPUBuffer(state->device, state->sphere_vb);
    if (state->sphere_ib)
        SDL_ReleaseGPUBuffer(state->device, state->sphere_ib);
    if (state->grid_vb)
        SDL_ReleaseGPUBuffer(state->device, state->grid_vb);
    if (state->grid_ib)
        SDL_ReleaseGPUBuffer(state->device, state->grid_ib);

    /* Release render target textures */
    if (state->depth_tex)
        SDL_ReleaseGPUTexture(state->device, state->depth_tex);
    if (state->shadow_map)
        SDL_ReleaseGPUTexture(state->device, state->shadow_map);

    /* Release samplers */
    if (state->shadow_sampler)
        SDL_ReleaseGPUSampler(state->device, state->shadow_sampler);

    /* Release window from GPU device, then destroy window and device */
    if (state->window)
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    if (state->window)
        SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);

    SDL_free(state);
}
