/*
 * Lesson 46 — Particle Animations
 *
 * Demonstrates a GPU-driven particle system using compute shaders for
 * simulation and billboard rendering via vertex pulling:
 *
 *   - Compute shader maintains a particle pool (position, velocity,
 *     lifetime, color, size) entirely on the GPU
 *   - Dead particles are recycled via atomic spawn counter — no CPU
 *     readback needed
 *   - Billboard quads face the camera using right/up vector expansion
 *   - 4x4 texture atlas animation indexed by particle age
 *   - Additive blending (fire/fountain) and alpha blending (smoke)
 *
 * The same concepts from Physics Lesson 01 (force accumulation, Euler
 * integration, gravity, drag) are implemented here on the GPU, enabling
 * thousands of particles at interactive frame rates.
 *
 * Controls:
 *   WASD / Mouse  — move/look
 *   Space / Shift — fly up/down
 *   1 / 2 / 3    — select emitter type (Fountain / Fire / Smoke)
 *   B             — burst spawn (1000 particles at once)
 *   Escape        — release mouse cursor
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */

#include "math/forge_math.h"

#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Compiled shader bytecode ─────────────────────────────────────────── */

/* Compute: emit + simulate — updates particle pool each frame */
#include "shaders/compiled/particle_sim_comp_spirv.h"
#include "shaders/compiled/particle_sim_comp_dxil.h"
#include "shaders/compiled/particle_sim_comp_msl.h"

/* Vertex: billboard expansion via vertex pulling from storage buffer */
#include "shaders/compiled/particle_vert_spirv.h"
#include "shaders/compiled/particle_vert_dxil.h"
#include "shaders/compiled/particle_vert_msl.h"

/* Fragment: textured particle with atlas sampling */
#include "shaders/compiled/particle_frag_spirv.h"
#include "shaders/compiled/particle_frag_dxil.h"
#include "shaders/compiled/particle_frag_msl.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

/* Particle pool size — each particle is 4 x float4 = 64 bytes on the GPU */
#define MAX_PARTICLES          4096
#define PARTICLE_STRIDE        64     /* bytes per particle (4 x float4) */
#define WORKGROUP_SIZE         256

/* Atlas texture: 4x4 grid of soft circles, 64 pixels per cell */
#define ATLAS_CELLS            4
#define CELL_SIZE              64
#define ATLAS_SIZE             (ATLAS_CELLS * CELL_SIZE)  /* 256 */

/* Emitter defaults */
#define DEFAULT_SPAWN_RATE     150.0f   /* particles per second */
#define DEFAULT_GRAVITY       -9.8f
#define DEFAULT_DRAG           1.0f
#define DEFAULT_EMITTER_SPEED  6.0f
#define DEFAULT_SIZE_MIN       0.08f   /* smallest billboard size (meters)  */
#define DEFAULT_SIZE_MAX       0.3f    /* largest billboard size (meters)   */
#define BURST_COUNT            1000

/* UI layout */
#define LABEL_HEIGHT           20
#define SLIDER_HEIGHT          20
#define BUTTON_HEIGHT          26

/* Maximum path buffer */
#define ASSET_PATH_SIZE        512

/* Compute pipeline resource counts (must match HLSL register layout) */
#define SIM_NUM_RW_STORAGE_BUFFERS  2  /* particle buffer + spawn counter */
#define SIM_NUM_UNIFORM_BUFFERS     1

/* Graphics vertex shader resource counts */
#define PART_VERT_STORAGE_BUFFERS   1  /* particle buffer (read-only) */
#define PART_VERT_UNIFORM_BUFFERS   1

/* Graphics fragment shader resource counts */
#define PART_FRAG_SAMPLERS          1  /* atlas texture + sampler */

/* ── Uniform structs (C-side, must match HLSL cbuffer layout) ─────────── */

/* Compute shader uniforms — passed via SDL_PushGPUComputeUniformData */
typedef struct SimUniforms {
    float    dt;              /* frame delta time in seconds               */
    float    gravity;         /* gravity acceleration (m/s², negative=down)*/
    float    drag;            /* velocity damping coefficient per second   */
    uint32_t frame_counter;   /* monotonic frame index for PRNG seeding    */
    float    emitter_pos[4];  /* xyz = emitter world position, w = unused  */
    float    emitter_params[4]; /* x=type, y=speed, z=size_min, w=size_max */
    float    extra_params[4]; /* per-emitter tunables (see shader comments) */
} SimUniforms;

