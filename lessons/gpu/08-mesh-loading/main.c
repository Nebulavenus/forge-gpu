/*
 * Lesson 08 — Loading a Mesh (OBJ)
 *
 * Load a real 3D model from a Wavefront OBJ file — the first lesson where
 * geometry comes from a file rather than being hard-coded.  We render a
 * textured space shuttle model with a fly-around camera.
 *
 * Concepts introduced:
 *   - OBJ file loading      — parsing vertices, normals, UVs, and faces
 *   - De-indexing            — why GPU can't use OBJ's triple-index scheme
 *   - Quad triangulation     — splitting quads into two triangles
 *   - File-based textures    — loading a PNG diffuse map with SDL_LoadSurface
 *   - Mipmapped textures     — auto-generating mip levels from a loaded image
 *   - Non-indexed rendering  — SDL_DrawGPUPrimitives (no index buffer)
 *
 * Libraries used:
 *   common/obj/forge_obj.h   — Header-only OBJ parser (new in this lesson)
 *   common/math/forge_math.h — Vectors, matrices, quaternions
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain     (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline             (Lesson 02)
 *   - Push uniforms for MVP matrix                           (Lesson 03)
 *   - Texture + sampler binding, mipmaps                     (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize         (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time        (Lesson 07)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   Escape             — release mouse / quit
 *
 * Model: Space Shuttle by Microsoft, CC Attribution
 *        https://sketchfab.com/3d-models/space-shuttle-0b4ef1a8fdd54b7286a2a374ac5e90d7
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include "math/forge_math.h"
#include "obj/forge_obj.h"

/* ── Frame capture (compile-time option) ─────────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────────── */
#include "shaders/mesh_vert_spirv.h"
#include "shaders/mesh_frag_spirv.h"
#include "shaders/mesh_vert_dxil.h"
#include "shaders/mesh_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 08 Loading a Mesh (OBJ)"
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

/* Dark clear color so the model stands out. */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.04f
#define CLEAR_A 1.0f

/* Depth buffer — same setup as Lesson 06/07. */
#define DEPTH_CLEAR  1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D16_UNORM

/* Vertex attributes: position (float3) + normal (float3) + uv (float2). */
#define NUM_VERTEX_ATTRIBUTES 3

/* Shader resource counts. */
#define VERT_NUM_SAMPLERS         0
#define VERT_NUM_STORAGE_TEXTURES 0
#define VERT_NUM_STORAGE_BUFFERS  0
#define VERT_NUM_UNIFORM_BUFFERS  1   /* MVP matrix */

#define FRAG_NUM_SAMPLERS         1   /* diffuse texture + sampler */
#define FRAG_NUM_STORAGE_TEXTURES 0
#define FRAG_NUM_STORAGE_BUFFERS  0
#define FRAG_NUM_UNIFORM_BUFFERS  0

/* File paths for the model and texture.  These files are copied next to
 * the executable at build time by CMakeLists.txt. */
#define MODEL_PATH       "assets/models/space-shuttle/space-shuttle.obj"
#define TEXTURE_PATH     "assets/models/space-shuttle/ShuttleDiffuseMap.png"
#define PATH_BUFFER_SIZE 512

/* Texture mip levels: log2(1024) + 1 = 11 levels for a 1024x1024 texture. */
#define TEXTURE_SIZE     1024
#define TEXTURE_MIP_LEVELS 11

/* Bytes per pixel for RGBA textures. */
#define BYTES_PER_PIXEL 4

/* Maximum LOD — effectively unlimited, standard GPU convention. */
#define MAX_LOD_UNLIMITED 1000.0f

/* ── Camera parameters ───────────────────────────────────────────────────── */

/* Starting position: behind and above the shuttle, looking toward it.
 * The shuttle is roughly 35 units wide and centered near the origin. */
#define CAM_START_X   0.0f
#define CAM_START_Y  12.0f
#define CAM_START_Z  50.0f

/* Movement speed — faster than Lesson 07 since the model is much larger. */
#define MOVE_SPEED    8.0f

