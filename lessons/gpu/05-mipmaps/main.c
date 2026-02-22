/*
 * Lesson 05 — Mipmaps
 *
 * Build on Lesson 04's texturing to add mipmaps: pre-computed smaller
 * versions of a texture that prevent aliasing when the surface is viewed
 * at a distance or at an angle.
 *
 * Concepts introduced:
 *   - Procedural texture   — Generate a checkerboard in code (no file loading)
 *   - Mipmap creation      — num_levels = log2(size) + 1, SAMPLER | COLOR_TARGET
 *   - Auto mip generation  — SDL_GenerateMipmapsForGPUTexture
 *   - Multiple samplers    — Trilinear, bilinear+nearest mip, no mipmaps
 *   - UV tiling            — UV scale factor to repeat the texture
 *   - Scale animation      — Quad pulses with sine wave to show mip transitions
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, index buffers, shaders, pipeline       (Lesson 02/04)
 *   - Push uniforms                                          (Lesson 03)
 *   - Texture + sampler binding                              (Lesson 04)
 *
 * Press SPACE to cycle between three sampler modes:
 *   1. Trilinear       — LINEAR min/mag + LINEAR mipmap (smooth)
 *   2. Bilinear+nearest — LINEAR min/mag + NEAREST mipmap (pops between levels)
 *   3. No mipmaps      — NEAREST everything, max_lod=0 (aliasing!)
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
#include "shaders/compiled/quad_vert_spirv.h"
#include "shaders/compiled/quad_frag_spirv.h"
#include "shaders/compiled/quad_vert_dxil.h"
#include "shaders/compiled/quad_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 05 Mipmaps"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Linear-space clear color — dark blue-grey. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.03f
#define CLEAR_A 1.0f

/* Quad geometry */
#define VERTEX_COUNT 4
#define INDEX_COUNT  6
#define NUM_VERTEX_ATTRIBUTES 2

/* Shader resource counts */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1

#define FRAG_NUM_SAMPLERS         1
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* Procedural checkerboard texture.
 * 256x256 is a nice power-of-two that gives us 9 mip levels
 * (256, 128, 64, 32, 16, 8, 4, 2, 1). */
#define CHECKER_SIZE 256

/* How many times the checkerboard repeats in each direction.
 * The total pattern is CHECKER_TILES x CHECKER_TILES squares.
 * 8 tiles = 8x8 alternating black/white squares (like a chess board). */
#define CHECKER_TILES 8

/* UV scale — how many times the texture tiles across the quad.
 * 2x tiling means 2 * CHECKER_TILES = 16 visible squares per axis,
 * clearly a checkerboard up close but enough to show aliasing
 * when the quad shrinks. */
#define UV_SCALE 2.0f

/* Quad extent */
#define QUAD_HALF_EXTENT 0.9f

/* Bytes per pixel */
#define BYTES_PER_PIXEL 4

/* Milliseconds-to-seconds */
#define MS_TO_SEC 1000.0f

/* Number of sampler modes we cycle through */
#define NUM_SAMPLER_MODES 3

/* Sentinel value for sampler max_lod — effectively unlimited mip levels.
 * Any value above the actual mip count works; 1000 is a GPU convention. */
#define MAX_LOD_UNLIMITED 1000.0f

/* Buffer length for the window title string (mode name + prefix) */
#define TITLE_BUF_LEN 256

/* ── Sampler mode names (shown in window title) ──────────────────────────── */

static const char *sampler_mode_names[NUM_SAMPLER_MODES] = {
    "Trilinear (LINEAR mip)",
    "Bilinear + NEAREST mip",
    "No mipmaps (aliasing!)"
};

/* ── Vertex format ────────────────────────────────────────────────────────── */

typedef struct Vertex {
    vec2 position;
    vec2 uv;
} Vertex;

/* ── Uniform data ─────────────────────────────────────────────────────────── */
/* NEW: uv_scale controls how many times the texture tiles.
 * Padding to 16-byte alignment for GPU uniform buffers. */