/* Vertex shader uniforms — passed via SDL_PushGPUVertexUniformData */
typedef struct BillboardUniforms {
    mat4  view_proj;       /* camera view-projection matrix              */
    float cam_right[4];    /* xyz = camera right vector, w = unused      */
    float cam_up[4];       /* xyz = camera up vector, w = unused         */
} BillboardUniforms;

/* ── Emitter types ────────────────────────────────────────────────────── */

typedef enum EmitterType {
    EMITTER_FOUNTAIN = 0,
    EMITTER_FIRE     = 1,
    EMITTER_SMOKE    = 2,
    EMITTER_COUNT    = 3
} EmitterType;

static const char *EMITTER_NAMES[EMITTER_COUNT] = {
    "Fountain", "Fire", "Smoke"
};

/* ── Application state ────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;

    /* Compute pipeline for particle simulation */
    SDL_GPUComputePipeline  *sim_pipeline;

    /* Graphics pipelines — one per blend mode */
    SDL_GPUGraphicsPipeline *additive_pipeline;  /* fire, fountain */
    SDL_GPUGraphicsPipeline *alpha_pipeline;      /* smoke */

    /* GPU buffers */
    SDL_GPUBuffer           *particle_buffer;     /* RW storage (compute) + read storage (vertex) */
    SDL_GPUBuffer           *counter_buffer;      /* RW storage, single int32 (signed for correct atomic decrement) */
    SDL_GPUTransferBuffer   *counter_transfer;    /* persistent 4-byte staging for counter reset */

    /* Atlas texture and sampler */
    SDL_GPUTexture          *atlas_texture;
    SDL_GPUSampler          *atlas_sampler;

    /* UI and simulation state */
    ForgeUiWindowState ui_window;    /* draggable UI panel state              */
    float    spawn_rate;             /* particles per second (UI-adjustable)  */
    float    gravity;                /* gravity acceleration (m/s², negative) */
    float    drag;                   /* velocity damping coefficient          */
    float    fire_spread;            /* fire width multiplier (1.0 = torch)   */
    float    smoke_rise_speed;       /* smoke vertical speed multiplier       */
    float    smoke_spread;           /* smoke horizontal spread multiplier    */
    float    smoke_opacity;          /* smoke base opacity (0–1)              */
    int      emitter_type;           /* active EmitterType enum value         */
    float    spawn_accum;            /* fractional spawn accumulator          */
    uint32_t frame_counter;          /* monotonic frame index for PRNG seed   */
    int      prev_emitter_type;      /* previous type — detect switches       */
    bool     burst_requested;        /* true = spawn BURST_COUNT next frame   */
} app_state;

/* ── Helper: build asset path ─────────────────────────────────────────── */

static const char *asset_path(char *buf, size_t buf_size,
                              const char *base, const char *relative)
{
    int len = SDL_snprintf(buf, buf_size, "%s%s", base, relative);
    if (len < 0 || (size_t)len >= buf_size) {
        SDL_Log("asset_path: path too long: %s%s", base, relative);
        return NULL;
    }
    return buf;
}

/* ── Helper: create compute pipeline from bytecode ────────────────────── */

static SDL_GPUComputePipeline *create_sim_pipeline(SDL_GPUDevice *device)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUComputePipelineCreateInfo info;
    SDL_zero(info);
    info.entrypoint                     = "main";
    info.num_readwrite_storage_buffers  = SIM_NUM_RW_STORAGE_BUFFERS;
    info.num_uniform_buffers            = SIM_NUM_UNIFORM_BUFFERS;
    info.threadcount_x                  = WORKGROUP_SIZE;
    info.threadcount_y                  = 1;
    info.threadcount_z                  = 1;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = particle_sim_comp_spirv;
        info.code_size = sizeof(particle_sim_comp_spirv);
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = particle_sim_comp_dxil;
        info.code_size = sizeof(particle_sim_comp_dxil);
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL)
               && particle_sim_comp_msl_size > 0) {
        info.format     = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
        info.code       = (const unsigned char *)particle_sim_comp_msl;
        info.code_size  = particle_sim_comp_msl_size;
    } else {
        SDL_Log("No supported compute shader format");
        return NULL;
    }

    SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (!pipeline) {
        SDL_Log("SDL_CreateGPUComputePipeline failed: %s", SDL_GetError());
    }
    return pipeline;
}

