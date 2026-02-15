/*
 * Lesson 04 — Textures & Samplers
 *
 * Load an image from disk, upload it to the GPU, and draw it on a spinning
 * quad.  This lesson introduces several new concepts at once because they
 * all connect: you need UV coordinates to tell the shader *where* to read
 * from the texture, a sampler to tell the GPU *how* to filter the read,
 * and an index buffer because a quad has four vertices but six indices.
 *
 * Concepts introduced:
 *   - Loading images      — SDL_LoadSurface reads PNG (or BMP) from disk,
 *                           SDL_ConvertSurface converts to GPU-ready RGBA8
 *   - GPU textures        — SDL_CreateGPUTexture with TEXTUREUSAGE_SAMPLER
 *   - Texture upload      — Transfer buffer → SDL_UploadToGPUTexture
 *   - Samplers            — SDL_CreateGPUSampler with filtering & address modes
 *   - UV coordinates      — New vertex attribute mapping texels to geometry
 *   - Index buffers       — Draw a quad with 4 vertices + 6 indices
 *   - sRGB texture format — R8G8B8A8_UNORM_SRGB for correct color pipeline
 *   - Fragment sampling   — Texture2D.Sample() in HLSL
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain  (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline          (Lesson 02)
 *   - Push uniforms, rotation animation                   (Lesson 03)
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
#include "shaders/quad_vert_spirv.h"
#include "shaders/quad_frag_spirv.h"
#include "shaders/quad_vert_dxil.h"
#include "shaders/quad_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 04 Textures & Samplers"
#define WINDOW_WIDTH  600
#define WINDOW_HEIGHT 600

/* Linear-space clear color — a dark blue-grey background. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.03f
#define CLEAR_A 1.0f

/* Number of vertices and indices for a quad.
 * A quad has 4 unique vertices, but requires 6 indices (two triangles).
 *
 *   v0------v1        Triangles:
 *   | \      |          0: v0, v1, v2
 *   |  \     |          1: v2, v3, v0
 *   |   \    |
 *   v3------v2        Sharing v0 and v2 saves 2 vertices of data.
 */
#define VERTEX_COUNT 4
#define INDEX_COUNT  6

/* Number of vertex attributes (position, uv). */
#define NUM_VERTEX_ATTRIBUTES 2

/* Shader resource counts.
 * The vertex shader uses 1 uniform buffer (time + aspect), same as Lesson 03.
 * NEW: the fragment shader now uses 1 sampler (texture + sampler pair). */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1

#define FRAG_NUM_SAMPLERS         1   /* ← NEW: one texture+sampler pair */
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* Rotation speed in radians per second. */
#define ROTATION_SPEED 1.0f

/* Path to the texture file, relative to the executable. */
#define TEXTURE_FILENAME "textures/brick_wall.png"

/* Half-extent of the quad in NDC — the quad spans from -0.6 to +0.6. */
#define QUAD_HALF_EXTENT 0.6f

/* Bytes per pixel for RGBA8 textures. */
#define BYTES_PER_PIXEL 4

/* Size of the path buffer for building the texture file path. */
#define PATH_BUFFER_SIZE 1024

/* Milliseconds-to-seconds conversion factor. */
#define MS_TO_SEC 1000.0f

/* ── Vertex format ────────────────────────────────────────────────────────── */
/* Each vertex has a 2D position and a 2D texture coordinate (UV).
 *
 * Unlike Lesson 02/03 which had per-vertex color, this lesson gets color
 * from the texture — so we replace vec3 color with vec2 uv.
 *
 * UV coordinates map each vertex to a position on the texture image:
 *   (0, 0) = top-left of the image
 *   (1, 1) = bottom-right of the image
 * The rasterizer interpolates these across the surface, so every fragment
 * gets a unique UV telling the shader exactly which texel to sample.
 *
 * Memory layout (16 bytes per vertex):
 *   offset 0:  vec2 position   (8 bytes)  → TEXCOORD0 in HLSL
 *   offset 8:  vec2 uv         (8 bytes)  → TEXCOORD1 in HLSL
 */

