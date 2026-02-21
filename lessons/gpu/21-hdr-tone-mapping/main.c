/*
 * Lesson 21 — HDR & Tone Mapping
 *
 * Why this lesson exists:
 *   Every lesson before this rendered directly to an 8-bit (UNORM) swapchain.
 *   Those formats store colors in [0, 1] — any lighting result above 1.0 is
 *   clamped to white, which loses all highlight detail.  In real scenes, light
 *   intensities vary enormously (sunlight on metal vs. shadow under a tree).
 *   Capturing that range requires a floating-point render target.
 *
 * What this lesson teaches:
 *   1. Creating a floating-point render target (R16G16B16A16_FLOAT)
 *   2. Why LDR clamping destroys highlight information
 *   3. Two-pass rendering: scene → HDR buffer → tone-mapped swapchain
 *   4. Tone mapping operators: Reinhard and ACES
 *   5. Exposure control as a pre-tone-mapping brightness multiplier
 *   6. The fullscreen blit pass pattern (SV_VertexID, no vertex buffer)
 *   7. Gamma correction via the sRGB swapchain (SDR_LINEAR)
 *   8. Cascaded shadow maps integrated into the HDR pipeline
 *
 * Scene:
 *   CesiumMilkTruck + BoxTextured models on a procedural grid floor,
 *   lit with a bright directional light (intensity > 1.0) that creates
 *   HDR specular highlights.  Cascaded shadow maps add directional
 *   shadows with 3x3 PCF soft edges.
 *
 * Render passes (per frame):
 *   1. Shadow passes (3 cascades) — depth-only from light's perspective
 *   2. Scene pass → HDR buffer — lit geometry with shadow receiving
 *   3. Tone map pass → swapchain — compress HDR to displayable range
 *
 * Controls:
 *   WASD / Space / LShift — Move camera
 *   Mouse                 — Look around
 *   1                     — No tone mapping (clamp)
 *   2                     — Reinhard tone mapping
 *   3                     — ACES filmic tone mapping
 *   =/+                   — Increase exposure
 *   -                     — Decrease exposure
 *   Escape                — Release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include "gltf/forge_gltf.h"
#include "math/forge_math.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecodes ────────────────────────────────────────────── */

/* Shadow pass — depth-only rendering from the light's perspective */
#include "shaders/shadow_frag_dxil.h"
#include "shaders/shadow_frag_spirv.h"
#include "shaders/shadow_vert_dxil.h"
#include "shaders/shadow_vert_spirv.h"

/* Scene shaders — Blinn-Phong + cascaded shadows → HDR */
#include "shaders/scene_frag_dxil.h"
#include "shaders/scene_frag_spirv.h"
#include "shaders/scene_vert_dxil.h"
#include "shaders/scene_vert_spirv.h"

/* Grid shaders — procedural grid + shadows → HDR */
#include "shaders/grid_frag_dxil.h"
#include "shaders/grid_frag_spirv.h"
#include "shaders/grid_vert_dxil.h"
#include "shaders/grid_vert_spirv.h"

/* Tone mapping — fullscreen quad, HDR → swapchain */
#include "shaders/tonemap_frag_dxil.h"
#include "shaders/tonemap_frag_spirv.h"
#include "shaders/tonemap_vert_dxil.h"
#include "shaders/tonemap_vert_spirv.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Window dimensions (16:9 standard for consistent screenshots). */
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

/* Camera parameters. */
#define FOV_DEG 60
#define NEAR_PLANE 0.1f
#define FAR_PLANE 100.0f
#define CAM_SPEED 5.0f
#define MOUSE_SENS 0.003f
#define PITCH_CLAMP 1.5f /* ~86 degrees — prevents camera from flipping */

/* Light — bright enough to push specular highlights past 1.0.
 * At intensity 3.0, a specular peak (specular_str * intensity) reaches 3.0,
 * clearly demonstrating why tone mapping is necessary. */
#define LIGHT_DIR_X 1.0f
#define LIGHT_DIR_Y 1.0f
#define LIGHT_DIR_Z 0.5f
#define LIGHT_INTENSITY 3.0f

/* Scene material defaults. */
#define MATERIAL_SHININESS 64.0f
#define MATERIAL_AMBIENT 0.1f
#define MATERIAL_SPECULAR_STR 1.0f
#define MAX_ANISOTROPY 4 /* diffuse sampler anisotropic filtering level */

/* Box layout — ring of boxes around the truck. */
#define BOX_GROUND_COUNT 8
#define BOX_STACK_COUNT 4
#define BOX_RING_RADIUS 5.0f
#define BOX_GROUND_Y 0.5f  /* center Y — box bottom sits at Y=0 */
#define BOX_STACK_Y 1.5f   /* center Y — stacked box bottom at Y=1 */
#define BOX_STACK_ROTATION_OFFSET 0.5f /* radians offset from base box */
#define TOTAL_BOX_COUNT (BOX_GROUND_COUNT + BOX_STACK_COUNT)

/* HDR render target format.
 * 16-bit float per channel gives sufficient precision for HDR values
 * while using half the memory of R32G32B32A32_FLOAT. */
#define HDR_FORMAT SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT

/* Exposure control. */
#define DEFAULT_EXPOSURE 1.0f
#define EXPOSURE_STEP 0.1f
#define MIN_EXPOSURE 0.1f
#define MAX_EXPOSURE 10.0f

/* Tone mapping modes (matching shader constants). */
#define TONEMAP_NONE 0
#define TONEMAP_REINHARD 1
#define TONEMAP_ACES 2

/* Camera initial position and orientation (looking down at the truck). */
#define CAM_START_X -6.1f
#define CAM_START_Y 7.0f
#define CAM_START_Z 4.4f
#define CAM_START_YAW_DEG -50.0f
#define CAM_START_PITCH_DEG -50.0f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f /* 100 ms cap prevents huge jumps after hitches */

/* Fullscreen quad — two triangles, no vertex buffer (SV_VertexID). */
#define FULLSCREEN_QUAD_VERTS 6

/* Grid geometry — 4-vertex quad, drawn with 6 indices (2 triangles). */
#define GRID_INDEX_COUNT 6

/* Grid appearance. */
#define GRID_HALF_SIZE 50.0f
#define GRID_SPACING 1.0f
#define GRID_LINE_WIDTH 0.02f
#define GRID_FADE_DISTANCE 40.0f
#define GRID_AMBIENT 0.15f
#define GRID_SHININESS 32.0f
#define GRID_SPECULAR_STR 0.5f

/* HDR clear color — forge-gpu dark theme background (#1a1a2e in linear). */
#define CLEAR_COLOR_R 0.008f
#define CLEAR_COLOR_G 0.008f
#define CLEAR_COLOR_B 0.026f
#define CLEAR_COLOR_A 1.0f

/* Grid line color — blue accent matching the forge-gpu brand. */
#define GRID_LINE_COLOR_R 0.15f
#define GRID_LINE_COLOR_G 0.55f
#define GRID_LINE_COLOR_B 0.85f
#define GRID_LINE_COLOR_A 1.0f

/* Grid background color — dark blue floor. */
#define GRID_BG_COLOR_R 0.04f
#define GRID_BG_COLOR_G 0.04f
#define GRID_BG_COLOR_B 0.08f
#define GRID_BG_COLOR_A 1.0f

/* Model asset paths (copied from shared assets/ at build time). */
#define TRUCK_MODEL_PATH "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH "assets/models/BoxTextured/BoxTextured.gltf"

/* ── Shadow map constants ─────────────────────────────────────────────────── */

#define NUM_CASCADES 3
#define SHADOW_MAP_SIZE 2048
#define SHADOW_MAP_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_TEXEL_SIZE (1.0f / (float)SHADOW_MAP_SIZE)
#define SHADOW_BIAS 0.0053f
#define SHADOW_DEPTH_BIAS 20.5f
#define SHADOW_SLOPE_BIAS 20.5f

/* A frustum has 8 corners: 4 on the near plane, 4 on the far plane. */
#define NUM_FRUSTUM_CORNERS 8
#define NUM_NEAR_CORNERS 4

/* Lambda controls the logarithmic vs linear blend for cascade splits.
 * 0.0 = purely linear, 1.0 = purely logarithmic.
 * 0.5 is a good practical balance (Lengyel's recommendation). */
#define CASCADE_LAMBDA 0.5f

/* Light VP computation: how far back to place the light from cascade center,
 * and extra Z range to capture shadow casters behind the frustum slice. */
#define LIGHT_DISTANCE 50.0f
#define SHADOW_Z_PADDING 50.0f

/* Sentinel values for AABB initialization. */
#define AABB_INIT_MIN 1e30f
#define AABB_INIT_MAX -1e30f

/* ── Uniform structures ──────────────────────────────────────────────────── */

/* Shadow vertex: just the light's MVP (64 bytes). */
typedef struct ShadowVertUniforms {
  mat4 light_mvp;
} ShadowVertUniforms;

/* Scene vertex uniforms — pushed per draw call (per node). */
typedef struct SceneVertUniforms {
  mat4 mvp;          /* model-view-projection matrix (64 bytes) */
  mat4 model;        /* model (world) matrix         (64 bytes) */
} SceneVertUniforms; /* 128 bytes */

/* Light VP matrices for all 3 cascades (192 bytes). */
typedef struct ShadowMatrices {
  mat4 light_vp[NUM_CASCADES];
} ShadowMatrices;

/* Scene fragment uniforms — pushed per draw call (per material).
 * Now includes cascade_splits and shadow parameters for CSM. */
