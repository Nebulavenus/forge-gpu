/*
 * Lesson 19 — Debug Lines
 *
 * This lesson introduces an immediate-mode debug drawing system — a
 * reusable pattern for rendering colored lines, circles, wireframe boxes,
 * and coordinate-axis gizmos.  Every game engine and renderer has some
 * form of this; it is the primary diagnostic tool for visualizing
 * positions, bounds, normals, directions, and other spatial data.
 *
 * The entire scene is built from debug lines.  No triangles, no textures,
 * no models — just lines drawn with SDL_GPU_PRIMITIVETYPE_LINELIST.
 * This keeps the lesson focused on the one new concept: dynamic per-frame
 * line rendering.
 *
 * The system uses an immediate-mode pattern:
 *   1. Each frame, reset the vertex counts to zero
 *   2. Call debug_* helper functions to accumulate line vertices into
 *      a CPU-side array
 *   3. Upload the entire array to the GPU via a transfer buffer
 *   4. Draw the lines in two passes: world-space (depth-tested) and
 *      overlay (always visible, drawn on top)
 *
 * Two pipelines share one GPU vertex buffer.  World-space lines render
 * first with depth testing enabled, then overlay lines render on top
 * with depth testing disabled.  The overlay region starts at vertex
 * index world_count, so a single draw call with first_vertex offset
 * selects the right batch.
 *
 * What's new:
 *   - Immediate-mode debug drawing API (add vertices, draw, reset)
 *   - Dynamic vertex buffer updated every frame via transfer buffer
 *   - LINELIST primitive type (two vertices per line segment)
 *   - Two pipelines from the same shaders (depth on vs depth off)
 *   - Debug shape helpers: line, grid, axes, circle, wireframe box
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline             (Lesson 02)
 *   - Push uniforms for the view-projection matrix           (Lesson 03)
 *   - Depth buffer, window resize                            (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time        (Lesson 07)
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
#include "math/forge_math.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

/* ── Frame capture (compile-time option) ─────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────── */

#include "shaders/debug_vert_spirv.h"
#include "shaders/debug_frag_spirv.h"
#include "shaders/debug_vert_dxil.h"
#include "shaders/debug_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 19 Debug Lines"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Maximum number of debug vertices per frame.  Each line segment uses
 * two vertices, so this allows up to 32768 line segments per frame.
 * The buffer is pre-allocated once and reused every frame. */
#define MAX_DEBUG_VERTICES 65536

/* Byte stride per vertex: vec3 position (12) + vec4 color (16) = 28. */
#define DEBUG_VERTEX_PITCH 28

/* Dark background so colored lines stand out clearly. */
#define CLEAR_R 0.05f
#define CLEAR_G 0.05f
#define CLEAR_B 0.07f
#define CLEAR_A 1.0f

/* Depth buffer */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D24_UNORM

/* Number of vertex attributes: position (float3) + color (float4). */
#define NUM_VERTEX_ATTRIBUTES 2

/* ── Shader resource counts ──────────────────────────────────────────── */

/* Both shaders: 0 samplers, 0 storage, 1 uniform (VP matrix) */
#define VS_NUM_SAMPLERS         0
#define VS_NUM_STORAGE_TEXTURES 0
#define VS_NUM_STORAGE_BUFFERS  0
#define VS_NUM_UNIFORM_BUFFERS  1

#define FS_NUM_SAMPLERS         0
#define FS_NUM_STORAGE_TEXTURES 0
#define FS_NUM_STORAGE_BUFFERS  0
#define FS_NUM_UNIFORM_BUFFERS  0

/* ── Camera parameters ───────────────────────────────────────────────── */

/* Start elevated and pulled back to see the full debug scene. */
#define CAM_START_X     0.0f
#define CAM_START_Y     4.0f
#define CAM_START_Z    12.0f
#define CAM_START_YAW   0.0f
#define CAM_START_PITCH 0.0f

#define MOVE_SPEED        5.0f
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG     89.0f

#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  200.0f

#define MAX_DELTA_TIME 0.1f

/* ── Debug scene parameters ──────────────────────────────────────────── */

/* Ground grid: 40 units in each direction from the origin, 1-unit spacing. */
#define GRID_HALF_SIZE  20
#define GRID_SPACING    1.0f
#define GRID_COLOR      0.3f   /* neutral gray so scene shapes stand out */

/* Number of segments for debug circles (more = smoother). */
#define CIRCLE_SEGMENTS 32

/* Animation speed for the rotating circle (radians per second). */
#define ANIM_SPEED      1.0f