typedef struct Vertex {
    vec2 position;   /* position in normalized device coordinates */
    vec2 uv;         /* texture coordinate (0–1 range)           */
} Vertex;

/* ── Uniform data ─────────────────────────────────────────────────────────── */
/* Same as Lesson 03: time for animation, aspect for shape correction. */

typedef struct Uniforms {
    float time;     /* elapsed time in seconds                     */
    float aspect;   /* window width / height — for correcting NDC  */
} Uniforms;

/* ── Quad data ────────────────────────────────────────────────────────────── */
/* A quad centered at the origin with UV coordinates mapping the full texture.
 *
 * Position layout:
 *   (-0.6, +0.6)------(+0.6, +0.6)     v0------v1
 *        |    \              |            |  \     |
 *        |     \             |            |   \    |
 *        |      \            |            |    \   |
 *   (-0.6, -0.6)------(+0.6, -0.6)     v3------v2
 *
 * UV layout (standard convention — origin at top-left):
 *   (0, 0)-------(1, 0)
 *     |              |
 *     |              |
 *   (0, 1)-------(1, 1)
 *
 * The centroid of the four positions is (0, 0), so the quad spins in place
 * just like the triangle in Lesson 03. */

static const Vertex quad_vertices[VERTEX_COUNT] = {
    { .position = { -QUAD_HALF_EXTENT,  QUAD_HALF_EXTENT }, .uv = { 0.0f, 0.0f } },  /* v0: top-left     */
    { .position = {  QUAD_HALF_EXTENT,  QUAD_HALF_EXTENT }, .uv = { 1.0f, 0.0f } },  /* v1: top-right    */
    { .position = {  QUAD_HALF_EXTENT, -QUAD_HALF_EXTENT }, .uv = { 1.0f, 1.0f } },  /* v2: bottom-right */
    { .position = { -QUAD_HALF_EXTENT, -QUAD_HALF_EXTENT }, .uv = { 0.0f, 1.0f } },  /* v3: bottom-left  */
};

/* ── Index data ───────────────────────────────────────────────────────────── */
/* Two triangles sharing vertices v0 and v2.  Using indices means we store
 * 4 vertices instead of 6, and — more importantly for larger meshes — the
 * GPU can reuse vertex shader output for shared vertices.
 *
 * Winding order is counter-clockwise (CCW) to match our pipeline's front
 * face setting, though we have backface culling disabled. */

static const Uint16 quad_indices[INDEX_COUNT] = {
    0, 1, 2,   /* first triangle:  v0 → v1 → v2 */
    2, 3, 0,   /* second triangle: v2 → v3 → v0 */
};

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer           *vertex_buffer;
    SDL_GPUBuffer           *index_buffer;    /* ← NEW: index buffer */
    SDL_GPUTexture          *texture;         /* ← NEW: GPU texture */
    SDL_GPUSampler          *sampler;         /* ← NEW: texture sampler */
    Uint64                   start_ticks;
#ifdef FORGE_CAPTURE
    ForgeCapture             capture;
#endif
} app_state;

/* ── Shader helper ────────────────────────────────────────────────────────── */
/* Same as Lesson 03 — creates a GPU shader from pre-compiled bytecodes. */

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