typedef struct SceneFragUniforms {
  float base_color[4];     /* material RGBA                 (16 bytes) */
  float light_dir[4];      /* world-space light dir, xyz     (16 bytes) */
  float eye_pos[4];        /* camera position, xyz           (16 bytes) */
  float cascade_splits[4]; /* cascade far distances (x,y,z)  (16 bytes) */
  Uint32 has_texture;      /* non-zero = sample texture       (4 bytes) */
  float shininess;         /* specular exponent               (4 bytes) */
  float ambient;           /* ambient intensity               (4 bytes) */
  float specular_str;      /* specular strength               (4 bytes) */
  float light_intensity;   /* brightness multiplier (HDR)     (4 bytes) */
  float shadow_texel_size; /* 1.0 / shadow_map_resolution     (4 bytes) */
  float shadow_bias;       /* depth bias for PCF              (4 bytes) */
  float _pad;              /* pad to 96 bytes                 (4 bytes) */
} SceneFragUniforms;       /* 96 bytes */

/* Grid vertex uniforms — one VP matrix. */
typedef struct GridVertUniforms {
  mat4 vp;          /* view-projection matrix (64 bytes) */
} GridVertUniforms; /* 64 bytes */

/* Grid fragment uniforms — now includes cascade_splits and shadow params. */
typedef struct GridFragUniforms {
  float line_color[4];     /* grid line color        (16 bytes) */
  float bg_color[4];       /* background color       (16 bytes) */
  float light_dir[4];      /* light direction        (16 bytes) */
  float eye_pos[4];        /* camera position        (16 bytes) */
  float cascade_splits[4]; /* cascade far distances  (16 bytes) */
  float grid_spacing;      /* world-space distance between grid lines (4 bytes) */
  float line_width;        /* grid line thickness in world units     (4 bytes) */
  float fade_distance;     /* distance at which grid fades to zero   (4 bytes) */
  float ambient;           /* global ambient light term              (4 bytes) */
  float shininess;         /* specular exponent (highlight tightness)(4 bytes) */
  float specular_str;      /* specular highlight strength multiplier (4 bytes) */
  float light_intensity;   /* directional light brightness (HDR)     (4 bytes) */
  float shadow_texel_size; /* 1/shadow_map_resolution for PCF offsets(4 bytes) */
  float shadow_bias;       /* depth bias to prevent shadow acne      (4 bytes) */
  float _pad[3];           /* pad to 128 bytes (std140 alignment)   (12 bytes) */
} GridFragUniforms;        /* 128 bytes */

/* Tone map fragment uniforms. */
typedef struct TonemapFragUniforms {
  float exposure;      /* exposure multiplier     (4 bytes) */
  Uint32 tonemap_mode; /* 0=clamp, 1=Reinh, 2=AC (4 bytes) */
  float _pad[2];       /* pad to 16 bytes         (8 bytes) */
} TonemapFragUniforms; /* 16 bytes */

/* ── GPU-side model types ─────────────────────────────────────────────────── */

/* One drawable primitive from a glTF mesh. */
typedef struct GpuPrimitive {
  SDL_GPUBuffer *vertex_buffer;
  SDL_GPUBuffer *index_buffer;
  Uint32 index_count;
  int material_index;
  SDL_GPUIndexElementSize index_type;
  bool has_uvs;
} GpuPrimitive;

/* Uploaded material data. */
typedef struct GpuMaterial {
  float base_color[4];
  SDL_GPUTexture *texture;
  bool has_texture;
} GpuMaterial;

/* A fully loaded glTF model ready for rendering. */
typedef struct ModelData {
  ForgeGltfScene scene;
  GpuPrimitive *primitives;
  int primitive_count;
  GpuMaterial *materials;
  int material_count;
} ModelData;

/* Box placement — position + Y rotation for each box in the ring. */
typedef struct BoxPlacement {
  vec3 position;
  float y_rotation;
} BoxPlacement;

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct app_state {
  SDL_Window *window;
  SDL_GPUDevice *device;

  /* Four pipelines:
   *   shadow_pipeline  — depth-only from light's perspective
   *   scene_pipeline   — lit geometry with shadows → HDR render target
   *   grid_pipeline    — procedural grid with shadows → HDR render target
   *   tonemap_pipeline — fullscreen quad, HDR → swapchain */
  SDL_GPUGraphicsPipeline *shadow_pipeline;
  SDL_GPUGraphicsPipeline *scene_pipeline;
  SDL_GPUGraphicsPipeline *grid_pipeline;
  SDL_GPUGraphicsPipeline *tonemap_pipeline;

  /* HDR render target — R16G16B16A16_FLOAT, both COLOR_TARGET and SAMPLER.
   * COLOR_TARGET lets us render to it; SAMPLER lets the tone map pass
   * read from it.  Recreated on window resize. */
  SDL_GPUTexture *hdr_target;
  SDL_GPUSampler *hdr_sampler;
  Uint32 hdr_width;
  Uint32 hdr_height;

  /* Depth buffer for the scene pass (D32_FLOAT). */
  SDL_GPUTexture *depth_texture;
  Uint32 depth_width;
  Uint32 depth_height;

  /* Shadow map textures — one per cascade. */
  SDL_GPUTexture *shadow_maps[NUM_CASCADES];
  SDL_GPUSampler *shadow_sampler;

  /* Grid geometry (flat quad on the XZ plane). */
  SDL_GPUBuffer *grid_vertex_buffer;
  SDL_GPUBuffer *grid_index_buffer;

  /* Scene textures and sampler. */
  SDL_GPUTexture *white_texture; /* 1x1 white fallback for untextured mats */
  SDL_GPUSampler *sampler;       /* LINEAR / REPEAT for diffuse textures   */

  /* Models. */
  ModelData truck;
  ModelData box;
  BoxPlacement box_placements[TOTAL_BOX_COUNT];
  int box_count;

  /* Camera. */
  vec3 cam_position;
  float cam_yaw;
  float cam_pitch;

  /* HDR settings — switchable at runtime. */
  float exposure;
  Uint32 tonemap_mode;

  /* Timing and input. */
  Uint64 last_ticks;
  bool mouse_captured;

#ifdef FORGE_CAPTURE
  ForgeCapture capture;
#endif
} app_state;

/* ── Helper: create HDR render target ─────────────────────────────────────── */

/* Creates a floating-point color texture for rendering HDR scene data.
 *
 * The texture needs two usage flags:
 *   COLOR_TARGET — so we can render to it in the scene pass
 *   SAMPLER      — so the tone map pass can sample from it
 *
 * R16G16B16A16_FLOAT provides 16-bit half-precision per channel.
 * This is the standard HDR format used in most real-time renderers:
 * enough precision for lighting (values 0–65504) at half the bandwidth
 * of 32-bit float. */
static SDL_GPUTexture *create_hdr_target(SDL_GPUDevice *device, Uint32 w, Uint32 h) {
  SDL_GPUTextureCreateInfo info;
  SDL_zero(info);
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = HDR_FORMAT;
  info.width = w;
  info.height = h;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &info);
  if (!tex) {
    SDL_Log("Failed to create HDR render target: %s", SDL_GetError());
  }
  return tex;
}

/* ── Helper: create depth texture ─────────────────────────────────────────── */

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device, Uint32 w, Uint32 h) {
  SDL_GPUTextureCreateInfo info;
  SDL_zero(info);
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  info.width = w;
  info.height = h;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

  SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &info);
  if (!tex) {
    SDL_Log("Failed to create depth texture: %s", SDL_GetError());
  }
  return tex;
}

/* ── Helper: create shadow map texture ────────────────────────────────────── */

/* Shadow maps need DEPTH_STENCIL_TARGET (for writing during shadow pass)
 * AND SAMPLER (for reading in the scene/grid passes).  This combination
 * is what distinguishes a shadow map from a normal depth buffer. */
static SDL_GPUTexture *create_shadow_map(SDL_GPUDevice *device) {
  SDL_GPUTextureCreateInfo info;
  SDL_zero(info);
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SHADOW_MAP_FORMAT;
  info.width = SHADOW_MAP_SIZE;
  info.height = SHADOW_MAP_SIZE;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &info);
  if (!tex) {
    SDL_Log(
        "Failed to create shadow map (%dx%d): %s",
        SHADOW_MAP_SIZE,
        SHADOW_MAP_SIZE,
        SDL_GetError()
    );
  }
  return tex;
}

/* ── Helper: create shader (SPIRV or DXIL) ────────────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code,
    size_t spirv_size,
    const Uint8 *dxil_code,
    size_t dxil_size,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers
) {
  SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

  SDL_GPUShaderCreateInfo info;
  SDL_zero(info);
  info.stage = stage;
  info.entrypoint = "main";
  info.num_samplers = num_samplers;
  info.num_uniform_buffers = num_uniform_buffers;

  if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.code = spirv_code;
    info.code_size = spirv_size;
  } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
    info.format = SDL_GPU_SHADERFORMAT_DXIL;
    info.code = dxil_code;
    info.code_size = dxil_size;
  } else {
    SDL_Log("No supported shader format available");
    return NULL;
  }

  SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
  if (!shader) {
    SDL_Log("Failed to create shader: %s", SDL_GetError());
  }
  return shader;
}

/* ── Helper: upload buffer data ───────────────────────────────────────────── */

/* Standard transfer-buffer upload: create → map → memcpy → unmap →
 * copy pass → release transfer buffer. */
