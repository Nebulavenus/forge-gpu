/*
 * Lesson 06 — Depth Buffer & 3D Transforms
 *
 * Render a colored spinning cube using the full Model-View-Projection
 * pipeline.  This is the first lesson that draws in true 3D — previous
 * lessons were all 2D (flat geometry in NDC space).
 *
 * Concepts introduced:
 *   - 3D vertex positions    — float3 instead of float2
 *   - MVP matrix             — Model * View * Projection composed on CPU
 *   - Depth buffer           — D16_UNORM texture, enable depth test/write
 *   - Back-face culling      — CULLMODE_BACK, only draw front faces
 *   - Window resize handling — recreate depth texture when size changes
 *   - Perspective projection — mat4_perspective for 3D foreshortening
 *   - Camera                 — mat4_look_at for view matrix
 *
 * All math operations use the forge_math library (common/math/forge_math.h,
 * see common/math/README.md).  The theory behind each transform is explained
 * in Math Lesson 05 — Matrices (lessons/math/05-matrices/).
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain  (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline          (Lesson 02)
 *   - Push uniforms (now a mat4 instead of time+aspect)   (Lesson 03)
 *   - Index buffers                                       (Lesson 04)
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include "math/forge_math.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
#include "shaders/cube_vert_spirv.h"
#include "shaders/cube_frag_spirv.h"
#include "shaders/cube_vert_dxil.h"
#include "shaders/cube_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 06 Depth Buffer & 3D Transforms"
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

/* Linear-space clear color — dark background so the cube stands out. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.04f
#define CLEAR_A 1.0f

/* Depth buffer clear value — 1.0 means "infinitely far away".
 * Anything closer will pass the depth test (LESS_OR_EQUAL). */
#define DEPTH_CLEAR 1.0f

/* Depth texture format — D16_UNORM is universally supported and sufficient
 * for our simple scene.  D24/D32 offer more precision for complex scenes. */
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D16_UNORM

/* Cube geometry: 6 faces × 4 vertices per face = 24 unique vertices.
 * We can't share vertices across faces because each face has a different
 * color, so every vertex needs to carry its face's color. */
#define VERTEX_COUNT 24
#define INDEX_COUNT  36   /* 6 faces × 2 triangles × 3 indices */

/* Number of vertex attributes (position, color). */
#define NUM_VERTEX_ATTRIBUTES 2

/* Shader resource counts. */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1   /* MVP matrix */

#define FRAG_NUM_SAMPLERS         0
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* Rotation speeds in radians per second — two different speeds
 * on different axes make the rotation look natural, not mechanical. */
#define ROTATE_Y_SPEED 1.0f
#define ROTATE_X_SPEED 0.7f

/* Camera parameters */
#define EYE_X    0.0f
#define EYE_Y    1.5f
#define EYE_Z    3.0f
#define FOV_DEG  60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

/* Milliseconds-to-seconds conversion factor. */
#define MS_TO_SEC 1000.0f

/* Half-extent of the cube — cube spans from -0.5 to +0.5 on each axis. */
#define CUBE_HALF 0.5f

/* ── Vertex format ────────────────────────────────────────────────────────── */
/* Each vertex has a 3D position and an RGB color.
 *
 * Unlike previous lessons which used vec2 positions (2D), this is our
 * first lesson with vec3 positions — true 3D geometry.
 *
 * Memory layout (24 bytes per vertex):
 *   offset 0:  vec3 position   (12 bytes) → TEXCOORD0 in HLSL
 *   offset 12: vec3 color      (12 bytes) → TEXCOORD1 in HLSL
 */

typedef struct Vertex {
    vec3 position;   /* 3D model-space position */
    vec3 color;      /* RGB per-vertex color    */
} Vertex;

/* ── Uniform data ─────────────────────────────────────────────────────────── */
/* A single MVP matrix — 64 bytes, naturally 16-byte aligned.
 * Composed on the CPU each frame: mvp = projection * view * model */

typedef struct Uniforms {
    mat4 mvp;
} Uniforms;