typedef struct Uniforms {
    float time;       /* elapsed seconds                          */
    float aspect;     /* window width / height                    */
    float uv_scale;   /* UV multiplier for tiling                 */
    float _pad;       /* padding to 16-byte boundary              */
} Uniforms;

/* ── Quad data ────────────────────────────────────────────────────────────── */

static const Vertex quad_vertices[VERTEX_COUNT] = {
    { .position = { -QUAD_HALF_EXTENT,  QUAD_HALF_EXTENT }, .uv = { 0.0f, 0.0f } },
    { .position = {  QUAD_HALF_EXTENT,  QUAD_HALF_EXTENT }, .uv = { 1.0f, 0.0f } },
    { .position = {  QUAD_HALF_EXTENT, -QUAD_HALF_EXTENT }, .uv = { 1.0f, 1.0f } },
    { .position = { -QUAD_HALF_EXTENT, -QUAD_HALF_EXTENT }, .uv = { 0.0f, 1.0f } },
};

static const Uint16 quad_indices[INDEX_COUNT] = {
    0, 1, 2,
    2, 3, 0,
};

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer           *vertex_buffer;
    SDL_GPUBuffer           *index_buffer;
    SDL_GPUTexture          *texture;
    SDL_GPUSampler          *samplers[NUM_SAMPLER_MODES];
    int                      current_sampler;
    Uint64                   start_ticks;
#ifdef FORGE_CAPTURE
    ForgeCapture             capture;
#endif
} app_state;

/* ── Shader helper ────────────────────────────────────────────────────────── */

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
    /* Query which shader bytecode formats the current GPU backend supports.
     * Vulkan uses SPIR-V, D3D12 uses DXIL — we ship both and pick at runtime
     * so the same binary runs on either backend. */
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    /* Fill SDL_GPUShaderCreateInfo with the shader's resource binding counts.
     * These tell the GPU driver how many samplers, storage textures/buffers,
     * and uniform buffers the shader expects — they must match the HLSL
     * register declarations exactly or binding will silently break. */
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage                = stage;
    info.entrypoint           = "main";
    info.num_samplers         = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers  = num_storage_buffers;
    info.num_uniform_buffers  = num_uniform_buffers;

    /* Prefer SPIR-V (Vulkan, portable) with DXIL as the Windows/D3D12
     * fallback.  This order maximises cross-platform coverage. */
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

    /* Pass the completed SDL_GPUShaderCreateInfo to create a runtime shader
     * object that can be attached to a graphics pipeline. */
    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("Failed to create %s shader: %s",
                stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
                SDL_GetError());
    }
    return shader;
}

/* ── Procedural checkerboard texture ─────────────────────────────────────── */
/* Generates a black-and-white checkerboard pattern directly in CPU memory.
 * No external image file needed!
 *
 * The pattern has CHECKER_TILES × CHECKER_TILES squares across the texture.
 * Each square is (CHECKER_SIZE / CHECKER_TILES) pixels wide.
 *
 * The texture is created with SAMPLER | COLOR_TARGET usage because
 * SDL_GenerateMipmapsForGPUTexture requires COLOR_TARGET to render
 * into lower mip levels internally. */

