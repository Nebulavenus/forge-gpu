/*
 * Lesson 11 — Compute Shaders
 *
 * Introduce GPU compute: general-purpose programs that run on the GPU but
 * are not tied to the graphics pipeline.  This lesson generates an animated
 * procedural plasma texture entirely on the GPU using a compute shader,
 * then displays it fullscreen using a simple graphics pipeline.
 *
 * Concepts introduced:
 *   - Compute pipeline    — SDL_GPUComputePipeline, separate from graphics
 *   - Storage textures    — RWTexture2D for random-access write from compute
 *   - Dispatch groups     — [numthreads(8,8,1)] workgroups, ceil dispatch
 *   - Compute uniforms    — SDL_PushGPUComputeUniformData for time/resolution
 *   - Compute-then-render — compute writes a texture, render pass samples it
 *   - Fullscreen triangle — 3 vertices from SV_VertexID, no vertex buffer
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Shader loading / format selection (SPIRV or DXIL)      (Lesson 02)
 *   - Push uniforms for per-frame data                       (Lesson 03)
 *   - Texture + sampler binding                              (Lesson 04)
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
/* Compute shader (plasma generator) */
#include "shaders/plasma_comp_spirv.h"
#include "shaders/plasma_comp_dxil.h"

/* Graphics shaders (fullscreen display) */
#include "shaders/fullscreen_vert_spirv.h"
#include "shaders/fullscreen_vert_dxil.h"
#include "shaders/fullscreen_frag_spirv.h"
#include "shaders/fullscreen_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 11 Compute Shaders"
#define WINDOW_WIDTH  600
#define WINDOW_HEIGHT 600

/* Plasma texture dimensions.  Fixed size keeps the lesson simple;
 * dynamic resize (matching the window) is left as an exercise. */
#define PLASMA_WIDTH  512
#define PLASMA_HEIGHT 512

/* Compute workgroup size — must match [numthreads(8, 8, 1)] in the HLSL.
 * 8x8 = 64 threads per group is a common choice for 2D image work. */
#define WORKGROUP_SIZE 8

/* Linear-space clear color — dark blue-grey (same as all lessons). */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.03f
#define CLEAR_A 1.0f

/* Milliseconds-to-seconds conversion */
#define MS_TO_SEC 1000.0f

/* Number of vertices for the fullscreen triangle (generated in the shader). */
#define FULLSCREEN_TRI_VERTS 3

/* ── Shader resource counts ──────────────────────────────────────────────── */
/* These must match the register declarations in each HLSL shader exactly.
 * SDL uses these counts to validate bindings at pipeline creation time. */

/* Compute shader: 1 RW storage texture (u0) + 1 uniform buffer (b0) */
#define COMP_NUM_SAMPLERS                    0
#define COMP_NUM_READONLY_STORAGE_TEXTURES   0
#define COMP_NUM_READONLY_STORAGE_BUFFERS    0
#define COMP_NUM_READWRITE_STORAGE_TEXTURES  1
#define COMP_NUM_READWRITE_STORAGE_BUFFERS   0
#define COMP_NUM_UNIFORM_BUFFERS             1

/* Vertex shader: no resources (pure geometry generation) */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  0

/* Fragment shader: 1 sampler (texture + sampler pair) */
#define FRAG_NUM_SAMPLERS         1
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* ── Compute uniform data ────────────────────────────────────────────────── */
/* Pushed to the compute shader each frame via SDL_PushGPUComputeUniformData.
 * Must be 16-byte aligned to match the GPU's cbuffer layout. */

typedef struct ComputeUniforms {
    float time;       /* elapsed seconds — drives the animation       */
    float width;      /* texture width in pixels                      */
    float height;     /* texture height in pixels                     */
    float _pad;       /* padding to 16-byte boundary                  */
} ComputeUniforms;

