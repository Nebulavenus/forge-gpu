/*
 * Lesson 03 — Uniforms & Motion
 *
 * Make the triangle from Lesson 02 spin by passing the elapsed time to
 * the vertex shader through a uniform buffer.
 *
 * Concepts introduced:
 *   - Uniform buffers  — small blocks of data pushed from the CPU to the
 *                         GPU each frame (or whenever they change)
 *   - Push uniforms    — SDL GPU's lightweight way of setting uniform
 *                         data without creating a GPU buffer object
 *   - Animation        — using elapsed time to drive shader math
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, swapchain   (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline      (Lesson 02)
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include "math/forge_math.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
/* This is NOT part of the lesson — it's build infrastructure that lets us
 * programmatically capture screenshots for the README.  Compiled only when
 * cmake is run with -DFORGE_CAPTURE=ON.  You can ignore these #ifdef blocks
 * entirely; the lesson works the same with or without them.
 * See: scripts/capture_lesson.py, common/capture/forge_capture.h */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
/* These headers contain SPIRV (Vulkan) and DXIL (D3D12) bytecodes compiled
 * from the HLSL source files in shaders/.  See README.md for how to
 * recompile them if you modify the HLSL. */
#include "shaders/triangle_vert_spirv.h"
#include "shaders/triangle_frag_spirv.h"
#include "shaders/triangle_vert_dxil.h"
#include "shaders/triangle_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 03 Uniforms & Motion"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Linear-space clear color — a dark blue-grey background. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.03f
#define CLEAR_A 1.0f

/* Number of vertices in our triangle. */
#define VERTEX_COUNT 3

/* Number of vertex attributes (position, color). */
#define NUM_VERTEX_ATTRIBUTES 2

/* Shader resource counts.
 * NEW: the vertex shader now uses 1 uniform buffer (for time).
 * The fragment shader still uses none. */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1   /* ← NEW: one uniform buffer */

#define FRAG_NUM_SAMPLERS         0
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* Rotation speed in radians per second. */
#define ROTATION_SPEED 1.0f

/* Starting angle so the triangle is visibly rotated in a static screenshot. */
#define INITIAL_ROTATION 0.8f

/* ── Vertex format ────────────────────────────────────────────────────────── */
/* Same as Lesson 02: each vertex has a 2D position and an RGB color.
 *
 * We use the forge-gpu math library types:
 *   - vec2 (HLSL: float2) for 2D positions
 *   - vec3 (HLSL: float3) for RGB colors
 *
 * Memory layout (20 bytes per vertex):
 *   offset 0:  vec2 position    (8 bytes)  → TEXCOORD0 in HLSL
 *   offset 8:  vec3 color      (12 bytes)  → TEXCOORD1 in HLSL
 *
 * See: lessons/math/01-vectors for an explanation of vector types.
 */

typedef struct Vertex {
    vec2 position;   /* position in normalized device coordinates */
    vec3 color;      /* color (0.0–1.0 per channel)              */
} Vertex;

/* ── Uniform data ─────────────────────────────────────────────────────────── */
/* This struct is pushed to the vertex shader every frame.
 *
 * The layout must follow std140 rules (the GPU's standard for uniform
 * buffer packing).  Two adjacent floats are fine — they're each 4-byte
 * aligned naturally.  If you add a vec3 or vec4 later, it must start at
 * a 16-byte boundary (add padding floats to get there). */

typedef struct Uniforms {
    float time;     /* elapsed time in seconds                     */
    float aspect;   /* window width / height — for correcting NDC  */
} Uniforms;

/* ── Triangle data ────────────────────────────────────────────────────────── */
/* We center the triangle so its centroid (average of all vertices) sits at
 * the origin.  This way, rotation in the shader spins it in place instead
 * of wobbling around an off-center point.
 *
 * Lesson 02's vertices had a centroid at (0, -0.167) because the bottom
 * edge was lower than the top was high.  These adjusted y-values put the
 * centroid exactly at (0, 0):  (0.5 + -0.25 + -0.25) / 3 = 0.  */

static const Vertex triangle_vertices[VERTEX_COUNT] = {
    /* Using math library: vec2 for position, vec3 for color */
    { .position = {  0.0f,  0.5f  }, .color = { 1.0f, 0.0f, 0.0f } },  /* top:          red   */
    { .position = { -0.5f, -0.25f }, .color = { 0.0f, 1.0f, 0.0f } },  /* bottom-left:  green */
    { .position = {  0.5f, -0.25f }, .color = { 0.0f, 0.0f, 1.0f } },  /* bottom-right: blue  */
};

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer           *vertex_buffer;
    Uint64                   start_ticks;   /* ← NEW: timestamp at startup */
#ifdef FORGE_CAPTURE
    ForgeCapture             capture;   /* screenshot infrastructure — see note above */
#endif
} app_state;