/* ── Helper: create particle graphics pipeline ────────────────────────── */
/* Creates a pipeline with vertex pulling (no vertex input), the particle
 * vertex + fragment shaders, and the specified blend mode.  additive=true
 * gives src=SRC_ALPHA, dst=ONE; false gives standard alpha blending. */

static SDL_GPUGraphicsPipeline *create_particle_pipeline(
    ForgeScene *scene, bool additive)
{
    /* Create vertex shader: 0 samplers, 0 storage textures,
     * 1 storage buffer (particle data), 1 uniform buffer */
    SDL_GPUShader *vert = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        particle_vert_spirv, sizeof(particle_vert_spirv),
        particle_vert_dxil,  sizeof(particle_vert_dxil),
        particle_vert_msl,   particle_vert_msl_size,
        0, 0, PART_VERT_STORAGE_BUFFERS, PART_VERT_UNIFORM_BUFFERS);

    /* Create fragment shader: 1 sampler (atlas), 0 storage, 0 uniforms */
    SDL_GPUShader *frag = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        particle_frag_spirv, sizeof(particle_frag_spirv),
        particle_frag_dxil,  sizeof(particle_frag_dxil),
        particle_frag_msl,   particle_frag_msl_size,
        PART_FRAG_SAMPLERS, 0, 0, 0);

    if (!vert || !frag) {
        SDL_GPUDevice *dev = forge_scene_device(scene);
        if (vert) SDL_ReleaseGPUShader(dev, vert);
        if (frag) SDL_ReleaseGPUShader(dev, frag);
        return NULL;
    }

    /* Empty vertex input — particle data comes from storage buffer */
    SDL_GPUVertexInputState vis;
    SDL_zero(vis);

    /* Color target with blending enabled, depth write OFF */
    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = forge_scene_swapchain_format(scene);
    ctd.blend_state.enable_blend         = true;
    ctd.blend_state.color_blend_op       = SDL_GPU_BLENDOP_ADD;
    ctd.blend_state.alpha_blend_op       = SDL_GPU_BLENDOP_ADD;
    ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

    if (additive) {
        /* Additive: colors accumulate (fire glow, energy effects) */
        ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    } else {
        /* Standard alpha: source over destination (smoke, clouds) */
        ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    }

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader   = vert;
    pi.fragment_shader = frag;
    pi.vertex_input_state = vis;

    pi.target_info.num_color_targets         = 1;
    pi.target_info.color_target_descriptions = &ctd;
    pi.target_info.has_depth_stencil_target  = true;
    pi.target_info.depth_stencil_format      = scene->depth_fmt;

    /* Depth test ON (particles behind geometry are occluded), write OFF
     * (transparent surfaces must not update the depth buffer) */
    pi.depth_stencil_state.enable_depth_test  = true;
    pi.depth_stencil_state.enable_depth_write = false;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    /* No culling — billboard quads can face any direction */
    pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    SDL_GPUDevice *dev = forge_scene_device(scene);

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(dev, &pi);

    /* Release shaders — pipeline keeps its own copy */
    SDL_ReleaseGPUShader(dev, vert);
    SDL_ReleaseGPUShader(dev, frag);

    if (!pipeline) {
        SDL_Log("SDL_CreateGPUGraphicsPipeline (%s) failed: %s",
                additive ? "additive" : "alpha", SDL_GetError());
    }
    return pipeline;
}

/* ── Helper: generate procedural atlas texture ────────────────────────── */
/* Creates a 256x256 RGBA surface with a 4x4 grid of soft Gaussian circles.
 * Frame 0 (top-left) is small and bright; frame 15 (bottom-right) is large
 * and dim.  Playing frames 0→15 over a particle's lifetime produces an
 * expanding-and-fading animation. */