/* ── Application state ───────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window               *window;
    SDL_GPUDevice            *device;
    SDL_GPUComputePipeline   *compute_pipeline;
    SDL_GPUGraphicsPipeline  *graphics_pipeline;
    SDL_GPUTexture           *plasma_texture;
    SDL_GPUSampler           *sampler;
    Uint64                    start_ticks;
#ifdef FORGE_CAPTURE
    ForgeCapture              capture;
#endif
} app_state;

/* ── Shader helper (same pattern as all previous lessons) ────────────────── */
/* Creates a vertex or fragment shader, selecting SPIRV or DXIL based on
 * the GPU backend.  See Lesson 02 for the full explanation. */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice       *device,
    SDL_GPUShaderStage   stage,
    const unsigned char *spirv_code,  unsigned int spirv_size,
    const unsigned char *dxil_code,   unsigned int dxil_size,
    int                  num_samplers,
    int                  num_storage_textures,
    int                  num_storage_buffers,
    int                  num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage                = stage;
    info.entrypoint           = "main";
    info.num_samplers         = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers  = num_storage_buffers;
    info.num_uniform_buffers  = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else {
        SDL_Log("No supported shader format (need SPIRV or DXIL)");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("Failed to create %s shader: %s",
                stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
                SDL_GetError());
    }
    return shader;
}

/* ── Compute pipeline helper ─────────────────────────────────────────────── */
/* NEW: Creates a compute pipeline.  This is similar to create_shader but
 * uses SDL_GPUComputePipelineCreateInfo instead of SDL_GPUShaderCreateInfo.
 *
 * Key differences from graphics shaders:
 *   - The create info embeds the shader code directly (no separate shader object)
 *   - threadcount_x/y/z must match [numthreads()] in the HLSL
 *   - Resource counts distinguish read-only from read-write storage */

static SDL_GPUComputePipeline *create_compute_pipeline(
    SDL_GPUDevice       *device,
    const unsigned char *spirv_code,  unsigned int spirv_size,
    const unsigned char *dxil_code,   unsigned int dxil_size,
    int num_samplers,
    int num_readonly_storage_textures,
    int num_readonly_storage_buffers,
    int num_readwrite_storage_textures,
    int num_readwrite_storage_buffers,
    int num_uniform_buffers,
    int threadcount_x,
    int threadcount_y,
    int threadcount_z)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUComputePipelineCreateInfo info;
    SDL_zero(info);
    info.entrypoint                     = "main";
    info.num_samplers                   = num_samplers;
    info.num_readonly_storage_textures  = num_readonly_storage_textures;
    info.num_readonly_storage_buffers   = num_readonly_storage_buffers;
    info.num_readwrite_storage_textures = num_readwrite_storage_textures;
    info.num_readwrite_storage_buffers  = num_readwrite_storage_buffers;
    info.num_uniform_buffers            = num_uniform_buffers;
    info.threadcount_x                  = threadcount_x;
    info.threadcount_y                  = threadcount_y;
    info.threadcount_z                  = threadcount_z;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else {
        SDL_Log("No supported shader format (need SPIRV or DXIL)");
        return NULL;
    }

    SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(
        device, &info);
    if (!pipeline) {
        SDL_Log("Failed to create compute pipeline: %s", SDL_GetError());
    }
    return pipeline;
}