/* ── Shader helper ────────────────────────────────────────────────────────── */
/* Creates a GPU shader from pre-compiled bytecodes, picking the right format
 * for the current backend (Vulkan → SPIRV, D3D12 → DXIL).
 *
 * Unlike Lesson 02, the resource counts are now parameters — the vertex
 * shader needs num_uniform_buffers = 1, the fragment shader needs 0. */

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
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Request an sRGB swapchain (same as Lesson 01 & 02) ─────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        SDL_SetGPUSwapchainParameters(
            device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
            SDL_GPU_PRESENTMODE_VSYNC);
    }

    /* ── 5. Create shaders ──────────────────────────────────────────────
     * NEW: the vertex shader declares num_uniform_buffers = 1, telling
     * SDL that we'll push one block of uniform data each frame.  The
     * fragment shader still uses 0. */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        triangle_vert_spirv, triangle_vert_spirv_size,
        triangle_vert_dxil,  triangle_vert_dxil_size,
        VERT_NUM_SAMPLERS,
        VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS,
        VERT_NUM_UNIFORM_BUFFERS);
    if (!vertex_shader) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader *fragment_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        triangle_frag_spirv, triangle_frag_spirv_size,
        triangle_frag_dxil,  triangle_frag_dxil_size,
        FRAG_NUM_SAMPLERS,
        FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS,
        FRAG_NUM_UNIFORM_BUFFERS);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 6. Create graphics pipeline (same as Lesson 02) ──────────────── */
    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(Vertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    vertex_attributes[0].location    = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[0].offset      = offsetof(Vertex, position);

    vertex_attributes[1].location    = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[1].offset      = offsetof(Vertex, color);

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);

    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions     = &vertex_buffer_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers              = 1;
    pipeline_info.vertex_input_state.vertex_attributes               = vertex_attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes           = NUM_VERTEX_ATTRIBUTES;

    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipeline_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = SDL_GetGPUSwapchainTextureFormat(device, window);

    pipeline_info.target_info.color_target_descriptions = &color_target_desc;
    pipeline_info.target_info.num_color_targets         = 1;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(
        device, &pipeline_info);
    if (!pipeline) {
        SDL_Log("Failed to create graphics pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, fragment_shader);
        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    /* ── 7. Create & upload vertex buffer (same as Lesson 02) ─────────── */
    SDL_GPUBufferCreateInfo buffer_info;
    SDL_zero(buffer_info);
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer_info.size  = sizeof(triangle_vertices);

    SDL_GPUBuffer *vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (!vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info;
    SDL_zero(transfer_info);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size  = sizeof(triangle_vertices);

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        device, &transfer_info);
    if (!transfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_memcpy(mapped, triangle_vertices, sizeof(triangle_vertices));
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = transfer;
    src.offset          = 0;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = vertex_buffer;
    dst.offset = 0;
    dst.size   = sizeof(triangle_vertices);

    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    /* ── 8. Store state ────────────────────────────────────────────────── */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window        = window;
    state->device        = device;
    state->pipeline      = pipeline;
    state->vertex_buffer = vertex_buffer;
    state->start_ticks   = SDL_GetTicks();   /* ← NEW: record start time */

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUBuffer(device, vertex_buffer);
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
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
/* Each frame:
 *   1. Compute elapsed time
 *   2. Push the time to the vertex shader as a uniform
 *   3. Clear, bind, draw (same as Lesson 02)
 *
 * The push happens BEFORE the render pass — SDL latches the uniform data
 * when you begin the pass, so it must be set first. */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute elapsed time and aspect ratio ────────────────────
     * SDL_GetTicks returns milliseconds since SDL_Init.  We subtract
     * start_ticks so the animation begins at 0 when the app launches,
     * then multiply by ROTATION_SPEED to control how fast it spins.
     *
     * The aspect ratio corrects for non-square windows.  Without it,
     * NDC coordinates map directly to pixels — a circle in NDC becomes
     * an ellipse on an 800×600 window because the x-axis is stretched.
     * We query the window size each frame so this stays correct even if
     * the window were resized (we don't handle resize yet, but it's
     * good practice). */
    Uint64 now_ms = SDL_GetTicks();
    float elapsed = (float)(now_ms - state->start_ticks) / 1000.0f;

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(state->window, &w, &h);
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

    Uniforms uniforms;
    uniforms.time   = elapsed * ROTATION_SPEED + INITIAL_ROTATION;
    uniforms.aspect = aspect;

    /* ── 2. Acquire command buffer ────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 3. Push uniform data ─────────────────────────────────────────
     * SDL_PushGPUVertexUniformData sends our Uniforms struct to the
     * vertex shader.  Parameters:
     *   cmd       — the command buffer this draw will be recorded into
     *   slot 0    — matches register(b0, space1) in the HLSL
     *   &uniforms — pointer to the data
     *   sizeof    — size in bytes
     *
     * The data is copied internally, so `uniforms` can live on the stack.
     * This must happen BEFORE SDL_BeginGPURenderPass. */
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* ── 4. Acquire swapchain & render ────────────────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        SDL_GPUColorTargetInfo color_target = { 0 };
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A };

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, NULL);

        SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

        SDL_GPUBufferBinding vertex_binding;
        SDL_zero(vertex_binding);
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

        SDL_DrawGPUPrimitives(pass, VERTEX_COUNT, 1, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            SDL_SubmitGPUCommandBuffer(cmd);
        }
        if (forge_capture_should_quit(&state->capture)) {
            return SDL_APP_SUCCESS;
        }
    } else
#endif
    {
        SDL_SubmitGPUCommandBuffer(cmd);
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
        SDL_ReleaseGPUBuffer(state->device, state->vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