/* Threshold for detecting near-vertical normals when building an
 * orthonormal basis for circles.  If |n.y| exceeds this, the
 * reference vector switches from Y to X to avoid degenerate cross
 * products.  0.9 corresponds to roughly ±26 degrees from vertical. */
#define NEAR_VERTICAL_THRESHOLD 0.9f

/* Origin axis gizmo size (overlay) and smaller gizmo size (world). */
#define AXES_SIZE_LARGE 2.0f
#define AXES_SIZE_SMALL 1.0f

/* ═══════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════ */

/* A single debug vertex: world-space position + RGBA color.
 * Lines are defined as pairs of DebugVertex (LINELIST primitive). */
typedef struct DebugVertex {
    vec3 position;   /* 12 bytes */
    vec4 color;      /* 16 bytes */
} DebugVertex;       /* 28 bytes total = DEBUG_VERTEX_PITCH */

/* Vertex uniform data: just the combined view-projection matrix. */
typedef struct DebugUniforms {
    mat4 view_projection;   /* 64 bytes */
} DebugUniforms;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Two pipelines from the same shaders — only depth state differs.
     * line_pipeline:    depth test ON, depth write ON (world-space lines)
     * overlay_pipeline: depth test OFF, depth write OFF (always on top) */
    SDL_GPUGraphicsPipeline *line_pipeline;
    SDL_GPUGraphicsPipeline *overlay_pipeline;

    /* Pre-allocated GPU vertex buffer (MAX_DEBUG_VERTICES capacity).
     * Updated every frame with the accumulated debug vertices. */
    SDL_GPUBuffer *vertex_buffer;

    /* Transfer buffer for uploading CPU vertices to GPU each frame. */
    SDL_GPUTransferBuffer *transfer_buffer;

    /* CPU-side vertex array.  Debug helper functions append vertices here
     * during the frame, then the entire array is uploaded before drawing. */
    DebugVertex *vertices;

    /* World-space lines occupy indices [0 .. world_count-1].
     * Overlay lines occupy indices [world_count .. world_count+overlay_count-1].
     * Total vertices = world_count + overlay_count. */
    Uint32 world_count;
    Uint32 overlay_count;

    /* Depth buffer (recreated on window resize). */
    SDL_GPUTexture *depth_texture;
    Uint32          depth_width;
    Uint32          depth_height;

    /* Camera state */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Timing */
    Uint64 last_ticks;
    float  time;   /* accumulated time for animation */
    bool   mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;

/* ═══════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Create a shader from embedded bytecode ──────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code, size_t dxil_size,
    int num_samplers, int num_storage_textures,
    int num_storage_buffers, int num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage = stage;
    info.num_samplers = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers = num_storage_buffers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv_code;
        info.code_size = spirv_size;
        info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil_code;
        info.code_size = dxil_size;
        info.entrypoint = "main";
    } else {
        SDL_Log("No supported shader format (need SPIRV or DXIL)");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("SDL_CreateGPUShader (%s) failed: %s",
                stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
                SDL_GetError());
    }
    return shader;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Debug Drawing Functions
 *
 * These functions implement the immediate-mode pattern: each call
 * appends vertices to the CPU array.  Call them between resetting the
 * counts (at frame start) and uploading to the GPU (before drawing).
 *
 * The `overlay` parameter controls which region receives the vertices:
 *   - overlay=false → world-space (depth-tested, can be occluded)
 *   - overlay=true  → overlay (always visible, drawn on top)
 *
 * World vertices are packed at the front of the array, overlay vertices
 * at the back (after world_count).  The layout in the buffer:
 *
 *   [ world vertex 0 ] [ world vertex 1 ] ... [ overlay vertex 0 ] ...
 *   |<---- world_count ---->|                  |<-- overlay_count -->|
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Add a single vertex to the CPU array ────────────────────────────── */
/* Returns false if the buffer is full (vertices are silently dropped). */

static bool add_vertex(app_state *state, vec3 position, vec4 color,
                        bool overlay)
{
    Uint32 total = state->world_count + state->overlay_count;
    if (total >= MAX_DEBUG_VERTICES) {
        return false;
    }

    /* World vertices go at index [world_count] (front of array).
     * Overlay vertices go at the end, but we need to make room by
     * shifting overlays when adding world vertices.  Instead, we use
     * a simpler layout: world vertices pack from 0 up, overlay
     * vertices pack from the end of capacity down.  At upload time
     * we compact overlay vertices right after world vertices. */
    if (overlay) {
        /* Overlay vertices are stored at the end of the array, growing
         * downward.  Index: MAX_DEBUG_VERTICES - 1 - overlay_count */
        Uint32 idx = MAX_DEBUG_VERTICES - 1 - state->overlay_count;
        state->vertices[idx].position = position;
        state->vertices[idx].color = color;
        state->overlay_count++;
    } else {
        state->vertices[state->world_count].position = position;
        state->vertices[state->world_count].color = color;
        state->world_count++;
    }
    return true;
}

