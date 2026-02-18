/*
 * Lesson 15 — Cascaded Shadow Maps
 *
 * Teach directional-light shadow mapping with cascaded shadow maps (CSM).
 * The view frustum is split into 3 depth ranges, each rendered from the
 * light's perspective into its own shadow map.  Near objects get high-
 * resolution shadows; far objects get lower resolution but still receive
 * shadows.  3x3 PCF (Percentage Closer Filtering) softens shadow edges.
 *
 * The scene features textured boxes arranged around the CesiumMilkTruck
 * glTF model, a procedural grid floor that receives shadows, and a
 * moveable FPS camera.  A --show-shadow-map flag renders the first
 * cascade's depth buffer as a debug overlay.
 *
 * What's new compared to Lesson 13/14:
 *   - Shadow map textures (D32_FLOAT, DEPTH_STENCIL_TARGET | SAMPLER)
 *   - Depth-only render passes (no color target)
 *   - Cascade frustum splitting (logarithmic-linear blend)
 *   - Light-space orthographic projection from frustum corners
 *   - 3x3 PCF shadow sampling
 *   - Front-face culling in shadow pass (reduces peter-panning)
 *   - Depth bias in rasterizer state
 *   - Debug visualization overlay
 *
 * What we keep from earlier lessons:
 *   - SDL callbacks, GPU device, window, sRGB swapchain       (Lesson 01)
 *   - Vertex buffers, shaders, graphics pipeline               (Lesson 02)
 *   - Push uniforms for matrices + fragment data               (Lesson 03)
 *   - Texture + sampler binding, mipmaps                       (Lesson 04/05)
 *   - Depth buffer, back-face culling, window resize           (Lesson 06)
 *   - First-person camera, keyboard/mouse, delta time          (Lesson 07)
 *   - glTF parsing, GPU upload, material handling              (Lesson 09)
 *   - Blinn-Phong lighting with normal transformation          (Lesson 10)
 *   - Procedural grid floor with fwidth anti-aliasing          (Lesson 12)
 *
 * Controls:
 *   WASD / Arrow keys  — move forward/back/left/right
 *   Space / Left Shift — fly up / fly down
 *   Mouse              — look around (captured in relative mode)
 *   Escape             — release mouse / quit
 *
 * CLI flags:
 *   --show-shadow-map  — render cascade 0 depth as debug overlay
 *
 * Models: CesiumMilkTruck and BoxTextured (from shared assets/models/).
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include "gltf/forge_gltf.h"
#include "math/forge_math.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

/* ── Frame capture (compile-time option) ─────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Pre-compiled shader bytecodes ───────────────────────────────────── */

/* Shadow pass — depth-only rendering from the light's perspective */
#include "shaders/shadow_frag_dxil.h"
#include "shaders/shadow_frag_spirv.h"
#include "shaders/shadow_vert_dxil.h"
#include "shaders/shadow_vert_spirv.h"

/* Scene shaders — Blinn-Phong + cascaded shadow receiving */
#include "shaders/scene_frag_dxil.h"
#include "shaders/scene_frag_spirv.h"
#include "shaders/scene_vert_dxil.h"
#include "shaders/scene_vert_spirv.h"

/* Grid shaders — procedural grid + shadow receiving */
#include "shaders/grid_frag_dxil.h"
#include "shaders/grid_frag_spirv.h"
#include "shaders/grid_vert_dxil.h"
#include "shaders/grid_vert_spirv.h"

/* Debug quad — shadow map visualization overlay */
#include "shaders/debug_quad_frag_dxil.h"
#include "shaders/debug_quad_frag_spirv.h"
#include "shaders/debug_quad_vert_dxil.h"
#include "shaders/debug_quad_vert_spirv.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_TITLE "Forge GPU - 15 Cascaded Shadow Maps"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

/* Dark background (linear space — SDR_LINEAR auto-converts to sRGB). */
#define CLEAR_R 0.0099f
#define CLEAR_G 0.0099f
#define CLEAR_B 0.0267f
#define CLEAR_A 1.0f

/* Depth buffer for the main scene pass. */
#define DEPTH_CLEAR 1.0f
#define DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* ── Shadow map constants ────────────────────────────────────────────── */

#define NUM_CASCADES 3
#define SHADOW_MAP_SIZE 2048
#define SHADOW_MAP_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_TEXEL_SIZE (1.0f / (float)SHADOW_MAP_SIZE)
#define SHADOW_BIAS 0.0005f
#define SHADOW_DEPTH_BIAS 1
#define SHADOW_SLOPE_BIAS 1.5f

/* A frustum has 8 corners: 4 on the near plane, 4 on the far plane. */
#define NUM_FRUSTUM_CORNERS 8
#define NUM_NEAR_CORNERS    4

/* Lambda controls the logarithmic vs linear blend for cascade splits.
 * 0.0 = purely linear, 1.0 = purely logarithmic.
 * 0.5 is a good practical balance (Lengyel's recommendation). */
#define CASCADE_LAMBDA 0.5f

/* ── Grid pipeline constants ─────────────────────────────────────────── */

#define GRID_NUM_VERTEX_ATTRIBUTES 1
#define GRID_VERTEX_PITCH 12 /* 3 floats * 4 bytes */

#define GRID_VERT_NUM_SAMPLERS 0
#define GRID_VERT_NUM_STORAGE_TEXTURES 0
#define GRID_VERT_NUM_STORAGE_BUFFERS 0
#define GRID_VERT_NUM_UNIFORM_BUFFERS 2 /* VP + light_vps */

#define GRID_FRAG_NUM_SAMPLERS 3 /* 3 shadow maps */
#define GRID_FRAG_NUM_STORAGE_TEXTURES 0
#define GRID_FRAG_NUM_STORAGE_BUFFERS 0
#define GRID_FRAG_NUM_UNIFORM_BUFFERS 1

/* Grid geometry: a large quad on the XZ plane (Y=0). */
#define GRID_HALF_SIZE 50.0f
#define GRID_NUM_VERTS 4
#define GRID_NUM_INDICES 6

/* Grid appearance (linear space for SDR_LINEAR swapchain). */
#define GRID_LINE_R 0.068f
#define GRID_LINE_G 0.534f
#define GRID_LINE_B 0.932f
#define GRID_LINE_A 1.0f

#define GRID_BG_R 0.014f
#define GRID_BG_G 0.014f
#define GRID_BG_B 0.045f
#define GRID_BG_A 1.0f

#define GRID_SPACING 1.0f
#define GRID_LINE_WIDTH 0.02f
#define GRID_FADE_DIST 40.0f
#define GRID_AMBIENT 0.3f
#define GRID_SHININESS 32.0f
#define GRID_SPECULAR_STR 0.2f

/* ── Scene pipeline constants ────────────────────────────────────────── */

#define SCENE_NUM_VERTEX_ATTRIBUTES 3 /* position, normal, UV */

#define SCENE_VERT_NUM_SAMPLERS 0
#define SCENE_VERT_NUM_STORAGE_TEXTURES 0
#define SCENE_VERT_NUM_STORAGE_BUFFERS 0
#define SCENE_VERT_NUM_UNIFORM_BUFFERS 2 /* MVP+model, light_vps */

#define SCENE_FRAG_NUM_SAMPLERS 4 /* diffuse + 3 shadow maps */
#define SCENE_FRAG_NUM_STORAGE_TEXTURES 0
#define SCENE_FRAG_NUM_STORAGE_BUFFERS 0
#define SCENE_FRAG_NUM_UNIFORM_BUFFERS 1

/* ── Shadow pipeline constants ───────────────────────────────────────── */

#define SHADOW_NUM_VERTEX_ATTRIBUTES 3 /* same layout as scene (pos+norm+uv) */

#define SHADOW_VERT_NUM_SAMPLERS 0
#define SHADOW_VERT_NUM_STORAGE_TEXTURES 0
#define SHADOW_VERT_NUM_STORAGE_BUFFERS 0
#define SHADOW_VERT_NUM_UNIFORM_BUFFERS 1 /* light_mvp */

#define SHADOW_FRAG_NUM_SAMPLERS 0
#define SHADOW_FRAG_NUM_STORAGE_TEXTURES 0
#define SHADOW_FRAG_NUM_STORAGE_BUFFERS 0
#define SHADOW_FRAG_NUM_UNIFORM_BUFFERS 0

/* ── Debug pipeline constants ────────────────────────────────────────── */

#define DEBUG_VERT_NUM_SAMPLERS 0
#define DEBUG_VERT_NUM_STORAGE_TEXTURES 0
#define DEBUG_VERT_NUM_STORAGE_BUFFERS 0
#define DEBUG_VERT_NUM_UNIFORM_BUFFERS 1 /* quad_bounds */

#define DEBUG_FRAG_NUM_SAMPLERS 1 /* shadow map 0 */
#define DEBUG_FRAG_NUM_STORAGE_TEXTURES 0
#define DEBUG_FRAG_NUM_STORAGE_BUFFERS 0
#define DEBUG_FRAG_NUM_UNIFORM_BUFFERS 0

#define DEBUG_QUAD_VERTICES 6 /* 2 triangles = 6 vertices */

/* Debug quad NDC bounds — full screen for shadow map visualization */
#define DEBUG_QUAD_LEFT -1.0f
#define DEBUG_QUAD_BOTTOM -1.0f
#define DEBUG_QUAD_RIGHT 1.0f
#define DEBUG_QUAD_TOP 1.0f

/* ── Scene layout constants ──────────────────────────────────────────── */

