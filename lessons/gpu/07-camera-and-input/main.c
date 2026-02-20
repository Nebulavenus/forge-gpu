/*
 * Lesson 07 — Camera & Input
 *
 * A first-person camera you can fly through a small scene of colored cubes.
 * This lesson brings together everything from Lesson 06 (depth, MVP) with
 * interactive input handling and quaternion-based camera orientation.
 *
 * Concepts introduced:
 *   - First-person camera     — quaternion orientation + position
 *   - Keyboard input          — SDL keyboard state polling for smooth movement
 *   - Mouse look              — relative mouse mode for FPS-style camera
 *   - Delta time              — frame-rate-independent movement
 *   - Multiple objects        — drawing several cubes with different model transforms
 *   - Pitch clamping          — prevent camera from flipping upside down
 *
 * Math library functions used (see common/math/forge_math.h):
 *   quat_from_euler   — build orientation from yaw + pitch
 *   quat_forward      — extract camera's look direction from quaternion
 *   quat_right        — extract camera's right direction from quaternion
 *   mat4_view_from_quat — build view matrix from position + quaternion
 *   mat4_perspective  — perspective projection
 *   mat4_translate    — position each cube in the scene
 *   mat4_rotate_y     — spin cubes for visual interest
 *   mat4_scale        — vary cube sizes
 *   mat4_multiply     — compose MVP matrices
 *
 * Theory behind this code:
 *   Math Lesson 08 — Orientation (quaternions, euler angles)
 *   Math Lesson 09 — View Matrix & Virtual Camera
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain  (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline          (Lesson 02)
 *   - Push uniforms for MVP matrix                        (Lesson 03)
 *   - Index buffers                                       (Lesson 04)
 *   - Depth buffer, back-face culling, window resize      (Lesson 06)
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
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include "math/forge_math.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
#include "shaders/scene_vert_spirv.h"
#include "shaders/scene_frag_spirv.h"
#include "shaders/scene_vert_dxil.h"
#include "shaders/scene_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 07 Camera & Input"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Dark clear color so the colored cubes stand out. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.04f
#define CLEAR_A 1.0f

/* Depth buffer — same setup as Lesson 06. */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D16_UNORM

/* Cube geometry (same 24-vertex cube from Lesson 06). */
#define VERTEX_COUNT 24
#define INDEX_COUNT  36  /* 6 faces x 2 triangles x 3 indices */
#define CUBE_HALF    0.5f

/* Vertex attributes and shader resources. */
#define NUM_VERTEX_ATTRIBUTES 2

#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1   /* MVP matrix */

#define FRAG_NUM_SAMPLERS         0
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* ── Camera parameters ───────────────────────────────────────────────────── */

/* Starting position: slightly above ground level, back from the scene center
 * so the user can see several cubes in front of them. */
#define CAM_START_X   0.0f
#define CAM_START_Y   1.6f   /* approximate eye height */
#define CAM_START_Z   6.0f   /* back from origin */

/* Movement speed in units per second.  Multiplied by delta time each frame
 * so the camera moves at the same speed regardless of frame rate.
 * See Math Lesson 09, Section 7 for the delta-time pattern. */
#define MOVE_SPEED    3.0f

/* Mouse sensitivity: radians of rotation per pixel of mouse movement.
 * Lower values = slower, more precise aiming.  Higher = twitchier. */
#define MOUSE_SENSITIVITY 0.002f

/* Pitch is clamped to slightly less than 90 degrees to prevent the camera
 * from flipping upside down (gimbal lock at the poles).  This is the same
 * reason flight simulators limit pitch — see Math Lesson 08 on gimbal lock. */
#define MAX_PITCH_DEG  89.0f

/* Perspective projection — same parameters as Lesson 06. */
#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  100.0f

/* Milliseconds to seconds. */
#define MS_TO_SEC 1000.0f

/* Maximum delta time (seconds) to prevent huge jumps when the app stalls
 * or is paused (e.g., alt-tabbing away).  100 ms = ~10 FPS floor. */