/* ── Draw a single line segment ──────────────────────────────────────── */

static void debug_line(app_state *state, vec3 start, vec3 end,
                        vec4 color, bool overlay)
{
    /* LINELIST requires vertex pairs — ensure two slots are available
     * before adding either vertex, otherwise we'd leave an unpaired
     * vertex that corrupts the draw call. */
    Uint32 total = state->world_count + state->overlay_count;
    if (total + 2 > MAX_DEBUG_VERTICES) {
        return;
    }
    add_vertex(state, start, color, overlay);
    add_vertex(state, end, color, overlay);
}

/* ── Draw a grid on the XZ plane ─────────────────────────────────────── */
/* Creates a grid of lines centered at the origin.  Lines run along
 * the X and Z axes at regular intervals.  Always world-space (depth-
 * tested) since the grid is part of the scene floor. */

static void debug_grid(app_state *state, int half_size, float spacing,
                        vec4 color)
{
    int i;
    float extent = (float)half_size * spacing;

    /* Lines parallel to the Z axis (varying X position). */
    for (i = -half_size; i <= half_size; i++) {
        float x = (float)i * spacing;
        debug_line(state,
                   vec3_create(x, 0.0f, -extent),
                   vec3_create(x, 0.0f,  extent),
                   color, false);
    }

    /* Lines parallel to the X axis (varying Z position). */
    for (i = -half_size; i <= half_size; i++) {
        float z = (float)i * spacing;
        debug_line(state,
                   vec3_create(-extent, 0.0f, z),
                   vec3_create( extent, 0.0f, z),
                   color, false);
    }
}

/* ── Draw a coordinate-axis gizmo ────────────────────────────────────── */
/* Three lines from the origin: red=X, green=Y, blue=Z.
 * Convention: RGB maps to XYZ — a standard in 3D tools. */

static void debug_axes(app_state *state, vec3 origin, float size,
                        bool overlay)
{
    /* X axis — red */
    debug_line(state, origin,
               vec3_add(origin, vec3_create(size, 0.0f, 0.0f)),
               vec4_create(1.0f, 0.0f, 0.0f, 1.0f), overlay);

    /* Y axis — green */
    debug_line(state, origin,
               vec3_add(origin, vec3_create(0.0f, size, 0.0f)),
               vec4_create(0.0f, 1.0f, 0.0f, 1.0f), overlay);

    /* Z axis — blue */
    debug_line(state, origin,
               vec3_add(origin, vec3_create(0.0f, 0.0f, size)),
               vec4_create(0.0f, 0.4f, 1.0f, 1.0f), overlay);
}

/* ── Draw a circle from line segments ────────────────────────────────── */
/* Approximates a circle using `segments` line segments.  The circle
 * lies in the plane perpendicular to `normal`, centered at `center`.
 *
 * To draw the circle, we need two vectors that are perpendicular to
 * the normal and to each other (an orthonormal basis for the plane).
 * We construct these using cross products:
 *   1. Pick a reference vector that isn't parallel to the normal
 *   2. Cross the normal with the reference to get the first tangent
 *   3. Cross the normal with the first tangent to get the second */

static void debug_circle(app_state *state, vec3 center, float radius,
                          vec3 normal, vec4 color, int segments,
                          bool overlay)
{
    /* Normalize the plane normal. */
    vec3 n = vec3_normalize(normal);

    /* Choose a reference vector that isn't parallel to n.
     * If n is mostly vertical (|n.y| > 0.9), use the X axis instead
     * of the Y axis to avoid a degenerate cross product. */
    vec3 ref;
    if (SDL_fabsf(n.y) > NEAR_VERTICAL_THRESHOLD) {
        ref = vec3_create(1.0f, 0.0f, 0.0f);
    } else {
        ref = vec3_create(0.0f, 1.0f, 0.0f);
    }

    /* Build orthonormal basis for the circle plane. */
    vec3 tangent1 = vec3_normalize(vec3_cross(n, ref));
    vec3 tangent2 = vec3_cross(n, tangent1);

    /* Generate line segments around the circle. */
    float angle_step = FORGE_TAU / (float)segments;
    int i;
    for (i = 0; i < segments; i++) {
        float a0 = (float)i * angle_step;
        float a1 = (float)((i + 1) % segments) * angle_step;

        /* Point on the circle: center + radius * (cos(a)*t1 + sin(a)*t2) */
        vec3 p0 = vec3_add(center,
            vec3_add(vec3_scale(tangent1, radius * SDL_cosf(a0)),
                     vec3_scale(tangent2, radius * SDL_sinf(a0))));
        vec3 p1 = vec3_add(center,
            vec3_add(vec3_scale(tangent1, radius * SDL_cosf(a1)),
                     vec3_scale(tangent2, radius * SDL_sinf(a1))));

        debug_line(state, p0, p1, color, overlay);
    }
}