/* Boxes around the truck: 8 ground-level + 4 stacked. */
#define BOX_GROUND_COUNT 8
#define BOX_STACK_COUNT 4
#define BOX_TOTAL_COUNT (BOX_GROUND_COUNT + BOX_STACK_COUNT)
#define BOX_GROUND_Y 0.5f
#define BOX_STACK_Y 1.5f
#define BOX_RING_RADIUS 5.0f
#define BOX_GROUND_ROT_OFFSET 0.3f /* per-box rotation increment (radians) */
#define BOX_STACK_ROT_OFFSET 0.5f  /* extra rotation for stacked boxes (radians) */

/* ── Model paths ─────────────────────────────────────────────────────── */

#define TRUCK_MODEL_PATH "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH "assets/models/BoxTextured/BoxTextured.gltf"
#define PATH_BUFFER_SIZE 512

/* ── Texture constants ───────────────────────────────────────────────── */

#define BYTES_PER_PIXEL 4
#define WHITE_TEX_DIM 1
#define WHITE_TEX_LAYERS 1
#define WHITE_TEX_LEVELS 1
#define WHITE_RGBA 255
#define MAX_LOD_UNLIMITED 1000.0f

/* ── Camera parameters ───────────────────────────────────────────────── */

#define CAM_START_X -6.1f
#define CAM_START_Y 7.0f
#define CAM_START_Z 4.4f
#define CAM_START_YAW -50.0f   /* degrees — look toward center */
#define CAM_START_PITCH -50.0f /* degrees — looking down at the scene */

#define MOVE_SPEED 5.0f
#define MOUSE_SENSITIVITY 0.002f
#define MAX_PITCH_DEG 89.0f

#define FOV_DEG 60.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE 100.0f

#define MS_TO_SEC 1000.0f
#define MAX_DELTA_TIME 0.1f

/* ── Lighting parameters ─────────────────────────────────────────────── */

#define LIGHT_DIR_X 1.0f
#define LIGHT_DIR_Y 1.0f
#define LIGHT_DIR_Z 0.5f

#define MODEL_SHININESS 64.0f
#define MODEL_AMBIENT_STR 0.15f
#define MODEL_SPECULAR_STR 0.5f

/* ── Shadow / light-VP computation constants ────────────────────────── */

#define AABB_INIT_MIN  1e30f   /* large sentinel for AABB min initialization */
#define AABB_INIT_MAX -1e30f   /* large negative sentinel for AABB max initialization */
#define LIGHT_DISTANCE 50.0f   /* how far back to place the light from cascade center */
#define SHADOW_Z_PADDING 50.0f /* extra Z range to capture casters behind the frustum */

/* ── Uniform data ────────────────────────────────────────────────────── */

/* Shadow vertex: just the light's MVP (64 bytes). */
typedef struct ShadowVertUniforms {
  mat4 light_mvp;
} ShadowVertUniforms;

/* Scene vertex: camera MVP + model matrix (128 bytes). */
typedef struct SceneVertUniforms {
  mat4 mvp;
  mat4 model;
} SceneVertUniforms;

/* Light VP matrices for all 3 cascades (192 bytes). */
typedef struct ShadowMatrices {
  mat4 light_vp[NUM_CASCADES];
} ShadowMatrices;

/* Scene fragment: lighting + shadow parameters (96 bytes). */
typedef struct SceneFragUniforms {
  float base_color[4];
  float light_dir[4];
  float eye_pos[4];
  /* View-space split depths for selecting which cascade to sample.
   * x=cascade 0/1 boundary, y=1/2, z=2/far, w=unused. */
  float cascade_splits[4];
  Uint32 has_texture;
  float shininess;        /* Blinn-Phong specular exponent (higher = tighter) */
  float ambient;          /* ambient light strength [0..1] */
  float specular_str;     /* specular highlight intensity [0..1] */
  float shadow_texel_size; /* 1.0 / shadow_map_resolution — PCF offset step */
  float shadow_bias;       /* depth bias to prevent shadow acne */
  float _pad0;             /* explicit padding to 16-byte alignment */
  float _pad1;             /* explicit padding to 16-byte alignment */
} SceneFragUniforms;

/* Grid vertex: VP matrix (64 bytes). */
typedef struct GridVertUniforms {
  mat4 vp;
} GridVertUniforms;

/* Grid fragment: appearance + shadow parameters (112 bytes). */
typedef struct GridFragUniforms {
  float line_color[4];
  float bg_color[4];
  float light_dir[4];
  float eye_pos[4];
  /* Same cascade split depths as SceneFragUniforms */
  float cascade_splits[4];
  float grid_spacing;       /* world-space distance between grid lines */
  float line_width;         /* grid line thickness in world units */
  float fade_distance;      /* distance at which grid fades to background */
  float ambient;            /* ambient light strength [0..1] */
  float shininess;          /* Blinn-Phong specular exponent */
  float specular_str;       /* specular highlight intensity [0..1] */
  float shadow_texel_size;  /* 1.0 / shadow_map_resolution — PCF offset step */
  float shadow_bias;        /* depth bias to prevent shadow acne */
} GridFragUniforms;

/* Debug quad vertex: NDC bounds (16 bytes). */
typedef struct DebugVertUniforms {
  /* NDC rectangle for the debug overlay: left, bottom, right, top */
  float quad_bounds[4];
} DebugVertUniforms;

/* ── GPU-side scene data ─────────────────────────────────────────────── */

typedef struct GpuPrimitive {
  SDL_GPUBuffer *vertex_buffer;
  SDL_GPUBuffer *index_buffer;
  Uint32 index_count;
  int material_index;
  SDL_GPUIndexElementSize index_type;
  bool has_uvs;
} GpuPrimitive;

typedef struct GpuMaterial {
  float base_color[4];
  SDL_GPUTexture *texture;
  bool has_texture;
} GpuMaterial;

/* ── Per-model data ──────────────────────────────────────────────────── */

typedef struct ModelData {
  ForgeGltfScene scene;
  GpuPrimitive *primitives;
  int primitive_count;
  GpuMaterial *materials;
  int material_count;
} ModelData;

/* ── Box placement ───────────────────────────────────────────────────── */

typedef struct BoxPlacement {
  vec3 position;
  float y_rotation;
} BoxPlacement;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
  SDL_Window *window;
  SDL_GPUDevice *device;

  /* Four pipelines: shadow, scene, grid, debug */
  SDL_GPUGraphicsPipeline *shadow_pipeline;
  SDL_GPUGraphicsPipeline *scene_pipeline;
  SDL_GPUGraphicsPipeline *grid_pipeline;
  SDL_GPUGraphicsPipeline *debug_pipeline;

  /* Shadow map textures — one per cascade */
  SDL_GPUTexture *shadow_maps[NUM_CASCADES];

  /* Shadow sampler: NEAREST filter, CLAMP_TO_EDGE */
  SDL_GPUSampler *shadow_sampler;

  /* Grid geometry */
  SDL_GPUBuffer *grid_vertex_buffer;
  SDL_GPUBuffer *grid_index_buffer;

  /* Shared resources */
  SDL_GPUTexture *depth_texture;
  SDL_GPUSampler *sampler;
  SDL_GPUTexture *white_texture;
  Uint32 depth_width;
  Uint32 depth_height;

  /* Two models loaded from glTF */
  ModelData truck;
  ModelData box;

  /* Pre-computed box placements (model matrices built each frame) */
  BoxPlacement box_placements[BOX_TOTAL_COUNT];
  int box_count;

  /* Camera state */
  vec3 cam_position;
  float cam_yaw;
  float cam_pitch;

  /* Timing */
  Uint64 last_ticks;

  /* Input */
  bool mouse_captured;

  /* Debug: show shadow map overlay */
  bool show_shadow_map;

#ifdef FORGE_CAPTURE
  ForgeCapture capture;
#endif
} app_state;

/* ── Depth texture helper ────────────────────────────────────────────── */

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device, Uint32 w, Uint32 h) {
  SDL_GPUTextureCreateInfo info;
  SDL_zero(info);
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = DEPTH_FORMAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  info.width = w;
  info.height = h;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &info);
  if (!texture) {
    SDL_Log("Failed to create depth texture (%ux%u): %s", w, h, SDL_GetError());
  }
  return texture;
}

/* ── Shadow map texture helper ───────────────────────────────────────── */
/* Shadow maps need DEPTH_STENCIL_TARGET (for writing during shadow pass)
 * AND SAMPLER (for reading in the main pass).  This combination is what
 * distinguishes a shadow map from a normal depth buffer. */

static SDL_GPUTexture *create_shadow_map(SDL_GPUDevice *device) {
  SDL_GPUTextureCreateInfo info;
  SDL_zero(info);
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SHADOW_MAP_FORMAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = SHADOW_MAP_SIZE;
  info.height = SHADOW_MAP_SIZE;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &info);
  if (!texture) {
    SDL_Log(
        "Failed to create shadow map (%dx%d): %s",
        SHADOW_MAP_SIZE,
        SHADOW_MAP_SIZE,
        SDL_GetError()
    );
  }
  return texture;
}

