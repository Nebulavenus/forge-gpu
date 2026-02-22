/*
 * Lesson 25 — Shader Noise
 *
 * Why this lesson exists:
 *   Math Lessons 12–14 teach noise algorithms on the CPU — hash functions,
 *   gradient noise, and blue noise.  But real-time graphics need noise
 *   evaluated per-pixel on the GPU.  This lesson ports those algorithms
 *   to HLSL fragment shaders and demonstrates six noise types that form
 *   the foundation of procedural content generation.
 *
 * What this lesson teaches:
 *   1. Porting integer hash functions to HLSL (Wang hash, hash_combine)
 *   2. White noise — per-cell random values from hash functions
 *   3. Value noise — bilinear interpolation of hashed lattice values
 *   4. Gradient noise — Perlin 2D with quintic interpolation
 *   5. fBm — octave stacking for natural fractal detail
 *   6. Domain warping — composing fBm with itself for organic patterns
 *   7. Procedural terrain — mapping noise height to biome colors
 *   8. Interleaved Gradient Noise for dithering (banding reduction)
 *   9. The fullscreen quad pattern (SV_VertexID, no vertex buffer)
 *
 * Scene:
 *   A fullscreen quad where the fragment shader generates noise patterns
 *   in real time.  No 3D geometry or textures — everything is procedural.
 *   Six modes demonstrate different noise types, switchable with number
 *   keys.  All noise functions are animated with time.
 *
 * Controls:
 *   1-6      — Switch noise mode
 *   D        — Toggle dithering (Interleaved Gradient Noise)
 *   =/+      — Increase noise scale (zoom in to detail)
 *   -        — Decrease noise scale (zoom out)
 *   Space    — Pause/resume animation
 *   Escape   — Quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include "math/forge_math.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecodes ────────────────────────────────────────────── */

/* Noise shaders — fullscreen quad vertex + noise fragment */
#include "shaders/compiled/noise_frag_dxil.h"
#include "shaders/compiled/noise_frag_spirv.h"
#include "shaders/compiled/noise_vert_dxil.h"
#include "shaders/compiled/noise_vert_spirv.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Window dimensions (16:9 standard for consistent screenshots). */
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Fullscreen quad — two triangles, no vertex buffer (SV_VertexID). */
#define FULLSCREEN_QUAD_VERTS 6

/* Number of noise modes available (matching shader). */
#define NUM_NOISE_MODES 6

/* Noise scale controls how many noise cells are visible.
 * Higher values show more (smaller) cells, revealing fine detail.
 * Lower values show fewer (larger) cells, emphasizing broad patterns. */
#define DEFAULT_SCALE 8.0f
#define MIN_SCALE     1.0f
#define MAX_SCALE     64.0f
#define SCALE_STEP    1.0f

/* Frame timing — cap delta time to prevent huge jumps after hitches. */
#define MAX_FRAME_DT 0.1f

/* Noise mode indices (must match the if/else chain in noise.frag.hlsl). */
#define MODE_WHITE_NOISE  0
#define MODE_VALUE_NOISE  1
#define MODE_PERLIN       2
#define MODE_FBM          3
#define MODE_DOMAIN_WARP  4
#define MODE_TERRAIN      5

/* Mode display names for log messages. */
static const char *MODE_NAMES[] = {
    "White Noise (hash-based)",
    "Value Noise (interpolated)",
    "Gradient Noise (Perlin 2D)",
    "fBm (Fractal Brownian Motion)",
    "Domain Warping",
    "Procedural Terrain"
};

/* ── Uniform struct ───────────────────────────────────────────────────────── */