#define MAX_DELTA_TIME 0.1f

/* ── Scene layout ────────────────────────────────────────────────────────── */
/* Several cubes placed around the origin to make navigation interesting.
 * Each has a position, a Y-rotation speed, a scale, and an RGB color. */

typedef struct CubeInstance {
    vec3  position;       /* world-space position */
    float rotation_speed; /* radians per second around Y */
    float scale;          /* uniform scale factor */
    vec3  color;          /* RGB face color */
} CubeInstance;

/* clang-format off */
static const CubeInstance scene_cubes[] = {
    /* Center — large, slowly spinning red cube */
    { { 0.0f, 0.5f, 0.0f},  0.5f, 1.0f, {0.9f, 0.2f, 0.2f} },

    /* Left cluster — green and teal */
    { {-3.0f, 0.3f, -1.0f}, -0.8f, 0.6f, {0.2f, 0.8f, 0.3f} },
    { {-2.0f, 0.7f,  1.5f},  1.2f, 0.4f, {0.2f, 0.7f, 0.7f} },

    /* Right cluster — blue and purple */
    { { 3.0f, 0.4f,  0.0f},  0.7f, 0.8f, {0.3f, 0.3f, 0.9f} },
    { { 2.5f, 1.0f, -2.0f}, -1.0f, 0.5f, {0.7f, 0.2f, 0.8f} },

    /* Far — yellow and orange */
    { { 0.0f, 0.3f, -4.0f},  0.9f, 0.7f, {0.9f, 0.8f, 0.1f} },
    { { 1.5f, 0.2f, -6.0f}, -0.6f, 0.4f, {0.9f, 0.5f, 0.1f} },

    /* Ground plane substitute — a large flat cube acting as a floor */
    { { 0.0f, -0.5f, -1.0f}, 0.0f, 20.0f, {0.15f, 0.15f, 0.18f} },
};
/* clang-format on */

#define NUM_CUBES (sizeof(scene_cubes) / sizeof(scene_cubes[0]))

/* ── Vertex format ───────────────────────────────────────────────────────── */
/* Same as Lesson 06 — position (float3) + color (float3). */

typedef struct Vertex {
    vec3 position;
    vec3 color;
} Vertex;

typedef struct Uniforms {
    mat4 mvp;
} Uniforms;

/* ── Cube geometry ───────────────────────────────────────────────────────── */
/* Same colored cube as Lesson 06 (6 faces, each a different color).
 * All cube instances share these vertices — variety comes from their
 * different positions, sizes, and rotation speeds defined in scene_cubes[].
 * Per-instance coloring would require either a fragment uniform or instanced
 * rendering (Lesson 14); we keep the shader identical to Lesson 06 so the
 * lesson stays focused on camera and input. */

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

static const Uint16 cube_indices[INDEX_COUNT] = {
    /* Front  */  0,  1,  2,   2,  3,  0,
    /* Back   */  4,  5,  6,   6,  7,  4,
    /* Right  */  8,  9, 10,  10, 11,  8,
    /* Left   */ 12, 13, 14,  14, 15, 12,
    /* Top    */ 16, 17, 18,  18, 19, 16,
    /* Bottom */ 20, 21, 22,  22, 23, 20,
};

/* ── Application state ───────────────────────────────────────────────────── */