/* ── Draw a wireframe axis-aligned bounding box ──────────────────────── */
/* An AABB has 8 corners and 12 edges.  We enumerate the 8 corners
 * from the min/max points, then draw the 12 edges connecting them:
 *
 *     6 ---- 7         Y
 *    /|     /|         |
 *   4 ---- 5 |        +-- X
 *   | 2 ---| 3       /
 *   |/     |/       Z
 *   0 ---- 1
 *
 * Bottom face: 0-1-3-2   Top face: 4-5-7-6   Verticals: 0-4, 1-5, 2-6, 3-7 */

static void debug_box_wireframe(app_state *state,
                                 vec3 min_pt, vec3 max_pt,
                                 vec4 color, bool overlay)
{
    /* Enumerate the 8 corners. */
    vec3 c[8];
    c[0] = vec3_create(min_pt.x, min_pt.y, max_pt.z);
    c[1] = vec3_create(max_pt.x, min_pt.y, max_pt.z);
    c[2] = vec3_create(min_pt.x, min_pt.y, min_pt.z);
    c[3] = vec3_create(max_pt.x, min_pt.y, min_pt.z);
    c[4] = vec3_create(min_pt.x, max_pt.y, max_pt.z);
    c[5] = vec3_create(max_pt.x, max_pt.y, max_pt.z);
    c[6] = vec3_create(min_pt.x, max_pt.y, min_pt.z);
    c[7] = vec3_create(max_pt.x, max_pt.y, min_pt.z);

    /* Bottom face (4 edges). */
    debug_line(state, c[0], c[1], color, overlay);
    debug_line(state, c[1], c[3], color, overlay);
    debug_line(state, c[3], c[2], color, overlay);
    debug_line(state, c[2], c[0], color, overlay);

    /* Top face (4 edges). */
    debug_line(state, c[4], c[5], color, overlay);
    debug_line(state, c[5], c[7], color, overlay);
    debug_line(state, c[7], c[6], color, overlay);
    debug_line(state, c[6], c[4], color, overlay);

    /* Vertical edges (4 edges). */
    debug_line(state, c[0], c[4], color, overlay);
    debug_line(state, c[1], c[5], color, overlay);
    debug_line(state, c[2], c[6], color, overlay);
    debug_line(state, c[3], c[7], color, overlay);
}