/* Fragment uniforms — must match the HLSL cbuffer layout in noise.frag.hlsl
 * exactly (same order, same sizes, same alignment).
 *
 * Layout (32 bytes total):
 *   float  time             (offset  0, 4 bytes)
 *   int    mode             (offset  4, 4 bytes)
 *   int    dither_enabled   (offset  8, 4 bytes)
 *   float  scale            (offset 12, 4 bytes)
 *   vec2   resolution       (offset 16, 8 bytes)
 *   float  _pad[2]          (offset 24, 8 bytes) — pad to 32 bytes
 */
typedef struct NoiseUniforms {
    float time;           /* elapsed animation time in seconds             */
    int   mode;           /* noise type index (0–5, see MODE_* constants)  */
    int   dither_enabled; /* 1 = IGN dithering active, 0 = off            */
    float scale;          /* spatial frequency — cells visible on screen   */
    vec2  resolution;     /* window size in pixels (width, height)         */
    float _pad[2];        /* pad to 32 bytes for cbuffer alignment         */
} NoiseUniforms;

/* ── Application state ────────────────────────────────────────────────────── */

/* All state persists across SDL callbacks via the appstate pointer. */
typedef struct {
    SDL_GPUDevice *device;                  /* GPU device handle (Vulkan/D3D12)    */
    SDL_Window    *window;                  /* main application window             */
    SDL_GPUGraphicsPipeline *pipeline;      /* fullscreen noise pipeline (no VB)   */

    /* Noise parameters (controlled by keyboard). */
    float time;            /* accumulated time for animation              */
    int   noise_mode;      /* current noise type (0-5)                    */
    int   dither_enabled;  /* 1 = IGN dithering active                    */
    float scale;           /* spatial frequency of noise                  */
    bool  paused;          /* true = animation frozen                     */

    Uint64 last_ticks;     /* timestamp of previous frame (for delta time) */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;  /* screenshot / GIF capture state */
#endif
} AppState;

/* ── Helper: create shader from SPIRV/DXIL bytecodes ──────────────────────── */

/* Queries the GPU device for its supported shader format (SPIRV for
 * Vulkan, DXIL for D3D12) and creates the shader from the matching
 * bytecode.  Both formats are compiled offline and embedded as C arrays. */
static SDL_GPUShader *create_shader(
    SDL_GPUDevice   *device,
    SDL_GPUShaderStage stage,
    const Uint8     *spirv_code,
    size_t           spirv_size,
    const Uint8     *dxil_code,
    size_t           dxil_size,
    Uint32           num_samplers,
    Uint32           num_uniform_buffers
) {
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage               = stage;
    info.entrypoint          = "main";
    info.num_samplers        = num_samplers;
    info.num_uniform_buffers = num_uniform_buffers;

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
        SDL_Log("Failed to create shader: %s", SDL_GetError());
    }
    return shader;
}