/* ── Shader helper ───────────────────────────────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const unsigned char *spirv_code,
    unsigned int spirv_size,
    const unsigned char *dxil_code,
    unsigned int dxil_size,
    int num_samplers,
    int num_storage_textures,
    int num_storage_buffers,
    int num_uniform_buffers
) {
  SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

  SDL_GPUShaderCreateInfo info;
  SDL_zero(info);
  info.stage = stage;
  info.entrypoint = "main";
  info.num_samplers = num_samplers;
  info.num_storage_textures = num_storage_textures;
  info.num_storage_buffers = num_storage_buffers;
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
    SDL_Log("No supported shader format (need SPIRV or DXIL)");
    return NULL;
  }

  SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
  if (!shader) {
    SDL_Log(
        "Failed to create %s shader: %s",
        stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
        SDL_GetError()
    );
  }
  return shader;
}

/* ── GPU buffer upload helper ────────────────────────────────────────── */

static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage, const void *data, Uint32 size
) {
  SDL_GPUBufferCreateInfo buf_info;
  SDL_zero(buf_info);
  buf_info.usage = usage;
  buf_info.size = size;

  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
  if (!buffer) {
    SDL_Log("Failed to create GPU buffer: %s", SDL_GetError());
    return NULL;
  }

  SDL_GPUTransferBufferCreateInfo xfer_info;
  SDL_zero(xfer_info);
  xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xfer_info.size = size;

  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
  if (!transfer) {
    SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (!mapped) {
    SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }
  SDL_memcpy(mapped, data, size);
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  if (!cmd) {
    SDL_Log("Failed to acquire cmd for buffer upload: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
  if (!copy) {
    SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
    SDL_CancelGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  SDL_GPUTransferBufferLocation src;
  SDL_zero(src);
  src.transfer_buffer = transfer;

  SDL_GPUBufferRegion dst;
  SDL_zero(dst);
  dst.buffer = buffer;
  dst.size = size;

  SDL_UploadToGPUBuffer(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return buffer;
}

/* ── Texture loading helper ──────────────────────────────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path) {
  SDL_Surface *surface = SDL_LoadSurface(path);
  if (!surface) {
    SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
    return NULL;
  }
  SDL_Log("Loaded texture: %dx%d from '%s'", surface->w, surface->h, path);

  SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
  SDL_DestroySurface(surface);
  if (!converted) {
    SDL_Log("Failed to convert surface: %s", SDL_GetError());
    return NULL;
  }

  int tex_w = converted->w;
  int tex_h = converted->h;
  int num_levels = (int)forge_log2f((float)(tex_w > tex_h ? tex_w : tex_h)) + 1;

  SDL_GPUTextureCreateInfo tex_info;
  SDL_zero(tex_info);
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  tex_info.width = (Uint32)tex_w;
  tex_info.height = (Uint32)tex_h;
  tex_info.layer_count_or_depth = 1;
  tex_info.num_levels = num_levels;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
  if (!texture) {
    SDL_Log("Failed to create GPU texture: %s", SDL_GetError());
    SDL_DestroySurface(converted);
    return NULL;
  }

  Uint32 total_bytes = (Uint32)(tex_w * tex_h * BYTES_PER_PIXEL);

  SDL_GPUTransferBufferCreateInfo xfer_info;
  SDL_zero(xfer_info);
  xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xfer_info.size = total_bytes;

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

  Uint32 dest_row_bytes = (Uint32)(tex_w * BYTES_PER_PIXEL);
  const Uint8 *row_src = (const Uint8 *)converted->pixels;
  Uint8 *row_dst = (Uint8 *)mapped;
  {
    Uint32 row;
    for (row = 0; row < (Uint32)tex_h; row++) {
      SDL_memcpy(row_dst + row * dest_row_bytes, row_src + row * converted->pitch, dest_row_bytes);
    }
  }
  SDL_UnmapGPUTransferBuffer(device, transfer);
  SDL_DestroySurface(converted);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  if (!cmd) {
    SDL_Log("Failed to acquire cmd for texture upload: %s", SDL_GetError());
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
  tex_src.pixels_per_row = (Uint32)tex_w;
  tex_src.rows_per_layer = (Uint32)tex_h;

  SDL_GPUTextureRegion tex_dst;
  SDL_zero(tex_dst);
  tex_dst.texture = texture;
  tex_dst.w = (Uint32)tex_w;
  tex_dst.h = (Uint32)tex_h;
  tex_dst.d = 1;

  SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
  SDL_EndGPUCopyPass(copy_pass);

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

/* ── 1x1 white placeholder texture ──────────────────────────────────── */

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device) {
  SDL_GPUTextureCreateInfo tex_info;
  SDL_zero(tex_info);
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  tex_info.width = WHITE_TEX_DIM;
  tex_info.height = WHITE_TEX_DIM;
  tex_info.layer_count_or_depth = WHITE_TEX_LAYERS;
  tex_info.num_levels = WHITE_TEX_LEVELS;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
  if (!texture) {
    SDL_Log("Failed to create white texture: %s", SDL_GetError());
    return NULL;
  }

  Uint8 white_pixel[BYTES_PER_PIXEL] = { WHITE_RGBA, WHITE_RGBA, WHITE_RGBA, WHITE_RGBA };

  SDL_GPUTransferBufferCreateInfo xfer_info;
  SDL_zero(xfer_info);
  xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xfer_info.size = sizeof(white_pixel);

  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
  if (!transfer) {
    SDL_Log("Failed to create white texture transfer: %s", SDL_GetError());
    SDL_ReleaseGPUTexture(device, texture);
    return NULL;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (!mapped) {
    SDL_Log("Failed to map white texture transfer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTexture(device, texture);
    return NULL;
  }
  SDL_memcpy(mapped, white_pixel, sizeof(white_pixel));
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  if (!cmd) {
    SDL_Log("Failed to acquire cmd for white texture: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTexture(device, texture);
    return NULL;
  }

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
  if (!copy) {
    SDL_Log("Failed to begin copy pass for white texture: %s", SDL_GetError());
    SDL_CancelGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTexture(device, texture);
    return NULL;
  }

  SDL_GPUTextureTransferInfo src;
  SDL_zero(src);
  src.transfer_buffer = transfer;

  SDL_GPUTextureRegion dst;
  SDL_zero(dst);
  dst.texture = texture;
  dst.w = WHITE_TEX_DIM;
  dst.h = WHITE_TEX_DIM;
  dst.d = 1;

  SDL_UploadToGPUTexture(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("Failed to submit white texture upload: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTexture(device, texture);
    return NULL;
  }
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return texture;
}

/* ── Free GPU-side model resources ───────────────────────────────────── */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model) {
  if (model->primitives) {
    int i;
    for (i = 0; i < model->primitive_count; i++) {
      if (model->primitives[i].vertex_buffer)
        SDL_ReleaseGPUBuffer(device, model->primitives[i].vertex_buffer);
      if (model->primitives[i].index_buffer)
        SDL_ReleaseGPUBuffer(device, model->primitives[i].index_buffer);
    }
    SDL_free(model->primitives);
    model->primitives = NULL;
  }

  if (model->materials) {
    SDL_GPUTexture *released[FORGE_GLTF_MAX_IMAGES];
    int released_count = 0;
    int i;
    SDL_memset(released, 0, sizeof(released));

    for (i = 0; i < model->material_count; i++) {
      SDL_GPUTexture *tex = model->materials[i].texture;
      int j;
      bool already;
      if (!tex)
        continue;

      already = false;
      for (j = 0; j < released_count; j++) {
        if (released[j] == tex) {
          already = true;
          break;
        }
      }
      if (!already && released_count < FORGE_GLTF_MAX_IMAGES) {
        SDL_ReleaseGPUTexture(device, tex);
        released[released_count++] = tex;
      }
    }
    SDL_free(model->materials);
    model->materials = NULL;
  }
}

/* ── Upload parsed scene to GPU ──────────────────────────────────────── */

static bool
upload_model_to_gpu(SDL_GPUDevice *device, ModelData *model, SDL_GPUTexture *white_texture) {
  ForgeGltfScene *scene = &model->scene;
  int i;

  /* Upload primitives (vertex + index buffers) */
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

  /* Load material textures */
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

      SDL_Log(
          "  Material %d: '%s' color=(%.2f,%.2f,%.2f) tex=%s",
          i,
          src->name,
          dst->base_color[0],
          dst->base_color[1],
          dst->base_color[2],
          dst->has_texture ? "yes" : "no"
      );
    }
  }

  (void)white_texture;
  return true;
}

/* ── Upload grid geometry to GPU ─────────────────────────────────────── */

static bool upload_grid_geometry(SDL_GPUDevice *device, app_state *state) {
  float vertices[GRID_NUM_VERTS * 3] = {
    -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE, GRID_HALF_SIZE,  0.0f, -GRID_HALF_SIZE,
    GRID_HALF_SIZE,  0.0f, GRID_HALF_SIZE,  -GRID_HALF_SIZE, 0.0f, GRID_HALF_SIZE,
  };

  Uint16 indices[GRID_NUM_INDICES] = { 0, 1, 2, 0, 2, 3 };

  state->grid_vertex_buffer =
      upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, sizeof(vertices));
  if (!state->grid_vertex_buffer)
    return false;

  state->grid_index_buffer =
      upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
  if (!state->grid_index_buffer) {
    SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    state->grid_vertex_buffer = NULL;
    return false;
  }

  return true;
}

/* ── Generate box placements ─────────────────────────────────────────── */
/* 8 boxes in a ring around the origin + 4 stacked on selected boxes. */

static void generate_box_placements(app_state *state) {
  int idx = 0;
  int i;

  /* Ground-level ring of boxes */
  for (i = 0; i < BOX_GROUND_COUNT; i++) {
    float angle = (float)i * (2.0f * FORGE_PI / (float)BOX_GROUND_COUNT);
    state->box_placements[idx].position =
        vec3_create(cosf(angle) * BOX_RING_RADIUS, BOX_GROUND_Y, sinf(angle) * BOX_RING_RADIUS);
    state->box_placements[idx].y_rotation = angle + BOX_GROUND_ROT_OFFSET * (float)i;
    idx++;
  }

  /* Stacked boxes on top of every other ground box */
  for (i = 0; i < BOX_STACK_COUNT; i++) {
    int base = i * 2; /* stack on boxes 0, 2, 4, 6 */
    state->box_placements[idx].position = vec3_create(
        state->box_placements[base].position.x,
        BOX_STACK_Y,
        state->box_placements[base].position.z
    );
    state->box_placements[idx].y_rotation = state->box_placements[base].y_rotation + BOX_STACK_ROT_OFFSET;
    idx++;
  }

  state->box_count = idx;
}

/* ── Load and set up one model ───────────────────────────────────────── */

static bool setup_model(
    SDL_GPUDevice *device,
    ModelData *model,
    const char *gltf_path,
    const char *name,
    SDL_GPUTexture *white_texture
) {
  SDL_Log("Loading %s from '%s'...", name, gltf_path);

  if (!forge_gltf_load(gltf_path, &model->scene)) {
    SDL_Log("Failed to load %s from '%s'", name, gltf_path);
    return false;
  }

  SDL_Log(
      "%s scene: %d nodes, %d meshes, %d primitives, %d materials",
      name,
      model->scene.node_count,
      model->scene.mesh_count,
      model->scene.primitive_count,
      model->scene.material_count
  );

  if (!upload_model_to_gpu(device, model, white_texture)) {
    SDL_Log("Failed to upload %s to GPU", name);
    forge_gltf_free(&model->scene);
    return false;
  }

  return true;
}

/* ── Cascade split computation ───────────────────────────────────────── */
/* Uses Lengyel's logarithmic-linear blend to compute cascade split
 * distances.  Pure logarithmic distributes resolution more evenly in
 * log-space (good for close objects), while linear is more uniform.
 * Lambda = 0.5 blends between the two for a practical balance. */

static void compute_cascade_splits(float near_plane, float far_plane, float splits[NUM_CASCADES]) {
  int i;
  for (i = 0; i < NUM_CASCADES; i++) {
    float p = (float)(i + 1) / (float)NUM_CASCADES;

    /* Logarithmic split: near * (far/near)^p */
    float log_split = near_plane * powf(far_plane / near_plane, p);

    /* Linear split: near + (far - near) * p */
    float lin_split = near_plane + (far_plane - near_plane) * p;

    /* Blend between log and linear */
    splits[i] = CASCADE_LAMBDA * log_split + (1.0f - CASCADE_LAMBDA) * lin_split;
  }
}

/* ── Compute light VP matrix for one cascade ─────────────────────────── */
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

  /* Unproject all NDC corners to world space */
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
      /* Lerp between near plane corner and far plane corner */
      cascade_corners[i] = vec3_lerp(world_corners[i], world_corners[i + NUM_NEAR_CORNERS], t_near);
      cascade_corners[i + NUM_NEAR_CORNERS] = vec3_lerp(world_corners[i], world_corners[i + NUM_NEAR_CORNERS], t_far);
    }
  }

  /* Compute the center of the cascade frustum slice */
  vec3 center = vec3_create(0.0f, 0.0f, 0.0f);
  {
    int i;
    for (i = 0; i < NUM_FRUSTUM_CORNERS; i++) {
      center = vec3_add(center, cascade_corners[i]);
    }
    center = vec3_scale(center, 1.0f / (float)NUM_FRUSTUM_CORNERS);
  }

  /* Build a light view matrix looking from above the center toward center.
   * light_dir points toward the light, so placing the camera at
   * center + light_dir * LIGHT_DISTANCE looks back down at the scene. */
  vec3 light_pos = vec3_add(center, vec3_scale(light_dir, LIGHT_DISTANCE));
  mat4 light_view = mat4_look_at(light_pos, center, vec3_create(0.0f, 1.0f, 0.0f));

  /* Transform cascade corners to light view space and find AABB */
  float min_x = AABB_INIT_MIN, max_x = AABB_INIT_MAX;
  float min_y = AABB_INIT_MIN, max_y = AABB_INIT_MAX;
  float min_z = AABB_INIT_MIN, max_z = AABB_INIT_MAX;
  {
    int i;
    for (i = 0; i < 8; i++) {
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

  /* Expand the Z range to capture shadow casters behind the frustum.
   * Without this, objects outside the cascade slice but between the
   * light and the frustum would not cast shadows into the frustum. */
  min_z -= SHADOW_Z_PADDING;

  /* Build orthographic projection from the tight AABB */
  mat4 light_proj = mat4_orthographic(min_x, max_x, min_y, max_y, -max_z, -min_z);

  return mat4_multiply(light_proj, light_view);
}

/* ── Draw a model for the shadow pass ────────────────────────────────── */
/* Renders all primitives of a model into the current shadow map using
 * the shadow pipeline.  The placement matrix positions the object in the
 * scene (translation + rotation); each node's world_transform handles
 * the glTF hierarchy (so multi-node models like the truck assemble
 * correctly).  The final transform is light_vp * placement * node. */

static void draw_model_shadow(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    mat4 placement,
    mat4 light_vp
) {
  const ForgeGltfScene *scene = &model->scene;
  int ni;

  for (ni = 0; ni < scene->node_count; ni++) {
    const ForgeGltfNode *node = &scene->nodes[ni];
    int pi;
    if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
      continue;

    {
      /* Per-node model matrix: placement * node's own hierarchy transform */
      mat4 model_matrix = mat4_multiply(placement, node->world_transform);
      mat4 light_mvp = mat4_multiply(light_vp, model_matrix);

      ShadowVertUniforms svu;
      svu.light_mvp = light_mvp;
      SDL_PushGPUVertexUniformData(cmd, 0, &svu, sizeof(svu));

      const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
      for (pi = 0; pi < mesh->primitive_count; pi++) {
        int prim_idx = mesh->first_primitive + pi;
        const GpuPrimitive *prim = &model->primitives[prim_idx];
        SDL_GPUBufferBinding vb_binding;
        SDL_GPUBufferBinding ib_binding;

        if (!prim->vertex_buffer || !prim->index_buffer)
          continue;

        SDL_zero(vb_binding);
        vb_binding.buffer = prim->vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

        SDL_zero(ib_binding);
        ib_binding.buffer = prim->index_buffer;
        SDL_BindGPUIndexBuffer(pass, &ib_binding, prim->index_type);

        SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
      }
    }
  }
}

/* ── Draw a model for the main scene pass ────────────────────────────── */
/* Renders all primitives with Blinn-Phong lighting and shadow receiving. */

static void draw_model_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    mat4 placement,
    mat4 cam_vp,
    const ShadowMatrices *shadow_mats,
    const vec3 *light_dir,
    const float *cascade_splits
) {
  const ForgeGltfScene *scene = &model->scene;
  int ni;

  for (ni = 0; ni < scene->node_count; ni++) {
    const ForgeGltfNode *node = &scene->nodes[ni];
    int pi;
    if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
      continue;

    {
      /* Per-node model matrix: placement * node's glTF hierarchy transform.
       * This is critical for multi-node models like CesiumMilkTruck where
       * each part (body, wheels, tank) has its own transform in the
       * glTF node hierarchy. */
      mat4 model_matrix = mat4_multiply(placement, node->world_transform);
      mat4 mvp = mat4_multiply(cam_vp, model_matrix);

      /* Push vertex uniforms: MVP + model matrix (per node) */
      SceneVertUniforms svu;
      svu.mvp = mvp;
      svu.model = model_matrix;
      SDL_PushGPUVertexUniformData(cmd, 0, &svu, sizeof(svu));

      /* Push shadow matrices (slot 1) */
      SDL_PushGPUVertexUniformData(cmd, 1, shadow_mats, sizeof(*shadow_mats));

      const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
      for (pi = 0; pi < mesh->primitive_count; pi++) {
        int prim_idx = mesh->first_primitive + pi;
        const GpuPrimitive *prim = &model->primitives[prim_idx];
        SceneFragUniforms fu;
        SDL_GPUTexture *tex;
        SDL_GPUTextureSamplerBinding tex_bindings[4];
        SDL_GPUBufferBinding vb_binding;
        SDL_GPUBufferBinding ib_binding;

        if (!prim->vertex_buffer || !prim->index_buffer)
          continue;

        /* Set up fragment uniforms */
        tex = state->white_texture;

        if (prim->material_index >= 0 && prim->material_index < model->material_count) {
          const GpuMaterial *mat = &model->materials[prim->material_index];
          fu.base_color[0] = mat->base_color[0];
          fu.base_color[1] = mat->base_color[1];
          fu.base_color[2] = mat->base_color[2];
          fu.base_color[3] = mat->base_color[3];
          fu.has_texture = mat->has_texture ? 1 : 0;
          if (mat->texture)
            tex = mat->texture;
        } else {
          fu.base_color[0] = 1.0f;
          fu.base_color[1] = 1.0f;
          fu.base_color[2] = 1.0f;
          fu.base_color[3] = 1.0f;
          fu.has_texture = 0;
        }

        fu.light_dir[0] = light_dir->x;
        fu.light_dir[1] = light_dir->y;
        fu.light_dir[2] = light_dir->z;
        fu.light_dir[3] = 0.0f;

        fu.eye_pos[0] = state->cam_position.x;
        fu.eye_pos[1] = state->cam_position.y;
        fu.eye_pos[2] = state->cam_position.z;
        fu.eye_pos[3] = 0.0f;

        fu.cascade_splits[0] = cascade_splits[0];
        fu.cascade_splits[1] = cascade_splits[1];
        fu.cascade_splits[2] = cascade_splits[2];
        fu.cascade_splits[3] = 0.0f;

        fu.shininess = MODEL_SHININESS;
        fu.ambient = MODEL_AMBIENT_STR;
        fu.specular_str = MODEL_SPECULAR_STR;
        fu.shadow_texel_size = SHADOW_TEXEL_SIZE;
        fu.shadow_bias = SHADOW_BIAS;
        fu._pad0 = 0.0f;
        fu._pad1 = 0.0f;

        SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

        /* Bind samplers: slot 0 = diffuse, slots 1-3 = shadow maps */
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

        /* Bind vertex and index buffers */
        SDL_zero(vb_binding);
        vb_binding.buffer = prim->vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

        SDL_zero(ib_binding);
        ib_binding.buffer = prim->index_buffer;
        SDL_BindGPUIndexBuffer(pass, &ib_binding, prim->index_type);

        SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
      }
    }
  }
}

/* ── SDL_AppInit ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  int i;

  /* ── 1. Initialise SDL ────────────────────────────────────────────── */
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* ── 2. Create GPU device ─────────────────────────────────────────── */
  SDL_GPUDevice *device = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
      true, /* debug mode */
      NULL  /* no backend preference */
  );
  if (!device) {
    SDL_Log("Failed to create GPU device: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

  /* ── 3. Create window & claim swapchain ───────────────────────────── */
  SDL_Window *window =
      SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
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

  /* ── 4. Request an sRGB swapchain ─────────────────────────────────── */
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
      SDL_ReleaseWindowFromGPUDevice(device, window);
      SDL_DestroyWindow(window);
      SDL_DestroyGPUDevice(device);
      return SDL_APP_FAILURE;
    }
  }

  SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

  /* ── 5. Create depth texture ──────────────────────────────────────── */
  int win_w = 0, win_h = 0;
  if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture *depth_texture = create_depth_texture(device, (Uint32)win_w, (Uint32)win_h);
  if (!depth_texture) {
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* ── 6. Create shadow map textures ────────────────────────────────── */
  SDL_GPUTexture *shadow_maps[NUM_CASCADES];
  for (i = 0; i < NUM_CASCADES; i++) {
    shadow_maps[i] = create_shadow_map(device);
    if (!shadow_maps[i]) {
      int j;
      for (j = 0; j < i; j++) {
        SDL_ReleaseGPUTexture(device, shadow_maps[j]);
      }
      SDL_ReleaseGPUTexture(device, depth_texture);
      SDL_ReleaseWindowFromGPUDevice(device, window);
      SDL_DestroyWindow(window);
      SDL_DestroyGPUDevice(device);
      return SDL_APP_FAILURE;
    }
  }

  /* ── 7. Create white placeholder texture ──────────────────────────── */
  SDL_GPUTexture *white_texture = create_white_texture(device);
  if (!white_texture) {
    for (i = 0; i < NUM_CASCADES; i++)
      SDL_ReleaseGPUTexture(device, shadow_maps[i]);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* ── 8. Create samplers ───────────────────────────────────────────── */

  /* Standard texture sampler (linear filtering + mipmaps). */
  SDL_GPUSamplerCreateInfo smp_info;
  SDL_zero(smp_info);
  smp_info.min_filter = SDL_GPU_FILTER_LINEAR;
  smp_info.mag_filter = SDL_GPU_FILTER_LINEAR;
  smp_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  smp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  smp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  smp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  smp_info.min_lod = 0.0f;
  smp_info.max_lod = MAX_LOD_UNLIMITED;

  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &smp_info);
  if (!sampler) {
    SDL_Log("Failed to create sampler: %s", SDL_GetError());
    SDL_ReleaseGPUTexture(device, white_texture);
    for (i = 0; i < NUM_CASCADES; i++)
      SDL_ReleaseGPUTexture(device, shadow_maps[i]);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* Shadow sampler: NEAREST filter, CLAMP_TO_EDGE to avoid sampling
   * outside the shadow map (which would give incorrect shadow results). */
  SDL_GPUSamplerCreateInfo shadow_smp_info;
  SDL_zero(shadow_smp_info);
  shadow_smp_info.min_filter = SDL_GPU_FILTER_NEAREST;
  shadow_smp_info.mag_filter = SDL_GPU_FILTER_NEAREST;
  shadow_smp_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  shadow_smp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  shadow_smp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  shadow_smp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  SDL_GPUSampler *shadow_sampler = SDL_CreateGPUSampler(device, &shadow_smp_info);
  if (!shadow_sampler) {
    SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
    SDL_ReleaseGPUSampler(device, sampler);
    SDL_ReleaseGPUTexture(device, white_texture);
    for (i = 0; i < NUM_CASCADES; i++)
      SDL_ReleaseGPUTexture(device, shadow_maps[i]);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* ── 9. Allocate app state ────────────────────────────────────────── */
  app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
  if (!state) {
    SDL_Log("Failed to allocate app state");
    SDL_ReleaseGPUSampler(device, shadow_sampler);
    SDL_ReleaseGPUSampler(device, sampler);
    SDL_ReleaseGPUTexture(device, white_texture);
    for (i = 0; i < NUM_CASCADES; i++)
      SDL_ReleaseGPUTexture(device, shadow_maps[i]);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }
  state->window = window;
  state->device = device;
  state->depth_texture = depth_texture;
  state->sampler = sampler;
  state->shadow_sampler = shadow_sampler;
  state->white_texture = white_texture;
  state->depth_width = (Uint32)win_w;
  state->depth_height = (Uint32)win_h;
  for (i = 0; i < NUM_CASCADES; i++) {
    state->shadow_maps[i] = shadow_maps[i];
  }

  /* Parse CLI flags */
  state->show_shadow_map = false;
  for (i = 1; i < argc; i++) {
    if (SDL_strcmp(argv[i], "--show-shadow-map") == 0) {
      state->show_shadow_map = true;
    }
  }

  /* ── 10. Load both glTF models ────────────────────────────────────── */
  const char *base_path = SDL_GetBasePath();
  if (!base_path) {
    SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
    goto fail_cleanup;
  }

  {
    char truck_path[PATH_BUFFER_SIZE];
    char box_path[PATH_BUFFER_SIZE];
    int len;

    len = SDL_snprintf(truck_path, sizeof(truck_path), "%s%s", base_path, TRUCK_MODEL_PATH);
    if (len < 0 || (size_t)len >= sizeof(truck_path)) {
      SDL_Log("Truck model path too long");
      goto fail_cleanup;
    }

    len = SDL_snprintf(box_path, sizeof(box_path), "%s%s", base_path, BOX_MODEL_PATH);
    if (len < 0 || (size_t)len >= sizeof(box_path)) {
      SDL_Log("Box model path too long");
      goto fail_cleanup;
    }

    if (!setup_model(device, &state->truck, truck_path, "CesiumMilkTruck", white_texture)) {
      goto fail_cleanup;
    }

    if (!setup_model(device, &state->box, box_path, "BoxTextured", white_texture)) {
      free_model_gpu(device, &state->truck);
      forge_gltf_free(&state->truck.scene);
      goto fail_cleanup;
    }
  }

  /* Generate box placement data */
  generate_box_placements(state);

  /* ── 11. Upload grid geometry ─────────────────────────────────────── */
  if (!upload_grid_geometry(device, state)) {
    SDL_Log("Failed to upload grid geometry");
    free_model_gpu(device, &state->box);
    forge_gltf_free(&state->box.scene);
    free_model_gpu(device, &state->truck);
    forge_gltf_free(&state->truck.scene);
    goto fail_cleanup;
  }

  /* ── 12. Create shadow pipeline ───────────────────────────────────── */
  {
    SDL_GPUShader *shadow_vs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv,
        shadow_vert_spirv_size,
        shadow_vert_dxil,
        shadow_vert_dxil_size,
        SHADOW_VERT_NUM_SAMPLERS,
        SHADOW_VERT_NUM_STORAGE_TEXTURES,
        SHADOW_VERT_NUM_STORAGE_BUFFERS,
        SHADOW_VERT_NUM_UNIFORM_BUFFERS
    );
    SDL_GPUShader *shadow_fs;
    SDL_GPUVertexBufferDescription shadow_vb_desc;
    SDL_GPUVertexAttribute shadow_attrs[SHADOW_NUM_VERTEX_ATTRIBUTES];
    SDL_GPUGraphicsPipelineCreateInfo shadow_pipe;

    if (!shadow_vs)
      goto fail_pipelines;

    shadow_fs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv,
        shadow_frag_spirv_size,
        shadow_frag_dxil,
        shadow_frag_dxil_size,
        SHADOW_FRAG_NUM_SAMPLERS,
        SHADOW_FRAG_NUM_STORAGE_TEXTURES,
        SHADOW_FRAG_NUM_STORAGE_BUFFERS,
        SHADOW_FRAG_NUM_UNIFORM_BUFFERS
    );
    if (!shadow_fs) {
      SDL_ReleaseGPUShader(device, shadow_vs);
      goto fail_pipelines;
    }

    /* Same vertex layout as ForgeGltfVertex — shadow shader only uses
     * position but all 3 attributes must match the pipeline layout. */
    SDL_zero(shadow_vb_desc);
    shadow_vb_desc.slot = 0;
    shadow_vb_desc.pitch = sizeof(ForgeGltfVertex);
    shadow_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_zero(shadow_attrs);
    shadow_attrs[0].location = 0;
    shadow_attrs[0].buffer_slot = 0;
    shadow_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shadow_attrs[0].offset = offsetof(ForgeGltfVertex, position);

    shadow_attrs[1].location = 1;
    shadow_attrs[1].buffer_slot = 0;
    shadow_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shadow_attrs[1].offset = offsetof(ForgeGltfVertex, normal);

    shadow_attrs[2].location = 2;
    shadow_attrs[2].buffer_slot = 0;
    shadow_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    shadow_attrs[2].offset = offsetof(ForgeGltfVertex, uv);

    SDL_zero(shadow_pipe);
    shadow_pipe.vertex_shader = shadow_vs;
    shadow_pipe.fragment_shader = shadow_fs;

    shadow_pipe.vertex_input_state.vertex_buffer_descriptions = &shadow_vb_desc;
    shadow_pipe.vertex_input_state.num_vertex_buffers = 1;
    shadow_pipe.vertex_input_state.vertex_attributes = shadow_attrs;
    shadow_pipe.vertex_input_state.num_vertex_attributes = SHADOW_NUM_VERTEX_ATTRIBUTES;

    shadow_pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Front-face culling: render back faces only during shadow pass.
     * This pushes the shadow slightly away from the surface, reducing
     * "peter-panning" artifacts where shadows detach from objects. */
    shadow_pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    shadow_pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
    shadow_pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth bias helps prevent shadow acne on surfaces nearly parallel
     * to the light direction. */
    shadow_pipe.rasterizer_state.depth_bias_constant_factor = SHADOW_DEPTH_BIAS;
    shadow_pipe.rasterizer_state.depth_bias_slope_factor = SHADOW_SLOPE_BIAS;

    shadow_pipe.depth_stencil_state.enable_depth_test = true;
    shadow_pipe.depth_stencil_state.enable_depth_write = true;
    shadow_pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    /* Depth-only: no color targets, only depth. */
    shadow_pipe.target_info.num_color_targets = 0;
    shadow_pipe.target_info.has_depth_stencil_target = true;
    shadow_pipe.target_info.depth_stencil_format = SHADOW_MAP_FORMAT;

    state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &shadow_pipe);
    if (!state->shadow_pipeline) {
      SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
      SDL_ReleaseGPUShader(device, shadow_fs);
      SDL_ReleaseGPUShader(device, shadow_vs);
      goto fail_pipelines;
    }

    SDL_ReleaseGPUShader(device, shadow_fs);
    SDL_ReleaseGPUShader(device, shadow_vs);
  }

  /* ── 13. Create scene pipeline ────────────────────────────────────── */
  {
    SDL_GPUShader *scene_vs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv,
        scene_vert_spirv_size,
        scene_vert_dxil,
        scene_vert_dxil_size,
        SCENE_VERT_NUM_SAMPLERS,
        SCENE_VERT_NUM_STORAGE_TEXTURES,
        SCENE_VERT_NUM_STORAGE_BUFFERS,
        SCENE_VERT_NUM_UNIFORM_BUFFERS
    );
    SDL_GPUShader *scene_fs;
    SDL_GPUVertexBufferDescription scene_vb_desc;
    SDL_GPUVertexAttribute scene_attrs[SCENE_NUM_VERTEX_ATTRIBUTES];
    SDL_GPUGraphicsPipelineCreateInfo scene_pipe;
    SDL_GPUColorTargetDescription scene_color;

    if (!scene_vs)
      goto fail_pipelines;

    scene_fs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv,
        scene_frag_spirv_size,
        scene_frag_dxil,
        scene_frag_dxil_size,
        SCENE_FRAG_NUM_SAMPLERS,
        SCENE_FRAG_NUM_STORAGE_TEXTURES,
        SCENE_FRAG_NUM_STORAGE_BUFFERS,
        SCENE_FRAG_NUM_UNIFORM_BUFFERS
    );
    if (!scene_fs) {
      SDL_ReleaseGPUShader(device, scene_vs);
      goto fail_pipelines;
    }

    SDL_zero(scene_vb_desc);
    scene_vb_desc.slot = 0;
    scene_vb_desc.pitch = sizeof(ForgeGltfVertex);
    scene_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_zero(scene_attrs);
    scene_attrs[0].location = 0;
    scene_attrs[0].buffer_slot = 0;
    scene_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    scene_attrs[0].offset = offsetof(ForgeGltfVertex, position);

    scene_attrs[1].location = 1;
    scene_attrs[1].buffer_slot = 0;
    scene_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    scene_attrs[1].offset = offsetof(ForgeGltfVertex, normal);

    scene_attrs[2].location = 2;
    scene_attrs[2].buffer_slot = 0;
    scene_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    scene_attrs[2].offset = offsetof(ForgeGltfVertex, uv);

    SDL_zero(scene_pipe);
    scene_pipe.vertex_shader = scene_vs;
    scene_pipe.fragment_shader = scene_fs;

    scene_pipe.vertex_input_state.vertex_buffer_descriptions = &scene_vb_desc;
    scene_pipe.vertex_input_state.num_vertex_buffers = 1;
    scene_pipe.vertex_input_state.vertex_attributes = scene_attrs;
    scene_pipe.vertex_input_state.num_vertex_attributes = SCENE_NUM_VERTEX_ATTRIBUTES;

    scene_pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* Back-face culling for solid objects. */
    scene_pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    scene_pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    scene_pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth testing ensures correct front-to-back ordering in the 3D scene.
     * LESS_OR_EQUAL allows coplanar surfaces (e.g. the grid on the ground
     * plane) to render without z-fighting. */
    scene_pipe.depth_stencil_state.enable_depth_test = true;
    scene_pipe.depth_stencil_state.enable_depth_write = true;
    scene_pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_zero(scene_color);
    /* Color format must match the swapchain to avoid conversion overhead. */
    scene_color.format = swapchain_format;

    scene_pipe.target_info.color_target_descriptions = &scene_color;
    scene_pipe.target_info.num_color_targets = 1;
    scene_pipe.target_info.has_depth_stencil_target = true;
    /* D32_FLOAT gives full 32-bit precision for depth — important for
     * shadow map comparison and large view distances. */
    scene_pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

    state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &scene_pipe);
    if (!state->scene_pipeline) {
      SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
      SDL_ReleaseGPUShader(device, scene_fs);
      SDL_ReleaseGPUShader(device, scene_vs);
      goto fail_pipelines;
    }

    SDL_ReleaseGPUShader(device, scene_fs);
    SDL_ReleaseGPUShader(device, scene_vs);
  }

  /* ── 14. Create grid pipeline ─────────────────────────────────────── */
  {
    SDL_GPUShader *grid_vs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv,
        grid_vert_spirv_size,
        grid_vert_dxil,
        grid_vert_dxil_size,
        GRID_VERT_NUM_SAMPLERS,
        GRID_VERT_NUM_STORAGE_TEXTURES,
        GRID_VERT_NUM_STORAGE_BUFFERS,
        GRID_VERT_NUM_UNIFORM_BUFFERS
    );
    SDL_GPUShader *grid_fs;
    SDL_GPUVertexBufferDescription grid_vb_desc;
    SDL_GPUVertexAttribute grid_attrs[GRID_NUM_VERTEX_ATTRIBUTES];
    SDL_GPUGraphicsPipelineCreateInfo grid_pipe;
    SDL_GPUColorTargetDescription grid_color;

    if (!grid_vs)
      goto fail_pipelines;

    grid_fs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv,
        grid_frag_spirv_size,
        grid_frag_dxil,
        grid_frag_dxil_size,
        GRID_FRAG_NUM_SAMPLERS,
        GRID_FRAG_NUM_STORAGE_TEXTURES,
        GRID_FRAG_NUM_STORAGE_BUFFERS,
        GRID_FRAG_NUM_UNIFORM_BUFFERS
    );
    if (!grid_fs) {
      SDL_ReleaseGPUShader(device, grid_vs);
      goto fail_pipelines;
    }

    SDL_zero(grid_vb_desc);
    grid_vb_desc.slot = 0;
    grid_vb_desc.pitch = GRID_VERTEX_PITCH;
    grid_vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_zero(grid_attrs);
    grid_attrs[0].location = 0;
    grid_attrs[0].buffer_slot = 0;
    grid_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    grid_attrs[0].offset = 0;

    SDL_zero(grid_pipe);
    grid_pipe.vertex_shader = grid_vs;
    grid_pipe.fragment_shader = grid_fs;

    grid_pipe.vertex_input_state.vertex_buffer_descriptions = &grid_vb_desc;
    grid_pipe.vertex_input_state.num_vertex_buffers = 1;
    grid_pipe.vertex_input_state.vertex_attributes = grid_attrs;
    grid_pipe.vertex_input_state.num_vertex_attributes = GRID_NUM_VERTEX_ATTRIBUTES;

    grid_pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    /* No culling for grid — visible from both sides. */
    grid_pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    grid_pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    grid_pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* Depth test + write so the grid occludes and is occluded by scene
     * geometry.  LESS_OR_EQUAL (not LESS) lets the grid coexist with
     * fragments at the same depth — avoids z-fighting at floor level. */
    grid_pipe.depth_stencil_state.enable_depth_test = true;
    grid_pipe.depth_stencil_state.enable_depth_write = true;
    grid_pipe.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_zero(grid_color);
    /* Must match the swapchain format so the grid renders into the same
     * color target as the scene pass (queried after sRGB configuration). */
    grid_color.format = swapchain_format;

    grid_pipe.target_info.color_target_descriptions = &grid_color;
    grid_pipe.target_info.num_color_targets = 1;
    grid_pipe.target_info.has_depth_stencil_target = true;
    /* Share the scene's depth buffer so grid depth-tests against models. */
    grid_pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

    state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &grid_pipe);
    if (!state->grid_pipeline) {
      SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
      SDL_ReleaseGPUShader(device, grid_fs);
      SDL_ReleaseGPUShader(device, grid_vs);
      goto fail_pipelines;
    }

    SDL_ReleaseGPUShader(device, grid_fs);
    SDL_ReleaseGPUShader(device, grid_vs);
  }

  /* ── 15. Create debug quad pipeline ───────────────────────────────── */
  {
    SDL_GPUShader *debug_vs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        debug_quad_vert_spirv,
        debug_quad_vert_spirv_size,
        debug_quad_vert_dxil,
        debug_quad_vert_dxil_size,
        DEBUG_VERT_NUM_SAMPLERS,
        DEBUG_VERT_NUM_STORAGE_TEXTURES,
        DEBUG_VERT_NUM_STORAGE_BUFFERS,
        DEBUG_VERT_NUM_UNIFORM_BUFFERS
    );
    SDL_GPUShader *debug_fs;
    SDL_GPUGraphicsPipelineCreateInfo debug_pipe;
    SDL_GPUColorTargetDescription debug_color;

    if (!debug_vs)
      goto fail_pipelines;

    debug_fs = create_shader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        debug_quad_frag_spirv,
        debug_quad_frag_spirv_size,
        debug_quad_frag_dxil,
        debug_quad_frag_dxil_size,
        DEBUG_FRAG_NUM_SAMPLERS,
        DEBUG_FRAG_NUM_STORAGE_TEXTURES,
        DEBUG_FRAG_NUM_STORAGE_BUFFERS,
        DEBUG_FRAG_NUM_UNIFORM_BUFFERS
    );
    if (!debug_fs) {
      SDL_ReleaseGPUShader(device, debug_vs);
      goto fail_pipelines;
    }

    SDL_zero(debug_pipe);
    debug_pipe.vertex_shader = debug_vs;
    debug_pipe.fragment_shader = debug_fs;

    /* No vertex input — positions generated from SV_VertexID. */
    debug_pipe.vertex_input_state.num_vertex_buffers = 0;
    debug_pipe.vertex_input_state.num_vertex_attributes = 0;

    debug_pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    debug_pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    debug_pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    debug_pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    /* No depth test — overlay draws on top of everything. */
    debug_pipe.depth_stencil_state.enable_depth_test = false;
    debug_pipe.depth_stencil_state.enable_depth_write = false;

    SDL_zero(debug_color);
    debug_color.format = swapchain_format;

    debug_pipe.target_info.color_target_descriptions = &debug_color;
    debug_pipe.target_info.num_color_targets = 1;
    debug_pipe.target_info.has_depth_stencil_target = true;
    debug_pipe.target_info.depth_stencil_format = DEPTH_FORMAT;

    state->debug_pipeline = SDL_CreateGPUGraphicsPipeline(device, &debug_pipe);
    if (!state->debug_pipeline) {
      SDL_Log("Failed to create debug pipeline: %s", SDL_GetError());
      SDL_ReleaseGPUShader(device, debug_fs);
      SDL_ReleaseGPUShader(device, debug_vs);
      goto fail_pipelines;
    }

    SDL_ReleaseGPUShader(device, debug_fs);
    SDL_ReleaseGPUShader(device, debug_vs);
  }

  /* ── 16. Camera and input setup ───────────────────────────────────── */
  state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
  state->cam_yaw = CAM_START_YAW * FORGE_DEG2RAD;
  state->cam_pitch = CAM_START_PITCH * FORGE_DEG2RAD;
  state->last_ticks = SDL_GetTicks();