/* Mouse sensitivity: radians per pixel. */
#define MOUSE_SENSITIVITY 0.002f

/* Pitch clamp to prevent flipping (same as Lesson 07). */
#define MAX_PITCH_DEG  89.0f

/* Perspective projection. */
#define FOV_DEG    60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE  500.0f

/* Time conversion and delta time clamping. */
#define MS_TO_SEC      1000.0f
#define MAX_DELTA_TIME 0.1f

/* Model rotation speed (radians per second around Y axis). */
#define MODEL_ROTATION_SPEED 0.3f

/* Initial rotation so the shuttle presents its front face to the starting
 * camera behind it.  Without this the first view is straight-on engines. */
#define MODEL_INITIAL_ROTATION (FORGE_PI * 1.15f)

/* ── Uniform data ─────────────────────────────────────────────────────────── */

typedef struct Uniforms {
    mat4 mvp;
} Uniforms;

/* ── Application state ───────────────────────────────────────────────────── */

typedef struct app_state {
    /* GPU resources */
    SDL_Window              *window;
    SDL_GPUDevice           *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer           *vertex_buffer;
    SDL_GPUTexture          *depth_texture;
    SDL_GPUTexture          *diffuse_texture;
    SDL_GPUSampler          *sampler;
    Uint32                   depth_width;
    Uint32                   depth_height;

    /* Mesh data */
    Uint32                   mesh_vertex_count;

    /* Camera state (same pattern as Lesson 07) */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Timing */
    Uint64 last_ticks;
    float  elapsed;

    /* Input */
    bool mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;

/* ── Depth texture helper ────────────────────────────────────────────────── */
/* Same as Lesson 06/07 — creates a depth texture matching the window size. */

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
/* Same as Lesson 07 — creates a shader from SPIRV or DXIL bytecodes. */

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

/* ── Texture loading helper ──────────────────────────────────────────────── */
/* Load a PNG file, convert to RGBA, upload to GPU with mipmaps.
 * Combines patterns from Lesson 04 (texture loading) and Lesson 05 (mipmaps). */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    /* ── 1. Load the image with SDL_LoadSurface ──────────────────────
     * SDL_LoadSurface supports BMP and PNG (not JPG — that's why we
     * converted the texture to PNG during asset preparation). */
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }
    SDL_Log("Loaded texture: %dx%d, format=%s",
            surface->w, surface->h,
            SDL_GetPixelFormatName(surface->format));

    /* ── 2. Convert to ABGR8888 (SDL's name for R8G8B8A8 in memory) ──
     * The GPU texture format R8G8B8A8_UNORM_SRGB expects bytes in
     * R, G, B, A order in memory.  SDL calls this ABGR8888 because
     * SDL names packed formats by their bit order from MSB to LSB,
     * while GPU formats name bytes in memory order.
     * See MEMORY.md: GPU R8G8B8A8 = SDL ABGR8888. */
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert surface to ABGR8888: %s", SDL_GetError());
        return NULL;
    }

    int tex_w = converted->w;
    int tex_h = converted->h;
    int num_levels = (int)forge_log2f((float)(tex_w > tex_h ? tex_w : tex_h)) + 1;

    SDL_Log("Creating %dx%d GPU texture with %d mip levels", tex_w, tex_h, num_levels);

    /* ── 3. Create GPU texture with mip levels ───────────────────────
     * SAMPLER — we'll sample this in the fragment shader.
     * COLOR_TARGET — required for SDL_GenerateMipmapsForGPUTexture. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width                = (Uint32)tex_w;
    tex_info.height               = (Uint32)tex_h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = num_levels;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    /* ── 4. Upload pixel data to GPU ─────────────────────────────────── */
    Uint32 total_bytes = (Uint32)(tex_w * tex_h * BYTES_PER_PIXEL);

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(converted);
        return NULL;
    }
    /* Copy row-by-row to respect SDL_Surface.pitch — the surface may
     * have padding bytes at the end of each row for alignment.  The GPU
     * transfer buffer is tightly packed (dest stride = width * bpp). */
    Uint32 dest_row_bytes = (Uint32)(tex_w * BYTES_PER_PIXEL);
    const Uint8 *src = (const Uint8 *)converted->pixels;
    Uint8 *dst = (Uint8 *)mapped;
    for (Uint32 row = 0; row < (Uint32)tex_h; row++) {
        SDL_memcpy(dst + row * dest_row_bytes,
                   src + row * converted->pitch,
                   dest_row_bytes);
    }
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(converted);

    /* ── 5. Copy pass → upload base level → generate mipmaps ──────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer for texture upload: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass for texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = transfer;
    tex_src.offset          = 0;
    tex_src.pixels_per_row  = (Uint32)tex_w;
    tex_src.rows_per_layer  = (Uint32)tex_h;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture   = texture;
    tex_dst.mip_level = 0;
    tex_dst.w         = (Uint32)tex_w;
    tex_dst.h         = (Uint32)tex_h;
    tex_dst.d         = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    /* Generate mipmaps — the GPU downsamples level 0 into all smaller levels.
     * This must be called outside any render or copy pass (Lesson 05 pattern). */
    SDL_GenerateMipmapsForGPUTexture(cmd, texture);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    return texture;
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

    /* ── 6. Load OBJ mesh ─────────────────────────────────────────────── */
    /* NEW: Load geometry from a file instead of hard-coding it.
     * forge_obj_load parses the OBJ file and returns a flat array of
     * de-indexed vertices — every 3 consecutive vertices form a triangle.
     *
     * "De-indexed" means we expand the separate OBJ index streams
     * (position/UV/normal) into one vertex per corner.  This wastes some
     * memory but lets us render with a simple DrawPrimitives call. */
    const char *base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    char obj_path[PATH_BUFFER_SIZE];
    char tex_path[PATH_BUFFER_SIZE];
    SDL_snprintf(obj_path, PATH_BUFFER_SIZE, "%s%s", base_path, MODEL_PATH);
    SDL_snprintf(tex_path, PATH_BUFFER_SIZE, "%s%s", base_path, TEXTURE_PATH);

    ForgeObjMesh mesh;
    if (!forge_obj_load(obj_path, &mesh)) {
        SDL_Log("Failed to load OBJ model");
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 7. Upload mesh to vertex buffer ──────────────────────────────── */
    /* Same transfer buffer pattern as every previous lesson. */
    Uint32 vertex_data_size = mesh.vertex_count * (Uint32)sizeof(ForgeObjVertex);

    SDL_GPUBufferCreateInfo vbuf_info;
    SDL_zero(vbuf_info);
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbuf_info.size  = vertex_data_size;

    SDL_GPUBuffer *vertex_buffer = SDL_CreateGPUBuffer(device, &vbuf_info);
    if (!vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        forge_obj_free(&mesh);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = vertex_data_size;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        device, &xfer_info);
    if (!transfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        forge_obj_free(&mesh);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
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
        forge_obj_free(&mesh);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_memcpy(mapped, mesh.vertices, vertex_data_size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    Uint32 mesh_vertex_count = mesh.vertex_count;
    forge_obj_free(&mesh);  /* CPU-side data no longer needed */

    SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!upload_cmd) {
        SDL_Log("Failed to acquire command buffer for upload: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
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
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
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
    SDL_EndGPUCopyPass(copy_pass);

    if (!SDL_SubmitGPUCommandBuffer(upload_cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    /* ── 8. Load diffuse texture with mipmaps ─────────────────────────── */
    /* Combines Lesson 04 (loading from file) and Lesson 05 (mipmaps). */
    SDL_GPUTexture *diffuse_texture = load_texture(device, tex_path);
    if (!diffuse_texture) {
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 9. Create sampler ────────────────────────────────────────────── */
    /* Trilinear filtering with REPEAT address mode — the best general-
     * purpose sampler for textured meshes (Lesson 05 explains why). */
    SDL_GPUSamplerCreateInfo smp_info;
    SDL_zero(smp_info);
    smp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
    smp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
    smp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    smp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    smp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    smp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    smp_info.min_lod        = 0.0f;
    smp_info.max_lod        = MAX_LOD_UNLIMITED;

    SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &smp_info);
    if (!sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, diffuse_texture);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 10. Create shaders ───────────────────────────────────────────── */
    SDL_GPUShader *vertex_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        mesh_vert_spirv, mesh_vert_spirv_size,
        mesh_vert_dxil,  mesh_vert_dxil_size,
        VERT_NUM_SAMPLERS,
        VERT_NUM_STORAGE_TEXTURES,
        VERT_NUM_STORAGE_BUFFERS,
        VERT_NUM_UNIFORM_BUFFERS);
    if (!vertex_shader) {
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, diffuse_texture);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader *fragment_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        mesh_frag_spirv, mesh_frag_spirv_size,
        mesh_frag_dxil,  mesh_frag_dxil_size,
        FRAG_NUM_SAMPLERS,
        FRAG_NUM_STORAGE_TEXTURES,
        FRAG_NUM_STORAGE_BUFFERS,
        FRAG_NUM_UNIFORM_BUFFERS);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, diffuse_texture);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 11. Create graphics pipeline ─────────────────────────────────── */
    /* Three vertex attributes (position + normal + UV) instead of two.
     * Otherwise same pipeline setup as Lesson 07: depth test + back-face cull. */

    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(ForgeObjVertex);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attributes[NUM_VERTEX_ATTRIBUTES];
    SDL_zero(vertex_attributes);

    /* Location 0: position (float3) — maps to HLSL TEXCOORD0 */
    vertex_attributes[0].location    = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset      = offsetof(ForgeObjVertex, position);

    /* Location 1: normal (float3) — maps to HLSL TEXCOORD1 */
    vertex_attributes[1].location    = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[1].offset      = offsetof(ForgeObjVertex, normal);

    /* Location 2: uv (float2) — maps to HLSL TEXCOORD2 */
    vertex_attributes[2].location    = 2;
    vertex_attributes[2].buffer_slot = 0;
    vertex_attributes[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[2].offset      = offsetof(ForgeObjVertex, uv);

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);

    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers          = 1;
    pipeline_info.vertex_input_state.vertex_attributes           = vertex_attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes       = NUM_VERTEX_ATTRIBUTES;

    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Back-face culling — same as Lesson 06/07. */
    pipeline_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth testing — same as Lesson 06/07. */
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
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, diffuse_texture);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Shaders can be released after pipeline creation. */
    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUShader(device, vertex_shader);

    /* ── 12. Store state ──────────────────────────────────────────────── */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUSampler(device, sampler);
        SDL_ReleaseGPUTexture(device, diffuse_texture);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTexture(device, depth_texture);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window            = window;
    state->device            = device;
    state->pipeline          = pipeline;
    state->vertex_buffer     = vertex_buffer;
    state->depth_texture     = depth_texture;
    state->diffuse_texture   = diffuse_texture;
    state->sampler           = sampler;
    state->depth_width       = (Uint32)win_w;
    state->depth_height      = (Uint32)win_h;
    state->mesh_vertex_count = mesh_vertex_count;

    /* Initialize camera (same pattern as Lesson 07). */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = 0.0f;
    state->cam_pitch    = 0.0f;
    state->last_ticks   = SDL_GetTicks();
    state->elapsed      = 0.0f;

    /* Capture mouse for FPS-style look. */
#ifndef FORGE_CAPTURE
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    } else {
        state->mouse_captured = true;
    }
#else
    state->mouse_captured = false;
#endif

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            SDL_ReleaseGPUSampler(device, sampler);
            SDL_ReleaseGPUTexture(device, diffuse_texture);
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

    SDL_Log("Mesh loaded: %u vertices", mesh_vertex_count);
    SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────────── */
/* Same mouse/keyboard handling as Lesson 07. */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    /* Escape: release mouse or quit. */
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        if (state->mouse_captured) {
            if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
                SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                        SDL_GetError());
            } else {
                state->mouse_captured = false;
            }
        } else {
            return SDL_APP_SUCCESS;
        }
    }

    /* Click to recapture mouse. */
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
        if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
            SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                    SDL_GetError());
        } else {
            state->mouse_captured = true;
        }
    }

    /* Mouse motion: update camera yaw and pitch. */
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->mouse_captured) {
        state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
        state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;

        float max_pitch = MAX_PITCH_DEG * FORGE_DEG2RAD;
        if (state->cam_pitch >  max_pitch) state->cam_pitch =  max_pitch;
        if (state->cam_pitch < -max_pitch) state->cam_pitch = -max_pitch;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────────── */
/* Each frame:
 *   1. Compute delta time
 *   2. Process keyboard input for camera movement (Lesson 07 pattern)
 *   3. Build MVP matrix with gentle model rotation
 *   4. Handle window resize
 *   5. Draw the mesh */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── 1. Compute delta time ────────────────────────────────────────── */
    Uint64 now_ms = SDL_GetTicks();
    float dt = (float)(now_ms - state->last_ticks) / MS_TO_SEC;
    state->last_ticks = now_ms;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;
    state->elapsed += dt;

    /* ── 2. Process keyboard input (same as Lesson 07) ────────────────── */
    quat cam_orientation = quat_from_euler(
        state->cam_yaw, state->cam_pitch, 0.0f);

    vec3 forward = quat_forward(cam_orientation);
    vec3 right   = quat_right(cam_orientation);

    const bool *keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(forward, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(forward, -MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(right, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_scale(right, -MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_SPACE]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_create(0.0f, MOVE_SPEED * dt, 0.0f));
    }
    if (keys[SDL_SCANCODE_LSHIFT]) {
        state->cam_position = vec3_add(state->cam_position,
            vec3_create(0.0f, -MOVE_SPEED * dt, 0.0f));
    }

    /* ── 3. Build MVP matrix ──────────────────────────────────────────── */
    mat4 view = mat4_view_from_quat(state->cam_position, cam_orientation);

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    float fov    = FOV_DEG * FORGE_DEG2RAD;
    mat4 proj    = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);

    mat4 vp = mat4_multiply(proj, view);

    /* Model transform: start at an initial rotation so the shuttle presents
     * a 3/4 front view, then spin slowly so the learner sees every angle. */
    mat4 model = mat4_rotate_y(MODEL_INITIAL_ROTATION
                               + state->elapsed * MODEL_ROTATION_SPEED);

    mat4 mvp = mat4_multiply(vp, model);

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

    /* ── 6. Push MVP uniform ──────────────────────────────────────────── */
    Uniforms uniforms;
    uniforms.mvp = mvp;
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* ── 7. Acquire swapchain & begin render pass ─────────────────────── */
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

        /* Bind vertex buffer. */
        SDL_GPUBufferBinding vertex_binding;
        SDL_zero(vertex_binding);
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

        /* Bind diffuse texture + sampler. */
        SDL_GPUTextureSamplerBinding tex_sampler_binding;
        SDL_zero(tex_sampler_binding);
        tex_sampler_binding.texture = state->diffuse_texture;
        tex_sampler_binding.sampler = state->sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_sampler_binding, 1);

        /* Draw the entire mesh — non-indexed, since we de-indexed
         * the OBJ data into a flat vertex array. */
        SDL_DrawGPUPrimitives(pass, state->mesh_vertex_count, 1, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            /* Capture submitted the command buffer internally. */
            if (forge_capture_should_quit(&state->capture)) {
                return SDL_APP_SUCCESS;
            }
            return SDL_APP_CONTINUE;
        }
        /* No capture this frame — fall through to normal submit below. */
    }
#endif
    /* NOTE: Submit consumes the command buffer whether it succeeds or fails.
     * Do NOT call SDL_CancelGPUCommandBuffer after a failed submit — the
     * buffer is already gone.  Cancel is only valid on a buffer that was
     * never submitted (e.g. when an earlier step like BeginRenderPass fails
     * and you need to abandon the whole frame). */
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
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
        SDL_ReleaseGPUSampler(state->device, state->sampler);
        SDL_ReleaseGPUTexture(state->device, state->diffuse_texture);
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        SDL_ReleaseGPUBuffer(state->device, state->vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->pipeline);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