/* ── Texture loading helper ───────────────────────────────────────────────── */
/* Loads a PNG image and uploads it to a GPU texture.
 *
 * The steps mirror the vertex buffer upload pattern from Lesson 02:
 *   1. Load image from disk into an SDL_Surface (CPU memory)
 *   2. Convert to RGBA8 pixel format (if not already)
 *   3. Create a GPU texture with TEXTUREUSAGE_SAMPLER
 *   4. Create a transfer buffer, copy pixel data row-by-row
 *   5. Upload to the GPU texture via a copy pass
 *   6. Release the transfer buffer and surface
 *
 * We use R8G8B8A8_UNORM_SRGB as the texture format.  The "_SRGB" suffix
 * tells the GPU that the texels are in sRGB color space, so when the
 * shader samples from this texture, the GPU automatically converts to
 * linear space.  Combined with the sRGB swapchain (which converts linear
 * back to sRGB on write), we get correct colors end-to-end without any
 * manual math. */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    /* ── 1. Load the image from disk ──────────────────────────────────
     * SDL3 has built-in PNG support — SDL_LoadSurface handles both BMP
     * and PNG files automatically (no SDL_image library needed). */
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load image '%s': %s", path, SDL_GetError());
        return NULL;
    }

    /* ── 2. Convert to RGBA8 format ───────────────────────────────────
     * SDL surfaces can be in many pixel formats depending on the source
     * file.  We convert to SDL_PIXELFORMAT_ABGR8888 which is SDL's name
     * for R8G8B8A8 in memory:
     *
     *   SDL naming:  ABGR8888 → bits MSB→LSB: A, B, G, R
     *   Memory order (little-endian): R, G, B, A ← what the GPU sees
     *
     * This matches the GPU format SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB. */
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert surface to RGBA8: %s", SDL_GetError());
        return NULL;
    }

    int tex_w = converted->w;
    int tex_h = converted->h;
    SDL_Log("Loaded texture: %s (%dx%d)", path, tex_w, tex_h);

    /* ── 3. Create the GPU texture ────────────────────────────────────
     * TEXTUREUSAGE_SAMPLER means we'll read from this texture in shaders
     * using a sampler.  The sRGB format tells the GPU to decode sRGB→linear
     * automatically when sampling. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = tex_w;
    tex_info.height               = tex_h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = 1;

    SDL_GPUTexture *gpu_texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!gpu_texture) {
        SDL_Log("Failed to create GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    /* ── 4. Create transfer buffer and copy pixel data ────────────────
     * Same pattern as vertex buffer upload: create a staging buffer in
     * shared memory, copy CPU data into it, then issue a GPU copy command.
     *
     * Important: SDL surfaces may have padding at the end of each row
     * (pitch > width * bytes_per_pixel), so we copy row by row to strip
     * any padding.  The GPU texture expects tightly-packed rows. */
    Uint32 bytes_per_row   = (Uint32)tex_w * BYTES_PER_PIXEL;
    Uint32 total_bytes     = bytes_per_row * (Uint32)tex_h;

    SDL_GPUTransferBufferCreateInfo transfer_info;
    SDL_zero(transfer_info);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        device, &transfer_info);
    if (!transfer) {
        SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, gpu_texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, gpu_texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    /* Copy row by row to handle surface pitch (padding).
     * If pitch == bytes_per_row, the memcpy is equivalent to a single
     * copy — but doing it row by row is always safe. */
    const Uint8 *src_pixels = (const Uint8 *)converted->pixels;
    Uint8 *dst_pixels = (Uint8 *)mapped;
    for (int y = 0; y < tex_h; y++) {
        SDL_memcpy(
            dst_pixels + y * bytes_per_row,
            src_pixels + y * converted->pitch,
            bytes_per_row);
    }

    SDL_UnmapGPUTransferBuffer(device, transfer);

    /* ── 5. Upload to the GPU texture ─────────────────────────────────
     * The texture upload uses SDL_UploadToGPUTexture instead of
     * SDL_UploadToGPUBuffer.  The source is a transfer buffer location
     * (with row pitch info), and the destination is a texture region
     * specifying which mip level and area to write to. */
    SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!upload_cmd) {
        SDL_Log("Failed to acquire command buffer for texture upload: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, gpu_texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass for texture upload: %s",
                SDL_GetError());
        SDL_CancelGPUCommandBuffer(upload_cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, gpu_texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = transfer;
    tex_src.offset          = 0;
    tex_src.pixels_per_row  = tex_w;
    tex_src.rows_per_layer  = tex_h;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = gpu_texture;
    tex_dst.w       = tex_w;
    tex_dst.h       = tex_h;
    tex_dst.d       = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd);

    /* ── 6. Clean up staging resources ────────────────────────────────
     * The transfer buffer and surface are no longer needed — the pixel
     * data now lives on the GPU. */
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(converted);

    return gpu_texture;
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

    /* ── 4. Request an sRGB swapchain (same as Lessons 01–03) ──────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        SDL_SetGPUSwapchainParameters(
            device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
            SDL_GPU_PRESENTMODE_VSYNC);
    }

    /* ── 5. Load texture from disk ──────────────────────────────────────
     * NEW: Load a PNG, convert to RGBA8, upload to the GPU.
     * We do this before creating the pipeline because — while not
     * strictly required — it keeps the "create resources" phase together.
     *
     * The texture file is copied next to the executable by CMake's
     * POST_BUILD step, so we can load it with a relative path. */
    const char *base_path = SDL_GetBasePath();
    char texture_path[PATH_BUFFER_SIZE];
    SDL_snprintf(texture_path, sizeof(texture_path), "%s%s",
                 base_path ? base_path : "", TEXTURE_FILENAME);

    SDL_GPUTexture *texture = load_texture(device, texture_path);
    if (!texture) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 6. Create sampler ──────────────────────────────────────────────
     * NEW: A sampler controls how the GPU reads from a texture:
     *
     *   Filtering — what to do when one texel doesn't map to one pixel:
     *     LINEAR:  blend between neighboring texels (smooth, the default)
     *     NEAREST: pick the closest texel (pixelated, good for pixel art)
     *
     *   Address mode — what to do when UVs go outside 0–1:
     *     REPEAT:       wrap around (tiles the texture)
     *     CLAMP_TO_EDGE: clamp to the edge color
     *     MIRRORED_REPEAT: mirror at each boundary
     *
     * We use LINEAR filtering and REPEAT addressing here. Exercise: try
     * changing to NEAREST to see the pixelated look, or change UVs to
     * go beyond 1.0 to see the texture tile. */
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter        = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter        = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (!sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 7. Create shaders ──────────────────────────────────────────────
     * The vertex shader is similar to Lesson 03 (uniform for time/aspect).
     * The fragment shader now declares num_samplers = 1, telling SDL it
     * will read from one texture+sampler pair. */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        quad_vert_spirv, quad_vert_spirv_size,
        quad_vert_dxil,  quad_vert_dxil_size,
        VERT_NUM_SAMPLERS,
        VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS,
        VERT_NUM_UNIFORM_BUFFERS);
    if (!vertex_shader) {
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader *fragment_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        quad_frag_spirv, quad_frag_spirv_size,
        quad_frag_dxil,  quad_frag_dxil_size,
        FRAG_NUM_SAMPLERS,
        FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS,
        FRAG_NUM_UNIFORM_BUFFERS);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Create graphics pipeline ────────────────────────────────────
     * The pipeline description is similar to Lesson 02/03, but with a
     * different vertex format: position + UV instead of position + color. */
    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(Vertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    /* Attribute 0: position (float2) */
    vertex_attributes[0].location    = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[0].offset      = offsetof(Vertex, position);

    /* Attribute 1: UV texture coordinate (float2) — was color (float3) in Lesson 03 */
    vertex_attributes[1].location    = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[1].offset      = offsetof(Vertex, uv);

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
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    /* ── 9. Create & upload vertex buffer ───────────────────────────────
     * Same pattern as Lesson 02, but with 4 vertices instead of 3. */
    SDL_GPUBufferCreateInfo vbuf_info;
    SDL_zero(vbuf_info);
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbuf_info.size  = sizeof(quad_vertices);

    SDL_GPUBuffer *vertex_buffer = SDL_CreateGPUBuffer(device, &vbuf_info);
    if (!vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 10. Create & upload index buffer ───────────────────────────────
     * NEW: An index buffer tells the GPU which vertices to use for each
     * triangle, allowing vertex reuse.  For our quad:
     *   - 4 unique vertices (saving 2 vs. 6 separate vertices)
     *   - 6 indices (two triangles, 3 indices each)
     *
     * We use Uint16 indices (16-bit), which supports up to 65535 vertices.
     * For larger meshes you'd use Uint32 (32-bit). */
    SDL_GPUBufferCreateInfo ibuf_info;
    SDL_zero(ibuf_info);
    ibuf_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ibuf_info.size  = sizeof(quad_indices);

    SDL_GPUBuffer *index_buffer = SDL_CreateGPUBuffer(device, &ibuf_info);
    if (!index_buffer) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Upload both vertex and index data in a single transfer + copy pass.
     * We allocate one transfer buffer large enough for both, then issue
     * two upload commands in the same copy pass. */
    Uint32 vertex_data_size = sizeof(quad_vertices);
    Uint32 index_data_size  = sizeof(quad_indices);
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
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
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
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_memcpy(mapped, quad_vertices, vertex_data_size);
    SDL_memcpy((Uint8 *)mapped + vertex_data_size, quad_indices, index_data_size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!upload_cmd) {
        SDL_Log("Failed to acquire command buffer for buffer upload: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass for buffer upload: %s",
                SDL_GetError());
        SDL_CancelGPUCommandBuffer(upload_cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
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
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    /* ── 11. Store state ──────────────────────────────────────────────── */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, texture);
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
    state->texture       = texture;
    state->sampler       = sampler;
    state->start_ticks   = SDL_GetTicks();

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUBuffer(device, index_buffer);
            SDL_ReleaseGPUBuffer(device, vertex_buffer);
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            SDL_ReleaseGPUSampler(device, sampler);
            SDL_ReleaseGPUTexture(device, texture);
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
 *   1. Compute elapsed time and aspect ratio
 *   2. Push uniforms to the vertex shader
 *   3. Clear, bind pipeline, bind vertex+index buffers
 *   4. Bind the texture+sampler to the fragment shader   ← NEW
 *   5. Draw indexed primitives                           ← NEW (was DrawGPUPrimitives)
 */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute elapsed time and aspect ratio ─────────────────── */
    Uint64 now_ms = SDL_GetTicks();
    float elapsed = (float)(now_ms - state->start_ticks) / MS_TO_SEC;

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("Failed to get window size: %s", SDL_GetError());
    }
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

    Uniforms uniforms;
    uniforms.time   = elapsed * ROTATION_SPEED;
    uniforms.aspect = aspect;

    /* ── 2. Acquire command buffer ────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 3. Push uniform data (before the render pass) ────────────── */
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* ── 4. Acquire swapchain & render ────────────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
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
        if (!pass) {
            SDL_Log("Failed to begin render pass: %s", SDL_GetError());
            SDL_CancelGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

        /* Bind vertex buffer (same as before) */
        SDL_GPUBufferBinding vertex_binding;
        SDL_zero(vertex_binding);
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

        /* NEW: Bind index buffer.
         * We specify the element size (16-bit) so the GPU knows how
         * to interpret each index value. */
        SDL_GPUBufferBinding index_binding;
        SDL_zero(index_binding);
        index_binding.buffer = state->index_buffer;
        index_binding.offset = 0;
        SDL_BindGPUIndexBuffer(pass, &index_binding,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);

        /* NEW: Bind texture + sampler to the fragment shader.
         * SDL GPU binds textures and samplers as pairs.  The array
         * index here (first_slot = 0, count = 1) matches
         * register(t0, space2) / register(s0, space2) in the HLSL. */
        SDL_GPUTextureSamplerBinding tex_sampler_binding;
        SDL_zero(tex_sampler_binding);
        tex_sampler_binding.texture = state->texture;
        tex_sampler_binding.sampler = state->sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_sampler_binding, 1);

        /* NEW: Draw with indices instead of raw vertices.
         * SDL_DrawGPUIndexedPrimitives reads INDEX_COUNT indices from
         * the index buffer, looks up the corresponding vertices, and
         * assembles triangles from them. */
        SDL_DrawGPUIndexedPrimitives(pass, INDEX_COUNT, 1, 0, 0, 0);

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
/* Clean up in reverse order of creation.
 * NEW: We now also release the texture, sampler, and index buffer. */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (state) {
#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, state->device);
#endif
        SDL_ReleaseGPUSampler(state->device, state->sampler);
        SDL_ReleaseGPUTexture(state->device, state->texture);
        SDL_ReleaseGPUBuffer(state->device, state->index_buffer);
        SDL_ReleaseGPUBuffer(state->device, state->vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