#ifndef FORGE_CAPTURE
  if (!SDL_SetWindowRelativeMouseMode(window, true)) {
    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    goto fail_pipelines;
  }
  state->mouse_captured = true;
#else
  state->mouse_captured = false;
#endif

#ifdef FORGE_CAPTURE
  forge_capture_parse_args(&state->capture, argc, argv);
  if (state->capture.mode != FORGE_CAPTURE_NONE) {
    if (!forge_capture_init(&state->capture, device, window)) {
      SDL_Log("Failed to initialise capture");
      goto fail_pipelines;
    }
  }
#endif

  *appstate = state;

  SDL_Log("Controls: WASD=move, Mouse=look, Space=up, LShift=down, Esc=quit");
  SDL_Log(
      "Shadow maps: %d cascades @ %dx%d, PCF 3x3",
      NUM_CASCADES,
      SHADOW_MAP_SIZE,
      SHADOW_MAP_SIZE
  );
  if (state->show_shadow_map) {
    SDL_Log("Debug: shadow map overlay enabled (--show-shadow-map)");
  }

  return SDL_APP_CONTINUE;

fail_pipelines:
  /* Clean up pipelines that were successfully created. */
  if (state->debug_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(device, state->debug_pipeline);
  if (state->grid_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
  if (state->scene_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(device, state->scene_pipeline);
  if (state->shadow_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(device, state->shadow_pipeline);

  SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
  SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
  free_model_gpu(device, &state->box);
  forge_gltf_free(&state->box.scene);
  free_model_gpu(device, &state->truck);
  forge_gltf_free(&state->truck.scene);

fail_cleanup:
  SDL_ReleaseGPUSampler(device, state->shadow_sampler);
  SDL_ReleaseGPUSampler(device, state->sampler);
  SDL_ReleaseGPUTexture(device, state->white_texture);
  for (i = 0; i < NUM_CASCADES; i++) {
    if (state->shadow_maps[i])
      SDL_ReleaseGPUTexture(device, state->shadow_maps[i]);
  }
  SDL_ReleaseGPUTexture(device, state->depth_texture);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyWindow(window);
  SDL_DestroyGPUDevice(device);
  SDL_free(state);
  return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  app_state *state = (app_state *)appstate;

  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  /* Escape: release mouse or quit. */
  if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
    if (state->mouse_captured) {
      if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
      }
      state->mouse_captured = false;
    } else {
      return SDL_APP_SUCCESS;
    }
  }

  /* Click to recapture mouse. */
  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
    if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
      SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    state->mouse_captured = true;
  }

  /* Mouse motion: update camera yaw and pitch. */
  if (event->type == SDL_EVENT_MOUSE_MOTION && state->mouse_captured) {
    state->cam_yaw -= event->motion.xrel * MOUSE_SENSITIVITY;
    state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;

    {
      float max_pitch = MAX_PITCH_DEG * FORGE_DEG2RAD;
      if (state->cam_pitch > max_pitch)
        state->cam_pitch = max_pitch;
      if (state->cam_pitch < -max_pitch)
        state->cam_pitch = -max_pitch;
    }
  }

  return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate) {
  app_state *state = (app_state *)appstate;
  int ci, bi;

  /* ── 1. Compute delta time ────────────────────────────────────────── */
  Uint64 now_ms = SDL_GetTicks();
  float dt = (float)(now_ms - state->last_ticks) / MS_TO_SEC;
  state->last_ticks = now_ms;
  if (dt > MAX_DELTA_TIME)
    dt = MAX_DELTA_TIME;

  /* ── 2. Process keyboard input ────────────────────────────────────── */
  {
    quat cam_orientation = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(cam_orientation);
    vec3 right = quat_right(cam_orientation);
    const bool *keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(forward, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(forward, -MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(right, MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
      state->cam_position = vec3_add(state->cam_position, vec3_scale(right, -MOVE_SPEED * dt));
    }
    if (keys[SDL_SCANCODE_SPACE]) {
      state->cam_position = vec3_add(state->cam_position, vec3_create(0.0f, MOVE_SPEED * dt, 0.0f));
    }
    if (keys[SDL_SCANCODE_LSHIFT]) {
      state->cam_position =
          vec3_add(state->cam_position, vec3_create(0.0f, -MOVE_SPEED * dt, 0.0f));
    }
  }

  /* ── 3. Build view-projection matrix ──────────────────────────────── */
  mat4 view;
  mat4 proj;
  mat4 cam_vp;
  mat4 inv_cam_vp;
  float aspect;
  float fov;
  int w = 0, h = 0;

  {
    quat cam_orientation = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    view = mat4_view_from_quat(state->cam_position, cam_orientation);
  }

  if (!SDL_GetWindowSizeInPixels(state->window, &w, &h)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  aspect = (h > 0) ? (float)w / (float)h : 1.0f;
  fov = FOV_DEG * FORGE_DEG2RAD;
  proj = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);
  cam_vp = mat4_multiply(proj, view);

  /* Inverse VP is needed to unproject frustum corners for cascade computation */
  inv_cam_vp = mat4_inverse(cam_vp);

  /* ── 4. Handle window resize ──────────────────────────────────────── */
  {
    Uint32 cur_w = (Uint32)w;
    Uint32 cur_h = (Uint32)h;

    if (cur_w != state->depth_width || cur_h != state->depth_height) {
      /* Skip resize when the window is minimized (zero dimensions).
       * Keep the existing depth texture until a valid size is available. */
      if (cur_w == 0 || cur_h == 0) {
        return SDL_APP_CONTINUE;
      }
      SDL_ReleaseGPUTexture(state->device, state->depth_texture);
      state->depth_texture = create_depth_texture(state->device, cur_w, cur_h);
      if (!state->depth_texture) {
        return SDL_APP_FAILURE;
      }
      state->depth_width = cur_w;
      state->depth_height = cur_h;
    }
  }

  /* ── 5. Compute cascade splits and light VP matrices ──────────────── */
  float cascade_splits[NUM_CASCADES];
  compute_cascade_splits(NEAR_PLANE, FAR_PLANE, cascade_splits);

  vec3 light_raw = vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z);
  vec3 light_dir = vec3_normalize(light_raw);

  ShadowMatrices shadow_mats;
  {
    float prev_split = NEAR_PLANE;
    for (ci = 0; ci < NUM_CASCADES; ci++) {
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

  /* ── 6. Acquire command buffer ────────────────────────────────────── */
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
  if (!cmd) {
    SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* ── 7. Shadow passes — one per cascade ───────────────────────────── */
  /* The truck placement is identity — glTF node transforms position each
   * part (body, wheels, tank) within the model's coordinate system.
   * Boxes use a placement transform (translation + rotation) to scatter
   * them around the scene. */
  mat4 truck_placement = mat4_identity();

  for (ci = 0; ci < NUM_CASCADES; ci++) {
    SDL_GPUDepthStencilTargetInfo shadow_depth;
    SDL_GPURenderPass *shadow_pass;

    SDL_zero(shadow_depth);
    shadow_depth.texture = state->shadow_maps[ci];
    shadow_depth.load_op = SDL_GPU_LOADOP_CLEAR;
    shadow_depth.store_op = SDL_GPU_STOREOP_STORE; /* MUST store — sampled later */
    shadow_depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    shadow_depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    shadow_depth.clear_depth = DEPTH_CLEAR;

    /* Begin depth-only render pass (no color targets). */
    shadow_pass = SDL_BeginGPURenderPass(cmd, NULL, 0, &shadow_depth);
    if (!shadow_pass) {
      SDL_Log("Failed to begin shadow pass %d: %s", ci, SDL_GetError());
      SDL_CancelGPUCommandBuffer(cmd);
      return SDL_APP_FAILURE;
    }

    SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

    /* Draw truck into shadow map */
    draw_model_shadow(shadow_pass, cmd, &state->truck, truck_placement, shadow_mats.light_vp[ci]);

    /* Draw all boxes into shadow map */
    for (bi = 0; bi < state->box_count; bi++) {
      mat4 t = mat4_translate(state->box_placements[bi].position);
      mat4 r = mat4_rotate_y(state->box_placements[bi].y_rotation);
      mat4 box_placement = mat4_multiply(t, r);

      draw_model_shadow(shadow_pass, cmd, &state->box, box_placement, shadow_mats.light_vp[ci]);
    }

    SDL_EndGPURenderPass(shadow_pass);
  }

  /* ── 8. Acquire swapchain & begin main render pass ────────────────── */
  {
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL)) {
      SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
      SDL_CancelGPUCommandBuffer(cmd);
      return SDL_APP_FAILURE;
    }

    if (swapchain) {
      SDL_GPUColorTargetInfo color_target;
      SDL_GPUDepthStencilTargetInfo depth_target;
      SDL_GPURenderPass *pass;

      SDL_zero(color_target);
      color_target.texture = swapchain;
      color_target.load_op = SDL_GPU_LOADOP_CLEAR;
      color_target.store_op = SDL_GPU_STOREOP_STORE;
      color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A };

      SDL_zero(depth_target);
      depth_target.texture = state->depth_texture;
      depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
      depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
      depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
      depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
      depth_target.clear_depth = DEPTH_CLEAR;

      pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
      if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
      }

      /* ── Draw grid ────────────────────────────────────────────── */
      SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

      {
        GridVertUniforms gvu;
        GridFragUniforms gfu;
        SDL_GPUTextureSamplerBinding shadow_bindings[3];
        SDL_GPUBufferBinding grid_vb;
        SDL_GPUBufferBinding grid_ib;

        gvu.vp = cam_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));
        SDL_PushGPUVertexUniformData(cmd, 1, &shadow_mats, sizeof(shadow_mats));

        gfu.line_color[0] = GRID_LINE_R;
        gfu.line_color[1] = GRID_LINE_G;
        gfu.line_color[2] = GRID_LINE_B;
        gfu.line_color[3] = GRID_LINE_A;

        gfu.bg_color[0] = GRID_BG_R;
        gfu.bg_color[1] = GRID_BG_G;
        gfu.bg_color[2] = GRID_BG_B;
        gfu.bg_color[3] = GRID_BG_A;

        gfu.light_dir[0] = light_dir.x;
        gfu.light_dir[1] = light_dir.y;
        gfu.light_dir[2] = light_dir.z;
        gfu.light_dir[3] = 0.0f;

        gfu.eye_pos[0] = state->cam_position.x;
        gfu.eye_pos[1] = state->cam_position.y;
        gfu.eye_pos[2] = state->cam_position.z;
        gfu.eye_pos[3] = 0.0f;

        gfu.cascade_splits[0] = cascade_splits[0];
        gfu.cascade_splits[1] = cascade_splits[1];
        gfu.cascade_splits[2] = cascade_splits[2];
        gfu.cascade_splits[3] = 0.0f;

        gfu.grid_spacing = GRID_SPACING;
        gfu.line_width = GRID_LINE_WIDTH;
        gfu.fade_distance = GRID_FADE_DIST;
        gfu.ambient = GRID_AMBIENT;
        gfu.shininess = GRID_SHININESS;
        gfu.specular_str = GRID_SPECULAR_STR;
        gfu.shadow_texel_size = SHADOW_TEXEL_SIZE;
        gfu.shadow_bias = SHADOW_BIAS;

        SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

        /* Bind shadow maps to fragment sampler slots 0-2 */
        SDL_zero(shadow_bindings);
        shadow_bindings[0].texture = state->shadow_maps[0];
        shadow_bindings[0].sampler = state->shadow_sampler;
        shadow_bindings[1].texture = state->shadow_maps[1];
        shadow_bindings[1].sampler = state->shadow_sampler;
        shadow_bindings[2].texture = state->shadow_maps[2];
        shadow_bindings[2].sampler = state->shadow_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, shadow_bindings, 3);

        SDL_zero(grid_vb);
        grid_vb.buffer = state->grid_vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &grid_vb, 1);

        SDL_zero(grid_ib);
        grid_ib.buffer = state->grid_index_buffer;
        SDL_BindGPUIndexBuffer(pass, &grid_ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES, 1, 0, 0, 0);
      }

      /* ── Draw scene objects ───────────────────────────────────── */
      SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);

      /* Draw truck */
      draw_model_scene(
          pass,
          cmd,
          &state->truck,
          state,
          truck_placement,
          cam_vp,
          &shadow_mats,
          &light_dir,
          cascade_splits
      );

      /* Draw all boxes */
      for (bi = 0; bi < state->box_count; bi++) {
        mat4 t = mat4_translate(state->box_placements[bi].position);
        mat4 r = mat4_rotate_y(state->box_placements[bi].y_rotation);
        mat4 box_placement = mat4_multiply(t, r);

        draw_model_scene(
            pass,
            cmd,
            &state->box,
            state,
            box_placement,
            cam_vp,
            &shadow_mats,
            &light_dir,
            cascade_splits
        );
      }

      /* ── Debug overlay ────────────────────────────────────────── */
      if (state->show_shadow_map) {
        DebugVertUniforms dvu;
        SDL_GPUTextureSamplerBinding debug_binding;

        SDL_BindGPUGraphicsPipeline(pass, state->debug_pipeline);

        dvu.quad_bounds[0] = DEBUG_QUAD_LEFT;
        dvu.quad_bounds[1] = DEBUG_QUAD_BOTTOM;
        dvu.quad_bounds[2] = DEBUG_QUAD_RIGHT;
        dvu.quad_bounds[3] = DEBUG_QUAD_TOP;
        SDL_PushGPUVertexUniformData(cmd, 0, &dvu, sizeof(dvu));

        SDL_zero(debug_binding);
        debug_binding.texture = state->shadow_maps[0];
        debug_binding.sampler = state->shadow_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &debug_binding, 1);

        SDL_DrawGPUPrimitives(pass, DEBUG_QUAD_VERTICES, 1, 0, 0);
      }

      SDL_EndGPURenderPass(pass);
    }

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE && swapchain) {
      if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
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
  }

  return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  (void)result;

  app_state *state = (app_state *)appstate;
  if (state) {
    int i;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, state->device);
#endif

    /* Release in reverse order of creation */
    free_model_gpu(state->device, &state->box);
    forge_gltf_free(&state->box.scene);
    free_model_gpu(state->device, &state->truck);
    forge_gltf_free(&state->truck.scene);

    SDL_ReleaseGPUBuffer(state->device, state->grid_index_buffer);
    SDL_ReleaseGPUBuffer(state->device, state->grid_vertex_buffer);

    SDL_ReleaseGPUSampler(state->device, state->shadow_sampler);
    SDL_ReleaseGPUSampler(state->device, state->sampler);
    SDL_ReleaseGPUTexture(state->device, state->white_texture);

    for (i = 0; i < NUM_CASCADES; i++) {
      SDL_ReleaseGPUTexture(state->device, state->shadow_maps[i]);
    }

    SDL_ReleaseGPUTexture(state->device, state->depth_texture);

    SDL_ReleaseGPUGraphicsPipeline(state->device, state->debug_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);
    SDL_free(state);
  }
}