typedef struct app_state {
    /* GPU resources */
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer           *vertex_buffer;
    SDL_GPUBuffer           *index_buffer;
    SDL_GPUTexture          *depth_texture;
    Uint32                   depth_width;
    Uint32                   depth_height;

    /* ── NEW: Camera state ─────────────────────────────────────────────
     * The camera is defined by a position and orientation.
     * Orientation is stored as yaw + pitch (euler angles) and converted
     * to a quaternion each frame.  This is the pattern recommended in
     * Math Lesson 08 (Orientation) and Math Lesson 09 (View Matrix):
     *
     *   User input -> euler angles -> quaternion -> view matrix
     *
     * We store euler angles (not the quaternion directly) because:
     *   1. Mouse deltas naturally map to yaw/pitch increments
     *   2. We need to clamp pitch to avoid flipping (gimbal lock)
     *   3. For an FPS camera, yaw + pitch is sufficient (no roll) */
    vec3  cam_position;   /* world-space camera position */
    float cam_yaw;        /* rotation around Y axis (radians, + = left) */
    float cam_pitch;      /* rotation around X axis (radians, + = up) */

    /* ── NEW: Timing ───────────────────────────────────────────────────
     * Delta time decouples movement speed from frame rate.
     * At 60 FPS, dt ~ 0.0167s.  At 30 FPS, dt ~ 0.033s.
     * Movement = speed * dt, so it's the same distance per second
     * regardless of how fast frames are rendering. */
    Uint64 last_ticks;    /* timestamp of previous frame (ms) */
    float  elapsed;       /* total elapsed time for cube rotation */

    /* ── NEW: Input state ──────────────────────────────────────────────
     * Track whether the mouse is captured for look-around.
     * When captured: mouse is hidden, movements rotate the camera.
     * When released: mouse is visible, movements don't affect camera. */
    bool mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;

/* ── Depth texture helper ────────────────────────────────────────────────── */
/* Same as Lesson 06 — creates a depth texture matching the window size. */

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

/* ── Shader helper ───────────────────────────────────────────────────────── */
/* Same as Lesson 06 — creates a shader from SPIRV or DXIL bytecodes. */

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

/* ── SDL_AppInit ─────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* ── 1. Initialise SDL ────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ─────────────────────────────────────────── */
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

    /* ── 3. Create window & claim swapchain ───────────────────────────── */
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

    /* ── 4. Request an sRGB swapchain ─────────────────────────────────── */
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

    /* ── 5. Create depth texture ──────────────────────────────────────── */
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

    /* ── 6. Create shaders ────────────────────────────────────────────── */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, scene_vert_spirv_size,
        scene_vert_dxil,  scene_vert_dxil_size,
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
        scene_frag_spirv, scene_frag_spirv_size,
        scene_frag_dxil,  scene_frag_dxil_size,
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

    /* ── 7. Create graphics pipeline ──────────────────────────────────── */
    /* Same pipeline setup as Lesson 06: depth testing + back-face culling. */

    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(Vertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    vertex_attributes[0].location    = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset      = offsetof(Vertex, position);

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

    /* Back-face culling — same as Lesson 06. */
    pipeline_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth testing — same as Lesson 06. */
    pipeline_info.depth_stencil_state.enable_depth_test  = true;
    pipeline_info.depth_stencil_state.enable_depth_write = true;
    pipeline_info.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = SDL_GetGPUSwapchainTextureFormat(device, window);

    pipeline_info.target_info.color_target_descriptions = &color_target_desc;
    pipeline_info.target_info.num_color_targets         = 1;
    pipeline_info.target_info.has_depth_stencil_target  = true;
    pipeline_info.target_info.depth_stencil_format      = DEPTH_FORMAT;

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

    /* Shaders can be released after pipeline creation. */
    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    /* ── 8. Create & upload vertex + index buffers ────────────────────── */
    /* Same upload pattern as Lesson 06 — one transfer buffer for both. */

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
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    /* ── 9. Store state ───────────────────────────────────────────────── */
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

    /* ── NEW: Initialize camera ───────────────────────────────────────
     * Position the camera behind and above the scene so the user can
     * see multiple cubes on startup.  Yaw and pitch start at zero
     * (looking down -Z, which is "into the screen"). */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = 0.0f;
    state->cam_pitch    = 0.0f;
    state->last_ticks   = SDL_GetTicks();
    state->elapsed      = 0.0f;

    /* ── NEW: Capture the mouse for FPS-style look ────────────────────
     * SDL_SetWindowRelativeMouseMode hides the cursor and reports
     * relative motion (delta X/Y) instead of absolute position.
     * This is how every first-person game handles mouse look. */
#ifndef FORGE_CAPTURE
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        /* Not fatal — the camera just won't respond to mouse movement. */
    } else {
        state->mouse_captured = true;
    }
#else
    /* When capturing frames, don't grab the mouse — the scene auto-animates
     * via the spinning cubes, and we want a fixed camera for screenshots. */
    state->mouse_captured = false;
#endif

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

    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────────── */
/* Handle quit, mouse capture toggle, and mouse look.
 *
 * NEW concepts:
 *   - SDL_EVENT_MOUSE_MOTION with relative mode gives delta X/Y each frame
 *   - We accumulate yaw and pitch from these deltas
 *   - Escape releases the mouse; clicking recaptures it */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    /* ── Escape key: release mouse or quit ────────────────────────────── */
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        if (state->mouse_captured) {
            /* First press: release the mouse so the user can interact
             * with the window title bar, taskbar, etc. */
            if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            } else {
                state->mouse_captured = false;
            }
        } else {
            /* Second press (mouse already released): quit the app. */
            return SDL_APP_SUCCESS;
        }
    }

    /* ── Click to recapture mouse ─────────────────────────────────────── */
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
        if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
            SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                    SDL_GetError());
        } else {
            state->mouse_captured = true;
        }
    }

    /* ── Mouse motion: update camera yaw and pitch ────────────────────── */
    /* SDL_EVENT_MOUSE_MOTION with relative mode gives xrel/yrel: how many
     * pixels the mouse moved since the last event.  We convert these to
     * rotation angles using the sensitivity constant.
     *
     * Yaw:   horizontal mouse movement rotates around the world Y axis.
     *        Moving mouse right = negative yaw (turn right, clockwise
     *        when viewed from above).
     *
     * Pitch: vertical mouse movement tilts up/down around the camera's
     *        local X axis.  Moving mouse up = positive pitch (look up).
     *        Clamped to avoid flipping — see Math Lesson 08 (gimbal lock). */
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->mouse_captured) {
        state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
        state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;

        /* Clamp pitch to prevent flipping.  At exactly +/-90 degrees,
         * forward would be straight up/down and the cross product used
         * to derive the right vector becomes degenerate (gimbal lock). */
        float max_pitch = MAX_PITCH_DEG * FORGE_DEG2RAD;
        if (state->cam_pitch >  max_pitch) state->cam_pitch =  max_pitch;
        if (state->cam_pitch < -max_pitch) state->cam_pitch = -max_pitch;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────────── */
/* Each frame:
 *   1. Compute delta time
 *   2. Process keyboard input for camera movement      ← NEW
 *   3. Build view matrix from camera state              ← NEW
 *   4. Handle window resize (recreate depth texture)
 *   5. For each cube: compute model * VP, push uniform, draw */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute delta time ────────────────────────────────────────── */
    /* Delta time is the elapsed time since the last frame, in seconds.
     * By multiplying movement by dt, we get consistent speed regardless
     * of frame rate.  See Math Lesson 09, Section 7 for this pattern. */
    Uint64 now_ms = SDL_GetTicks();
    float dt = (float)(now_ms - state->last_ticks) / MS_TO_SEC;
    state->last_ticks = now_ms;

    /* Clamp dt to prevent huge jumps if the app stalls or is paused.
     * Without this, alt-tabbing away for 5 seconds would teleport
     * the camera forward by 5 * MOVE_SPEED units on the next frame. */
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* Accumulate elapsed time for cube rotation animation. */
    state->elapsed += dt;

    /* ── 2. Process keyboard input ────────────────────────────────────── */
    /* SDL_GetKeyboardState returns a snapshot of which keys are currently
     * held down.  Unlike SDL_EVENT_KEY_DOWN (which fires once per press),
     * this lets us check continuously — essential for smooth movement.
     *
     * We extract the camera's forward and right directions from its
     * quaternion orientation, then move along those directions based
     * on which keys are held.  This is the exact pattern from
     * Math Lesson 09, Section 7:
     *
     *   forward = quat_forward(orientation)
     *   right   = quat_right(orientation)
     *   position += forward * speed * dt   (W/S)
     *   position += right * speed * dt     (A/D)
     */
    quat cam_orientation = quat_from_euler(
        state->cam_yaw, state->cam_pitch, 0.0f);

    vec3 forward = quat_forward(cam_orientation);
    vec3 right   = quat_right(cam_orientation);

    const bool *keys = SDL_GetKeyboardState(NULL);

    /* Move along camera's forward direction (W/S or Up/Down arrows).
     * forward points where the camera is looking (into -Z initially). */
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(forward, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(forward, -MOVE_SPEED * dt));
    }

    /* Strafe along camera's right direction (A/D or Left/Right arrows). */
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(right, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(right, -MOVE_SPEED * dt));
    }

    /* Fly up/down along world Y axis (Space / Left Shift).
     * We use world Y (not camera up) so "up" always means up,
     * even when looking at the ground — like a noclip camera. */
    if (keys[SDL_SCANCODE_SPACE]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_create(0.0f, MOVE_SPEED * dt, 0.0f));
    }
    if (keys[SDL_SCANCODE_LSHIFT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_create(0.0f, -MOVE_SPEED * dt, 0.0f));
    }

    /* ── 3. Build view and projection matrices ────────────────────────── */
    /* The view matrix is rebuilt every frame from the camera's current
     * position and quaternion orientation.  This replaces the static
     * mat4_look_at from Lesson 06 with mat4_view_from_quat — the
     * key function from Math Lesson 09. */
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orientation);

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    float fov    = FOV_DEG * FORGE_DEG2RAD;
    mat4 proj    = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);

    /* View-projection is the same for all objects in the scene —
     * only the model matrix changes per-object. */
    mat4 vp = mat4_multiply(proj, view);

    /* ── 4. Handle window resize ──────────────────────────────────────── */
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

    /* ── 5. Acquire command buffer ────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 6. Acquire swapchain & begin render pass ─────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain, NULL, NULL)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G,
                                                  CLEAR_B, CLEAR_A };

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

        /* Bind vertex and index buffers once — shared by all cubes. */
        SDL_GPUBufferBinding vertex_binding;
        SDL_zero(vertex_binding);
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

        SDL_GPUBufferBinding index_binding;
        SDL_zero(index_binding);
        index_binding.buffer = state->index_buffer;
        index_binding.offset = 0;
        SDL_BindGPUIndexBuffer(pass, &index_binding,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);

        /* ── 7. Draw each cube with its own model matrix ──────────────── */
        /* For each cube instance:
         *   1. Build model = translate * rotate * scale
         *   2. Compose MVP = view_proj * model
         *   3. Push MVP as vertex uniform
         *   4. Draw the same 36 indices
         *
         * This is a simple multi-object rendering pattern.  Each draw call
         * has a different MVP pushed as a uniform.  For many objects,
         * instanced rendering (Lesson 14) is more efficient, but this
         * approach is easier to understand and fine for a small scene. */
        for (Uint32 i = 0; i < NUM_CUBES; i++) {
            const CubeInstance *cube = &scene_cubes[i];

            /* Model transform: position + rotation + scale */
            mat4 translate = mat4_translate(cube->position);
            mat4 rotate    = mat4_rotate_y(
                state->elapsed * cube->rotation_speed);
            mat4 scale     = mat4_scale(vec3_create(
                cube->scale, cube->scale, cube->scale));

            /* Compose: model = translate * rotate * scale
             * (scale first, then rotate, then position in world) */
            mat4 model = mat4_multiply(translate,
                         mat4_multiply(rotate, scale));

            /* MVP = projection * view * model */
            mat4 mvp = mat4_multiply(vp, model);

            Uniforms uniforms;
            uniforms.mvp = mvp;

            SDL_PushGPUVertexUniformData(cmd, 0,
                                          &uniforms, sizeof(uniforms));
            SDL_DrawGPUIndexedPrimitives(pass, INDEX_COUNT, 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
                SDL_CancelGPUCommandBuffer(cmd);
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
            SDL_CancelGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────────────── */
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