/* ══════════════════════════════════════════════════════════════════════════
 * SDL_AppInit — create device, window, and pipeline
 * ══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Allocate persistent state — SDL_calloc zeros all fields. */
    AppState *state = SDL_calloc(1, sizeof(AppState));
    if (!state) {
        SDL_Log("Failed to allocate AppState");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
#endif

    /* Initialize SDL with video subsystem. */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Create GPU device — request any backend (Vulkan, D3D12, Metal). */
    state->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true,  /* debug mode — enables validation layers */
        NULL   /* no preferred backend */
    );
    if (!state->device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU driver: %s", SDL_GetGPUDeviceDriver(state->device));

    /* Create window. */
    state->window = SDL_CreateWindow(
        "Lesson 25 — Shader Noise",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        0 /* no special flags */
    );
    if (!state->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Claim the window for GPU rendering. */
    if (!SDL_ClaimWindowForGPUDevice(state->device, state->window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Set sRGB swapchain for correct gamma output.
     * SDR_LINEAR gives a B8G8R8A8_UNORM_SRGB format — the GPU
     * automatically converts linear fragment shader output to sRGB
     * when writing to the swapchain.  Without this, colors appear
     * too dark because the display applies gamma on top of the
     * already-gamma-encoded values. */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            state->device, state->window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                state->device, state->window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
        }
    }

    SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(
        state->device, state->window
    );

    /* ── Create shaders ─────────────────────────────────────────────── */

    /* Vertex shader: generates fullscreen quad from SV_VertexID.
     * No samplers, no uniforms — purely procedural geometry. */
    SDL_GPUShader *vert = create_shader(
        state->device, SDL_GPU_SHADERSTAGE_VERTEX,
        noise_vert_spirv, sizeof(noise_vert_spirv),
        noise_vert_dxil,  sizeof(noise_vert_dxil),
        0, 0  /* no samplers, no uniform buffers */
    );

    /* Fragment shader: evaluates noise functions per pixel.
     * No samplers (all noise is procedural), 1 uniform buffer
     * (time, mode, scale, resolution). */
    SDL_GPUShader *frag = create_shader(
        state->device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        noise_frag_spirv, sizeof(noise_frag_spirv),
        noise_frag_dxil,  sizeof(noise_frag_dxil),
        0, 1  /* no samplers, 1 uniform buffer */
    );

    if (!vert || !frag) {
        SDL_Log("Failed to create noise shaders");
        if (vert) SDL_ReleaseGPUShader(state->device, vert);
        if (frag) SDL_ReleaseGPUShader(state->device, frag);
        return SDL_APP_FAILURE;
    }

    /* ── Create graphics pipeline ───────────────────────────────────── */
    {
        /* No vertex input — SV_VertexID generates fullscreen quad
         * positions entirely in the vertex shader. */
        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        color_desc.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe_info;
        SDL_zero(pipe_info);
        pipe_info.vertex_shader   = vert;
        pipe_info.fragment_shader = frag;
        /* No vertex_input_state — positions from SV_VertexID. */
        pipe_info.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        /* No depth testing — 2D fullscreen pass needs no depth buffer. */
        pipe_info.target_info.color_target_descriptions = &color_desc;
        pipe_info.target_info.num_color_targets         = 1;
        pipe_info.target_info.has_depth_stencil_target  = false;

        state->pipeline = SDL_CreateGPUGraphicsPipeline(
            state->device, &pipe_info
        );
    }

    /* Release shader modules after pipeline creation — the pipeline
     * keeps its own copy of the compiled shader bytecode. */
    SDL_ReleaseGPUShader(state->device, vert);
    SDL_ReleaseGPUShader(state->device, frag);

    if (!state->pipeline) {
        SDL_Log(
            "SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError()
        );
        return SDL_APP_FAILURE;
    }

    /* ── Initialize noise parameters ────────────────────────────────── */

    /* Start with fBm — the most visually interesting mode for a
     * first impression.  Users can switch with number keys. */
    state->noise_mode     = MODE_FBM;
    state->scale          = DEFAULT_SCALE;
    state->dither_enabled = 0;
    state->paused         = false;
    state->last_ticks     = SDL_GetTicks();

    SDL_Log("Mode: %s (press 1-6 to switch)", MODE_NAMES[state->noise_mode]);
    SDL_Log("Scale: %.0f (press +/- to adjust)", state->scale);
    SDL_Log("Dithering: off (press D to toggle)");
    SDL_Log("Press Space to pause/resume animation");

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        forge_capture_init(&state->capture, state->device, state->window);
    }
#endif

    return SDL_APP_CONTINUE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * SDL_AppEvent — handle input
 * ══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *state = (AppState *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key = event->key.key;

        /* Mode selection: keys 1-6 switch noise type.
         * SDLK_1 through SDLK_6 are sequential, so subtracting
         * SDLK_1 gives the mode index (0-5). */
        if (key >= SDLK_1 && key <= SDLK_6) {
            state->noise_mode = (int)(key - SDLK_1);
            SDL_Log("Mode: %s", MODE_NAMES[state->noise_mode]);
        }

        /* Toggle Interleaved Gradient Noise dithering.
         * Dithering adds sub-pixel noise to reduce color banding
         * in 8-bit output — most visible in smooth gradients. */
        if (key == SDLK_D) {
            state->dither_enabled = !state->dither_enabled;
            SDL_Log("Dithering: %s",
                    state->dither_enabled ? "on" : "off");
        }

        /* Scale adjustment — zoom in/out of noise patterns.
         * Higher scale reveals finer detail (more noise cells visible).
         * Lower scale shows broad structure. */
        if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
            state->scale += SCALE_STEP;
            if (state->scale > MAX_SCALE) state->scale = MAX_SCALE;
            SDL_Log("Scale: %.0f", state->scale);
        }
        if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
            state->scale -= SCALE_STEP;
            if (state->scale < MIN_SCALE) state->scale = MIN_SCALE;
            SDL_Log("Scale: %.0f", state->scale);
        }

        /* Pause/resume animation. */
        if (key == SDLK_SPACE) {
            state->paused = !state->paused;
            SDL_Log("Animation: %s",
                    state->paused ? "paused" : "running");
        }

        /* Quit on Escape. */
        if (key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * SDL_AppIterate — render one frame
 * ══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;

    /* ── Delta time ─────────────────────────────────────────────────── */

    Uint64 now = SDL_GetTicks();
    float dt = (float)(now - state->last_ticks) / 1000.0f;
    state->last_ticks = now;

    /* Cap delta time to avoid huge jumps after window drag or hitch. */
    if (dt > MAX_FRAME_DT) {
        dt = MAX_FRAME_DT;
    }
    if (!state->paused) {
        state->time += dt;
    }

    /* ── Acquire command buffer and swapchain ───────────────────────── */

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_CONTINUE;
    }

    SDL_GPUTexture *swapchain = NULL;
    Uint32 sw, sh;
    if (!SDL_AcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain, &sw, &sh)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    if (!swapchain) {
        /* Window minimized or swapchain unavailable — submit empty
         * command buffer and wait for next frame. */
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ── Render pass: fullscreen noise ──────────────────────────────── */
    {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture  = swapchain;
        /* DONT_CARE because the fragment shader writes every pixel —
         * no need to clear first. */
        color_target.load_op  = SDL_GPU_LOADOP_DONT_CARE;
        color_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, NULL
        );
        if (!pass) {
            SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            return SDL_APP_CONTINUE;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

        /* Push fragment uniforms — noise parameters updated each frame. */
        NoiseUniforms uniforms;
        SDL_zero(uniforms);
        uniforms.time            = state->time;
        uniforms.mode            = state->noise_mode;
        uniforms.dither_enabled  = state->dither_enabled;
        uniforms.scale           = state->scale;
        uniforms.resolution      = vec2_create((float)sw, (float)sh);
        SDL_PushGPUFragmentUniformData(
            cmd, 0, &uniforms, sizeof(uniforms)
        );

        /* Draw fullscreen quad — 6 vertices forming 2 triangles.
         * No vertex buffer is bound; positions come from SV_VertexID
         * in the vertex shader. */
        SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    /* ── Submit ────────────────────────────────────────────────────── */

#ifdef FORGE_CAPTURE
    if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
        if (forge_capture_should_quit(&state->capture)) {
            return SDL_APP_SUCCESS;
        }
        return SDL_APP_CONTINUE;
    }
#endif

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }

    return SDL_APP_CONTINUE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * SDL_AppQuit — release all resources
 * ══════════════════════════════════════════════════════════════════════════ */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    AppState *state = (AppState *)appstate;
    if (!state) return;

    /* Release GPU resources in reverse creation order. */
    if (state->pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->pipeline);
    }

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    /* Release the window from the GPU device before destroying it. */
    if (state->window) {
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
    }

    if (state->device) {
        SDL_DestroyGPUDevice(state->device);
    }

    SDL_free(state);
}