/* ── Cube geometry ────────────────────────────────────────────────────────── */
/* 24 vertices: 4 per face, with position and color.
 * Each face is a different solid color (complementary pairs on opposite faces):
 *   Front/Back:   Red / Cyan
 *   Right/Left:   Green / Magenta
 *   Top/Bottom:   Blue / Yellow
 *
 * Winding order: counter-clockwise as viewed from outside each face.
 * This matches our pipeline's front face (CCW) + back-face culling. */

static const Vertex cube_vertices[VERTEX_COUNT] = {
    /* Front face (Z = +0.5) — Red */
    { .position = { -CUBE_HALF, -CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 0.0f, 0.0f } },
    { .position = {  CUBE_HALF, -CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 0.0f, 0.0f } },
    { .position = {  CUBE_HALF,  CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 0.0f, 0.0f } },
    { .position = { -CUBE_HALF,  CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 0.0f, 0.0f } },

    /* Back face (Z = -0.5) — Cyan */
    { .position = {  CUBE_HALF, -CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 1.0f, 1.0f } },
    { .position = { -CUBE_HALF, -CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 1.0f, 1.0f } },
    { .position = { -CUBE_HALF,  CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 1.0f, 1.0f } },
    { .position = {  CUBE_HALF,  CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 1.0f, 1.0f } },

    /* Right face (X = +0.5) — Green */
    { .position = {  CUBE_HALF, -CUBE_HALF,  CUBE_HALF }, .color = { 0.0f, 1.0f, 0.0f } },
    { .position = {  CUBE_HALF, -CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 1.0f, 0.0f } },
    { .position = {  CUBE_HALF,  CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 1.0f, 0.0f } },
    { .position = {  CUBE_HALF,  CUBE_HALF,  CUBE_HALF }, .color = { 0.0f, 1.0f, 0.0f } },

    /* Left face (X = -0.5) — Magenta */
    { .position = { -CUBE_HALF, -CUBE_HALF, -CUBE_HALF }, .color = { 1.0f, 0.0f, 1.0f } },
    { .position = { -CUBE_HALF, -CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 0.0f, 1.0f } },
    { .position = { -CUBE_HALF,  CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 0.0f, 1.0f } },
    { .position = { -CUBE_HALF,  CUBE_HALF, -CUBE_HALF }, .color = { 1.0f, 0.0f, 1.0f } },

    /* Top face (Y = +0.5) — Blue */
    { .position = { -CUBE_HALF,  CUBE_HALF,  CUBE_HALF }, .color = { 0.0f, 0.0f, 1.0f } },
    { .position = {  CUBE_HALF,  CUBE_HALF,  CUBE_HALF }, .color = { 0.0f, 0.0f, 1.0f } },
    { .position = {  CUBE_HALF,  CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 0.0f, 1.0f } },
    { .position = { -CUBE_HALF,  CUBE_HALF, -CUBE_HALF }, .color = { 0.0f, 0.0f, 1.0f } },

    /* Bottom face (Y = -0.5) — Yellow */
    { .position = { -CUBE_HALF, -CUBE_HALF, -CUBE_HALF }, .color = { 1.0f, 1.0f, 0.0f } },
    { .position = {  CUBE_HALF, -CUBE_HALF, -CUBE_HALF }, .color = { 1.0f, 1.0f, 0.0f } },
    { .position = {  CUBE_HALF, -CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 1.0f, 0.0f } },
    { .position = { -CUBE_HALF, -CUBE_HALF,  CUBE_HALF }, .color = { 1.0f, 1.0f, 0.0f } },
};

/* ── Index data ───────────────────────────────────────────────────────────── */
/* Two triangles per face, 6 faces = 36 indices.
 * Each face's four vertices are indexed as: 0,1,2, 2,3,0
 * (CCW winding as viewed from outside). */