static SDL_GPUTexture *create_checker_texture(SDL_GPUDevice *device)
{
    int tex_size = CHECKER_SIZE;
    int num_levels = (int)forge_log2f((float)tex_size) + 1;
    SDL_Log("Creating %dx%d checkerboard texture with %d mip levels",
            tex_size, tex_size, num_levels);

    /* ── 1. Create the GPU texture with mip levels ────────────────────
     * TEXTUREUSAGE_SAMPLER — we'll sample this in the fragment shader.
     * TEXTUREUSAGE_COLOR_TARGET — required for SDL_GenerateMipmapsForGPUTexture
     *   because the GPU generates mipmaps by rendering into each level. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width                = tex_size;
    tex_info.height               = tex_size;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = num_levels;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create checker texture: %s", SDL_GetError());
        return NULL;
    }

    /* ── 2. Generate checkerboard pixel data on the CPU ──────────────
     * Each texel is black or white depending on which tile it falls in.
     * tile_size = texture pixels per checkerboard square. */
    Uint32 total_bytes = tex_size * tex_size * BYTES_PER_PIXEL;
    Uint8 *pixels = (Uint8 *)SDL_malloc(total_bytes);
    if (!pixels) {
        SDL_Log("Failed to allocate checker pixels");
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    int tile_size = tex_size / CHECKER_TILES;
    for (int y = 0; y < tex_size; y++) {
        for (int x = 0; x < tex_size; x++) {
            /* Which tile are we in? If (tile_x + tile_y) is even, white;
             * if odd, black. Classic checkerboard pattern. */
            int tile_x = x / tile_size;
            int tile_y = y / tile_size;
            Uint8 color = ((tile_x + tile_y) % 2 == 0) ? 255 : 0;

            int idx = (y * tex_size + x) * BYTES_PER_PIXEL;
            pixels[idx + 0] = color;  /* R */
            pixels[idx + 1] = color;  /* G */
            pixels[idx + 2] = color;  /* B */
            pixels[idx + 3] = 255;    /* A */
        }
    }

    /* ── 3. Upload base level to GPU ────────────────────────────────── */
    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_free(pixels);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_free(pixels);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_memcpy(mapped, pixels, total_bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_free(pixels);

    /* ── 4. Upload base level + generate mipmaps ─────────────────────
     * We upload the base level (mip 0) via a copy pass, then call
     * SDL_GenerateMipmapsForGPUTexture to auto-generate all smaller
     * levels.  This must be called OUTSIDE any render or copy pass,
     * but within the same command buffer submission. */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    /* Copy pass: upload base level */
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = transfer;
    tex_src.offset          = 0;
    tex_src.pixels_per_row  = tex_size;
    tex_src.rows_per_layer  = tex_size;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture   = texture;
    tex_dst.mip_level = 0;
    tex_dst.w         = tex_size;
    tex_dst.h         = tex_size;
    tex_dst.d         = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    /* Generate mipmaps — the GPU automatically downsamples level 0
     * into levels 1, 2, 3, ... using a series of blit operations.
     * This call must be outside any pass. */
    SDL_GenerateMipmapsForGPUTexture(cmd, texture);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit texture upload command buffer: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    SDL_Log("Checkerboard texture created with %d mip levels", num_levels);
    return texture;
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
        true,
        NULL
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

    /* ── 4. Request sRGB swapchain ──────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 5. Create procedural checkerboard texture with mipmaps ──────── */
    SDL_GPUTexture *texture = create_checker_texture(device);
    if (!texture) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 6. Create three samplers for comparison ─────────────────────
     * Each sampler demonstrates a different mipmap filtering approach.
     * Press SPACE to cycle between them and observe the differences. */

    SDL_GPUSampler *samplers[NUM_SAMPLER_MODES];

    /* Sampler 0: Trilinear — the gold standard for smooth rendering.
     * LINEAR min/mag filter + LINEAR mipmap mode = trilinear filtering.
     * This blends between mip levels smoothly. */
    {
        SDL_GPUSamplerCreateInfo info;
        SDL_zero(info);
        info.min_filter     = SDL_GPU_FILTER_LINEAR;
        info.mag_filter     = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.min_lod        = 0.0f;
        info.max_lod        = MAX_LOD_UNLIMITED;
        samplers[0] = SDL_CreateGPUSampler(device, &info);
    }

    /* Sampler 1: Bilinear + nearest mip — smooth within a level but
     * "pops" visibly when switching between levels. */
    {
        SDL_GPUSamplerCreateInfo info;
        SDL_zero(info);
        info.min_filter     = SDL_GPU_FILTER_LINEAR;
        info.mag_filter     = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.min_lod        = 0.0f;
        info.max_lod        = MAX_LOD_UNLIMITED;
        samplers[1] = SDL_CreateGPUSampler(device, &info);
    }

    /* Sampler 2: No mipmaps — NEAREST everything, max_lod = 0 forces
     * the GPU to always use level 0 (the base texture).  This shows
     * the aliasing problem that mipmaps solve. */
    {
        SDL_GPUSamplerCreateInfo info;
        SDL_zero(info);
        info.min_filter     = SDL_GPU_FILTER_NEAREST;
        info.mag_filter     = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.min_lod        = 0.0f;
        info.max_lod        = 0.0f;   /* Force level 0 only */
        samplers[2] = SDL_CreateGPUSampler(device, &info);
    }

    for (int i = 0; i < NUM_SAMPLER_MODES; i++) {
        if (!samplers[i]) {
            SDL_Log("Failed to create sampler %d: %s", i, SDL_GetError());
            for (int j = 0; j < i; j++) {
                SDL_ReleaseGPUSampler(device, samplers[j]);
            }
            SDL_ReleaseGPUTexture(device, texture);
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 7. Create shaders ──────────────────────────────────────────── */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        quad_vert_spirv, quad_vert_spirv_size,
        quad_vert_dxil,  quad_vert_dxil_size,
        VERT_NUM_SAMPLERS, VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS, VERT_NUM_UNIFORM_BUFFERS);
    if (!vertex_shader) {
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
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
        FRAG_NUM_SAMPLERS, FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS, FRAG_NUM_UNIFORM_BUFFERS);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 8. Create graphics pipeline ────────────────────────────────── */
    /* Describe the vertex buffer layout — the GPU needs to know the byte
     * stride (pitch) between consecutive vertices so it can step through
     * the interleaved Vertex structs in memory. */
    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(Vertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    /* Map each Vertex struct field to a shader input location.
     * FLOAT2 matches vec2 (two 32-bit floats) — position and UV are both
     * 2-component vectors.  offsetof() gives the byte offset of each
     * field within the interleaved Vertex struct so the GPU knows where
     * to read each attribute.  Location N maps to HLSL TEXCOORD{N}. */
    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    vertex_attributes[0].location    = 0;   /* TEXCOORD0 = position */
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[0].offset      = offsetof(Vertex, position);

    vertex_attributes[1].location    = 1;   /* TEXCOORD1 = uv */
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

    /* Triangle list: every 3 indices form one triangle.  Simple and
     * universal — good for a quad (2 triangles, 6 indices). */
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Solid fill (not wireframe), no backface culling (the quad is flat
     * and may face either way), CCW winding matches our vertex order. */
    pipeline_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* The pipeline's color target format must match the swapchain format
     * exactly — query it at runtime because it varies by backend and
     * swapchain composition (e.g. SDR_LINEAR gives an _SRGB format). */
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
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Safe to release shader objects now — the pipeline keeps its own
     * compiled copy, so the originals are no longer needed. */
    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    /* ── 9. Create & upload vertex + index buffers ───────────────────── */
    SDL_GPUBufferCreateInfo vbuf_info;
    SDL_zero(vbuf_info);
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbuf_info.size  = sizeof(quad_vertices);

    SDL_GPUBuffer *vertex_buffer = SDL_CreateGPUBuffer(device, &vbuf_info);
    if (!vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUBufferCreateInfo ibuf_info;
    SDL_zero(ibuf_info);
    ibuf_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ibuf_info.size  = sizeof(quad_indices);

    SDL_GPUBuffer *index_buffer = SDL_CreateGPUBuffer(device, &ibuf_info);
    if (!index_buffer) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

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
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
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
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
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
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
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
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
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
        SDL_Log("Failed to submit buffer upload command buffer: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    /* ── 10. Store state ─────────────────────────────────────────────── */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUBuffer(device, index_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        for (int i = 0; i < NUM_SAMPLER_MODES; i++)
            SDL_ReleaseGPUSampler(device, samplers[i]);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window          = window;
    state->device          = device;
    state->pipeline        = pipeline;
    state->vertex_buffer   = vertex_buffer;
    state->index_buffer    = index_buffer;
    state->texture         = texture;
    state->current_sampler = 0;
    state->start_ticks     = SDL_GetTicks();

    for (int i = 0; i < NUM_SAMPLER_MODES; i++) {
        state->samplers[i] = samplers[i];
    }

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUBuffer(device, index_buffer);
            SDL_ReleaseGPUBuffer(device, vertex_buffer);
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            for (int i = 0; i < NUM_SAMPLER_MODES; i++)
                SDL_ReleaseGPUSampler(device, samplers[i]);
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

    SDL_Log("Press SPACE to cycle sampler modes");
    SDL_Log("Current: %s", sampler_mode_names[0]);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────────── */
/* NEW: Press SPACE to cycle between sampler modes. */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN &&
        event->key.key == SDLK_SPACE) {
        state->current_sampler = (state->current_sampler + 1) % NUM_SAMPLER_MODES;

        /* Update window title to show current mode */
        char title[TITLE_BUF_LEN];
        SDL_snprintf(title, sizeof(title), "%s — %s",
                     WINDOW_TITLE, sampler_mode_names[state->current_sampler]);
        if (!SDL_SetWindowTitle(state->window, title)) {
            SDL_Log("SDL_SetWindowTitle failed: %s", SDL_GetError());
        }

        SDL_Log("Sampler: %s", sampler_mode_names[state->current_sampler]);
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────────── */

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
    uniforms.time     = elapsed;
    uniforms.aspect   = aspect;
    uniforms.uv_scale = UV_SCALE;
    uniforms._pad     = 0.0f;

    /* ── 2. Acquire command buffer ────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 3. Push per-frame uniforms ───────────────────────────────── */
    /* The uniforms struct contains values that change every frame:
     *   time     — drives sine-wave animation (quad pulsing)
     *   aspect   — keeps the quad square regardless of window resize
     *   uv_scale — controls how many times the texture tiles
     * We must push these each frame so the vertex shader sees the
     * latest values via its cbuffer. */
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

        /* Select the GPU program and fixed-function pipeline state
         * (shaders, vertex layout, rasterizer, blend modes). */
        SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

        /* Bind the vertex buffer — provides position and UV attributes
         * to the vertex shader for each vertex in the quad. */
        SDL_GPUBufferBinding vertex_binding;
        SDL_zero(vertex_binding);
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

        /* Bind the index buffer — supplies triangle indices so we can
         * draw the quad with 4 vertices instead of 6.  16-bit indices
         * are sufficient for small meshes. */
        SDL_GPUBufferBinding index_binding;
        SDL_zero(index_binding);
        index_binding.buffer = state->index_buffer;
        index_binding.offset = 0;
        SDL_BindGPUIndexBuffer(pass, &index_binding,
                               SDL_GPU_INDEXELEMENTSIZE_16BIT);

        /* Bind the texture and the currently selected sampler so the
         * fragment shader can perform texture lookups (sampling). */
        SDL_GPUTextureSamplerBinding tex_sampler_binding;
        SDL_zero(tex_sampler_binding);
        tex_sampler_binding.texture = state->texture;
        tex_sampler_binding.sampler = state->samplers[state->current_sampler];
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_sampler_binding, 1);

        /* Issue the indexed draw call — renders the quad using the
         * pipeline, vertex/index buffers, and texture+sampler bound above. */
        SDL_DrawGPUIndexedPrimitives(pass, INDEX_COUNT, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("Failed to submit render command buffer: %s",
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
            SDL_Log("Failed to submit render command buffer: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    /* Release GPU resources in reverse order of creation so nothing
     * references an already-freed object. */
    app_state *state = (app_state *)appstate;
    if (state) {
#ifdef FORGE_CAPTURE
        forge_capture_destroy(&state->capture, state->device);
#endif
        for (int i = 0; i < NUM_SAMPLER_MODES; i++) {
            SDL_ReleaseGPUSampler(state->device, state->samplers[i]);
        }
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