/* ═══════════════════════════════════════════════════════════════════════
 * SDL Application Callbacks
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppInit ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── 1. Initialise SDL ──────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ───────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true,  /* debug mode */
        NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create window ───────────────────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Claim window for GPU rendering ──────────────────────────── */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 5. Request sRGB swapchain ──────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 6. Allocate application state ──────────────────────────────── */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window = window;
    state->device = device;

    /* ── 7. Create shaders ──────────────────────────────────────────── */
    SDL_GPUShader *vert_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        debug_vert_spirv, sizeof(debug_vert_spirv),
        debug_vert_dxil, sizeof(debug_vert_dxil),
        VS_NUM_SAMPLERS, VS_NUM_STORAGE_TEXTURES,
        VS_NUM_STORAGE_BUFFERS, VS_NUM_UNIFORM_BUFFERS);
    if (!vert_shader) goto fail;

    SDL_GPUShader *frag_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        debug_frag_spirv, sizeof(debug_frag_spirv),
        debug_frag_dxil, sizeof(debug_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_STORAGE_TEXTURES,
        FS_NUM_STORAGE_BUFFERS, FS_NUM_UNIFORM_BUFFERS);
    if (!frag_shader) {
        SDL_ReleaseGPUShader(device, vert_shader);
        goto fail;
    }

    /* ── 8. Define vertex layout ────────────────────────────────────── */
    /* DebugVertex: position (float3) at offset 0, color (float4) at offset 12. */
    {
        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot = 0;
        vb_desc.pitch = DEBUG_VERTEX_PITCH;
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_desc.instance_step_rate = 0;

        SDL_GPUVertexAttribute attrs[NUM_VERTEX_ATTRIBUTES];
        SDL_zero(attrs);

        /* Location 0: position (float3) — maps to HLSL TEXCOORD0 */
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = (Uint32)offsetof(DebugVertex, position);

        /* Location 1: color (float4) — maps to HLSL TEXCOORD1 */
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[1].offset = (Uint32)offsetof(DebugVertex, color);

        /* ── 9. Create line pipeline (depth-tested) ───────────────── */
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pipe;
        SDL_zero(pipe);
        pipe.vertex_shader = vert_shader;
        pipe.fragment_shader = frag_shader;
        pipe.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pipe.vertex_input_state.num_vertex_buffers = 1;
        pipe.vertex_input_state.vertex_attributes = attrs;
        pipe.vertex_input_state.num_vertex_attributes = NUM_VERTEX_ATTRIBUTES;
        pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;

        /* No culling for lines — they have no face orientation. */
        pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

        /* Depth testing ON: world-space lines are occluded by closer
         * geometry (in this lesson, by other lines in front of them). */
        pipe.depth_stencil_state.enable_depth_test = true;
        pipe.depth_stencil_state.enable_depth_write = true;
        pipe.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipe.target_info.color_target_descriptions = &ctd;
        pipe.target_info.num_color_targets = 1;
        pipe.target_info.has_depth_stencil_target = true;
        pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

        state->line_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->line_pipeline) {
            SDL_Log("Failed to create line pipeline: %s", SDL_GetError());
            SDL_ReleaseGPUShader(device, frag_shader);
            SDL_ReleaseGPUShader(device, vert_shader);
            goto fail;
        }

        /* ── 10. Create overlay pipeline (no depth test) ──────────── */
        /* Same shaders, same vertex layout — only depth state changes.
         * Depth test OFF means these lines are always visible, even
         * when they're behind other geometry.  This is essential for
         * always-on-top indicators like axis gizmos. */
        pipe.depth_stencil_state.enable_depth_test = false;
        pipe.depth_stencil_state.enable_depth_write = false;

        state->overlay_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe);
        if (!state->overlay_pipeline) {
            SDL_Log("Failed to create overlay pipeline: %s",
                    SDL_GetError());
            SDL_ReleaseGPUShader(device, frag_shader);
            SDL_ReleaseGPUShader(device, vert_shader);
            goto fail;
        }
    }

    /* Shaders are baked into both pipelines — safe to release now. */
    SDL_ReleaseGPUShader(device, frag_shader);
    SDL_ReleaseGPUShader(device, vert_shader);

    /* ── 11. Pre-allocate GPU vertex buffer ─────────────────────────── */
    /* A single buffer holds all debug vertices for the frame.  It is
     * large enough for MAX_DEBUG_VERTICES and reused every frame. */
    {
        Uint32 buffer_size = MAX_DEBUG_VERTICES * DEBUG_VERTEX_PITCH;

        SDL_GPUBufferCreateInfo bci;
        SDL_zero(bci);
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = buffer_size;
        state->vertex_buffer = SDL_CreateGPUBuffer(device, &bci);
        if (!state->vertex_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (vertex) failed: %s",
                    SDL_GetError());
            goto fail;
        }

        /* ── 12. Pre-allocate transfer buffer ───────────────────────── */
        /* The transfer buffer is the staging area for CPU → GPU upload.
         * We map it, memcpy the CPU vertices in, unmap, then issue a
         * copy command to transfer data to the GPU vertex buffer. */
        SDL_GPUTransferBufferCreateInfo tbci;
        SDL_zero(tbci);
        tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbci.size = buffer_size;
        state->transfer_buffer =
            SDL_CreateGPUTransferBuffer(device, &tbci);
        if (!state->transfer_buffer) {
            SDL_Log("SDL_CreateGPUTransferBuffer failed: %s",
                    SDL_GetError());
            goto fail;
        }
    }

    /* ── 13. Allocate CPU vertex array ──────────────────────────────── */
    state->vertices = (DebugVertex *)SDL_calloc(
        MAX_DEBUG_VERTICES, sizeof(DebugVertex));
    if (!state->vertices) {
        SDL_Log("Failed to allocate CPU vertex array");
        goto fail;
    }

    /* ── 14. Create depth texture ───────────────────────────────────── */
    {
        int win_w, win_h;
        if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
            SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
            goto fail;
        }

        SDL_GPUTextureCreateInfo dci;
        SDL_zero(dci);
        dci.type = SDL_GPU_TEXTURETYPE_2D;
        dci.format = DEPTH_FORMAT;
        dci.width = (Uint32)win_w;
        dci.height = (Uint32)win_h;
        dci.layer_count_or_depth = 1;
        dci.num_levels = 1;
        dci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->depth_texture = SDL_CreateGPUTexture(device, &dci);
        if (!state->depth_texture) {
            SDL_Log("SDL_CreateGPUTexture (depth) failed: %s",
                    SDL_GetError());
            goto fail;
        }
        state->depth_width = (Uint32)win_w;
        state->depth_height = (Uint32)win_h;
    }

    /* ── 15. Camera initial state ───────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw = CAM_START_YAW * FORGE_DEG2RAD;
    state->cam_pitch = CAM_START_PITCH * FORGE_DEG2RAD;
    state->last_ticks = SDL_GetPerformanceCounter();
    state->time = 0.0f;
    state->mouse_captured = false;

    /* ── 16. Capture mouse ──────────────────────────────────────────── */