static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage, const void *data, Uint32 size
) {
  /* Create the GPU buffer. */
  SDL_GPUBufferCreateInfo buf_info;
  SDL_zero(buf_info);
  buf_info.usage = usage;
  buf_info.size = size;

  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
  if (!buffer) {
    SDL_Log("Failed to create GPU buffer: %s", SDL_GetError());
    return NULL;
  }

  /* Create a transfer buffer to stage the data. */
  SDL_GPUTransferBufferCreateInfo xfer_info;
  SDL_zero(xfer_info);
  xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xfer_info.size = size;

  SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
  if (!xfer) {
    SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  /* Map, copy, unmap. */
  void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
  if (!mapped) {
    SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }
  SDL_memcpy(mapped, data, size);
  SDL_UnmapGPUTransferBuffer(device, xfer);

  /* Copy pass: transfer buffer → GPU buffer. */
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  if (!cmd) {
    SDL_Log("Failed to acquire command buffer for upload: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
  if (!copy) {
    SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
    SDL_CancelGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  SDL_GPUTransferBufferLocation src;
  SDL_zero(src);
  src.transfer_buffer = xfer;

  SDL_GPUBufferRegion dst;
  SDL_zero(dst);
  dst.buffer = buffer;
  dst.size = size;

  SDL_UploadToGPUBuffer(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("Failed to submit upload command buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  SDL_ReleaseGPUTransferBuffer(device, xfer);
  return buffer;
}

/* ── Helper: load texture from file ───────────────────────────────────────── */

/* Bytes per pixel for RGBA textures. */
#define BYTES_PER_PIXEL 4

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path) {
  SDL_Surface *surface = SDL_LoadSurface(path);
  if (!surface) {
    SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
    return NULL;
  }

  /* Convert to ABGR8888 (R8G8B8A8 in memory). */
  SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
  SDL_DestroySurface(surface);
  if (!converted) {
    SDL_Log("Failed to convert surface: %s", SDL_GetError());
    return NULL;
  }

  Uint32 w = (Uint32)converted->w;
  Uint32 h = (Uint32)converted->h;
  Uint32 max_dim = w > h ? w : h;
  Uint32 mip_levels = (Uint32)(forge_log2f((float)max_dim)) + 1;

  SDL_GPUTextureCreateInfo tex_info;
  SDL_zero(tex_info);
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  tex_info.width = w;
  tex_info.height = h;
  tex_info.layer_count_or_depth = 1;
  tex_info.num_levels = mip_levels;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
  if (!tex) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
    SDL_DestroySurface(converted);
    return NULL;
  }

  /* Upload pixel data via transfer buffer (row-by-row for pitch safety). */
  Uint32 dest_row_bytes = w * BYTES_PER_PIXEL;
  Uint32 total_bytes = w * h * BYTES_PER_PIXEL;

  SDL_GPUTransferBufferCreateInfo xfer_info;
  SDL_zero(xfer_info);
  xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xfer_info.size = total_bytes;

  SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
  if (!xfer) {
    SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTexture(device, tex);
    SDL_DestroySurface(converted);
    return NULL;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
  if (!mapped) {
    SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    SDL_DestroySurface(converted);
    return NULL;
  }

  /* Copy row-by-row to handle surface pitch vs texture row stride. */
  {
    const Uint8 *row_src = (const Uint8 *)converted->pixels;
    Uint8 *row_dst = (Uint8 *)mapped;
    Uint32 row;
    for (row = 0; row < h; row++) {
      SDL_memcpy(row_dst + row * dest_row_bytes, row_src + row * converted->pitch, dest_row_bytes);
    }
  }
  SDL_UnmapGPUTransferBuffer(device, xfer);
  SDL_DestroySurface(converted);

  /* Copy pass: transfer → texture, then generate mipmaps. */
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  if (!cmd) {
    SDL_Log("Failed to acquire command buffer for texture upload: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
  if (!copy) {
    SDL_Log("Failed to begin texture copy pass: %s", SDL_GetError());
    SDL_CancelGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  SDL_GPUTextureTransferInfo src;
  SDL_zero(src);
  src.transfer_buffer = xfer;

  SDL_GPUTextureRegion dst;
  SDL_zero(dst);
  dst.texture = tex;
  dst.w = w;
  dst.h = h;
  dst.d = 1;

  SDL_UploadToGPUTexture(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);

  SDL_GenerateMipmapsForGPUTexture(cmd, tex);

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("Failed to submit texture upload: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  SDL_ReleaseGPUTransferBuffer(device, xfer);
  return tex;
}

/* ── Helper: 1x1 white texture ────────────────────────────────────────────── */

/* Fallback texture for materials without a diffuse map.  Sampling this
 * returns (1, 1, 1, 1), so the material's base_color shows through. */
static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device) {
  SDL_GPUTextureCreateInfo tex_info;
  SDL_zero(tex_info);
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  tex_info.width = 1;
  tex_info.height = 1;
  tex_info.layer_count_or_depth = 1;
  tex_info.num_levels = 1;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
  if (!tex) {
    SDL_Log("Failed to create white texture: %s", SDL_GetError());
    return NULL;
  }

  /* Upload a single white pixel. */
  Uint8 white[4] = { 255, 255, 255, 255 };

  SDL_GPUTransferBufferCreateInfo xfer_info;
  SDL_zero(xfer_info);
  xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xfer_info.size = sizeof(white);

  SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
  if (!xfer) {
    SDL_Log("Failed to create white texture transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
  if (!mapped) {
    SDL_Log("Failed to map white texture transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }
  SDL_memcpy(mapped, white, sizeof(white));
  SDL_UnmapGPUTransferBuffer(device, xfer);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  if (!cmd) {
    SDL_Log("Failed to acquire command buffer for white texture: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
  if (!copy) {
    SDL_Log("Failed to begin copy pass for white texture: %s", SDL_GetError());
    SDL_CancelGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  SDL_GPUTextureTransferInfo src;
  SDL_zero(src);
  src.transfer_buffer = xfer;

  SDL_GPUTextureRegion dst;
  SDL_zero(dst);
  dst.texture = tex;
  dst.w = 1;
  dst.h = 1;
  dst.d = 1;

  SDL_UploadToGPUTexture(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("Failed to submit white texture upload: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, tex);
    return NULL;
  }

  SDL_ReleaseGPUTransferBuffer(device, xfer);
  return tex;
}

/* ── Helper: free model GPU resources ─────────────────────────────────────── */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model) {
  if (model->primitives) {
    for (int i = 0; i < model->primitive_count; i++) {
      if (model->primitives[i].vertex_buffer) {
        SDL_ReleaseGPUBuffer(device, model->primitives[i].vertex_buffer);
      }
      if (model->primitives[i].index_buffer) {
        SDL_ReleaseGPUBuffer(device, model->primitives[i].index_buffer);
      }
    }
    SDL_free(model->primitives);
    model->primitives = NULL;
  }

  if (model->materials) {
    /* Release textures, avoiding double-free of shared textures. */
    for (int i = 0; i < model->material_count; i++) {
      if (!model->materials[i].texture)
        continue;

      /* Check if a previous material already uses this texture. */
      bool already_released = false;
      for (int j = 0; j < i; j++) {
        if (model->materials[j].texture == model->materials[i].texture) {
          already_released = true;
          break;
        }
      }
      if (!already_released) {
        SDL_ReleaseGPUTexture(device, model->materials[i].texture);
      }
    }
    SDL_free(model->materials);
    model->materials = NULL;
  }

  forge_gltf_free(&model->scene);
}

/* ── Helper: upload glTF model to GPU ─────────────────────────────────────── */

static bool
upload_model_to_gpu(SDL_GPUDevice *device, SDL_GPUTexture *white_texture, ModelData *model) {
  ForgeGltfScene *scene = &model->scene;
  int i;

  /* Upload primitives (vertex + index buffers).
   * The glTF parser stores all primitives in a flat array. */
  model->primitive_count = scene->primitive_count;
  model->primitives =
      (GpuPrimitive *)SDL_calloc((size_t)scene->primitive_count, sizeof(GpuPrimitive));
  if (!model->primitives) {
    SDL_Log("Failed to allocate GPU primitives");
    return false;
  }

  for (i = 0; i < scene->primitive_count; i++) {
    const ForgeGltfPrimitive *src = &scene->primitives[i];
    GpuPrimitive *dst = &model->primitives[i];

    dst->material_index = src->material_index;
    dst->index_count = src->index_count;
    dst->has_uvs = src->has_uvs;

    if (src->vertices && src->vertex_count > 0) {
      Uint32 vb_size = src->vertex_count * (Uint32)sizeof(ForgeGltfVertex);
      dst->vertex_buffer =
          upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, src->vertices, vb_size);
      if (!dst->vertex_buffer) {
        free_model_gpu(device, model);
        return false;
      }
    }

    if (src->indices && src->index_count > 0) {
      Uint32 ib_size = src->index_count * src->index_stride;
      dst->index_buffer =
          upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX, src->indices, ib_size);
      if (!dst->index_buffer) {
        free_model_gpu(device, model);
        return false;
      }
      dst->index_type = (src->index_stride == 2) ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                                                 : SDL_GPU_INDEXELEMENTSIZE_32BIT;
    }
  }

  /* Load material textures. */
  model->material_count = scene->material_count;
  model->materials = (GpuMaterial *)SDL_calloc(
      (size_t)(scene->material_count > 0 ? scene->material_count : 1),
      sizeof(GpuMaterial)
  );
  if (!model->materials) {
    SDL_Log("Failed to allocate GPU materials");
    free_model_gpu(device, model);
    return false;
  }

  /* Deduplicate textures — avoid loading the same image file twice. */
  {
    SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
    const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
    int loaded_count = 0;
    SDL_memset(loaded_textures, 0, sizeof(loaded_textures));
    SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

    for (i = 0; i < scene->material_count; i++) {
      const ForgeGltfMaterial *src = &scene->materials[i];
      GpuMaterial *dst = &model->materials[i];

      dst->base_color[0] = src->base_color[0];
      dst->base_color[1] = src->base_color[1];
      dst->base_color[2] = src->base_color[2];
      dst->base_color[3] = src->base_color[3];
      dst->has_texture = src->has_texture;
      dst->texture = NULL;

      if (src->has_texture && src->texture_path[0] != '\0') {
        /* Check if this texture path was already loaded. */
        bool found = false;
        int j;
        for (j = 0; j < loaded_count; j++) {
          if (loaded_paths[j] && SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
            dst->texture = loaded_textures[j];
            found = true;
            break;
          }
        }

        if (!found && loaded_count < FORGE_GLTF_MAX_IMAGES) {
          dst->texture = load_texture(device, src->texture_path);
          if (dst->texture) {
            loaded_textures[loaded_count] = dst->texture;
            loaded_paths[loaded_count] = src->texture_path;
            loaded_count++;
          } else {
            dst->has_texture = false;
          }
        }
      }
    }
  }

  (void)white_texture; /* Used by the caller, not during upload */
  return true;
}

/* ── Helper: load and upload a glTF model ─────────────────────────────────── */

static bool setup_model(
    SDL_GPUDevice *device, SDL_GPUTexture *white_texture, ModelData *model, const char *path
) {
  if (!forge_gltf_load(path, &model->scene)) {
    SDL_Log("Failed to load glTF: %s", path);
    return false;
  }
  return upload_model_to_gpu(device, white_texture, model);
}

/* ── Helper: upload grid geometry ─────────────────────────────────────────── */

/* 4-vertex quad on the XZ plane at Y = 0, covering ±GRID_HALF_SIZE. */
static void upload_grid_geometry(SDL_GPUDevice *device, app_state *state) {
  /* Grid vertices — just positions, no normals or UVs.
   * The fragment shader computes the grid pattern procedurally. */
  float vertices[] = {
    -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE, GRID_HALF_SIZE,  0.0f, -GRID_HALF_SIZE,
    GRID_HALF_SIZE,  0.0f, GRID_HALF_SIZE,  -GRID_HALF_SIZE, 0.0f, GRID_HALF_SIZE,
  };

  Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };

  state->grid_vertex_buffer =
      upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, sizeof(vertices));
  state->grid_index_buffer =
      upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
}

/* ── Helper: generate box placements ──────────────────────────────────────── */

/* Places 8 boxes in a ring on the ground, plus 4 stacked on alternating
 * ground boxes — identical to the Lesson 15 layout. */
static void generate_box_placements(app_state *state) {
  int count = 0;

  /* Ground ring of BOX_GROUND_COUNT boxes. */
  for (int i = 0; i < BOX_GROUND_COUNT; i++) {
    float angle = (float)i * (2.0f * FORGE_PI / BOX_GROUND_COUNT);
    state->box_placements[count].position = vec3_create(
        BOX_RING_RADIUS * SDL_cosf(angle),
        BOX_GROUND_Y,
        BOX_RING_RADIUS * SDL_sinf(angle)
    );
    state->box_placements[count].y_rotation = angle;
    count++;
  }

  /* Stack BOX_STACK_COUNT boxes on every other ground box. */
  for (int i = 0; i < BOX_STACK_COUNT; i++) {
    int base = i * 2; /* every other ground box */
    vec3 base_pos = state->box_placements[base].position;
    state->box_placements[count].position = vec3_create(base_pos.x, BOX_STACK_Y, base_pos.z);
    state->box_placements[count].y_rotation = state->box_placements[base].y_rotation + BOX_STACK_ROTATION_OFFSET;
    count++;
  }

  state->box_count = count;
}

/* ── Cascade split computation ────────────────────────────────────────────── */

/* Lengyel's logarithmic-linear blend to compute cascade split distances.
 * Pure logarithmic distributes resolution more evenly in log-space (good
 * for close objects), while linear is more uniform.  Lambda = 0.5 blends
 * between the two for a practical balance. */
static void compute_cascade_splits(float near_plane, float far_plane, float splits[NUM_CASCADES]) {
  for (int i = 0; i < NUM_CASCADES; i++) {
    float p = (float)(i + 1) / (float)NUM_CASCADES;
    float log_split = near_plane * powf(far_plane / near_plane, p);
    float lin_split = near_plane + (far_plane - near_plane) * p;
    splits[i] = CASCADE_LAMBDA * log_split + (1.0f - CASCADE_LAMBDA) * lin_split;
  }
}

/* ── Compute light VP matrix for one cascade ──────────────────────────────── */

/* Given the camera's inverse VP matrix, compute the 8 frustum corners
 * for a cascade slice, transform them to light space, fit a tight AABB,
 * and build an orthographic projection from the light's view. */
static mat4 compute_cascade_light_vp(
    mat4 inv_cam_vp,
    float split_near,
    float split_far,
    float cam_near,
    float cam_far,
    vec3 light_dir
) {
  /* NDC corners of the full frustum.  Z range is [0, 1] (0-to-1 depth). */
  static const vec4 ndc_corners[NUM_FRUSTUM_CORNERS] = {
    { -1.0f, -1.0f, 0.0f, 1.0f }, /* near bottom-left  */
    { 1.0f, -1.0f, 0.0f, 1.0f },  /* near bottom-right */
    { 1.0f, 1.0f, 0.0f, 1.0f },   /* near top-right    */
    { -1.0f, 1.0f, 0.0f, 1.0f },  /* near top-left     */
    { -1.0f, -1.0f, 1.0f, 1.0f }, /* far bottom-left   */
    { 1.0f, -1.0f, 1.0f, 1.0f },  /* far bottom-right  */
    { 1.0f, 1.0f, 1.0f, 1.0f },   /* far top-right     */
    { -1.0f, 1.0f, 1.0f, 1.0f },  /* far top-left      */
  };

  /* Unproject all NDC corners to world space. */
  vec3 world_corners[NUM_FRUSTUM_CORNERS];
  {
    int i;
    for (i = 0; i < NUM_FRUSTUM_CORNERS; i++) {
      vec4 wp = mat4_multiply_vec4(inv_cam_vp, ndc_corners[i]);
      world_corners[i] = vec3_perspective_divide(wp);
    }
  }

  /* Interpolate between near and far planes to get this cascade's slice.
   * t_near/t_far map the cascade split distances to [0,1] range within
   * the camera's full frustum depth range. */
  float t_near = (split_near - cam_near) / (cam_far - cam_near);
  float t_far = (split_far - cam_near) / (cam_far - cam_near);

  vec3 cascade_corners[NUM_FRUSTUM_CORNERS];
  {
    int i;
    for (i = 0; i < NUM_NEAR_CORNERS; i++) {
      cascade_corners[i] = vec3_lerp(world_corners[i], world_corners[i + NUM_NEAR_CORNERS], t_near);
      cascade_corners[i + NUM_NEAR_CORNERS] =
          vec3_lerp(world_corners[i], world_corners[i + NUM_NEAR_CORNERS], t_far);
    }
  }

  /* Compute the center of the cascade frustum slice. */
  vec3 center = vec3_create(0.0f, 0.0f, 0.0f);
  {
    int i;
    for (i = 0; i < NUM_FRUSTUM_CORNERS; i++) {
      center = vec3_add(center, cascade_corners[i]);
    }
    center = vec3_scale(center, 1.0f / (float)NUM_FRUSTUM_CORNERS);
  }

  /* Build a light view matrix looking from above the center toward center. */
  vec3 light_pos = vec3_add(center, vec3_scale(light_dir, LIGHT_DISTANCE));
  mat4 light_view = mat4_look_at(light_pos, center, vec3_create(0.0f, 1.0f, 0.0f));

  /* Transform cascade corners to light view space and find AABB. */
  float min_x = AABB_INIT_MIN, max_x = AABB_INIT_MAX;
  float min_y = AABB_INIT_MIN, max_y = AABB_INIT_MAX;
  float min_z = AABB_INIT_MIN, max_z = AABB_INIT_MAX;
  {
    int i;
    for (i = 0; i < NUM_FRUSTUM_CORNERS; i++) {
      vec4 lp = mat4_multiply_vec4(
          light_view,
          vec4_create(cascade_corners[i].x, cascade_corners[i].y, cascade_corners[i].z, 1.0f)
      );
      if (lp.x < min_x)
        min_x = lp.x;
      if (lp.x > max_x)
        max_x = lp.x;
      if (lp.y < min_y)
        min_y = lp.y;
      if (lp.y > max_y)
        max_y = lp.y;
      if (lp.z < min_z)
        min_z = lp.z;
      if (lp.z > max_z)
        max_z = lp.z;
    }
  }

  /* Expand the Z range to capture shadow casters behind the frustum. */
  min_z -= SHADOW_Z_PADDING;

  /* Build orthographic projection from the tight AABB. */
  mat4 light_proj = mat4_orthographic(min_x, max_x, min_y, max_y, -max_z, -min_z);

  return mat4_multiply(light_proj, light_view);
}

/* ── Helper: draw model into shadow map (depth-only) ──────────────────────── */

/* Renders all primitives of a model into the current shadow map using
 * the shadow pipeline.  Only pushes light_mvp to vertex slot 0. */
static void draw_model_shadow(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const mat4 *placement,
    const mat4 *light_vp
) {
  const ForgeGltfScene *scene = &model->scene;

  for (int ni = 0; ni < scene->node_count; ni++) {
    const ForgeGltfNode *node = &scene->nodes[ni];
    if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
      continue;

    mat4 model_mat = mat4_multiply(*placement, node->world_transform);
    mat4 light_mvp = mat4_multiply(*light_vp, model_mat);

    ShadowVertUniforms svu;
    svu.light_mvp = light_mvp;
    SDL_PushGPUVertexUniformData(cmd, 0, &svu, sizeof(svu));

    const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
    for (int pi = 0; pi < mesh->primitive_count; pi++) {
      int prim_idx = mesh->first_primitive + pi;
      const GpuPrimitive *prim = &model->primitives[prim_idx];

      if (!prim->vertex_buffer || !prim->index_buffer)
        continue;

      SDL_GPUBufferBinding vb;
      SDL_zero(vb);
      vb.buffer = prim->vertex_buffer;
      SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

      SDL_GPUBufferBinding ib;
      SDL_zero(ib);
      ib.buffer = prim->index_buffer;
      SDL_BindGPUIndexBuffer(pass, &ib, prim->index_type);

      SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
    }
  }
}

/* ── Helper: draw model for scene pass (lit + shadows + HDR) ──────────────── */

/* Renders all primitives with Blinn-Phong lighting, shadow receiving,
 * and HDR output.  Binds diffuse texture + 3 shadow maps per primitive. */
static void draw_model_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    const mat4 *placement,
    const mat4 *cam_vp,
    const ShadowMatrices *shadow_mats,
    const vec3 *light_dir,
    const float *cascade_splits
) {
  const ForgeGltfScene *scene = &model->scene;

  for (int ni = 0; ni < scene->node_count; ni++) {
    const ForgeGltfNode *node = &scene->nodes[ni];
    if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
      continue;

    /* Per-node model matrix: placement * node hierarchy transform. */
    mat4 model_mat = mat4_multiply(*placement, node->world_transform);
    mat4 mvp = mat4_multiply(*cam_vp, model_mat);

    /* Push vertex uniforms slot 0: MVP + model matrix. */
    SceneVertUniforms vert_u;
    vert_u.mvp = mvp;
    vert_u.model = model_mat;
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    /* Push vertex uniforms slot 1: shadow matrices. */
    SDL_PushGPUVertexUniformData(cmd, 1, shadow_mats, sizeof(*shadow_mats));

    const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
    for (int pi = 0; pi < mesh->primitive_count; pi++) {
      int prim_idx = mesh->first_primitive + pi;
      const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

      if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
        continue;

      /* Fragment uniforms: material + lighting + shadows. */
      SDL_GPUTexture *tex = state->white_texture;

      SceneFragUniforms frag_u;
      SDL_zero(frag_u);

      if (gpu_prim->material_index >= 0 && gpu_prim->material_index < model->material_count) {
        const GpuMaterial *mat = &model->materials[gpu_prim->material_index];
        frag_u.base_color[0] = mat->base_color[0];
        frag_u.base_color[1] = mat->base_color[1];
        frag_u.base_color[2] = mat->base_color[2];
        frag_u.base_color[3] = mat->base_color[3];
        frag_u.has_texture = mat->has_texture ? 1 : 0;
        if (mat->texture)
          tex = mat->texture;
      } else {
        frag_u.base_color[0] = 1.0f;
        frag_u.base_color[1] = 1.0f;
        frag_u.base_color[2] = 1.0f;
        frag_u.base_color[3] = 1.0f;
        frag_u.has_texture = 0;
      }

      frag_u.light_dir[0] = light_dir->x;
      frag_u.light_dir[1] = light_dir->y;
      frag_u.light_dir[2] = light_dir->z;
      frag_u.eye_pos[0] = state->cam_position.x;
      frag_u.eye_pos[1] = state->cam_position.y;
      frag_u.eye_pos[2] = state->cam_position.z;
      frag_u.cascade_splits[0] = cascade_splits[0];
      frag_u.cascade_splits[1] = cascade_splits[1];
      frag_u.cascade_splits[2] = cascade_splits[2];
      frag_u.shininess = MATERIAL_SHININESS;
      frag_u.ambient = MATERIAL_AMBIENT;
      frag_u.specular_str = MATERIAL_SPECULAR_STR;
      frag_u.light_intensity = LIGHT_INTENSITY;
      frag_u.shadow_texel_size = SHADOW_TEXEL_SIZE;
      frag_u.shadow_bias = SHADOW_BIAS;

      SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

      /* Bind samplers: slot 0 = diffuse, slots 1-3 = shadow maps. */
      SDL_GPUTextureSamplerBinding tex_bindings[4];
      SDL_zero(tex_bindings);
      tex_bindings[0].texture = tex;
      tex_bindings[0].sampler = state->sampler;
      tex_bindings[1].texture = state->shadow_maps[0];
      tex_bindings[1].sampler = state->shadow_sampler;
      tex_bindings[2].texture = state->shadow_maps[1];
      tex_bindings[2].sampler = state->shadow_sampler;
      tex_bindings[3].texture = state->shadow_maps[2];
      tex_bindings[3].sampler = state->shadow_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 4);

      /* Bind vertex and index buffers. */
      SDL_GPUBufferBinding vb;
      SDL_zero(vb);
      vb.buffer = gpu_prim->vertex_buffer;
      SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

      SDL_GPUBufferBinding ib;
      SDL_zero(ib);
      ib.buffer = gpu_prim->index_buffer;
      SDL_BindGPUIndexBuffer(pass, &ib, gpu_prim->index_type);

      SDL_DrawGPUIndexedPrimitives(pass, gpu_prim->index_count, 1, 0, 0, 0);
    }
  }
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*                              SDL CALLBACKS                               */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppInit ──────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  int i;

  /* Step 1 — Initialize SDL video subsystem. */
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* Step 2 — Create GPU device with debug enabled for development.
   * Request both SPIRV and DXIL so we pick whichever the driver supports. */
  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
  if (!device) {
    SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* Step 3 — Create window. */
  SDL_Window *window =
      SDL_CreateWindow("Lesson 21 — HDR & Tone Mapping", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* Step 4 — Claim the window for GPU rendering. */
  if (!SDL_ClaimWindowForGPUDevice(device, window)) {
    SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* Step 5 — Request SDR_LINEAR for correct gamma handling.
   * SDR_LINEAR gives us a B8G8R8A8_UNORM_SRGB swapchain: the GPU
   * automatically converts our linear shader output to sRGB on write.
   * This is the correct pipeline for HDR rendering:
   *   Scene → HDR buffer (linear) → tone map (linear) → sRGB swapchain.
   * Without SDR_LINEAR, we'd need manual pow(1/2.2) in the shader. */
  if (SDL_WindowSupportsGPUSwapchainComposition(
          device,
          window,
          SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
      )) {
    if (!SDL_SetGPUSwapchainParameters(
            device,
            window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
            SDL_GPU_PRESENTMODE_VSYNC
        )) {
      SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
      SDL_DestroyWindow(window);
      SDL_DestroyGPUDevice(device);
      return SDL_APP_FAILURE;
    }
  }

  /* Query swapchain format after setting composition — it may have
   * changed from UNORM to UNORM_SRGB. */
  SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

  /* Step 6 — Allocate app_state. */
  app_state *state = SDL_calloc(1, sizeof(app_state));
  if (!state) {
    SDL_Log("Failed to allocate app_state");
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }
  state->window = window;
  state->device = device;

#ifdef FORGE_CAPTURE
  forge_capture_parse_args(&state->capture, argc, argv);
#else
  (void)argc;
  (void)argv;
#endif

  /* Step 7 — Get initial window size for render targets. */
  int draw_w = 0, draw_h = 0;
  if (!SDL_GetWindowSizeInPixels(window, &draw_w, &draw_h)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    draw_w = WINDOW_WIDTH;
    draw_h = WINDOW_HEIGHT;
  }
  Uint32 w = (Uint32)draw_w;
  Uint32 h = (Uint32)draw_h;

  /* Step 8 — Create the HDR render target.
   * This is the core of the lesson: a floating-point texture that
   * preserves lighting values above 1.0 instead of clamping them. */
  state->hdr_target = create_hdr_target(device, w, h);
  if (!state->hdr_target) {
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }
  state->hdr_width = w;
  state->hdr_height = h;

  /* Step 9 — Create depth texture for the scene pass. */
  state->depth_texture = create_depth_texture(device, w, h);
  if (!state->depth_texture) {
    SDL_ReleaseGPUTexture(device, state->hdr_target);
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }
  state->depth_width = w;
  state->depth_height = h;

  /* Step 10 — Create shadow map textures (one per cascade).
   * Each shadow map is a D32_FLOAT texture with both DEPTH_STENCIL_TARGET
   * (written during shadow pass) and SAMPLER (read during scene pass). */
  for (i = 0; i < NUM_CASCADES; i++) {
    state->shadow_maps[i] = create_shadow_map(device);
    if (!state->shadow_maps[i]) {
      /* Clean up already-created shadow maps. */
      for (int j = 0; j < i; j++) {
        SDL_ReleaseGPUTexture(device, state->shadow_maps[j]);
      }
      SDL_ReleaseGPUTexture(device, state->depth_texture);
      SDL_ReleaseGPUTexture(device, state->hdr_target);
      SDL_free(state);
      SDL_DestroyWindow(window);
      SDL_DestroyGPUDevice(device);
      return SDL_APP_FAILURE;
    }
  }

  /* Step 11 — Create the 1x1 white fallback texture. */
  state->white_texture = create_white_texture(device);
  if (!state->white_texture) {
    SDL_Log("Failed to create white texture: %s", SDL_GetError());
    goto init_fail;
  }

  /* Step 12 — Create samplers.
   * Linear/repeat sampler for diffuse textures (same as previous lessons).
   * Nearest sampler for the HDR target in the tone mapping pass.
   * Nearest/clamp sampler for shadow maps. */
  {
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.max_anisotropy = MAX_ANISOTROPY;
    sampler_info.enable_anisotropy = true;
    state->sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (!state->sampler) {
      SDL_Log("Failed to create diffuse sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }
  {
    SDL_GPUSamplerCreateInfo hdr_samp_info;
    SDL_zero(hdr_samp_info);
    hdr_samp_info.min_filter = SDL_GPU_FILTER_NEAREST;
    hdr_samp_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    hdr_samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    hdr_samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    state->hdr_sampler = SDL_CreateGPUSampler(device, &hdr_samp_info);
    if (!state->hdr_sampler) {
      SDL_Log("Failed to create HDR sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }
  {
    /* Shadow sampler: NEAREST filter, CLAMP_TO_EDGE to avoid sampling
     * outside the shadow map (which would give incorrect results). */
    SDL_GPUSamplerCreateInfo shadow_samp_info;
    SDL_zero(shadow_samp_info);
    shadow_samp_info.min_filter = SDL_GPU_FILTER_NEAREST;
    shadow_samp_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    shadow_samp_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    shadow_samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    shadow_samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    shadow_samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    state->shadow_sampler = SDL_CreateGPUSampler(device, &shadow_samp_info);
    if (!state->shadow_sampler) {
      SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 13 — Load glTF models. */
  if (!setup_model(device, state->white_texture, &state->truck, TRUCK_MODEL_PATH)) {
    SDL_Log("Failed to set up truck model");
    goto init_fail;
  }
  if (!setup_model(device, state->white_texture, &state->box, BOX_MODEL_PATH)) {
    SDL_Log("Failed to set up box model");
    goto init_fail;
  }

  /* Step 14 — Upload grid geometry and generate box placements. */
  upload_grid_geometry(device, state);
  generate_box_placements(state);

  /* Step 15 — Create the shadow pipeline (depth-only).
   * Front-face culling reduces peter-panning; depth bias reduces acne. */
  {
    SDL_GPUShader *vert = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv,
        sizeof(shadow_vert_spirv),
        shadow_vert_dxil,
        sizeof(shadow_vert_dxil),
        0,
        1
    ); /* 0 samplers, 1 uniform buffer (light_mvp) */
    SDL_GPUShader *frag = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv,
        sizeof(shadow_frag_spirv),
        shadow_frag_dxil,
        sizeof(shadow_frag_dxil),
        0,
        0
    ); /* no samplers, no uniforms */

    if (!vert || !frag) {
      SDL_Log("Failed to create shadow shaders");
      if (vert)
        SDL_ReleaseGPUShader(device, vert);
      if (frag)
        SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    {
      /* Same vertex layout as ForgeGltfVertex — shadow shader only
       * uses position but all 3 attributes must match. */
      SDL_GPUVertexBufferDescription vb_desc;
      SDL_zero(vb_desc);
      vb_desc.slot = 0;
      vb_desc.pitch = sizeof(ForgeGltfVertex);
      vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

      SDL_GPUVertexAttribute attrs[3];
      SDL_zero(attrs);
      attrs[0].location = 0;
      attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
      attrs[0].offset = offsetof(ForgeGltfVertex, position);
      attrs[1].location = 1;
      attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
      attrs[1].offset = offsetof(ForgeGltfVertex, normal);
      attrs[2].location = 2;
      attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
      attrs[2].offset = offsetof(ForgeGltfVertex, uv);

      SDL_GPUGraphicsPipelineCreateInfo pipe_info;
      SDL_zero(pipe_info);
      pipe_info.vertex_shader = vert;
      pipe_info.fragment_shader = frag;
      pipe_info.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
      pipe_info.vertex_input_state.num_vertex_buffers = 1;
      pipe_info.vertex_input_state.vertex_attributes = attrs;
      pipe_info.vertex_input_state.num_vertex_attributes = 3;
      pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

      /* Back-face culling in the shadow pass.  Front-face culling is a
       * common alternative that eliminates shadow acne, but it causes
       * peter panning (shadows detach from object bases) because back
       * faces are deeper than the actual surface.  Back-face culling
       * with a small slope bias avoids both artifacts. */
      pipe_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
      pipe_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
      pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
      pipe_info.rasterizer_state.depth_bias_constant_factor = SHADOW_DEPTH_BIAS;
      pipe_info.rasterizer_state.depth_bias_slope_factor = SHADOW_SLOPE_BIAS;

      pipe_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
      pipe_info.depth_stencil_state.enable_depth_test = true;
      pipe_info.depth_stencil_state.enable_depth_write = true;

      /* Depth-only: no color targets. */
      pipe_info.target_info.num_color_targets = 0;
      pipe_info.target_info.has_depth_stencil_target = true;
      pipe_info.target_info.depth_stencil_format = SHADOW_MAP_FORMAT;

      state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

      SDL_ReleaseGPUShader(device, vert);
      SDL_ReleaseGPUShader(device, frag);

      if (!state->shadow_pipeline) {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
        goto init_fail;
      }
    }
  }

  /* Step 16 — Create the scene pipeline.
   * Key differences from Lesson 15:
   *   - Color target format is HDR_FORMAT (R16G16B16A16_FLOAT)
   *   - Fragment shader uses 4 samplers (diffuse + 3 shadow maps)
   *   - Vertex shader uses 2 uniform buffers (MVP+model, shadow matrices) */
  {
    SDL_GPUShader *vert = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv,
        sizeof(scene_vert_spirv),
        scene_vert_dxil,
        sizeof(scene_vert_dxil),
        0,
        2
    ); /* 0 samplers, 2 uniform buffers */
    SDL_GPUShader *frag = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv,
        sizeof(scene_frag_spirv),
        scene_frag_dxil,
        sizeof(scene_frag_dxil),
        4,
        1
    ); /* 4 samplers (diffuse + 3 shadow), 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create scene shaders");
      if (vert)
        SDL_ReleaseGPUShader(device, vert);
      if (frag)
        SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    /* Vertex layout matching ForgeGltfVertex: pos(3) + norm(3) + uv(2). */
    SDL_GPUVertexBufferDescription vb_desc;
    SDL_zero(vb_desc);
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(ForgeGltfVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attrs[3];
    SDL_zero(attrs);
    attrs[0].location = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = offsetof(ForgeGltfVertex, position);
    attrs[1].location = 1;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = offsetof(ForgeGltfVertex, normal);
    attrs[2].location = 2;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset = offsetof(ForgeGltfVertex, uv);

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT; /* Render to HDR, not swapchain */

    SDL_GPUGraphicsPipelineCreateInfo pipe_info;
    SDL_zero(pipe_info);
    pipe_info.vertex_shader = vert;
    pipe_info.fragment_shader = frag;
    pipe_info.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
    pipe_info.vertex_input_state.num_vertex_buffers = 1;
    pipe_info.vertex_input_state.vertex_attributes = attrs;
    pipe_info.vertex_input_state.num_vertex_attributes = 3;
    pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipe_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipe_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipe_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pipe_info.depth_stencil_state.enable_depth_test = true;
    pipe_info.depth_stencil_state.enable_depth_write = true;
    pipe_info.target_info.color_target_descriptions = &color_desc;
    pipe_info.target_info.num_color_targets = 1;
    pipe_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pipe_info.target_info.has_depth_stencil_target = true;

    state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!state->scene_pipeline) {
      SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 17 — Create the grid pipeline.
   * Same HDR target format, no backface culling (grid is a flat quad).
   * Vertex shader uses 2 uniform buffers (VP + shadow matrices).
   * Fragment shader uses 3 samplers (shadow maps only — no diffuse). */
  {
    SDL_GPUShader *vert = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv,
        sizeof(grid_vert_spirv),
        grid_vert_dxil,
        sizeof(grid_vert_dxil),
        0,
        2
    ); /* 0 samplers, 2 uniform buffers */
    SDL_GPUShader *frag = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv,
        sizeof(grid_frag_spirv),
        grid_frag_dxil,
        sizeof(grid_frag_dxil),
        3,
        1
    ); /* 3 samplers (shadow maps), 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create grid shaders");
      if (vert)
        SDL_ReleaseGPUShader(device, vert);
      if (frag)
        SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    {
      SDL_GPUVertexBufferDescription vb_desc;
      SDL_zero(vb_desc);
      vb_desc.slot = 0;
      vb_desc.pitch = sizeof(float) * 3; /* position only */
      vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

      SDL_GPUVertexAttribute attr;
      SDL_zero(attr);
      attr.location = 0;
      attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
      attr.offset = 0;

      SDL_GPUColorTargetDescription color_desc;
      SDL_zero(color_desc);
      color_desc.format = HDR_FORMAT;

      SDL_GPUGraphicsPipelineCreateInfo pipe_info;
      SDL_zero(pipe_info);
      pipe_info.vertex_shader = vert;
      pipe_info.fragment_shader = frag;
      pipe_info.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
      pipe_info.vertex_input_state.num_vertex_buffers = 1;
      pipe_info.vertex_input_state.vertex_attributes = &attr;
      pipe_info.vertex_input_state.num_vertex_attributes = 1;
      pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      pipe_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
      pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
      pipe_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
      pipe_info.depth_stencil_state.enable_depth_test = true;
      pipe_info.depth_stencil_state.enable_depth_write = true;
      pipe_info.target_info.color_target_descriptions = &color_desc;
      pipe_info.target_info.num_color_targets = 1;
      pipe_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
      pipe_info.target_info.has_depth_stencil_target = true;

      state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

      SDL_ReleaseGPUShader(device, vert);
      SDL_ReleaseGPUShader(device, frag);

      if (!state->grid_pipeline) {
        SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
        goto init_fail;
      }
    }
  }

  /* Step 18 — Create the tone mapping pipeline.
   * This pipeline renders a fullscreen quad with NO vertex buffer
   * (positions generated from SV_VertexID in the vertex shader),
   * NO depth test, and outputs to the SWAPCHAIN format. */
  {
    SDL_GPUShader *vert = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        tonemap_vert_spirv,
        sizeof(tonemap_vert_spirv),
        tonemap_vert_dxil,
        sizeof(tonemap_vert_dxil),
        0,
        0
    ); /* no samplers, no uniforms in vertex */
    SDL_GPUShader *frag = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        tonemap_frag_spirv,
        sizeof(tonemap_frag_spirv),
        tonemap_frag_dxil,
        sizeof(tonemap_frag_dxil),
        1,
        1
    ); /* 1 sampler (HDR texture), 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create tonemap shaders");
      if (vert)
        SDL_ReleaseGPUShader(device, vert);
      if (frag)
        SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    {
      /* No vertex input — SV_VertexID generates everything. */
      SDL_GPUColorTargetDescription color_desc;
      SDL_zero(color_desc);
      color_desc.format = swapchain_format;

      SDL_GPUGraphicsPipelineCreateInfo pipe_info;
      SDL_zero(pipe_info);
      pipe_info.vertex_shader = vert;
      pipe_info.fragment_shader = frag;
      /* No vertex_input_state — using SV_VertexID */
      pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
      /* No depth testing for fullscreen post-processing. */
      pipe_info.target_info.color_target_descriptions = &color_desc;
      pipe_info.target_info.num_color_targets = 1;
      pipe_info.target_info.has_depth_stencil_target = false;

      state->tonemap_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

      SDL_ReleaseGPUShader(device, vert);
      SDL_ReleaseGPUShader(device, frag);

      if (!state->tonemap_pipeline) {
        SDL_Log("Failed to create tonemap pipeline: %s", SDL_GetError());
        goto init_fail;
      }
    }
  }

  /* Step 19 — Initialize camera and HDR settings. */
  state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
  state->cam_yaw = CAM_START_YAW_DEG * FORGE_DEG2RAD;
  state->cam_pitch = CAM_START_PITCH_DEG * FORGE_DEG2RAD;
  state->exposure = DEFAULT_EXPOSURE;
  state->tonemap_mode = TONEMAP_ACES; /* Start with ACES — best default */
  state->last_ticks = SDL_GetTicks();

  /* Capture mouse for FPS-style camera control. */
  if (SDL_SetWindowRelativeMouseMode(window, true)) {
    state->mouse_captured = true;
  } else {
    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    state->mouse_captured = false;
  }

  SDL_Log("Tone mapping: ACES (press 1/2/3 to switch)");
  SDL_Log("Exposure: %.1f (press +/- to adjust)", state->exposure);
  SDL_Log(
      "Shadow maps: %d cascades @ %dx%d, PCF 3x3",
      NUM_CASCADES,
      SHADOW_MAP_SIZE,
      SHADOW_MAP_SIZE
  );

#ifdef FORGE_CAPTURE
  if (state->capture.mode != FORGE_CAPTURE_NONE) {
    forge_capture_init(&state->capture, device, window);
  }
#endif

  *appstate = state;
  return SDL_APP_CONTINUE;

init_fail:
  /* Centralized cleanup for late init failures.
   * Setting *appstate lets SDL call SDL_AppQuit, which null-guards
   * every resource and releases only what was successfully created. */
  *appstate = state;
  return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  app_state *state = (app_state *)appstate;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;

  case SDL_EVENT_KEY_DOWN:
    if (event->key.key == SDLK_ESCAPE) {
      if (state->mouse_captured) {
        /* Release mouse first, quit on second Escape. */
        if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
          SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        } else {
          state->mouse_captured = false;
        }
      } else {
        return SDL_APP_SUCCESS;
      }
    }

    /* Tone mapping mode selection. */
    if (event->key.key == SDLK_1) {
      state->tonemap_mode = TONEMAP_NONE;
      SDL_Log("Tone mapping: None (clamp)");
    } else if (event->key.key == SDLK_2) {
      state->tonemap_mode = TONEMAP_REINHARD;
      SDL_Log("Tone mapping: Reinhard");
    } else if (event->key.key == SDLK_3) {
      state->tonemap_mode = TONEMAP_ACES;
      SDL_Log("Tone mapping: ACES");
    }

    /* Exposure control. */
    if (event->key.key == SDLK_EQUALS) {
      state->exposure += EXPOSURE_STEP;
      if (state->exposure > MAX_EXPOSURE) {
        state->exposure = MAX_EXPOSURE;
      }
      SDL_Log("Exposure: %.1f", state->exposure);
    } else if (event->key.key == SDLK_MINUS) {
      state->exposure -= EXPOSURE_STEP;
      if (state->exposure < MIN_EXPOSURE) {
        state->exposure = MIN_EXPOSURE;
      }
      SDL_Log("Exposure: %.1f", state->exposure);
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (!state->mouse_captured) {
      if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
      } else {
        state->mouse_captured = true;
      }
    }
    break;

  case SDL_EVENT_MOUSE_MOTION:
    if (state->mouse_captured) {
      state->cam_yaw -= event->motion.xrel * MOUSE_SENS;
      state->cam_pitch -= event->motion.yrel * MOUSE_SENS;

      /* Clamp pitch to avoid flipping. */
      if (state->cam_pitch > PITCH_CLAMP)
        state->cam_pitch = PITCH_CLAMP;
      if (state->cam_pitch < -PITCH_CLAMP)
        state->cam_pitch = -PITCH_CLAMP;
    }
    break;

  default:
    break;
  }

  return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate) {
  app_state *state = (app_state *)appstate;

  /* ── Delta time ───────────────────────────────────────────────────── */
  Uint64 now = SDL_GetTicks();
  float dt = (float)(now - state->last_ticks) / 1000.0f;
  state->last_ticks = now;
  if (dt > MAX_FRAME_DT)
    dt = MAX_FRAME_DT;

  /* ── Camera movement ──────────────────────────────────────────────── */
  const bool *keys = SDL_GetKeyboardState(NULL);
  if (state->mouse_captured) {
    quat orientation = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(orientation);
    vec3 right = quat_right(orientation);
    vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
    float speed = CAM_SPEED * dt;

    if (keys[SDL_SCANCODE_W]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(forward, speed));
    }
    if (keys[SDL_SCANCODE_S]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(forward, -speed));
    }
    if (keys[SDL_SCANCODE_D]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(right, speed));
    }
    if (keys[SDL_SCANCODE_A]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(right, -speed));
    }
    if (keys[SDL_SCANCODE_SPACE]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(up, speed));
    }
    if (keys[SDL_SCANCODE_LSHIFT]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(up, -speed));
    }
  }

  /* ── Camera matrices ──────────────────────────────────────────────── */
  quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
  mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);

  /* Get current drawable size for aspect ratio. */
  int draw_w = 0, draw_h = 0;
  if (!SDL_GetWindowSizeInPixels(state->window, &draw_w, &draw_h)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }
  if (draw_w <= 0 || draw_h <= 0) {
    return SDL_APP_CONTINUE; /* Minimized — skip frame */
  }
  Uint32 w = (Uint32)draw_w;
  Uint32 h = (Uint32)draw_h;

  float aspect = (float)w / (float)h;
  mat4 proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
  mat4 cam_vp = mat4_multiply(proj, view);

  /* Inverse VP needed to unproject frustum corners for cascade splits. */
  mat4 inv_cam_vp = mat4_inverse(cam_vp);

  /* ── Resize HDR target and depth texture if window changed ────────── */
  if (w != state->hdr_width || h != state->hdr_height) {
    SDL_GPUTexture *new_hdr = create_hdr_target(state->device, w, h);
    if (!new_hdr) {
      SDL_Log("Failed to recreate HDR target on resize: %s", SDL_GetError());
      return SDL_APP_CONTINUE;
    }
    SDL_ReleaseGPUTexture(state->device, state->hdr_target);
    state->hdr_target = new_hdr;
    state->hdr_width = w;
    state->hdr_height = h;
  }
  if (w != state->depth_width || h != state->depth_height) {
    SDL_GPUTexture *new_depth = create_depth_texture(state->device, w, h);
    if (!new_depth) {
      SDL_Log("Failed to recreate depth texture on resize: %s", SDL_GetError());
      return SDL_APP_CONTINUE;
    }
    SDL_ReleaseGPUTexture(state->device, state->depth_texture);
    state->depth_texture = new_depth;
    state->depth_width = w;
    state->depth_height = h;
  }

  /* ── Light direction (normalized) ─────────────────────────────────── */
  vec3 light_dir = vec3_normalize(vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));

  /* ── Compute cascade splits and light VP matrices ─────────────────── */
  float cascade_splits[NUM_CASCADES];
  compute_cascade_splits(NEAR_PLANE, FAR_PLANE, cascade_splits);

  ShadowMatrices shadow_mats;
  {
    float prev_split = NEAR_PLANE;
    for (int ci = 0; ci < NUM_CASCADES; ci++) {
      shadow_mats.light_vp[ci] = compute_cascade_light_vp(
          inv_cam_vp,
          prev_split,
          cascade_splits[ci],
          NEAR_PLANE,
          FAR_PLANE,
          light_dir
      );
      prev_split = cascade_splits[ci];
    }
  }

  /* ── Acquire command buffer ───────────────────────────────────────── */
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
  if (!cmd) {
    SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }

  /* ════════════════════════════════════════════════════════════════════
   * SHADOW PASSES — One per cascade (depth-only)
   *
   * Each cascade renders the scene from the light's perspective into
   * its own shadow map.  These are depth-only passes (no color target).
   * ════════════════════════════════════════════════════════════════════ */
  {
    mat4 truck_placement = mat4_identity();

    for (int ci = 0; ci < NUM_CASCADES; ci++) {
      SDL_GPUDepthStencilTargetInfo shadow_depth;
      SDL_zero(shadow_depth);
      shadow_depth.texture = state->shadow_maps[ci];
      shadow_depth.load_op = SDL_GPU_LOADOP_CLEAR;
      shadow_depth.store_op = SDL_GPU_STOREOP_STORE; /* MUST store */
      shadow_depth.clear_depth = 1.0f;

      SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(cmd, NULL, 0, &shadow_depth);
      if (!shadow_pass) {
        SDL_Log("Failed to begin shadow pass %d: %s", ci, SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
          SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
      }

      SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

      /* Draw truck into shadow map. */
      draw_model_shadow(
          shadow_pass,
          cmd,
          &state->truck,
          &truck_placement,
          &shadow_mats.light_vp[ci]
      );

      /* Draw all boxes into shadow map. */
      for (int bi = 0; bi < state->box_count; bi++) {
        BoxPlacement *bp = &state->box_placements[bi];
        mat4 box_placement =
            mat4_multiply(mat4_translate(bp->position), mat4_rotate_y(bp->y_rotation));
        draw_model_shadow(shadow_pass, cmd, &state->box, &box_placement, &shadow_mats.light_vp[ci]);
      }

      SDL_EndGPURenderPass(shadow_pass);
    }
  }

  /* ── Acquire swapchain texture ────────────────────────────────────── */
  SDL_GPUTexture *swapchain = NULL;
  if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL)) {
    SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
      SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }
    return SDL_APP_CONTINUE;
  }
  if (!swapchain) {
    /* Window is minimized — submit empty buffer and skip. */
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
      SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }
    return SDL_APP_CONTINUE;
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 1 — Render scene to HDR target (with shadows)
   *
   * This pass renders the lit scene into the floating-point HDR buffer.
   * Lighting values above 1.0 are preserved instead of being clamped.
   * Shadow maps modulate diffuse and specular terms.
   * ════════════════════════════════════════════════════════════════════ */
  {
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = state->hdr_target;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color.r = CLEAR_COLOR_R;
    color_target.clear_color.g = CLEAR_COLOR_G;
    color_target.clear_color.b = CLEAR_COLOR_B;
    color_target.clear_color.a = CLEAR_COLOR_A;

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture = state->depth_texture;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.clear_depth = 1.0f;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
    if (!pass) {
      SDL_Log("Failed to begin HDR render pass: %s", SDL_GetError());
      if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      }
      return SDL_APP_CONTINUE;
    }

    /* ── Draw grid ────────────────────────────────────────────────── */
    if (state->grid_pipeline && state->grid_vertex_buffer && state->grid_index_buffer) {
      SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

      /* Grid vertex uniforms slot 0: VP matrix. */
      GridVertUniforms grid_vu;
      grid_vu.vp = cam_vp;
      SDL_PushGPUVertexUniformData(cmd, 0, &grid_vu, sizeof(grid_vu));

      /* Grid vertex uniforms slot 1: shadow matrices. */
      SDL_PushGPUVertexUniformData(cmd, 1, &shadow_mats, sizeof(shadow_mats));

      /* Grid fragment uniforms. */
      GridFragUniforms grid_fu;
      SDL_zero(grid_fu);
      grid_fu.line_color[0] = GRID_LINE_COLOR_R;
      grid_fu.line_color[1] = GRID_LINE_COLOR_G;
      grid_fu.line_color[2] = GRID_LINE_COLOR_B;
      grid_fu.line_color[3] = GRID_LINE_COLOR_A;
      grid_fu.bg_color[0] = GRID_BG_COLOR_R;
      grid_fu.bg_color[1] = GRID_BG_COLOR_G;
      grid_fu.bg_color[2] = GRID_BG_COLOR_B;
      grid_fu.bg_color[3] = GRID_BG_COLOR_A;
      grid_fu.light_dir[0] = light_dir.x;
      grid_fu.light_dir[1] = light_dir.y;
      grid_fu.light_dir[2] = light_dir.z;
      grid_fu.eye_pos[0] = state->cam_position.x;
      grid_fu.eye_pos[1] = state->cam_position.y;
      grid_fu.eye_pos[2] = state->cam_position.z;
      grid_fu.cascade_splits[0] = cascade_splits[0];
      grid_fu.cascade_splits[1] = cascade_splits[1];
      grid_fu.cascade_splits[2] = cascade_splits[2];
      grid_fu.grid_spacing = GRID_SPACING;
      grid_fu.line_width = GRID_LINE_WIDTH;
      grid_fu.fade_distance = GRID_FADE_DISTANCE;
      grid_fu.ambient = GRID_AMBIENT;
      grid_fu.shininess = GRID_SHININESS;
      grid_fu.specular_str = GRID_SPECULAR_STR;
      grid_fu.light_intensity = LIGHT_INTENSITY;
      grid_fu.shadow_texel_size = SHADOW_TEXEL_SIZE;
      grid_fu.shadow_bias = SHADOW_BIAS;
      SDL_PushGPUFragmentUniformData(cmd, 0, &grid_fu, sizeof(grid_fu));

      /* Bind shadow maps to fragment sampler slots 0-2. */
      SDL_GPUTextureSamplerBinding shadow_bindings[3];
      SDL_zero(shadow_bindings);
      shadow_bindings[0].texture = state->shadow_maps[0];
      shadow_bindings[0].sampler = state->shadow_sampler;
      shadow_bindings[1].texture = state->shadow_maps[1];
      shadow_bindings[1].sampler = state->shadow_sampler;
      shadow_bindings[2].texture = state->shadow_maps[2];
      shadow_bindings[2].sampler = state->shadow_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, shadow_bindings, 3);

      SDL_GPUBufferBinding vb;
      SDL_zero(vb);
      vb.buffer = state->grid_vertex_buffer;
      SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

      SDL_GPUBufferBinding ib;
      SDL_zero(ib);
      ib.buffer = state->grid_index_buffer;
      SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

      SDL_DrawGPUIndexedPrimitives(pass, GRID_INDEX_COUNT, 1, 0, 0, 0);
    }

    /* ── Draw scene models ────────────────────────────────────────── */
    if (state->scene_pipeline) {
      SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);

      /* Draw the truck at the origin. */
      mat4 truck_placement = mat4_identity();
      draw_model_scene(
          pass,
          cmd,
          &state->truck,
          state,
          &truck_placement,
          &cam_vp,
          &shadow_mats,
          &light_dir,
          cascade_splits
      );

      /* Draw all boxes at their ring positions. */
      for (int bi = 0; bi < state->box_count; bi++) {
        BoxPlacement *bp = &state->box_placements[bi];
        mat4 box_placement =
            mat4_multiply(mat4_translate(bp->position), mat4_rotate_y(bp->y_rotation));
        draw_model_scene(
            pass,
            cmd,
            &state->box,
            state,
            &box_placement,
            &cam_vp,
            &shadow_mats,
            &light_dir,
            cascade_splits
        );
      }
    }

    SDL_EndGPURenderPass(pass);
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 2 — Tone map HDR to swapchain
   *
   * A fullscreen quad samples the HDR texture, applies exposure and
   * tone mapping, and writes the result to the sRGB swapchain.
   * The swapchain's sRGB format handles gamma correction automatically.
   * ════════════════════════════════════════════════════════════════════ */
  {
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = swapchain;
    color_target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    if (!pass) {
      SDL_Log("Failed to begin tonemap render pass: %s", SDL_GetError());
      if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      }
      return SDL_APP_CONTINUE;
    }

    if (state->tonemap_pipeline) {
      SDL_BindGPUGraphicsPipeline(pass, state->tonemap_pipeline);

      /* Bind the HDR render target as input texture. */
      SDL_GPUTextureSamplerBinding hdr_binding;
      hdr_binding.texture = state->hdr_target;
      hdr_binding.sampler = state->hdr_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, &hdr_binding, 1);

      /* Push exposure and tone mapping mode. */
      TonemapFragUniforms tonemap_u;
      SDL_zero(tonemap_u);
      tonemap_u.exposure = state->exposure;
      tonemap_u.tonemap_mode = state->tonemap_mode;
      SDL_PushGPUFragmentUniformData(cmd, 0, &tonemap_u, sizeof(tonemap_u));

      /* Two triangles forming a fullscreen quad.
       * No vertex buffer is bound; positions come from SV_VertexID. */
      SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
    }

    SDL_EndGPURenderPass(pass);
  }

  /* ── Submit ───────────────────────────────────────────────────────── */