static const Uint16 cube_indices[INDEX_COUNT] = {
    /* Front face  */  0,  1,  2,   2,  3,  0,
    /* Back face   */  4,  5,  6,   6,  7,  4,
    /* Right face  */  8,  9, 10,  10, 11,  8,
    /* Left face   */ 12, 13, 14,  14, 15, 12,
    /* Top face    */ 16, 17, 18,  18, 19, 16,
    /* Bottom face */ 20, 21, 22,  22, 23, 20,
};

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer           *vertex_buffer;
    SDL_GPUBuffer           *index_buffer;
    SDL_GPUTexture          *depth_texture;   /* ← NEW: depth buffer */
    Uint32                   depth_width;     /* ← NEW: track size for resize */
    Uint32                   depth_height;
    Uint64                   start_ticks;
#ifdef FORGE_CAPTURE
    ForgeCapture             capture;
#endif
} app_state;

/* ── Depth texture helper ─────────────────────────────────────────────────── */
/* Creates (or recreates) a depth texture matching the window size.
 *
 * A depth texture stores the distance of each rendered pixel from the camera.
 * The GPU uses it to determine which fragments are in front of which:
 *   - Each new fragment's depth is compared against the stored depth
 *   - If the new fragment is closer (less depth), it passes and overwrites
 *   - If it's farther, it's discarded — the closer surface "wins"
 *
 * We recreate this texture whenever the window is resized because it must
 * match the color target's dimensions exactly. */

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device,
                                             Uint32 w, Uint32 h)
{
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = DEPTH_FORMAT;
    info.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width                = w;
    info.height               = h;
    info.layer_count_or_depth = 1;
    info.num_levels           = 1;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &info);
    if (!texture) {
        SDL_Log("Failed to create depth texture (%ux%u): %s",
                w, h, SDL_GetError());
    }
    return texture;
}