/* ── SDL_AppInit ──────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* ── 1. Initialise SDL ─────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ──────────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV |
        SDL_GPU_SHADERFORMAT_DXIL,
        true,   /* debug mode */
        NULL    /* no backend preference */
    );
    if (!device) {
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create window & claim swapchain ────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Request an sRGB swapchain ──────────────────────────────────
     * SDR_LINEAR gives us B8G8R8A8_UNORM_SRGB — the GPU automatically
     * converts our linear-space output to sRGB on write.  The compute
     * shader writes linear values; sRGB conversion happens here. */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
        }
    }

    /* ── 5. Create compute pipeline ───────────────────────────────────
     * NEW: This is a compute pipeline, not a graphics pipeline.  It takes
     * the compute shader directly — no separate shader object step.
     * The threadcount values must match [numthreads(8, 8, 1)] in the HLSL. */
    SDL_GPUComputePipeline *compute_pipeline = create_compute_pipeline(
        device,
        plasma_comp_spirv, plasma_comp_spirv_size,
        plasma_comp_dxil,  plasma_comp_dxil_size,
        COMP_NUM_SAMPLERS,
        COMP_NUM_READONLY_STORAGE_TEXTURES,
        COMP_NUM_READONLY_STORAGE_BUFFERS,
        COMP_NUM_READWRITE_STORAGE_TEXTURES,
        COMP_NUM_READWRITE_STORAGE_BUFFERS,
        COMP_NUM_UNIFORM_BUFFERS,
        WORKGROUP_SIZE, WORKGROUP_SIZE, 1);
    if (!compute_pipeline) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 6. Create graphics shaders & pipeline ────────────────────────
     * The graphics pipeline is minimal: no vertex buffer, no depth,
     * no culling.  It just draws a fullscreen triangle that samples
     * the compute-generated texture. */
    SDL_GPUShader *vert_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, fullscreen_vert_spirv_size,
        fullscreen_vert_dxil,  fullscreen_vert_dxil_size,
        VERT_NUM_SAMPLERS,
        VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS,
        VERT_NUM_UNIFORM_BUFFERS);
    if (!vert_shader) {
        SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader *frag_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        fullscreen_frag_spirv, fullscreen_frag_spirv_size,
        fullscreen_frag_dxil,  fullscreen_frag_dxil_size,
        FRAG_NUM_SAMPLERS,
        FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS,
        FRAG_NUM_UNIFORM_BUFFERS);
    if (!frag_shader) {
        SDL_ReleaseGPUShader(device, vert_shader);
        SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Graphics pipeline: no vertex input, no depth, no culling.
     * This is the simplest possible pipeline — it only needs to draw
     * a fullscreen triangle that samples a texture. */
    SDL_GPUGraphicsPipelineCreateInfo gfx_info;
    SDL_zero(gfx_info);

    gfx_info.vertex_shader   = vert_shader;
    gfx_info.fragment_shader = frag_shader;

    /* No vertex input — positions generated from SV_VertexID */
    gfx_info.vertex_input_state.num_vertex_buffers    = 0;
    gfx_info.vertex_input_state.num_vertex_attributes = 0;

    gfx_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* No culling or depth — this is a flat 2D fullscreen effect */
    gfx_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    gfx_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    gfx_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Color target must match the swapchain format (sRGB if available) */
    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = SDL_GetGPUSwapchainTextureFormat(device, window);

    gfx_info.target_info.color_target_descriptions = &color_target_desc;
    gfx_info.target_info.num_color_targets         = 1;

    SDL_GPUGraphicsPipeline *graphics_pipeline =
        SDL_CreateGPUGraphicsPipeline(device, &gfx_info);
    if (!graphics_pipeline) {
        SDL_Log("Failed to create graphics pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, frag_shader);
        SDL_ReleaseGPUShader(device, vert_shader);
        SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Shaders can be released after pipeline creation — the pipeline
     * keeps its own internal copy of the compiled shader code. */
    SDL_ReleaseGPUShader(device, frag_shader);
    SDL_ReleaseGPUShader(device, vert_shader);

    /* ── 7. Create the shared plasma texture ─────────────────────────
     * This texture is shared between the compute and graphics pipelines:
     *   - COMPUTE_STORAGE_WRITE: the compute shader writes to it as RWTexture2D
     *   - SAMPLER: the fragment shader samples it as Texture2D
     *
     * Format is R8G8B8A8_UNORM (not _SRGB) because the compute shader
     * writes raw linear values.  The sRGB conversion happens at the
     * swapchain when the graphics pipeline renders to it. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE |
                                    SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = PLASMA_WIDTH;
    tex_info.height               = PLASMA_HEIGHT;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = 1;  /* no mipmaps needed */

    SDL_GPUTexture *plasma_texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!plasma_texture) {
        SDL_Log("Failed to create plasma texture: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, graphics_pipeline);
        SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Create sampler ────────────────────────────────────────────
     * Linear filtering smooths the plasma texture when the window size
     * doesn't exactly match the texture resolution. */
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter    = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter    = SDL_GPU_FILTER_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (!sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, plasma_texture);
        SDL_ReleaseGPUGraphicsPipeline(device, graphics_pipeline);
        SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 9. Store state ──────────────────────────────────────────────── */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, plasma_texture);
        SDL_ReleaseGPUGraphicsPipeline(device, graphics_pipeline);
        SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window            = window;
    state->device            = device;
    state->compute_pipeline  = compute_pipeline;
    state->graphics_pipeline = graphics_pipeline;
    state->plasma_texture    = plasma_texture;
    state->sampler           = sampler;
    state->start_ticks       = SDL_GetTicks();

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUSampler(device, sampler);
            SDL_ReleaseGPUTexture(device, plasma_texture);
            SDL_ReleaseGPUGraphicsPipeline(device, graphics_pipeline);
            SDL_ReleaseGPUComputePipeline(device, compute_pipeline);
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            SDL_free(state);
            return SDL_APP_FAILURE;
        }
    }
#endif

    *appstate = state;

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────────── */
/* Each frame has two phases:
 *
 *   1. COMPUTE PASS — bind the plasma texture as a RW storage texture,
 *      push time/resolution uniforms, dispatch enough workgroups to
 *      cover every pixel.
 *
 *   2. RENDER PASS — bind the same texture as a sampled texture,
 *      draw a fullscreen triangle to display the result.
 *
 * SDL3 automatically synchronises between the compute pass and the
 * render pass on the same command buffer — no manual barriers needed. */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute elapsed time ─────────────────────────────────────── */
    Uint64 now_ms = SDL_GetTicks();
    float elapsed = (float)(now_ms - state->start_ticks) / MS_TO_SEC;

    /* ── 2. Acquire command buffer ───────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 3. Push compute uniforms ────────────────────────────────────
     * Uniforms must be pushed BEFORE the compute pass begins.
     * The slot index (0) maps to register(b0, space2) in the HLSL. */
    ComputeUniforms uniforms;
    uniforms.time   = elapsed;
    uniforms.width  = (float)PLASMA_WIDTH;
    uniforms.height = (float)PLASMA_HEIGHT;
    uniforms._pad   = 0.0f;

    SDL_PushGPUComputeUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* ── 4. COMPUTE PASS — generate the plasma texture ──────────────
     * The storage texture binding tells SDL which texture the compute
     * shader will write to.  `cycle = true` enables frame pipelining:
     * SDL may use a different backing texture if the previous frame's
     * data is still in flight, avoiding a stall. */
    SDL_GPUStorageTextureReadWriteBinding storage_binding;
    SDL_zero(storage_binding);
    storage_binding.texture   = state->plasma_texture;
    storage_binding.mip_level = 0;
    storage_binding.layer     = 0;
    storage_binding.cycle     = true;

    SDL_GPUComputePass *compute_pass = SDL_BeginGPUComputePass(
        cmd,
        &storage_binding, 1,   /* 1 read-write storage texture */
        NULL, 0                /* no read-write storage buffers */
    );

    SDL_BindGPUComputePipeline(compute_pass, state->compute_pipeline);

    /* Dispatch enough workgroups to cover every pixel.
     * Integer ceiling division: (size + group - 1) / group
     * ensures we dispatch at least enough groups even when the
     * texture dimensions aren't exact multiples of the workgroup size.
     * The shader has a bounds check to discard out-of-range threads. */
    Uint32 groups_x = (PLASMA_WIDTH  + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    Uint32 groups_y = (PLASMA_HEIGHT + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);

    SDL_EndGPUComputePass(compute_pass);

    /* ── 5. RENDER PASS — display the plasma fullscreen ─────────────
     * SDL automatically synchronises: the compute pass finishes writing
     * before the render pass reads the same texture.  No manual
     * barriers or fences needed. */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain, NULL, NULL)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A };

        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, NULL);

        SDL_BindGPUGraphicsPipeline(render_pass, state->graphics_pipeline);

        /* Bind the plasma texture + sampler for the fragment shader.
         * Slot 0 maps to register(t0, space2) / register(s0, space2). */
        SDL_GPUTextureSamplerBinding tex_sampler_binding;
        SDL_zero(tex_sampler_binding);
        tex_sampler_binding.texture = state->plasma_texture;
        tex_sampler_binding.sampler = state->sampler;

        SDL_BindGPUFragmentSamplers(render_pass, 0,
                                    &tex_sampler_binding, 1);

        /* Draw 3 vertices — the fullscreen triangle.  No vertex buffer
         * is bound; the vertex shader generates positions from SV_VertexID. */
        SDL_DrawGPUPrimitives(render_pass, FULLSCREEN_TRI_VERTS, 1, 0, 0);

        SDL_EndGPURenderPass(render_pass);
    }

    /* ── 6. Submit ───────────────────────────────────────────────────── */
#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
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

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────────── */
/* Clean up in reverse order of creation. */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (state) {
#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, state->device);
#endif
        SDL_ReleaseGPUSampler(state->device, state->sampler);
        SDL_ReleaseGPUTexture(state->device, state->plasma_texture);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->graphics_pipeline);
        SDL_ReleaseGPUComputePipeline(state->device, state->compute_pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