#ifdef FORGE_CAPTURE
  if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
    /* Command buffer was consumed by capture — don't submit again. */
    if (forge_capture_should_quit(&state->capture)) {
      return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
  }
#endif

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
  }

#ifdef FORGE_CAPTURE
  if (forge_capture_should_quit(&state->capture)) {
    return SDL_APP_SUCCESS;
  }
#endif

  return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  (void)result;

  app_state *state = (app_state *)appstate;
  if (!state)
    return;

#ifdef FORGE_CAPTURE
  forge_capture_destroy(&state->capture, state->device);
#endif

  /* Release in reverse creation order. */
  free_model_gpu(state->device, &state->box);
  free_model_gpu(state->device, &state->truck);

  if (state->grid_vertex_buffer) {
    SDL_ReleaseGPUBuffer(state->device, state->grid_vertex_buffer);
  }
  if (state->grid_index_buffer) {
    SDL_ReleaseGPUBuffer(state->device, state->grid_index_buffer);
  }

  if (state->white_texture) {
    SDL_ReleaseGPUTexture(state->device, state->white_texture);
  }
  if (state->sampler) {
    SDL_ReleaseGPUSampler(state->device, state->sampler);
  }
  if (state->hdr_sampler) {
    SDL_ReleaseGPUSampler(state->device, state->hdr_sampler);
  }
  if (state->shadow_sampler) {
    SDL_ReleaseGPUSampler(state->device, state->shadow_sampler);
  }
  if (state->hdr_target) {
    SDL_ReleaseGPUTexture(state->device, state->hdr_target);
  }
  if (state->depth_texture) {
    SDL_ReleaseGPUTexture(state->device, state->depth_texture);
  }

  /* Release shadow map textures. */
  {
    int ci;
    for (ci = 0; ci < NUM_CASCADES; ci++) {
      if (state->shadow_maps[ci]) {
        SDL_ReleaseGPUTexture(state->device, state->shadow_maps[ci]);
      }
    }
  }

  if (state->tonemap_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->tonemap_pipeline);
  }
  if (state->grid_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
  }
  if (state->scene_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
  }
  if (state->shadow_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
  }

  SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
  SDL_DestroyWindow(state->window);
  SDL_DestroyGPUDevice(state->device);
  SDL_free(state);
}