static SDL_Surface *generate_atlas(void)
{
    SDL_Surface *surface = SDL_CreateSurface(ATLAS_SIZE, ATLAS_SIZE,
                                             SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        SDL_Log("SDL_CreateSurface failed: %s", SDL_GetError());
        return NULL;
    }

    Uint8 *pixels = (Uint8 *)surface->pixels;
    int pitch = surface->pitch;

    for (int cell_row = 0; cell_row < ATLAS_CELLS; cell_row++) {
        for (int cell_col = 0; cell_col < ATLAS_CELLS; cell_col++) {
            int frame = cell_row * ATLAS_CELLS + cell_col;
            /* Normalize frame index to [0, 1] across all atlas cells */
            float t = (float)frame / (float)(ATLAS_CELLS * ATLAS_CELLS - 1);

            /* Radius grows over the animation (normalized to cell size) */
            float radius = 0.15f + t * 0.33f;
            /* Intensity (brightness) decreases over time */
            float intensity = 1.0f - t * 0.7f;

            int base_x = cell_col * CELL_SIZE;
            int base_y = cell_row * CELL_SIZE;

            for (int py = 0; py < CELL_SIZE; py++) {
                for (int px = 0; px < CELL_SIZE; px++) {
                    /* Normalized distance from cell center [0, ~0.7] */
                    float nx = ((float)px + 0.5f) / CELL_SIZE - 0.5f;
                    float ny = ((float)py + 0.5f) / CELL_SIZE - 0.5f;
                    float dist = SDL_sqrtf(nx * nx + ny * ny);

                    /* Gaussian falloff */
                    float sigma = radius * 0.5f;
                    float alpha = intensity
                                * SDL_expf(-(dist * dist) / (2.0f * sigma * sigma));

                    /* Clamp and convert to byte */
                    if (alpha > 1.0f) alpha = 1.0f;
                    Uint8 a = (Uint8)(alpha * 255.0f);

                    /* White circle with varying alpha */
                    int idx = (base_y + py) * pitch + (base_x + px) * 4;
                    pixels[idx + 0] = 255;   /* R */
                    pixels[idx + 1] = 255;   /* G */
                    pixels[idx + 2] = 255;   /* B */
                    pixels[idx + 3] = a;     /* A */
                }
            }
        }
    }

    return surface;
}
/* ═══════════════════════════════════════════════════════════════════════
 * SDL_AppInit — Set up rendering, compute pipelines, and particle system
 * ═══════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("SDL_calloc failed for app_state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    const char *base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── Font path for the UI panel ───────────────────────────────── */
    char font_buf[ASSET_PATH_SIZE];
    const char *font = asset_path(font_buf, sizeof(font_buf), base,
                        "assets/fonts/liberation_mono/LiberationMono-Regular.ttf");
    if (!font) return SDL_APP_FAILURE;

    /* ── Scene configuration ──────────────────────────────────────── */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Lesson 46 \xe2\x80\x94 Particle Animations");
    cfg.cam_start_pos   = vec3_create(0.0f, 3.0f, 4.0f);
    cfg.cam_start_pitch = -0.35f;
    cfg.font_path       = font;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *dev = forge_scene_device(&state->scene);

    /* ── 1. Create compute pipeline for particle simulation ───────── */
    state->sim_pipeline = create_sim_pipeline(dev);
    if (!state->sim_pipeline) {
        SDL_Log("Failed to create simulation compute pipeline");
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create graphics pipelines for particle rendering ──────── */
    /* Additive blend pipeline for fire and fountain particles */
    state->additive_pipeline = create_particle_pipeline(&state->scene, true);
    if (!state->additive_pipeline) {
        SDL_Log("Failed to create additive particle pipeline");
        return SDL_APP_FAILURE;
    }

    /* Alpha blend pipeline for smoke particles */
    state->alpha_pipeline = create_particle_pipeline(&state->scene, false);
    if (!state->alpha_pipeline) {
        SDL_Log("Failed to create alpha particle pipeline");
        return SDL_APP_FAILURE;
    }

    /* ── 3. Generate and upload atlas texture ─────────────────────── */
    {
        SDL_Surface *atlas_surface = generate_atlas();
        if (!atlas_surface) {
            SDL_Log("Failed to generate atlas surface");
            return SDL_APP_FAILURE;
        }

        /* Upload as non-sRGB (data texture — we control values directly) */
        state->atlas_texture = forge_scene_upload_texture(
            &state->scene, atlas_surface, false);
        SDL_DestroySurface(atlas_surface);

        if (!state->atlas_texture) {
            SDL_Log("Failed to upload atlas texture");
            return SDL_APP_FAILURE;
        }

        /* Linear filtering, clamp to edge (avoid atlas cell bleeding) */
        SDL_GPUSamplerCreateInfo samp_info;
        SDL_zero(samp_info);
        samp_info.min_filter       = SDL_GPU_FILTER_LINEAR;
        samp_info.mag_filter       = SDL_GPU_FILTER_LINEAR;
        samp_info.mipmap_mode      = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        samp_info.address_mode_u   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samp_info.address_mode_v   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samp_info.address_mode_w   = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        state->atlas_sampler = SDL_CreateGPUSampler(dev, &samp_info);
        if (!state->atlas_sampler) {
            SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 4. Create particle buffer ────────────────────────────────── */
    /* Dual usage: compute writes particle data, vertex shader reads it.
     * Initialize with zeros — all particles start dead (lifetime = 0)
     * and will be spawned by the compute shader on the first few frames. */
    {
        Uint32 buf_size = MAX_PARTICLES * PARTICLE_STRIDE;

        SDL_GPUBufferCreateInfo buf_info;
        SDL_zero(buf_info);
        buf_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
                       | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        buf_info.size  = buf_size;

        state->particle_buffer = SDL_CreateGPUBuffer(dev, &buf_info);
        if (!state->particle_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (particles) failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        /* Upload zeros to initialize all particles as dead */
        void *zeros = SDL_calloc(1, buf_size);
        if (!zeros) {
            SDL_Log("SDL_calloc failed for particle init data");
            return SDL_APP_FAILURE;
        }

        SDL_GPUTransferBufferCreateInfo xfer_info;
        SDL_zero(xfer_info);
        xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        xfer_info.size  = buf_size;

        SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(dev, &xfer_info);
        if (!xfer) {
            SDL_free(zeros);
            SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        void *mapped = SDL_MapGPUTransferBuffer(dev, xfer, false);
        if (!mapped) {
            SDL_free(zeros);
            SDL_ReleaseGPUTransferBuffer(dev, xfer);
            SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        SDL_memcpy(mapped, zeros, buf_size);
        SDL_UnmapGPUTransferBuffer(dev, xfer);
        SDL_free(zeros);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
        if (!cmd) {
            SDL_ReleaseGPUTransferBuffer(dev, xfer);
            SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        if (!copy) {
            SDL_Log("SDL_BeginGPUCopyPass (particle init) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            SDL_ReleaseGPUTransferBuffer(dev, xfer);
            return SDL_APP_FAILURE;
        }
        {
            SDL_GPUTransferBufferLocation src = { xfer, 0 };
            SDL_GPUBufferRegion dst = { state->particle_buffer, 0, buf_size };
            SDL_UploadToGPUBuffer(copy, &src, &dst, false);
            SDL_EndGPUCopyPass(copy);
        }

        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer (particle init) failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(dev, xfer);
            return SDL_APP_FAILURE;
        }
        SDL_ReleaseGPUTransferBuffer(dev, xfer);
    }

    /* ── 5. Create spawn counter buffer ───────────────────────────── */
    /* A single int32 that the CPU resets each frame and the compute
     * shader atomically decrements when spawning particles.  Signed so
     * that InterlockedAdd past zero produces negative values (not
     * unsigned wraparound), letting prev > 0 correctly reject excess. */
    {
        SDL_GPUBufferCreateInfo buf_info;
        SDL_zero(buf_info);
        buf_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        buf_info.size  = sizeof(int32_t);

        state->counter_buffer = SDL_CreateGPUBuffer(dev, &buf_info);
        if (!state->counter_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (counter) failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        /* Persistent transfer buffer for resetting the counter each frame.
         * Avoids creating/destroying a transfer buffer every frame. */
        SDL_GPUTransferBufferCreateInfo xfer_info;
        SDL_zero(xfer_info);
        xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        xfer_info.size  = sizeof(int32_t);

        state->counter_transfer = SDL_CreateGPUTransferBuffer(dev, &xfer_info);
        if (!state->counter_transfer) {
            SDL_Log("SDL_CreateGPUTransferBuffer (counter) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── 6. Initialize simulation defaults ────────────────────────── */
    state->spawn_rate      = DEFAULT_SPAWN_RATE;
    state->gravity         = DEFAULT_GRAVITY;
    state->drag            = DEFAULT_DRAG;
    state->fire_spread     = 1.0f;
    state->smoke_rise_speed = 1.0f;
    state->smoke_spread    = 1.0f;
    state->smoke_opacity   = 0.5f;
    state->emitter_type     = EMITTER_FIRE;
    state->prev_emitter_type = EMITTER_FIRE;
    state->spawn_accum  = 0.0f;
    state->frame_counter = 0;
    state->burst_requested = false;
    state->ui_window = forge_ui_window_state_default(10, 10, 280, 480);

    SDL_Log("Lesson 46 initialized: %d max particles, %d workgroup size",
            MAX_PARTICLES, WORKGROUP_SIZE);
    return SDL_APP_CONTINUE;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SDL_AppEvent — Input handling
 * ═══════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    /* Delegate camera/window events to the scene renderer */
    SDL_AppResult r = forge_scene_handle_event(&state->scene, event);
    if (r != SDL_APP_CONTINUE) return r;

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        switch (event->key.scancode) {
        /* Emitter type selection */
        case SDL_SCANCODE_1:
            state->emitter_type = EMITTER_FOUNTAIN;
            break;
        case SDL_SCANCODE_2:
            state->emitter_type = EMITTER_FIRE;
            break;
        case SDL_SCANCODE_3:
            state->emitter_type = EMITTER_SMOKE;
            break;

        /* Burst spawn — request 1000 particles on next frame */
        case SDL_SCANCODE_B:
            state->burst_requested = true;
            break;

        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
}
/* ═══════════════════════════════════════════════════════════════════════
 * SDL_AppIterate — Per-frame simulation and rendering
 * ═══════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    float dt = forge_scene_dt(s);
    SDL_GPUCommandBuffer *cmd = forge_scene_cmd(s);

    /* ── Reset pool when emitter type changes ────────────────────── */
    /* Setting frame_counter to 0 triggers the shader's pre-fill path,
     * which respawns all particles with the new emitter type at
     * randomized ages — no visible ramp-up on type switch. */
    if (state->emitter_type != state->prev_emitter_type) {
        state->frame_counter = 0;
        state->prev_emitter_type = state->emitter_type;
    }

    /* ── Compute spawn count for this frame ───────────────────────── */
    /* Accumulate fractional spawns so a 150/s rate at 60fps = 2.5/frame
     * correctly spawns 2 or 3 particles alternating. */
    state->spawn_accum += state->spawn_rate * dt;
    int32_t spawn_count = (int32_t)state->spawn_accum;
    state->spawn_accum -= (float)spawn_count;

    /* Burst spawn: add extra particles on top of regular rate */
    if (state->burst_requested) {
        spawn_count += BURST_COUNT;
        state->burst_requested = false;
    }

    /* Cap to max pool size — the shader reads this as a signed int */
    if (spawn_count > MAX_PARTICLES) spawn_count = MAX_PARTICLES;

    /* ── Reset spawn counter via copy pass ────────────────────────── */
    /* Write spawn budget to the transfer buffer, then copy to GPU.
     * Track success so we can skip the compute pass if reset failed. */
    bool counter_copied = false;
    {
        void *mapped = SDL_MapGPUTransferBuffer(
            forge_scene_device(s), state->counter_transfer, false);
        if (mapped) {
            SDL_memcpy(mapped, &spawn_count, sizeof(int32_t));
            SDL_UnmapGPUTransferBuffer(forge_scene_device(s),
                                        state->counter_transfer);

            /* Copy the fresh spawn budget to the GPU counter buffer */
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            if (copy) {
                SDL_GPUTransferBufferLocation src = {
                    state->counter_transfer, 0
                };
                SDL_GPUBufferRegion dst = {
                    state->counter_buffer, 0, sizeof(int32_t)
                };
                SDL_UploadToGPUBuffer(copy, &src, &dst, false);
                SDL_EndGPUCopyPass(copy);
                counter_copied = true;
            } else {
                SDL_Log("SDL_BeginGPUCopyPass (counter) failed: %s",
                        SDL_GetError());
            }
        } else {
            SDL_Log("SDL_MapGPUTransferBuffer (counter) failed: %s",
                    SDL_GetError());
        }
    }

    /* ── Compute pass: simulate and emit particles ────────────────── */
    /* Only dispatch if the counter was successfully reset — otherwise
     * the compute shader would run with stale spawn budget data. */
    if (counter_copied) {
        SimUniforms sim;
        sim.dt            = dt;
        sim.gravity       = state->gravity;
        sim.drag          = state->drag;
        sim.frame_counter = state->frame_counter;

        /* Emitter position: slightly above the grid floor */
        sim.emitter_pos[0] = 0.0f;
        sim.emitter_pos[1] = 0.5f;
        sim.emitter_pos[2] = 0.0f;
        sim.emitter_pos[3] = 0.0f;

        /* Emitter params: type, speed, size range */
        sim.emitter_params[0] = (float)state->emitter_type;
        sim.emitter_params[1] = DEFAULT_EMITTER_SPEED;
        sim.emitter_params[2] = DEFAULT_SIZE_MIN;
        sim.emitter_params[3] = DEFAULT_SIZE_MAX;

        /* Per-emitter tunables — meaning depends on active emitter type */
        sim.extra_params[0] = 0.0f;
        sim.extra_params[1] = 0.0f;
        sim.extra_params[2] = 0.0f;
        sim.extra_params[3] = 0.0f;
        if (state->emitter_type == EMITTER_FIRE) {
            sim.extra_params[0] = state->fire_spread;
        } else if (state->emitter_type == EMITTER_SMOKE) {
            sim.extra_params[0] = state->smoke_rise_speed;
            sim.extra_params[1] = state->smoke_spread;
            sim.extra_params[2] = state->smoke_opacity;
        }

        /* Push uniform data before beginning the compute pass */
        SDL_PushGPUComputeUniformData(cmd, 0, &sim, sizeof(sim));

        /* Bind particle buffer and spawn counter as read-write storage */
        SDL_GPUStorageBufferReadWriteBinding rw_bindings[2];
        SDL_zero(rw_bindings);
        rw_bindings[0].buffer = state->particle_buffer;
        rw_bindings[0].cycle  = false;  /* keep contents between frames */
        rw_bindings[1].buffer = state->counter_buffer;
        rw_bindings[1].cycle  = false;  /* just uploaded fresh value */

        SDL_GPUComputePass *compute = SDL_BeginGPUComputePass(
            cmd, NULL, 0, rw_bindings, 2);
        if (compute) {
            SDL_BindGPUComputePipeline(compute, state->sim_pipeline);

            Uint32 groups = (MAX_PARTICLES + WORKGROUP_SIZE - 1)
                          / WORKGROUP_SIZE;
            SDL_DispatchGPUCompute(compute, groups, 1, 1);

            SDL_EndGPUComputePass(compute);
        }
    }

    state->frame_counter++;

    /* ── Shadow pass (no particle shadows — just bookkeeping) ─────── */
    forge_scene_begin_shadow_pass(s);
    forge_scene_end_shadow_pass(s);

    /* ── Main render pass ─────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);

    /* Grid floor provides spatial reference for the particle effects */
    forge_scene_draw_grid(s);

    /* ── Draw particles ───────────────────────────────────────────── */
    {
        SDL_GPURenderPass *pass = forge_scene_main_pass(s);
        if (pass) {
            /* Choose pipeline based on emitter type */
            SDL_GPUGraphicsPipeline *pipeline =
                (state->emitter_type == EMITTER_SMOKE)
                    ? state->alpha_pipeline
                    : state->additive_pipeline;

            SDL_BindGPUGraphicsPipeline(pass, pipeline);

            /* Bind particle buffer as vertex storage (read-only in VS) */
            SDL_GPUBuffer *storage_bufs[1] = { state->particle_buffer };
            SDL_BindGPUVertexStorageBuffers(pass, 0, storage_bufs, 1);

            /* Bind atlas texture + sampler for the fragment shader */
            SDL_GPUTextureSamplerBinding atlas_bind = {
                state->atlas_texture, state->atlas_sampler
            };
            SDL_BindGPUFragmentSamplers(pass, 0, &atlas_bind, 1);

            /* Push billboard uniforms to the vertex shader */
            BillboardUniforms bb;
            bb.view_proj = forge_scene_view_proj(s);

            /* Extract camera right and up vectors from scene orientation */
            quat cam_orient = quat_from_euler(s->cam_yaw, s->cam_pitch, 0.0f);
            vec3 right = quat_right(cam_orient);
            vec3 up    = quat_up(cam_orient);

            bb.cam_right[0] = right.x;
            bb.cam_right[1] = right.y;
            bb.cam_right[2] = right.z;
            bb.cam_right[3] = 0.0f;

            bb.cam_up[0] = up.x;
            bb.cam_up[1] = up.y;
            bb.cam_up[2] = up.z;
            bb.cam_up[3] = 0.0f;

            SDL_PushGPUVertexUniformData(cmd, 0, &bb, sizeof(bb));

            /* Draw all particles: 6 vertices per particle (2 triangles).
             * Dead particles produce degenerate triangles (zero area). */
            SDL_DrawGPUPrimitives(pass,
                                   (Uint32)(MAX_PARTICLES * 6), 1, 0, 0);
        }
    }

    forge_scene_end_main_pass(s);

    /* ── UI pass ──────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Particles",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* ── Emitter type selection ───────────── */
                forge_ui_ctx_label_layout(ui, "Emitter Type:", LABEL_HEIGHT);
                for (int i = 0; i < EMITTER_COUNT; i++) {
                    bool selected = (state->emitter_type == i);
                    if (forge_ui_ctx_checkbox_layout(ui, EMITTER_NAMES[i],
                                                      &selected, LABEL_HEIGHT)) {
                        if (selected) state->emitter_type = i;
                    }
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT / 2);

                /* ── Spawn rate slider (all types) ────── */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Spawn: %.0f/s",
                                 (double)state->spawn_rate);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##spawn",
                                           &state->spawn_rate,
                                           0.0f, 500.0f, SLIDER_HEIGHT);

                /* ── Per-emitter sliders ──────────────── */
                if (state->emitter_type == EMITTER_FOUNTAIN) {
                    /* Gravity slider */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Gravity: %.1f",
                                     (double)state->gravity);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##gravity",
                                               &state->gravity,
                                               -20.0f, 0.0f, SLIDER_HEIGHT);

                    /* Drag slider */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Drag: %.2f",
                                     (double)state->drag);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##drag",
                                               &state->drag,
                                               0.0f, 5.0f, SLIDER_HEIGHT);
                } else if (state->emitter_type == EMITTER_FIRE) {
                    /* Spread slider — widens the flame */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Spread: %.1f",
                                     (double)state->fire_spread);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##fire_spread",
                                               &state->fire_spread,
                                               0.2f, 5.0f, SLIDER_HEIGHT);
                } else if (state->emitter_type == EMITTER_SMOKE) {
                    /* Rise speed slider */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Rise Speed: %.1f",
                                     (double)state->smoke_rise_speed);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##smoke_rise",
                                               &state->smoke_rise_speed,
                                               0.2f, 3.0f, SLIDER_HEIGHT);

                    /* Spread slider */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Spread: %.1f",
                                     (double)state->smoke_spread);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##smoke_spread",
                                               &state->smoke_spread,
                                               0.2f, 5.0f, SLIDER_HEIGHT);

                    /* Opacity slider */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Opacity: %.2f",
                                     (double)state->smoke_opacity);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##smoke_opacity",
                                               &state->smoke_opacity,
                                               0.05f, 1.0f, SLIDER_HEIGHT);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT / 2);

                /* ── Burst button ─────────────────────── */
                if (forge_ui_ctx_button_layout(ui, "Burst (B)",
                                                BUTTON_HEIGHT)) {
                    state->burst_requested = true;
                }

                /* ── Info labels ──────────────────────── */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT / 2);
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Pool: %d particles",
                                 MAX_PARTICLES);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Frame: %.1f ms",
                                 (double)(dt * 1000.0f));
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ═══════════════════════════════════════════════════════════════════════
 * SDL_AppQuit — Release all resources
 * ═══════════════════════════════════════════════════════════════════════ */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    if (!appstate) return;

    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *dev = forge_scene_device(&state->scene);

    if (dev) {
        /* Wait for all GPU work to finish before releasing resources */
        if (!SDL_WaitForGPUIdle(dev)) {
            SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }

        /* Release compute pipeline */
        if (state->sim_pipeline) {
            SDL_ReleaseGPUComputePipeline(dev, state->sim_pipeline);
        }

        /* Release graphics pipelines */
        if (state->additive_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(dev, state->additive_pipeline);
        }
        if (state->alpha_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(dev, state->alpha_pipeline);
        }

        /* Release GPU buffers */
        if (state->particle_buffer) {
            SDL_ReleaseGPUBuffer(dev, state->particle_buffer);
        }
        if (state->counter_buffer) {
            SDL_ReleaseGPUBuffer(dev, state->counter_buffer);
        }

        /* Release transfer buffer */
        if (state->counter_transfer) {
            SDL_ReleaseGPUTransferBuffer(dev, state->counter_transfer);
        }

        /* Release atlas resources */
        if (state->atlas_texture) {
            SDL_ReleaseGPUTexture(dev, state->atlas_texture);
        }
        if (state->atlas_sampler) {
            SDL_ReleaseGPUSampler(dev, state->atlas_sampler);
        }
    }

    /* Destroy the scene renderer (device, window, all internal resources) */
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