#ifndef FORGE_CAPTURE
    if (SDL_SetWindowRelativeMouseMode(window, true)) {
        state->mouse_captured = true;
    } else {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    }
#endif

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            goto fail;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    *appstate = state;
    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, "
            "Esc=quit");

    return SDL_APP_CONTINUE;

fail:
    /* Centralised cleanup on init failure. */
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    if (state->vertices)
        SDL_free(state->vertices);
    if (state->transfer_buffer)
        SDL_ReleaseGPUTransferBuffer(device, state->transfer_buffer);
    if (state->vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->vertex_buffer);
    if (state->overlay_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->overlay_pipeline);
    if (state->line_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->line_pipeline);
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            if (state->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                            SDL_GetError());
                }
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            if (SDL_SetWindowRelativeMouseMode(state->window, true)) {
                state->mouse_captured = true;
            } else {
                SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            state->cam_yaw -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            float max_pitch = MAX_PITCH_DEG * FORGE_DEG2RAD;
            if (state->cam_pitch > max_pitch) state->cam_pitch = max_pitch;
            if (state->cam_pitch < -max_pitch)
                state->cam_pitch = -max_pitch;
        }
        break;

    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Delta time ──────────────────────────────────────────────────── */
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - state->last_ticks) /
               (float)SDL_GetPerformanceFrequency();
    state->last_ticks = now;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;
    state->time += dt;

    /* ── Camera movement ─────────────────────────────────────────────── */
    {
        quat orient = quat_from_euler(state->cam_yaw, state->cam_pitch,
                                       0.0f);
        vec3 forward = quat_forward(orient);
        vec3 right   = quat_right(orient);
        vec3 up      = vec3_create(0.0f, 1.0f, 0.0f);

        const bool *keys = SDL_GetKeyboardState(NULL);
        float speed = MOVE_SPEED * dt;

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(forward, speed));
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(right, speed));
        if (keys[SDL_SCANCODE_SPACE])
            state->cam_position = vec3_add(
                state->cam_position, vec3_scale(up, speed));
        if (keys[SDL_SCANCODE_LSHIFT])
            state->cam_position = vec3_sub(
                state->cam_position, vec3_scale(up, speed));
    }

    /* ═════════════════════════════════════════════════════════════════
     * Build the debug scene
     *
     * This is the immediate-mode pattern in action: reset the counts,
     * call debug_* functions to accumulate vertices, then upload and
     * draw.  In a real engine, these calls would be scattered across
     * gameplay systems, physics, AI, etc. — each system adding its
     * own debug visualization.
     * ═════════════════════════════════════════════════════════════════ */

    /* Reset vertex counts — previous frame's data is discarded. */
    state->world_count = 0;
    state->overlay_count = 0;

    /* ── Ground grid ─────────────────────────────────────────────────── */
    /* A gray grid on the XZ plane gives spatial reference. */
    debug_grid(state, GRID_HALF_SIZE, GRID_SPACING,
               vec4_create(GRID_COLOR, GRID_COLOR, GRID_COLOR, 1.0f));

    /* ── Origin axis gizmo (overlay) ─────────────────────────────────── */
    /* The origin gizmo is drawn as overlay so it's always visible,
     * even when the camera looks through grid lines or boxes. */
    debug_axes(state, vec3_create(0.0f, 0.0f, 0.0f), AXES_SIZE_LARGE, true);

    /* ── Wireframe boxes at various positions ────────────────────────── */
    /* World-space boxes — they are occluded by lines in front of them. */
    debug_box_wireframe(state,
        vec3_create(-6.0f, 0.0f, -3.0f),
        vec3_create(-4.0f, 2.0f, -1.0f),
        vec4_create(1.0f, 0.6f, 0.0f, 1.0f), false);  /* orange */

    debug_box_wireframe(state,
        vec3_create(4.0f, 0.0f, -4.0f),
        vec3_create(7.0f, 3.0f, -1.0f),
        vec4_create(0.2f, 0.8f, 1.0f, 1.0f), false);  /* cyan */

    debug_box_wireframe(state,
        vec3_create(-2.0f, 0.0f, -8.0f),
        vec3_create(2.0f, 4.0f, -5.0f),
        vec4_create(1.0f, 0.3f, 0.5f, 1.0f), false);  /* pink */

    /* A small box drawn as overlay — always visible for emphasis. */
    debug_box_wireframe(state,
        vec3_create(-0.5f, 0.0f, -0.5f),
        vec3_create(0.5f, 1.0f, 0.5f),
        vec4_create(1.0f, 1.0f, 0.0f, 1.0f), true);   /* yellow overlay */

    /* ── Circles on various planes ───────────────────────────────────── */
    /* Horizontal circle on the XZ plane (normal = Y up). */
    debug_circle(state,
        vec3_create(6.0f, 0.5f, 4.0f), 1.5f,
        vec3_create(0.0f, 1.0f, 0.0f),
        vec4_create(0.0f, 1.0f, 0.5f, 1.0f),
        CIRCLE_SEGMENTS, false);  /* green */

    /* Vertical circle on the XY plane (normal = Z forward). */
    debug_circle(state,
        vec3_create(-6.0f, 2.0f, 4.0f), 1.5f,
        vec3_create(0.0f, 0.0f, 1.0f),
        vec4_create(0.8f, 0.2f, 1.0f, 1.0f),
        CIRCLE_SEGMENTS, false);  /* purple */

    /* Animated circle — the normal rotates over time, creating a
     * spinning hoop effect.  Shows that debug drawing can be dynamic. */
    {
        float angle = state->time * ANIM_SPEED;
        vec3 anim_normal = vec3_create(
            SDL_sinf(angle), 0.5f, SDL_cosf(angle));  /* tilted axis */
        debug_circle(state,
            vec3_create(0.0f, 3.0f, -3.0f), 2.0f,
            anim_normal,
            vec4_create(1.0f, 0.8f, 0.2f, 1.0f),
            CIRCLE_SEGMENTS, false);  /* gold */
    }

    /* ── Axis gizmos on boxes (world-space) ──────────────────────────── */
    /* Small axes at box centers — world-space so they're occluded
     * correctly by the box edges when viewed from behind. */
    debug_axes(state, vec3_create(-5.0f, 1.0f, -2.0f), AXES_SIZE_SMALL, false);
    debug_axes(state, vec3_create(5.5f, 1.5f, -2.5f), AXES_SIZE_SMALL, false);

    /* ═════════════════════════════════════════════════════════════════
     * Upload debug vertices to GPU
     *
     * At this point, world vertices are in [0..world_count-1] and
     * overlay vertices are stored at the end of the array (growing
     * downward from MAX_DEBUG_VERTICES-1).  We compact the overlay
     * vertices to sit right after the world vertices before uploading.
     * ═════════════════════════════════════════════════════════════════ */

    Uint32 total_vertices = state->world_count + state->overlay_count;

    if (total_vertices > 0) {
        /* Compact overlay vertices: move them from the end of the array
         * to sit right after the world vertices.
         *
         * Overlay vertices were stored growing downward from the end:
         * the first overlay added is at index MAX-1, the second at
         * MAX-2, etc.  A forward copy from overlay_start preserves
         * this order: the first-added overlay ends up at the start
         * of the overlay region, which is the correct draw order. */
        if (state->overlay_count > 0) {
            Uint32 i;
            Uint32 overlay_start = MAX_DEBUG_VERTICES - state->overlay_count;
            for (i = 0; i < state->overlay_count; i++) {
                state->vertices[state->world_count + i] =
                    state->vertices[overlay_start + i];
            }
        }

        /* Map the transfer buffer, copy all vertices in, and unmap. */
        void *mapped = SDL_MapGPUTransferBuffer(
            device, state->transfer_buffer, true);
        if (!mapped) {
            SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        SDL_memcpy(mapped, state->vertices,
                   total_vertices * sizeof(DebugVertex));
        SDL_UnmapGPUTransferBuffer(device, state->transfer_buffer);

        /* Issue a copy command to transfer data to the GPU buffer. */
        SDL_GPUCommandBuffer *copy_cmd =
            SDL_AcquireGPUCommandBuffer(device);
        if (!copy_cmd) {
            SDL_Log("SDL_AcquireGPUCommandBuffer (copy) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(copy_cmd);
        if (!copy) {
            SDL_Log("SDL_BeginGPUCopyPass failed: %s", SDL_GetError());
            SDL_CancelGPUCommandBuffer(copy_cmd);
            return SDL_APP_FAILURE;
        }

        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = state->transfer_buffer;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = state->vertex_buffer;
        dst.size = total_vertices * sizeof(DebugVertex);

        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
        SDL_EndGPUCopyPass(copy);

        if (!SDL_SubmitGPUCommandBuffer(copy_cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer (copy) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* ── Acquire swapchain texture ───────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw_w, sw_h;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain_tex, &sw_w, &sw_h)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }
    if (!swapchain_tex) {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ── Resize depth buffer if needed ───────────────────────────────── */
    if (sw_w != state->depth_width || sw_h != state->depth_height) {
        if (state->depth_texture)
            SDL_ReleaseGPUTexture(device, state->depth_texture);

        SDL_GPUTextureCreateInfo dci;
        SDL_zero(dci);
        dci.type = SDL_GPU_TEXTURETYPE_2D;
        dci.format = DEPTH_FORMAT;
        dci.width = sw_w;
        dci.height = sw_h;
        dci.layer_count_or_depth = 1;
        dci.num_levels = 1;
        dci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->depth_texture = SDL_CreateGPUTexture(device, &dci);
        if (!state->depth_texture) {
            SDL_Log("SDL_CreateGPUTexture (depth resize) failed: %s",
                    SDL_GetError());
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }
        state->depth_width = sw_w;
        state->depth_height = sw_h;
    }

    /* ── Build camera matrices ───────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch,
                                       0.0f);
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)sw_w / (float)sw_h;
    mat4 proj = mat4_perspective(
        FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 vp = mat4_multiply(proj, view);

    /* ── Begin render pass ───────────────────────────────────────────── */
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = swapchain_tex;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color.r = CLEAR_R;
    color_target.clear_color.g = CLEAR_G;
    color_target.clear_color.b = CLEAR_B;
    color_target.clear_color.a = CLEAR_A;

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture = state->depth_texture;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.clear_depth = DEPTH_CLEAR;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
        cmd, &color_target, 1, &depth_target);
    if (!pass) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    SDL_SetGPUViewport(pass, &(SDL_GPUViewport){
        0, 0, (float)sw_w, (float)sw_h, 0.0f, 1.0f });
    SDL_SetGPUScissor(pass, &(SDL_Rect){ 0, 0, (int)sw_w, (int)sw_h });

    /* Push the VP matrix once — both pipelines use the same uniform. */
    DebugUniforms uniforms;
    uniforms.view_projection = vp;

    /* ── Draw world-space lines (depth-tested) ───────────────────────── */
    if (state->world_count > 0) {
        SDL_BindGPUGraphicsPipeline(pass, state->line_pipeline);
        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

        SDL_GPUBufferBinding vbb;
        SDL_zero(vbb);
        vbb.buffer = state->vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);

        SDL_DrawGPUPrimitives(pass, state->world_count, 1, 0, 0);
    }

    /* ── Draw overlay lines (always visible) ─────────────────────────── */
    if (state->overlay_count > 0) {
        SDL_BindGPUGraphicsPipeline(pass, state->overlay_pipeline);
        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

        SDL_GPUBufferBinding vbb;
        SDL_zero(vbb);
        vbb.buffer = state->vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);

        /* Overlay vertices start at index world_count — the first_vertex
         * parameter offsets into the vertex buffer. */
        SDL_DrawGPUPrimitives(pass, state->overlay_count, 1,
                              state->world_count, 0);
    }

    /* ── End render pass ─────────────────────────────────────────────── */
    SDL_EndGPURenderPass(pass);

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE && swapchain_tex) {
        if (forge_capture_finish_frame(&state->capture, cmd,
                                        swapchain_tex)) {
            if (forge_capture_should_quit(&state->capture)) {
                return SDL_APP_SUCCESS;
            }
            return SDL_APP_CONTINUE;
        }
    }
#endif

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    app_state *state = (app_state *)appstate;
    (void)result;

    if (!state) return;

    SDL_GPUDevice *device = state->device;

    /* Wait for GPU to finish all pending work before releasing. */
    if (!SDL_WaitForGPUIdle(device)) {
        SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* Release in reverse order of creation. */
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    if (state->vertices)
        SDL_free(state->vertices);
    if (state->transfer_buffer)
        SDL_ReleaseGPUTransferBuffer(device, state->transfer_buffer);
    if (state->vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->vertex_buffer);
    if (state->overlay_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->overlay_pipeline);
    if (state->line_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->line_pipeline);

    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
}