/* ── Shader helper ────────────────────────────────────────────────────────── */
/* Same pattern as previous lessons — creates a GPU shader from pre-compiled
 * bytecodes, selecting SPIRV or DXIL based on the backend. */

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
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
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

    /* ── 4. Request an sRGB swapchain ──────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 5. Create depth texture ───────────────────────────────────────── */
    int win_w = 0, win_h = 0;
    if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *depth_texture = create_depth_texture(
        device, (Uint32)win_w, (Uint32)win_h);
    if (!depth_texture) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 6. Create shaders ─────────────────────────────────────────────── */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        cube_vert_spirv, cube_vert_spirv_size,
        cube_vert_dxil,  cube_vert_dxil_size,
        VERT_NUM_SAMPLERS,
        VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS,
        VERT_NUM_UNIFORM_BUFFERS);
    if (!vertex_shader) {
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader *fragment_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        cube_frag_spirv, cube_frag_spirv_size,
        cube_frag_dxil,  cube_frag_dxil_size,
        FRAG_NUM_SAMPLERS,
        FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS,
        FRAG_NUM_UNIFORM_BUFFERS);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 7. Create graphics pipeline ───────────────────────────────────── */

    /* Vertex input: position (float3) + color (float3) */
    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(Vertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    /* Attribute 0: position (float3) — first 3D position in the series! */
    vertex_attributes[0].location    = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset      = offsetof(Vertex, position);

    /* Attribute 1: color (float3) — per-vertex face color */
    vertex_attributes[1].location    = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[1].offset      = offsetof(Vertex, color);

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);

    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers          = 1;
    pipeline_info.vertex_input_state.vertex_attributes           = vertex_attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes       = NUM_VERTEX_ATTRIBUTES;

    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* ── NEW: Back-face culling ──────────────────────────────────────────
     * Previous lessons used CULLMODE_NONE because 2D geometry is always
     * face-on.  In 3D, back faces (facing away from the camera) should
     * be skipped — they're inside the cube and invisible.
     * This halves the fragment shader work for closed meshes. */
    pipeline_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* ── NEW: Depth testing ──────────────────────────────────────────────
     * Without depth testing, triangles draw in submission order — later
     * triangles always cover earlier ones, regardless of distance.
     * That makes the cube look "inside out" from some angles.
     *
     * With depth testing:
     *   - enable_depth_test:  compare each fragment's depth before drawing
     *   - enable_depth_write: update the depth buffer when a fragment passes
     *   - compare_op LESS_OR_EQUAL: closer fragments win (lower Z in [0,1]) */
    pipeline_info.depth_stencil_state.enable_depth_test  = true;
    pipeline_info.depth_stencil_state.enable_depth_write = true;
    pipeline_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    /* Color target — query the swapchain format (includes sRGB if set above) */
    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = SDL_GetGPUSwapchainTextureFormat(device, window);

    pipeline_info.target_info.color_target_descriptions = &color_target_desc;
    pipeline_info.target_info.num_color_targets         = 1;

    /* ── NEW: Depth target in pipeline ───────────────────────────────────
     * Previous lessons passed has_depth_stencil_target = false (default).
     * Now we must tell the pipeline about the depth target format. */
    pipeline_info.target_info.has_depth_stencil_target     = true;
    pipeline_info.target_info.depth_stencil_format         = DEPTH_FORMAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(
        device, &pipeline_info);
    if (!pipeline) {
        SDL_Log("Failed to create graphics pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, fragment_shader);
        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    /* ── 8. Create & upload vertex + index buffers ─────────────────────── */
    SDL_GPUBufferCreateInfo vbuf_info;
    SDL_zero(vbuf_info);
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbuf_info.size  = sizeof(cube_vertices);

    SDL_GPUBuffer *vertex_buffer = SDL_CreateGPUBuffer(device, &vbuf_info);
    if (!vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUBufferCreateInfo ibuf_info;
    SDL_zero(ibuf_info);
    ibuf_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ibuf_info.size  = sizeof(cube_indices);

    SDL_GPUBuffer *index_buffer = SDL_CreateGPUBuffer(device, &ibuf_info);
    if (!index_buffer) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Upload vertex + index data in one transfer buffer */
    Uint32 vertex_data_size = sizeof(cube_vertices);
    Uint32 index_data_size  = sizeof(cube_indices);
    Uint32 total_upload     = vertex_data_size + index_data_size;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_upload;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_memcpy(mapped, cube_vertices, vertex_data_size);
    SDL_memcpy((Uint8 *)mapped + vertex_data_size, cube_indices, index_data_size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!upload_cmd) {
        SDL_Log("Failed to acquire command buffer for upload: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(upload_cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Upload vertex data */
    SDL_GPUTransferBufferLocation vtx_src;
    SDL_zero(vtx_src);
    vtx_src.transfer_buffer = transfer;
    vtx_src.offset          = 0;

    SDL_GPUBufferRegion vtx_dst;
    SDL_zero(vtx_dst);
    vtx_dst.buffer = vertex_buffer;
    vtx_dst.offset = 0;
    vtx_dst.size   = vertex_data_size;

    SDL_UploadToGPUBuffer(copy_pass, &vtx_src, &vtx_dst, false);

    /* Upload index data */
    SDL_GPUTransferBufferLocation idx_src;
    SDL_zero(idx_src);
    idx_src.transfer_buffer = transfer;
    idx_src.offset          = vertex_data_size;

    SDL_GPUBufferRegion idx_dst;
    SDL_zero(idx_dst);
    idx_dst.buffer = index_buffer;
    idx_dst.offset = 0;
    idx_dst.size   = index_data_size;

    SDL_UploadToGPUBuffer(copy_pass, &idx_src, &idx_dst, false);

    SDL_EndGPUCopyPass(copy_pass);
    if (!SDL_SubmitGPUCommandBuffer(upload_cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    /* ── 9. Store state ──────────────────────────────────────────────────── */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window        = window;
    state->device        = device;
    state->pipeline      = pipeline;
    state->vertex_buffer = vertex_buffer;
    state->index_buffer  = index_buffer;
    state->depth_texture = depth_texture;
    state->depth_width   = (Uint32)win_w;
    state->depth_height  = (Uint32)win_h;
    state->start_ticks   = SDL_GetTicks();

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUBuffer(device, index_buffer);
            SDL_ReleaseGPUBuffer(device, vertex_buffer);
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            SDL_ReleaseGPUTexture(device, depth_texture);
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
 *   2. Build MVP matrix (model rotation + view + projection)
 *   3. Handle window resize (recreate depth texture if needed)
 *   4. Push MVP uniform
 *   5. Render with depth target                              ← NEW
 */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute elapsed time ───────────────────────────────────────── */
    Uint64 now_ms = SDL_GetTicks();
    float elapsed = (float)(now_ms - state->start_ticks) / MS_TO_SEC;

    /* ── 2. Build the MVP matrix ───────────────────────────────────────── */

    /* Model: rotate the cube around Y and X axes.
     * Two different speeds make the motion look natural. */
    mat4 rotate_y = mat4_rotate_y(elapsed * ROTATE_Y_SPEED);
    mat4 rotate_x = mat4_rotate_x(elapsed * ROTATE_X_SPEED);
    mat4 model    = mat4_multiply(rotate_y, rotate_x);

    /* View: camera at (0, 1.5, 3) looking at the origin. */
    vec3 eye    = vec3_create(EYE_X, EYE_Y, EYE_Z);
    vec3 target = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 up     = vec3_create(0.0f, 1.0f, 0.0f);
    mat4 view   = mat4_look_at(eye, target, up);

    /* Projection: perspective with aspect ratio from current window size. */
    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    float fov    = FOV_DEG * FORGE_DEG2RAD;
    mat4 proj    = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);

    /* Compose: MVP = projection * view * model */
    mat4 vp  = mat4_multiply(proj, view);
    mat4 mvp = mat4_multiply(vp, model);

    Uniforms uniforms;
    uniforms.mvp = mvp;

    /* ── 3. Handle window resize ───────────────────────────────────────── */
    /* The depth texture must match the swapchain dimensions.  If the window
     * has been resized since last frame, release the old one and create a
     * new one at the current size. */
    Uint32 cur_w = (Uint32)w;
    Uint32 cur_h = (Uint32)h;

    if (cur_w != state->depth_width || cur_h != state->depth_height) {
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        state->depth_texture = create_depth_texture(state->device, cur_w, cur_h);
        if (!state->depth_texture) {
            return SDL_APP_FAILURE;
        }
        state->depth_width  = cur_w;
        state->depth_height = cur_h;
    }

    /* ── 4. Acquire command buffer & push uniform ──────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* ── 5. Acquire swapchain & render ─────────────────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain, NULL, NULL)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        /* Color target — same as previous lessons */
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G,
                                                  CLEAR_B, CLEAR_A };

        /* ── NEW: Depth target ──────────────────────────────────────
         * In previous lessons, the depth_stencil_target_info parameter
         * to SDL_BeginGPURenderPass was NULL (no depth testing).
         * Now we pass a depth target that:
         *   - Clears to 1.0 (far plane) at the start of each frame
         *   - Uses DONT_CARE for store_op (we never read it back)
         *   - Uses DONT_CARE for stencil (we only use depth) */
        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture          = state->depth_texture;
        depth_target.load_op          = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op         = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
        depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.clear_depth      = DEPTH_CLEAR;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, &depth_target);
        if (!pass) {
            SDL_Log("Failed to begin render pass: %s", SDL_GetError());
            SDL_CancelGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

        /* Bind vertex buffer */
        SDL_GPUBufferBinding vertex_binding;
        SDL_zero(vertex_binding);
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

        /* Bind index buffer */
        SDL_GPUBufferBinding index_binding;
        SDL_zero(index_binding);
        index_binding.buffer = state->index_buffer;
        index_binding.offset = 0;
        SDL_BindGPUIndexBuffer(pass, &index_binding,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);

        /* Draw the cube — 36 indices = 12 triangles = 6 faces */
        SDL_DrawGPUIndexedPrimitives(pass, INDEX_COUNT, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

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
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        SDL_ReleaseGPUBuffer(state->device, state->index_buffer);
        SDL_ReleaseGPUBuffer(state->device, state->vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
