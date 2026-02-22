/*
 * GPU Lesson 23 — Point Lights & Shadows
 *
 * What this lesson teaches:
 *   1. Multiple point lights with per-light color and intensity
 *   2. Omnidirectional shadow mapping with cube map depth textures
 *   3. Shadow bias and Peter Panning prevention
 *   4. Quadratic attenuation falloff for point lights
 *   5. Building on HDR + Jimenez bloom from Lessons 21/22
 *
 * Scene:
 *   CesiumMilkTruck + BoxTextured ring on a procedural grid floor,
 *   lit by 4 colored point lights (visible as emissive spheres).
 *   Each light casts omnidirectional shadows via cube depth maps.
 *
 * Render passes (per frame):
 *   1. Shadow passes -> 4 cube maps (6 faces each, truck + boxes only)
 *   2. Scene pass -> HDR buffer (grid + truck + boxes + emissive spheres)
 *   3. Bloom downsample (5 passes) -> bloom mip chain
 *   4. Bloom upsample (4 passes) -> accumulate back up the chain
 *   5. Tone map pass -> swapchain (combine HDR + bloom, tone map)
 *
 * Controls:
 *   WASD / Space / LShift — Move camera
 *   Mouse                 — Look around
 *   1                     — Toggle light 0 (cyan, orbiting)
 *   2                     — Toggle light 1 (orange)
 *   3                     — Toggle light 2 (green)
 *   4                     — Toggle light 3 (purple)
 *   =/+                   — Increase exposure
 *   -                     — Decrease exposure
 *   B                     — Toggle bloom on/off
 *   Up/Down               — Bloom intensity +/-
 *   Left/Right            — Bloom threshold +/-
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

/* Scene shaders — Blinn-Phong with multiple point lights → HDR */
#include "shaders/compiled/scene_frag_dxil.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_vert_spirv.h"

/* Grid shaders — procedural grid with multiple point lights → HDR */
#include "shaders/compiled/grid_frag_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_vert_spirv.h"

/* Emissive shader — constant HDR emission (reuses scene vertex shader) */
#include "shaders/compiled/emissive_frag_dxil.h"
#include "shaders/compiled/emissive_frag_spirv.h"

/* Fullscreen vertex — shared by bloom downsample, upsample, and tonemap */
#include "shaders/compiled/fullscreen_vert_dxil.h"
#include "shaders/compiled/fullscreen_vert_spirv.h"

/* Bloom downsample — 13-tap Jimenez filter */
#include "shaders/compiled/bloom_downsample_frag_dxil.h"
#include "shaders/compiled/bloom_downsample_frag_spirv.h"

/* Bloom upsample — 9-tap tent filter */
#include "shaders/compiled/bloom_upsample_frag_dxil.h"
#include "shaders/compiled/bloom_upsample_frag_spirv.h"

/* Tone mapping — HDR + bloom → swapchain */
#include "shaders/compiled/tonemap_frag_dxil.h"
#include "shaders/compiled/tonemap_frag_spirv.h"

/* Shadow shaders — cube map depth rendering for point light shadows */
#include "shaders/compiled/shadow_frag_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_vert_spirv.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

/* Camera. */
#define FOV_DEG 60
#define NEAR_PLANE 0.1f
#define FAR_PLANE 100.0f
#define CAM_SPEED 5.0f
#define MOUSE_SENS 0.003f
#define PITCH_CLAMP 1.5f

/* Camera initial position — elevated, looking at the scene. */
#define CAM_START_X 5.0f
#define CAM_START_Y 4.0f
#define CAM_START_Z 8.0f
#define CAM_START_YAW_DEG 34.0f
#define CAM_START_PITCH_DEG -20.0f

/* Point lights — 4 colored lights at different positions. */
#define MAX_POINT_LIGHTS 4

/* Light 0: cyan (#4fc3f7), orbiting the scene. */
#define LIGHT0_COLOR_R 0.08f
#define LIGHT0_COLOR_G 0.55f
#define LIGHT0_COLOR_B 0.93f
#define LIGHT0_INTENSITY 8.0f
#define LIGHT0_ORBIT_RADIUS 4.0f
#define LIGHT0_ORBIT_HEIGHT 3.5f
#define LIGHT0_ORBIT_SPEED 0.5f

/* Light 1: orange (#ff7043), positioned to the right. */
#define LIGHT1_COLOR_R 1.00f
#define LIGHT1_COLOR_G 0.16f
#define LIGHT1_COLOR_B 0.05f
#define LIGHT1_INTENSITY 6.0f
#define LIGHT1_POS_X 6.0f
#define LIGHT1_POS_Y 2.5f
#define LIGHT1_POS_Z -3.0f

/* Light 2: green (#66bb6a), positioned behind. */
#define LIGHT2_COLOR_R 0.13f
#define LIGHT2_COLOR_G 0.51f
#define LIGHT2_COLOR_B 0.14f
#define LIGHT2_INTENSITY 5.0f
#define LIGHT2_POS_X -5.0f
#define LIGHT2_POS_Y 4.0f
#define LIGHT2_POS_Z -2.0f

/* Light 3: purple (#ab47bc), positioned in front. */
#define LIGHT3_COLOR_R 0.42f
#define LIGHT3_COLOR_G 0.06f
#define LIGHT3_COLOR_B 0.51f
#define LIGHT3_INTENSITY 6.0f
#define LIGHT3_POS_X 2.0f
#define LIGHT3_POS_Y 3.0f
#define LIGHT3_POS_Z 5.0f

/* Emissive sphere — visible representation of each point light source. */
#define SPHERE_RADIUS 0.2f
#define SPHERE_STACKS 12
#define SPHERE_SLICES 24
#define SPHERE_VERTEX_COUNT ((SPHERE_STACKS + 1) * (SPHERE_SLICES + 1))
#define SPHERE_INDEX_COUNT (SPHERE_STACKS * SPHERE_SLICES * 6)
#define EMISSION_SCALE 30.0f /* multiplied by light color for HDR glow */

/* Scene material defaults. */
#define MATERIAL_SHININESS 64.0f
#define MATERIAL_AMBIENT 0.02f
#define MATERIAL_SPECULAR_STR 1.0f
#define MAX_ANISOTROPY 4

/* Box layout — ring of boxes around the truck. */
#define BOX_GROUND_COUNT 8
#define BOX_STACK_COUNT 4
#define BOX_RING_RADIUS 5.0f
#define BOX_GROUND_Y 0.5f
#define BOX_STACK_Y 1.5f
#define BOX_STACK_ROTATION_OFFSET 0.5f
#define TOTAL_BOX_COUNT (BOX_GROUND_COUNT + BOX_STACK_COUNT)

/* HDR render target format. */
#define HDR_FORMAT SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT

/* Bloom mip chain — 5 levels of progressive half-resolution. */
#define BLOOM_MIP_COUNT 5

/* Bloom defaults. */
#define DEFAULT_BLOOM_INTENSITY 0.04f
#define BLOOM_INTENSITY_STEP 0.005f
#define MIN_BLOOM_INTENSITY 0.0f
#define MAX_BLOOM_INTENSITY 0.5f
#define DEFAULT_BLOOM_THRESHOLD 1.0f
#define BLOOM_THRESHOLD_STEP 0.1f
#define MIN_BLOOM_THRESHOLD 0.0f
#define MAX_BLOOM_THRESHOLD 10.0f

/* Exposure control. */
#define DEFAULT_EXPOSURE 1.0f
#define EXPOSURE_STEP 0.1f
#define MIN_EXPOSURE 0.1f
#define MAX_EXPOSURE 10.0f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* Fullscreen quad — SV_VertexID triangle, no vertex buffer. */
#define FULLSCREEN_QUAD_VERTS 3

/* Grid. */
#define GRID_INDEX_COUNT 6
#define GRID_HALF_SIZE 50.0f
#define GRID_SPACING 1.0f
#define GRID_LINE_WIDTH 0.02f
#define GRID_FADE_DISTANCE 40.0f
#define GRID_AMBIENT 0.02f
#define GRID_SHININESS 32.0f
#define GRID_SPECULAR_STR 0.5f

/* Shadow mapping — omnidirectional cube map shadows for point lights. */
#define SHADOW_MAP_SIZE 512
#define SHADOW_MAP_FORMAT SDL_GPU_TEXTUREFORMAT_R32_FLOAT
#define SHADOW_DEPTH_FORMAT SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define SHADOW_NEAR_PLANE 0.1f
#define SHADOW_FAR_PLANE 25.0f
#define CUBE_FACE_COUNT 6

/* HDR clear color — forge-gpu dark theme background. */
#define CLEAR_COLOR_R 0.008f
#define CLEAR_COLOR_G 0.008f
#define CLEAR_COLOR_B 0.026f
#define CLEAR_COLOR_A 1.0f

/* Grid colors (linear space). */
#define GRID_LINE_COLOR_R 0.15f
#define GRID_LINE_COLOR_G 0.55f
#define GRID_LINE_COLOR_B 0.85f
#define GRID_LINE_COLOR_A 1.0f
#define GRID_BG_COLOR_R 0.04f
#define GRID_BG_COLOR_G 0.04f
#define GRID_BG_COLOR_B 0.08f
#define GRID_BG_COLOR_A 1.0f

/* Model asset paths. */
#define TRUCK_MODEL_PATH "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH "assets/models/BoxTextured/BoxTextured.gltf"

#define BYTES_PER_PIXEL 4

/* ── Shared point light struct (matches HLSL PointLight) ──────────────────── */

typedef struct PointLight {
  float position[3]; /* world-space position    (12 bytes) */
  float intensity;   /* HDR brightness scalar    (4 bytes) */
  float color[3];    /* RGB light color         (12 bytes) */
  float _pad;        /* align to 32 bytes        (4 bytes) */
} PointLight;        /* 32 bytes total */

/* ── Uniform structures ──────────────────────────────────────────────────── */

/* Scene vertex uniforms — pushed per draw call (per node). */
typedef struct SceneVertUniforms {
  mat4 mvp;   /* model-view-projection matrix (64 bytes) */
  mat4 model; /* model (world) matrix         (64 bytes) */
} SceneVertUniforms; /* 128 bytes */

/* Scene fragment uniforms — point lights with shadow far plane. */
typedef struct SceneFragUniforms {
  float base_color[4];                  /* material RGBA            (16 bytes) */
  float eye_pos[3];                     /* camera position          (12 bytes) */
  float has_texture;                    /* > 0.5 = textured          (4 bytes) */
  float shininess;                      /* specular exponent          (4 bytes) */
  float ambient;                        /* ambient intensity          (4 bytes) */
  float specular_str;                   /* specular strength          (4 bytes) */
  float shadow_far_plane;               /* shadow map far plane       (4 bytes) */
  PointLight lights[MAX_POINT_LIGHTS];  /* point light array        (128 bytes) */
} SceneFragUniforms; /* 176 bytes */

/* Emissive fragment uniforms — just the emission color. */
typedef struct EmissiveFragUniforms {
  float emission_color[3]; /* HDR emission RGB (12 bytes) */
  float _pad;              /* pad to 16 bytes   (4 bytes) */
} EmissiveFragUniforms;    /* 16 bytes */

/* Grid vertex uniforms — one VP matrix. */
typedef struct GridVertUniforms {
  mat4 vp; /* view-projection matrix (64 bytes) */
} GridVertUniforms;  /* 64 bytes */

/* Grid fragment uniforms — point lights with shadow far plane. */
typedef struct GridFragUniforms {
  float line_color[4];                  /* grid line color        (16 bytes) */
  float bg_color[4];                    /* background color       (16 bytes) */
  float eye_pos[3];                     /* camera position        (12 bytes) */
  float grid_spacing;                   /* grid line spacing       (4 bytes) */
  float line_width;                     /* grid line thickness     (4 bytes) */
  float fade_distance;                  /* fade-out distance       (4 bytes) */
  float ambient;                        /* ambient term            (4 bytes) */
  float shininess;                      /* specular exponent       (4 bytes) */
  float specular_str;                   /* specular strength       (4 bytes) */
  float shadow_far_plane;               /* shadow map far plane    (4 bytes) */
  float _pad1;                          /*                         (4 bytes) */
  float _pad2;                          /*                         (4 bytes) */
  PointLight lights[MAX_POINT_LIGHTS];  /* point light array     (128 bytes) */
} GridFragUniforms;  /* 208 bytes */

/* Bloom downsample uniforms. */
typedef struct BloomDownsampleUniforms {
  float texel_size[2]; /* 1/source_width, 1/source_height           */
  float threshold;     /* brightness cutoff for bloom extraction     */
  float use_karis;     /* 1.0 = first pass (Karis + threshold), 0.0 = standard */
} BloomDownsampleUniforms; /* 16 bytes */

/* Bloom upsample uniforms. */
typedef struct BloomUpsampleUniforms {
  float texel_size[2]; /* 1/source_width, 1/source_height */
  float _pad[2];       /* pad to 16 bytes                 */
} BloomUpsampleUniforms; /* 16 bytes */

/* Tone map fragment uniforms (matches tonemap.frag.hlsl cbuffer). */
typedef struct TonemapFragUniforms {
  float exposure;        /* scene exposure multiplier        */
  float bloom_intensity; /* bloom contribution strength [0..1] */
  float _pad0;           /* pad to 16 bytes                  */
  float _pad1;           /* pad to 16 bytes                  */
} TonemapFragUniforms; /* 16 bytes */

/* Shadow vertex uniforms — light view-projection and model matrix. */
typedef struct ShadowVertUniforms {
  mat4 light_mvp; /* light VP * model matrix  (64 bytes) */
  mat4 model;     /* model (world) matrix     (64 bytes) */
} ShadowVertUniforms; /* 128 bytes */

/* Shadow fragment uniforms — light position and far plane. */
typedef struct ShadowFragUniforms {
  float light_pos[3]; /* world-space light position (12 bytes) */
  float far_plane;    /* shadow far plane distance   (4 bytes) */
} ShadowFragUniforms; /* 16 bytes */

/* ── GPU-side model types ─────────────────────────────────────────────────── */

typedef struct GpuPrimitive {
  SDL_GPUBuffer *vertex_buffer; /* GPU-side vertex data (position, normal, UV) */
  SDL_GPUBuffer *index_buffer;  /* GPU-side index data for indexed drawing     */
  Uint32 index_count;           /* number of indices in this primitive         */
  int material_index;           /* index into ModelData.materials (-1 = none)  */
  SDL_GPUIndexElementSize index_type; /* 16-bit or 32-bit indices              */
  bool has_uvs;                 /* true if vertices contain texture UVs        */
} GpuPrimitive;

typedef struct GpuMaterial {
  float base_color[4];     /* RGBA base color factor (linear, premultiplied) */
  SDL_GPUTexture *texture; /* diffuse/albedo texture (NULL if untextured)    */
  bool has_texture;        /* true if texture is valid and should be sampled */
} GpuMaterial;

typedef struct ModelData {
  ForgeGltfScene scene;        /* parsed glTF scene (CPU-side geometry + metadata) */
  GpuPrimitive *primitives;    /* GPU buffers for each mesh primitive              */
  int primitive_count;         /* number of primitives uploaded to GPU             */
  GpuMaterial *materials;      /* per-material colors and textures                */
  int material_count;          /* number of materials in the model                */
} ModelData;

typedef struct BoxPlacement {
  vec3 position;    /* world-space center of the box  */
  float y_rotation; /* rotation around Y axis (radians) */
} BoxPlacement;

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct app_state {
  SDL_Window *window;
  SDL_GPUDevice *device;

  /* Pipelines. */
  SDL_GPUGraphicsPipeline *scene_pipeline;
  SDL_GPUGraphicsPipeline *grid_pipeline;
  SDL_GPUGraphicsPipeline *emissive_pipeline;
  SDL_GPUGraphicsPipeline *shadow_pipeline;
  SDL_GPUGraphicsPipeline *downsample_pipeline;
  SDL_GPUGraphicsPipeline *upsample_pipeline;
  SDL_GPUGraphicsPipeline *tonemap_pipeline;

  /* HDR render target. */
  SDL_GPUTexture *hdr_target;
  SDL_GPUSampler *hdr_sampler;
  Uint32 hdr_width;
  Uint32 hdr_height;

  /* Depth buffer. */
  SDL_GPUTexture *depth_texture;
  Uint32 depth_width;
  Uint32 depth_height;

  /* Shadow mapping — one R32_FLOAT cube map per light + shared depth buffer. */
  SDL_GPUTexture *shadow_cubes[MAX_POINT_LIGHTS];
  SDL_GPUTexture *shadow_depth;
  SDL_GPUSampler *shadow_sampler;

  /* Bloom mip chain. */
  SDL_GPUTexture *bloom_mips[BLOOM_MIP_COUNT];
  Uint32 bloom_widths[BLOOM_MIP_COUNT];
  Uint32 bloom_heights[BLOOM_MIP_COUNT];
  SDL_GPUSampler *bloom_sampler;

  /* Grid geometry. */
  SDL_GPUBuffer *grid_vertex_buffer;
  SDL_GPUBuffer *grid_index_buffer;

  /* Emissive sphere geometry. */
  SDL_GPUBuffer *sphere_vertex_buffer;
  SDL_GPUBuffer *sphere_index_buffer;

  /* Textures and sampler. */
  SDL_GPUTexture *white_texture;
  SDL_GPUSampler *sampler;

  /* Models. */
  ModelData truck;
  ModelData box;
  BoxPlacement box_placements[TOTAL_BOX_COUNT];
  int box_count;

  /* Camera. */
  vec3 cam_position;
  float cam_yaw;
  float cam_pitch;

  /* HDR settings. */
  float exposure;

  /* Bloom settings. */
  bool bloom_enabled;
  float bloom_intensity;
  float bloom_threshold;

  /* Per-light toggles (1/2/3 keys). */
  bool light_enabled[MAX_POINT_LIGHTS];

  /* Point light animation. */
  float light0_angle;

  /* Timing and input. */
  Uint64 last_ticks;
  bool mouse_captured;

#ifdef FORGE_CAPTURE
  ForgeCapture capture;
#endif
} app_state;

/* ── Helper: fill point light array ───────────────────────────────────────── */

static void fill_lights(const app_state *state, PointLight lights[MAX_POINT_LIGHTS]) {
  SDL_memset(lights, 0, sizeof(PointLight) * MAX_POINT_LIGHTS);

  /* Light 0: cyan, orbiting. */
  lights[0].position[0] = LIGHT0_ORBIT_RADIUS * forge_cosf(state->light0_angle);
  lights[0].position[1] = LIGHT0_ORBIT_HEIGHT;
  lights[0].position[2] = LIGHT0_ORBIT_RADIUS * forge_sinf(state->light0_angle);
  lights[0].intensity = state->light_enabled[0] ? LIGHT0_INTENSITY : 0.0f;
  lights[0].color[0] = LIGHT0_COLOR_R;
  lights[0].color[1] = LIGHT0_COLOR_G;
  lights[0].color[2] = LIGHT0_COLOR_B;

  /* Light 1: orange, static. */
  lights[1].position[0] = LIGHT1_POS_X;
  lights[1].position[1] = LIGHT1_POS_Y;
  lights[1].position[2] = LIGHT1_POS_Z;
  lights[1].intensity = state->light_enabled[1] ? LIGHT1_INTENSITY : 0.0f;
  lights[1].color[0] = LIGHT1_COLOR_R;
  lights[1].color[1] = LIGHT1_COLOR_G;
  lights[1].color[2] = LIGHT1_COLOR_B;

  /* Light 2: green, static. */
  lights[2].position[0] = LIGHT2_POS_X;
  lights[2].position[1] = LIGHT2_POS_Y;
  lights[2].position[2] = LIGHT2_POS_Z;
  lights[2].intensity = state->light_enabled[2] ? LIGHT2_INTENSITY : 0.0f;
  lights[2].color[0] = LIGHT2_COLOR_R;
  lights[2].color[1] = LIGHT2_COLOR_G;
  lights[2].color[2] = LIGHT2_COLOR_B;

  /* Light 3: purple, static. */
  lights[3].position[0] = LIGHT3_POS_X;
  lights[3].position[1] = LIGHT3_POS_Y;
  lights[3].position[2] = LIGHT3_POS_Z;
  lights[3].intensity = state->light_enabled[3] ? LIGHT3_INTENSITY : 0.0f;
  lights[3].color[0] = LIGHT3_COLOR_R;
  lights[3].color[1] = LIGHT3_COLOR_G;
  lights[3].color[2] = LIGHT3_COLOR_B;
}

/* ── Helper: create HDR render target ─────────────────────────────────────── */

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

/* ── Helper: create bloom mip chain ───────────────────────────────────────── */

static bool create_bloom_mip_chain(app_state *state) {
  Uint32 w = state->hdr_width / 2;
  Uint32 h = state->hdr_height / 2;

  for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = HDR_FORMAT;
    info.width = w;
    info.height = h;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

    state->bloom_mips[i] = SDL_CreateGPUTexture(state->device, &info);
    if (!state->bloom_mips[i]) {
      SDL_Log("Failed to create bloom mip %d (%ux%u): %s", i, w, h, SDL_GetError());
      for (int j = 0; j < i; j++) {
        SDL_ReleaseGPUTexture(state->device, state->bloom_mips[j]);
        state->bloom_mips[j] = NULL;
      }
      return false;
    }

    state->bloom_widths[i] = w;
    state->bloom_heights[i] = h;
    w /= 2;
    h /= 2;
  }
  return true;
}

static void release_bloom_mip_chain(app_state *state) {
  for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
    if (state->bloom_mips[i]) {
      SDL_ReleaseGPUTexture(state->device, state->bloom_mips[i]);
      state->bloom_mips[i] = NULL;
    }
  }
}

/* ── Helper: create shader (SPIRV or DXIL) ────────────────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code, size_t dxil_size,
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

  SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
  if (!xfer) {
    SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
  if (!mapped) {
    SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return NULL;
  }
  SDL_memcpy(mapped, data, size);
  SDL_UnmapGPUTransferBuffer(device, xfer);

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

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path) {
  SDL_Surface *surface = SDL_LoadSurface(path);
  if (!surface) {
    SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
    return NULL;
  }

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

  {
    const Uint8 *row_src = (const Uint8 *)converted->pixels;
    Uint8 *row_dst = (Uint8 *)mapped;
    Uint32 row;
    for (row = 0; row < h; row++) {
      SDL_memcpy(row_dst + row * dest_row_bytes,
                 row_src + row * converted->pitch, dest_row_bytes);
    }
  }
  SDL_UnmapGPUTransferBuffer(device, xfer);
  SDL_DestroySurface(converted);

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
      if (model->primitives[i].vertex_buffer)
        SDL_ReleaseGPUBuffer(device, model->primitives[i].vertex_buffer);
      if (model->primitives[i].index_buffer)
        SDL_ReleaseGPUBuffer(device, model->primitives[i].index_buffer);
    }
    SDL_free(model->primitives);
    model->primitives = NULL;
  }

  if (model->materials) {
    for (int i = 0; i < model->material_count; i++) {
      if (!model->materials[i].texture)
        continue;
      bool already_released = false;
      for (int j = 0; j < i; j++) {
        if (model->materials[j].texture == model->materials[i].texture) {
          already_released = true;
          break;
        }
      }
      if (!already_released)
        SDL_ReleaseGPUTexture(device, model->materials[i].texture);
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

  model->material_count = scene->material_count;
  model->materials = (GpuMaterial *)SDL_calloc(
      (size_t)(scene->material_count > 0 ? scene->material_count : 1),
      sizeof(GpuMaterial));
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
    }
  }

  (void)white_texture;
  return true;
}

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

static bool upload_grid_geometry(SDL_GPUDevice *device, app_state *state) {
  float vertices[] = {
    -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE, GRID_HALF_SIZE,  0.0f, -GRID_HALF_SIZE,
    GRID_HALF_SIZE,  0.0f, GRID_HALF_SIZE,  -GRID_HALF_SIZE, 0.0f, GRID_HALF_SIZE,
  };
  Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };

  state->grid_vertex_buffer =
      upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, sizeof(vertices));
  if (!state->grid_vertex_buffer) return false;

  state->grid_index_buffer =
      upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
  if (!state->grid_index_buffer) return false;

  return true;
}

/* ── Helper: generate UV sphere ───────────────────────────────────────────── */

static bool generate_and_upload_sphere(SDL_GPUDevice *device, app_state *state) {
  ForgeGltfVertex vertices[SPHERE_VERTEX_COUNT];
  Uint16 indices[SPHERE_INDEX_COUNT];
  int vi = 0, ii = 0;

  for (int stack = 0; stack <= SPHERE_STACKS; stack++) {
    float phi = FORGE_PI * (float)stack / (float)SPHERE_STACKS;
    float sin_phi = forge_sinf(phi);
    float cos_phi = forge_cosf(phi);

    for (int slice = 0; slice <= SPHERE_SLICES; slice++) {
      float theta = 2.0f * FORGE_PI * (float)slice / (float)SPHERE_SLICES;
      float nx = sin_phi * forge_cosf(theta);
      float ny = cos_phi;
      float nz = sin_phi * forge_sinf(theta);

      vertices[vi].position = vec3_create(
          SPHERE_RADIUS * nx, SPHERE_RADIUS * ny, SPHERE_RADIUS * nz);
      vertices[vi].normal = vec3_create(nx, ny, nz);
      vertices[vi].uv.x = (float)slice / (float)SPHERE_SLICES;
      vertices[vi].uv.y = (float)stack / (float)SPHERE_STACKS;
      vi++;
    }
  }

  for (int stack = 0; stack < SPHERE_STACKS; stack++) {
    for (int slice = 0; slice < SPHERE_SLICES; slice++) {
      int tl = stack * (SPHERE_SLICES + 1) + slice;
      int tr = tl + 1;
      int bl = tl + (SPHERE_SLICES + 1);
      int br = bl + 1;
      indices[ii++] = (Uint16)tl;
      indices[ii++] = (Uint16)bl;
      indices[ii++] = (Uint16)tr;
      indices[ii++] = (Uint16)tr;
      indices[ii++] = (Uint16)bl;
      indices[ii++] = (Uint16)br;
    }
  }

  state->sphere_vertex_buffer = upload_gpu_buffer(
      device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, (Uint32)sizeof(vertices));
  if (!state->sphere_vertex_buffer) return false;

  state->sphere_index_buffer = upload_gpu_buffer(
      device, SDL_GPU_BUFFERUSAGE_INDEX, indices, (Uint32)sizeof(indices));
  if (!state->sphere_index_buffer) return false;

  return true;
}

/* ── Helper: generate box placements ──────────────────────────────────────── */

static void generate_box_placements(app_state *state) {
  int count = 0;
  for (int i = 0; i < BOX_GROUND_COUNT; i++) {
    float angle = (float)i * (2.0f * FORGE_PI / BOX_GROUND_COUNT);
    state->box_placements[count].position = vec3_create(
        BOX_RING_RADIUS * forge_cosf(angle), BOX_GROUND_Y,
        BOX_RING_RADIUS * forge_sinf(angle));
    state->box_placements[count].y_rotation = angle;
    count++;
  }
  for (int i = 0; i < BOX_STACK_COUNT; i++) {
    int base = i * 2;
    vec3 base_pos = state->box_placements[base].position;
    state->box_placements[count].position = vec3_create(base_pos.x, BOX_STACK_Y, base_pos.z);
    state->box_placements[count].y_rotation =
        state->box_placements[base].y_rotation + BOX_STACK_ROTATION_OFFSET;
    count++;
  }
  state->box_count = count;
}

/* ── Helper: create one R32_FLOAT cube map for shadow mapping ─────────────── */

static SDL_GPUTexture *create_shadow_cube(SDL_GPUDevice *device) {
  SDL_GPUTextureCreateInfo info;
  SDL_zero(info);
  info.type = SDL_GPU_TEXTURETYPE_CUBE;
  info.format = SHADOW_MAP_FORMAT;
  info.width = SHADOW_MAP_SIZE;
  info.height = SHADOW_MAP_SIZE;
  info.layer_count_or_depth = CUBE_FACE_COUNT;
  info.num_levels = 1;
  info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &info);
  if (!tex) {
    SDL_Log("Failed to create shadow cube map: %s", SDL_GetError());
  }
  return tex;
}

/* ── Helper: build 6 view-projection matrices for a cube map ─────────────── */
/*
 * Standard cube map face order (matches SDL_GPUCubeMapFace):
 *   Face 0 (+X): look right,   up = (0,-1, 0)
 *   Face 1 (-X): look left,    up = (0,-1, 0)
 *   Face 2 (+Y): look up,      up = (0, 0, 1)
 *   Face 3 (-Y): look down,    up = (0, 0,-1)
 *   Face 4 (+Z): look forward, up = (0,-1, 0)
 *   Face 5 (-Z): look back,    up = (0,-1, 0)
 */
static void build_cube_face_vp(vec3 light_pos, mat4 out_vp[CUBE_FACE_COUNT]) {
  /* Cube face look directions and up vectors. */
  const vec3 look_dirs[CUBE_FACE_COUNT] = {
    { 1.0f,  0.0f,  0.0f }, /* +X */
    { -1.0f,  0.0f,  0.0f }, /* -X */
    { 0.0f,  1.0f,  0.0f }, /* +Y */
    { 0.0f, -1.0f,  0.0f }, /* -Y */
    { 0.0f,  0.0f,  1.0f }, /* +Z */
    { 0.0f,  0.0f, -1.0f }, /* -Z */
  };
  const vec3 up_dirs[CUBE_FACE_COUNT] = {
    { 0.0f, -1.0f,  0.0f }, /* +X */
    { 0.0f, -1.0f,  0.0f }, /* -X */
    { 0.0f,  0.0f,  1.0f }, /* +Y */
    { 0.0f,  0.0f, -1.0f }, /* -Y */
    { 0.0f, -1.0f,  0.0f }, /* +Z */
    { 0.0f, -1.0f,  0.0f }, /* -Z */
  };

  /* 90-degree FOV, 1:1 aspect, shared near/far planes.
   *
   * Negate Y (m[5]) to compensate for the texture row order mismatch:
   * SDL3 GPU maps NDC Y=+1 to texture row 0 (top), but the cube map
   * sampler's t-coordinate convention expects the opposite vertical
   * orientation. Negating Y in the projection flips the rendered image
   * so each face matches what TextureCube.Sample() expects. */
  mat4 shadow_proj = mat4_perspective(
      FORGE_PI / 2.0f, 1.0f, SHADOW_NEAR_PLANE, SHADOW_FAR_PLANE);
  shadow_proj.m[5] = -shadow_proj.m[5];

  for (int face = 0; face < CUBE_FACE_COUNT; face++) {
    vec3 target = vec3_add(light_pos, look_dirs[face]);
    mat4 shadow_view = mat4_look_at(light_pos, target, up_dirs[face]);
    out_vp[face] = mat4_multiply(shadow_proj, shadow_view);
  }
}

/* ── Helper: draw model into shadow pass ──────────────────────────────────── */

static void draw_model_shadow(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const mat4 *placement,
    const mat4 *face_vp,
    const float light_pos[3]
) {
  const ForgeGltfScene *scene = &model->scene;

  for (int ni = 0; ni < scene->node_count; ni++) {
    const ForgeGltfNode *node = &scene->nodes[ni];
    if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
      continue;

    mat4 model_mat = mat4_multiply(*placement, node->world_transform);
    mat4 mvp = mat4_multiply(*face_vp, model_mat);

    ShadowVertUniforms vert_u;
    vert_u.light_mvp = mvp;
    vert_u.model = model_mat;
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    ShadowFragUniforms frag_u;
    frag_u.light_pos[0] = light_pos[0];
    frag_u.light_pos[1] = light_pos[1];
    frag_u.light_pos[2] = light_pos[2];
    frag_u.far_plane = SHADOW_FAR_PLANE;
    SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

    const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
    for (int pi = 0; pi < mesh->primitive_count; pi++) {
      int prim_idx = mesh->first_primitive + pi;
      const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

      if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
        continue;

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

/* ── Helper: draw model for scene pass ────────────────────────────────────── */

static void draw_model_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    const mat4 *placement,
    const mat4 *cam_vp,
    const PointLight lights[MAX_POINT_LIGHTS]
) {
  const ForgeGltfScene *scene = &model->scene;

  for (int ni = 0; ni < scene->node_count; ni++) {
    const ForgeGltfNode *node = &scene->nodes[ni];
    if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
      continue;

    mat4 model_mat = mat4_multiply(*placement, node->world_transform);
    mat4 mvp = mat4_multiply(*cam_vp, model_mat);

    SceneVertUniforms vert_u;
    vert_u.mvp = mvp;
    vert_u.model = model_mat;
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
    for (int pi = 0; pi < mesh->primitive_count; pi++) {
      int prim_idx = mesh->first_primitive + pi;
      const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

      if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
        continue;

      SDL_GPUTexture *tex = state->white_texture;

      SceneFragUniforms frag_u;
      SDL_zero(frag_u);

      if (gpu_prim->material_index >= 0 &&
          gpu_prim->material_index < model->material_count) {
        const GpuMaterial *mat = &model->materials[gpu_prim->material_index];
        frag_u.base_color[0] = mat->base_color[0];
        frag_u.base_color[1] = mat->base_color[1];
        frag_u.base_color[2] = mat->base_color[2];
        frag_u.base_color[3] = mat->base_color[3];
        frag_u.has_texture = mat->has_texture ? 1.0f : 0.0f;
        if (mat->texture)
          tex = mat->texture;
      } else {
        frag_u.base_color[0] = 1.0f;
        frag_u.base_color[1] = 1.0f;
        frag_u.base_color[2] = 1.0f;
        frag_u.base_color[3] = 1.0f;
        frag_u.has_texture = 0.0f;
      }

      frag_u.eye_pos[0] = state->cam_position.x;
      frag_u.eye_pos[1] = state->cam_position.y;
      frag_u.eye_pos[2] = state->cam_position.z;
      frag_u.shininess = MATERIAL_SHININESS;
      frag_u.ambient = MATERIAL_AMBIENT;
      frag_u.specular_str = MATERIAL_SPECULAR_STR;
      frag_u.shadow_far_plane = SHADOW_FAR_PLANE;
      SDL_memcpy(frag_u.lights, lights, sizeof(PointLight) * MAX_POINT_LIGHTS);

      SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

      /* Bind diffuse texture (slot 0) + 4 shadow cube maps (slots 1-4). */
      SDL_GPUTextureSamplerBinding tex_bindings[5];
      SDL_zero(tex_bindings);
      tex_bindings[0].texture = tex;
      tex_bindings[0].sampler = state->sampler;
      {
        int si;
        for (si = 0; si < MAX_POINT_LIGHTS; si++) {
          tex_bindings[si + 1].texture = state->shadow_cubes[si];
          tex_bindings[si + 1].sampler = state->shadow_sampler;
        }
      }
      SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 5);

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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
  if (!device) {
    SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_Window *window = SDL_CreateWindow(
      "Lesson 23 \xe2\x80\x94 Point Lights & Shadows",
      WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
  if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(device, window)) {
    SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* Request SDR_LINEAR for correct gamma handling. */
  if (SDL_WindowSupportsGPUSwapchainComposition(
          device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
    if (!SDL_SetGPUSwapchainParameters(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
            SDL_GPU_PRESENTMODE_VSYNC)) {
      SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
      SDL_DestroyWindow(window);
      SDL_DestroyGPUDevice(device);
      return SDL_APP_FAILURE;
    }
  }

  SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

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

  /* Get initial window size for render targets. */
  int draw_w = 0, draw_h = 0;
  if (!SDL_GetWindowSizeInPixels(window, &draw_w, &draw_h)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    draw_w = WINDOW_WIDTH;
    draw_h = WINDOW_HEIGHT;
  }
  Uint32 w = (Uint32)draw_w;
  Uint32 h = (Uint32)draw_h;

  /* Create HDR render target. */
  state->hdr_target = create_hdr_target(device, w, h);
  if (!state->hdr_target) {
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }
  state->hdr_width = w;
  state->hdr_height = h;

  /* Create depth texture. */
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

  /* Create bloom mip chain. */
  if (!create_bloom_mip_chain(state)) {
    SDL_ReleaseGPUTexture(device, state->depth_texture);
    SDL_ReleaseGPUTexture(device, state->hdr_target);
    SDL_free(state);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
  }

  /* Create white fallback texture. */
  state->white_texture = create_white_texture(device);
  if (!state->white_texture) goto init_fail;

  /* Create samplers. */
  {
    SDL_GPUSamplerCreateInfo si;
    SDL_zero(si);
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.max_anisotropy = MAX_ANISOTROPY;
    si.enable_anisotropy = true;
    state->sampler = SDL_CreateGPUSampler(device, &si);
    if (!state->sampler) {
      SDL_Log("Failed to create diffuse sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }
  {
    SDL_GPUSamplerCreateInfo si;
    SDL_zero(si);
    si.min_filter = SDL_GPU_FILTER_NEAREST;
    si.mag_filter = SDL_GPU_FILTER_NEAREST;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    state->hdr_sampler = SDL_CreateGPUSampler(device, &si);
    if (!state->hdr_sampler) {
      SDL_Log("Failed to create HDR sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }
  {
    SDL_GPUSamplerCreateInfo si;
    SDL_zero(si);
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    state->bloom_sampler = SDL_CreateGPUSampler(device, &si);
    if (!state->bloom_sampler) {
      SDL_Log("Failed to create bloom sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Create shadow sampler (nearest, clamp-to-edge). */
  {
    SDL_GPUSamplerCreateInfo si;
    SDL_zero(si);
    si.min_filter = SDL_GPU_FILTER_NEAREST;
    si.mag_filter = SDL_GPU_FILTER_NEAREST;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    state->shadow_sampler = SDL_CreateGPUSampler(device, &si);
    if (!state->shadow_sampler) {
      SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Create shadow cube maps (one per light) and shared depth buffer. */
  {
    int li;
    for (li = 0; li < MAX_POINT_LIGHTS; li++) {
      state->shadow_cubes[li] = create_shadow_cube(device);
      if (!state->shadow_cubes[li]) goto init_fail;
    }

    SDL_GPUTextureCreateInfo depth_info;
    SDL_zero(depth_info);
    depth_info.type = SDL_GPU_TEXTURETYPE_2D;
    depth_info.format = SHADOW_DEPTH_FORMAT;
    depth_info.width = SHADOW_MAP_SIZE;
    depth_info.height = SHADOW_MAP_SIZE;
    depth_info.layer_count_or_depth = 1;
    depth_info.num_levels = 1;
    depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    state->shadow_depth = SDL_CreateGPUTexture(device, &depth_info);
    if (!state->shadow_depth) {
      SDL_Log("Failed to create shadow depth texture: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Load glTF models. */
  if (!setup_model(device, state->white_texture, &state->truck, TRUCK_MODEL_PATH))
    goto init_fail;
  if (!setup_model(device, state->white_texture, &state->box, BOX_MODEL_PATH))
    goto init_fail;

  /* Upload grid and sphere geometry. */
  if (!upload_grid_geometry(device, state)) goto init_fail;
  if (!generate_and_upload_sphere(device, state)) goto init_fail;
  generate_box_placements(state);

  /* ── Scene pipeline (lit geometry → HDR) ────────────────────────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil), 0, 1);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil, sizeof(scene_frag_dxil), 5, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

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
    color_desc.format = HDR_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
    pi.vertex_input_state.num_vertex_buffers = 1;
    pi.vertex_input_state.vertex_attributes = attrs;
    pi.vertex_input_state.num_vertex_attributes = 3;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pi.depth_stencil_state.enable_depth_test = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;
    pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pi.target_info.has_depth_stencil_target = true;

    state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->scene_pipeline) {
      SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* ── Grid pipeline ──────────────────────────────────────────────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil, sizeof(grid_vert_dxil), 0, 1);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil, sizeof(grid_frag_dxil), 4, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUVertexBufferDescription vb_desc;
    SDL_zero(vb_desc);
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(float) * 3;
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attr;
    SDL_zero(attr);
    attr.location = 0;
    attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attr.offset = 0;

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
    pi.vertex_input_state.num_vertex_buffers = 1;
    pi.vertex_input_state.vertex_attributes = &attr;
    pi.vertex_input_state.num_vertex_attributes = 1;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pi.depth_stencil_state.enable_depth_test = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;
    pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pi.target_info.has_depth_stencil_target = true;

    state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->grid_pipeline) {
      SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* ── Emissive pipeline (constant HDR emission) ──────────────────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil, sizeof(scene_vert_dxil), 0, 1);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        emissive_frag_spirv, sizeof(emissive_frag_spirv),
        emissive_frag_dxil, sizeof(emissive_frag_dxil), 0, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

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
    color_desc.format = HDR_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
    pi.vertex_input_state.num_vertex_buffers = 1;
    pi.vertex_input_state.vertex_attributes = attrs;
    pi.vertex_input_state.num_vertex_attributes = 3;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pi.depth_stencil_state.enable_depth_test = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;
    pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pi.target_info.has_depth_stencil_target = true;

    state->emissive_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->emissive_pipeline) {
      SDL_Log("Failed to create emissive pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* ── Shadow pipeline (renders linear depth to cube map faces) ────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil, sizeof(shadow_vert_dxil), 0, 1);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil, sizeof(shadow_frag_dxil), 0, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

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
    color_desc.format = SHADOW_MAP_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
    pi.vertex_input_state.num_vertex_buffers = 1;
    pi.vertex_input_state.vertex_attributes = attrs;
    pi.vertex_input_state.num_vertex_attributes = 3;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pi.depth_stencil_state.enable_depth_test = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;
    pi.target_info.depth_stencil_format = SHADOW_DEPTH_FORMAT;
    pi.target_info.has_depth_stencil_target = true;

    state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->shadow_pipeline) {
      SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* ── Bloom downsample pipeline ──────────────────────────────────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        bloom_downsample_frag_spirv, sizeof(bloom_downsample_frag_spirv),
        bloom_downsample_frag_dxil, sizeof(bloom_downsample_frag_dxil), 1, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;

    state->downsample_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->downsample_pipeline) {
      SDL_Log("Failed to create bloom downsample pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* ── Bloom upsample pipeline (additive blending) ────────────────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        bloom_upsample_frag_spirv, sizeof(bloom_upsample_frag_spirv),
        bloom_upsample_frag_dxil, sizeof(bloom_upsample_frag_dxil), 1, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT;
    color_desc.blend_state.enable_blend = true;
    color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;

    state->upsample_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->upsample_pipeline) {
      SDL_Log("Failed to create bloom upsample pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* ── Tone mapping pipeline ──────────────────────────────────────────── */
  {
    SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil), 0, 0);
    SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        tonemap_frag_spirv, sizeof(tonemap_frag_spirv),
        tonemap_frag_dxil, sizeof(tonemap_frag_dxil), 2, 1);
    if (!vert || !frag) {
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = swapchain_format;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader = vert;
    pi.fragment_shader = frag;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets = 1;

    state->tonemap_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);
    if (!state->tonemap_pipeline) {
      SDL_Log("Failed to create tonemap pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Initialize camera, HDR, and bloom settings. */
  state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
  state->cam_yaw = CAM_START_YAW_DEG * FORGE_DEG2RAD;
  state->cam_pitch = CAM_START_PITCH_DEG * FORGE_DEG2RAD;
  state->exposure = DEFAULT_EXPOSURE;
  state->bloom_enabled = true;
  state->bloom_intensity = DEFAULT_BLOOM_INTENSITY;
  state->bloom_threshold = DEFAULT_BLOOM_THRESHOLD;
  state->light_enabled[0] = true;
  state->light_enabled[1] = true;
  state->light_enabled[2] = true;
  state->light_enabled[3] = true;
  state->light0_angle = FORGE_PI / 3.0f;
  state->last_ticks = SDL_GetTicks();

  if (SDL_SetWindowRelativeMouseMode(window, true)) {
    state->mouse_captured = true;
  } else {
    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    state->mouse_captured = false;
  }

  SDL_Log("Lights: 1/2/3/4 to toggle each light");
  SDL_Log("Exposure: %.1f (+/- to adjust)", state->exposure);
  SDL_Log("Bloom: ON (B toggle, Up/Down intensity, Left/Right threshold)");

#ifdef FORGE_CAPTURE
  if (state->capture.mode != FORGE_CAPTURE_NONE) {
    if (!forge_capture_init(&state->capture, device, window)) {
      SDL_Log("forge_capture_init failed — disabling capture");
      state->capture.mode = FORGE_CAPTURE_NONE;
    }
  }
#endif

  *appstate = state;
  return SDL_APP_CONTINUE;

init_fail:
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
        if (!SDL_SetWindowRelativeMouseMode(state->window, false))
          SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        else
          state->mouse_captured = false;
      } else {
        return SDL_APP_SUCCESS;
      }
    }

    /* Toggle individual point lights. */
    if (event->key.key == SDLK_1) {
      state->light_enabled[0] = !state->light_enabled[0];
      SDL_Log("Light 0 (cyan): %s", state->light_enabled[0] ? "ON" : "OFF");
    } else if (event->key.key == SDLK_2) {
      state->light_enabled[1] = !state->light_enabled[1];
      SDL_Log("Light 1 (orange): %s", state->light_enabled[1] ? "ON" : "OFF");
    } else if (event->key.key == SDLK_3) {
      state->light_enabled[2] = !state->light_enabled[2];
      SDL_Log("Light 2 (green): %s", state->light_enabled[2] ? "ON" : "OFF");
    } else if (event->key.key == SDLK_4) {
      state->light_enabled[3] = !state->light_enabled[3];
      SDL_Log("Light 3 (purple): %s", state->light_enabled[3] ? "ON" : "OFF");
    }

    /* Exposure. */
    if (event->key.key == SDLK_EQUALS) {
      state->exposure += EXPOSURE_STEP;
      if (state->exposure > MAX_EXPOSURE) state->exposure = MAX_EXPOSURE;
      SDL_Log("Exposure: %.1f", state->exposure);
    } else if (event->key.key == SDLK_MINUS) {
      state->exposure -= EXPOSURE_STEP;
      if (state->exposure < MIN_EXPOSURE) state->exposure = MIN_EXPOSURE;
      SDL_Log("Exposure: %.1f", state->exposure);
    }

    /* Bloom toggle. */
    if (event->key.key == SDLK_B) {
      state->bloom_enabled = !state->bloom_enabled;
      SDL_Log("Bloom: %s", state->bloom_enabled ? "ON" : "OFF");
    }

    /* Bloom intensity. */
    if (event->key.key == SDLK_UP) {
      state->bloom_intensity += BLOOM_INTENSITY_STEP;
      if (state->bloom_intensity > MAX_BLOOM_INTENSITY)
        state->bloom_intensity = MAX_BLOOM_INTENSITY;
      SDL_Log("Bloom intensity: %.3f", state->bloom_intensity);
    } else if (event->key.key == SDLK_DOWN) {
      state->bloom_intensity -= BLOOM_INTENSITY_STEP;
      if (state->bloom_intensity < MIN_BLOOM_INTENSITY)
        state->bloom_intensity = MIN_BLOOM_INTENSITY;
      SDL_Log("Bloom intensity: %.3f", state->bloom_intensity);
    }

    /* Bloom threshold. */
    if (event->key.key == SDLK_RIGHT) {
      state->bloom_threshold += BLOOM_THRESHOLD_STEP;
      if (state->bloom_threshold > MAX_BLOOM_THRESHOLD)
        state->bloom_threshold = MAX_BLOOM_THRESHOLD;
      SDL_Log("Bloom threshold: %.1f", state->bloom_threshold);
    } else if (event->key.key == SDLK_LEFT) {
      state->bloom_threshold -= BLOOM_THRESHOLD_STEP;
      if (state->bloom_threshold < MIN_BLOOM_THRESHOLD)
        state->bloom_threshold = MIN_BLOOM_THRESHOLD;
      SDL_Log("Bloom threshold: %.1f", state->bloom_threshold);
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (!state->mouse_captured) {
      if (!SDL_SetWindowRelativeMouseMode(state->window, true))
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
      else
        state->mouse_captured = true;
    }
    break;

  case SDL_EVENT_MOUSE_MOTION:
    if (state->mouse_captured) {
      state->cam_yaw -= event->motion.xrel * MOUSE_SENS;
      state->cam_pitch -= event->motion.yrel * MOUSE_SENS;
      if (state->cam_pitch > PITCH_CLAMP) state->cam_pitch = PITCH_CLAMP;
      if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
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
  if (dt > MAX_FRAME_DT) dt = MAX_FRAME_DT;

  /* ── Animate light 0 orbit ────────────────────────────────────────── */
  state->light0_angle += LIGHT0_ORBIT_SPEED * dt;

  /* ── Compute current light positions ──────────────────────────────── */
  PointLight lights[MAX_POINT_LIGHTS];
  fill_lights(state, lights);

  /* ── Camera movement ──────────────────────────────────────────────── */
  const bool *keys = SDL_GetKeyboardState(NULL);
  if (state->mouse_captured) {
    quat orientation = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(orientation);
    vec3 right = quat_right(orientation);
    vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
    float speed = CAM_SPEED * dt;

    if (keys[SDL_SCANCODE_W])
      state->cam_position = vec3_add(state->cam_position, vec3_scale(forward, speed));
    if (keys[SDL_SCANCODE_S])
      state->cam_position = vec3_add(state->cam_position, vec3_scale(forward, -speed));
    if (keys[SDL_SCANCODE_D])
      state->cam_position = vec3_add(state->cam_position, vec3_scale(right, speed));
    if (keys[SDL_SCANCODE_A])
      state->cam_position = vec3_add(state->cam_position, vec3_scale(right, -speed));
    if (keys[SDL_SCANCODE_SPACE])
      state->cam_position = vec3_add(state->cam_position, vec3_scale(up, speed));
    if (keys[SDL_SCANCODE_LSHIFT])
      state->cam_position = vec3_add(state->cam_position, vec3_scale(up, -speed));
  }

  /* ── Camera matrices ──────────────────────────────────────────────── */
  quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
  mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);

  int draw_w = 0, draw_h = 0;
  if (!SDL_GetWindowSizeInPixels(state->window, &draw_w, &draw_h)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }
  if (draw_w <= 0 || draw_h <= 0) return SDL_APP_CONTINUE;

  Uint32 w = (Uint32)draw_w;
  Uint32 h = (Uint32)draw_h;
  float aspect = (float)w / (float)h;
  mat4 proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
  mat4 cam_vp = mat4_multiply(proj, view);

  /* ── Resize render targets if window changed ──────────────────────── */
  if (w != state->hdr_width || h != state->hdr_height) {
    SDL_GPUTexture *new_hdr = create_hdr_target(state->device, w, h);
    if (!new_hdr) return SDL_APP_CONTINUE;
    SDL_ReleaseGPUTexture(state->device, state->hdr_target);
    state->hdr_target = new_hdr;
    state->hdr_width = w;
    state->hdr_height = h;

    SDL_GPUTexture *old_bloom[BLOOM_MIP_COUNT];
    Uint32 old_widths[BLOOM_MIP_COUNT], old_heights[BLOOM_MIP_COUNT];
    for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
      old_bloom[i] = state->bloom_mips[i];
      old_widths[i] = state->bloom_widths[i];
      old_heights[i] = state->bloom_heights[i];
      state->bloom_mips[i] = NULL;
    }
    if (!create_bloom_mip_chain(state)) {
      for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
        state->bloom_mips[i] = old_bloom[i];
        state->bloom_widths[i] = old_widths[i];
        state->bloom_heights[i] = old_heights[i];
      }
      return SDL_APP_CONTINUE;
    }
    for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
      if (old_bloom[i]) SDL_ReleaseGPUTexture(state->device, old_bloom[i]);
    }
  }
  if (w != state->depth_width || h != state->depth_height) {
    SDL_GPUTexture *new_depth = create_depth_texture(state->device, w, h);
    if (!new_depth) return SDL_APP_CONTINUE;
    SDL_ReleaseGPUTexture(state->device, state->depth_texture);
    state->depth_texture = new_depth;
    state->depth_width = w;
    state->depth_height = h;
  }

  /* ── Acquire command buffer ───────────────────────────────────────── */
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
  if (!cmd) {
    SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }

  /* ── Acquire swapchain texture ────────────────────────────────────── */
  SDL_GPUTexture *swapchain = NULL;
  if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL)) {
    SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
    if (!SDL_SubmitGPUCommandBuffer(cmd))
      SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }
  if (!swapchain) {
    if (!SDL_SubmitGPUCommandBuffer(cmd))
      SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }

  /* ════════════════════════════════════════════════════════════════════
   * SHADOW PASSES — Render linear depth into cube maps for each light
   * ════════════════════════════════════════════════════════════════════ */
  if (state->shadow_pipeline) {
    mat4 truck_placement_shadow = mat4_identity();

    for (int li = 0; li < MAX_POINT_LIGHTS; li++) {
      if (lights[li].intensity <= 0.0f) continue;

      vec3 light_pos = vec3_create(
          lights[li].position[0], lights[li].position[1], lights[li].position[2]);
      mat4 face_vps[CUBE_FACE_COUNT];
      build_cube_face_vp(light_pos, face_vps);

      for (int face = 0; face < CUBE_FACE_COUNT; face++) {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture = state->shadow_cubes[li];
        color_target.layer_or_depth_plane = (Uint32)face;
        color_target.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_STORE;
        color_target.clear_color.r = 1.0f; /* max depth = fully lit */
        color_target.clear_color.g = 0.0f;
        color_target.clear_color.b = 0.0f;
        color_target.clear_color.a = 1.0f;

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture = state->shadow_depth;
        depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.clear_depth = 1.0f;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, &depth_target);
        if (!pass) {
          SDL_Log("Failed to begin shadow pass (light %d, face %d): %s",
                  li, face, SDL_GetError());
          continue;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->shadow_pipeline);

        /* Draw truck into shadow map. */
        draw_model_shadow(pass, cmd, &state->truck,
                          &truck_placement_shadow, &face_vps[face],
                          lights[li].position);

        /* Draw boxes into shadow map. */
        for (int bi = 0; bi < state->box_count; bi++) {
          BoxPlacement *bp = &state->box_placements[bi];
          mat4 box_placement = mat4_multiply(
              mat4_translate(bp->position), mat4_rotate_y(bp->y_rotation));
          draw_model_shadow(pass, cmd, &state->box,
                            &box_placement, &face_vps[face],
                            lights[li].position);
        }

        SDL_EndGPURenderPass(pass);
      }
    }
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 1 — Render scene to HDR target
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
      if (!SDL_SubmitGPUCommandBuffer(cmd))
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      return SDL_APP_CONTINUE;
    }

    /* ── Draw grid ────────────────────────────────────────────────── */
    if (state->grid_pipeline && state->grid_vertex_buffer && state->grid_index_buffer) {
      SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

      GridVertUniforms grid_vu;
      grid_vu.vp = cam_vp;
      SDL_PushGPUVertexUniformData(cmd, 0, &grid_vu, sizeof(grid_vu));

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
      grid_fu.eye_pos[0] = state->cam_position.x;
      grid_fu.eye_pos[1] = state->cam_position.y;
      grid_fu.eye_pos[2] = state->cam_position.z;
      grid_fu.grid_spacing = GRID_SPACING;
      grid_fu.line_width = GRID_LINE_WIDTH;
      grid_fu.fade_distance = GRID_FADE_DISTANCE;
      grid_fu.ambient = GRID_AMBIENT;
      grid_fu.shininess = GRID_SHININESS;
      grid_fu.specular_str = GRID_SPECULAR_STR;
      grid_fu.shadow_far_plane = SHADOW_FAR_PLANE;
      SDL_memcpy(grid_fu.lights, lights, sizeof(lights));
      SDL_PushGPUFragmentUniformData(cmd, 0, &grid_fu, sizeof(grid_fu));

      /* Bind 4 shadow cube maps for the grid shader. */
      {
        SDL_GPUTextureSamplerBinding grid_shadow_bindings[4];
        SDL_zero(grid_shadow_bindings);
        int si;
        for (si = 0; si < MAX_POINT_LIGHTS; si++) {
          grid_shadow_bindings[si].texture = state->shadow_cubes[si];
          grid_shadow_bindings[si].sampler = state->shadow_sampler;
        }
        SDL_BindGPUFragmentSamplers(pass, 0, grid_shadow_bindings, 4);
      }

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

      mat4 truck_placement = mat4_identity();
      draw_model_scene(pass, cmd, &state->truck, state,
                       &truck_placement, &cam_vp, lights);

      for (int bi = 0; bi < state->box_count; bi++) {
        BoxPlacement *bp = &state->box_placements[bi];
        mat4 box_placement =
            mat4_multiply(mat4_translate(bp->position), mat4_rotate_y(bp->y_rotation));
        draw_model_scene(pass, cmd, &state->box, state,
                         &box_placement, &cam_vp, lights);
      }
    }

    /* ── Draw emissive spheres at each light position ─────────────── */
    if (state->emissive_pipeline && state->sphere_vertex_buffer &&
        state->sphere_index_buffer) {
      SDL_BindGPUGraphicsPipeline(pass, state->emissive_pipeline);

      for (int li = 0; li < MAX_POINT_LIGHTS; li++) {
        if (lights[li].intensity <= 0.0f) continue;

        vec3 light_pos = vec3_create(
            lights[li].position[0], lights[li].position[1], lights[li].position[2]);
        mat4 sphere_model = mat4_translate(light_pos);
        mat4 sphere_mvp = mat4_multiply(cam_vp, sphere_model);

        SceneVertUniforms sphere_vu;
        sphere_vu.mvp = sphere_mvp;
        sphere_vu.model = sphere_model;
        SDL_PushGPUVertexUniformData(cmd, 0, &sphere_vu, sizeof(sphere_vu));

        EmissiveFragUniforms emissive_fu;
        emissive_fu.emission_color[0] = lights[li].color[0] * EMISSION_SCALE;
        emissive_fu.emission_color[1] = lights[li].color[1] * EMISSION_SCALE;
        emissive_fu.emission_color[2] = lights[li].color[2] * EMISSION_SCALE;
        emissive_fu._pad = 0.0f;
        SDL_PushGPUFragmentUniformData(cmd, 0, &emissive_fu, sizeof(emissive_fu));

        SDL_GPUBufferBinding vb;
        SDL_zero(vb);
        vb.buffer = state->sphere_vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

        SDL_GPUBufferBinding ib;
        SDL_zero(ib);
        ib.buffer = state->sphere_index_buffer;
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, SPHERE_INDEX_COUNT, 1, 0, 0, 0);
      }
    }

    SDL_EndGPURenderPass(pass);
  }

  /* ════════════════════════════════════════════════════════════════════
   * BLOOM PASSES — Downsample + Upsample
   * ════════════════════════════════════════════════════════════════════ */
  bool bloom_ok = false;
  if (state->bloom_enabled) {
    bloom_ok = true;

    /* ── Bloom downsample (5 passes) ─────────────────────────────── */
    for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
      SDL_GPUColorTargetInfo ct;
      SDL_zero(ct);
      ct.texture = state->bloom_mips[i];
      ct.load_op = SDL_GPU_LOADOP_CLEAR;
      ct.store_op = SDL_GPU_STOREOP_STORE;

      SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, NULL);
      if (!pass) {
        SDL_Log("Failed to begin bloom downsample pass %d: %s", i, SDL_GetError());
        bloom_ok = false;
        break;
      }

      SDL_BindGPUGraphicsPipeline(pass, state->downsample_pipeline);

      SDL_GPUTextureSamplerBinding src_binding;
      SDL_zero(src_binding);
      src_binding.texture = (i == 0) ? state->hdr_target : state->bloom_mips[i - 1];
      src_binding.sampler = state->bloom_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, &src_binding, 1);

      BloomDownsampleUniforms ds_u;
      if (i == 0) {
        ds_u.texel_size[0] = 1.0f / (float)state->hdr_width;
        ds_u.texel_size[1] = 1.0f / (float)state->hdr_height;
      } else {
        ds_u.texel_size[0] = 1.0f / (float)state->bloom_widths[i - 1];
        ds_u.texel_size[1] = 1.0f / (float)state->bloom_heights[i - 1];
      }
      ds_u.threshold = state->bloom_threshold;
      ds_u.use_karis = (i == 0) ? 1.0f : 0.0f;
      SDL_PushGPUFragmentUniformData(cmd, 0, &ds_u, sizeof(ds_u));

      SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
      SDL_EndGPURenderPass(pass);
    }

    /* ── Bloom upsample (4 passes, additive blend) ───────────────── */
    if (bloom_ok) {
      for (int i = BLOOM_MIP_COUNT - 2; i >= 0; i--) {
        SDL_GPUColorTargetInfo ct;
        SDL_zero(ct);
        ct.texture = state->bloom_mips[i];
        ct.load_op = SDL_GPU_LOADOP_LOAD;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, NULL);
        if (!pass) {
          SDL_Log("Failed to begin bloom upsample pass %d: %s", i, SDL_GetError());
          bloom_ok = false;
          break;
        }

        SDL_BindGPUGraphicsPipeline(pass, state->upsample_pipeline);

        SDL_GPUTextureSamplerBinding src_binding;
        SDL_zero(src_binding);
        src_binding.texture = state->bloom_mips[i + 1];
        src_binding.sampler = state->bloom_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &src_binding, 1);

        BloomUpsampleUniforms us_u;
        SDL_zero(us_u);
        us_u.texel_size[0] = 1.0f / (float)state->bloom_widths[i + 1];
        us_u.texel_size[1] = 1.0f / (float)state->bloom_heights[i + 1];
        SDL_PushGPUFragmentUniformData(cmd, 0, &us_u, sizeof(us_u));

        SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
      }
    }
  }

  /* ════════════════════════════════════════════════════════════════════
   * TONE MAP PASS — HDR + bloom → swapchain
   * ════════════════════════════════════════════════════════════════════ */
  {
    SDL_GPUColorTargetInfo ct;
    SDL_zero(ct);
    ct.texture = swapchain;
    ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
    ct.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, NULL);
    if (!pass) {
      SDL_Log("Failed to begin tonemap render pass: %s", SDL_GetError());
      if (!SDL_SubmitGPUCommandBuffer(cmd))
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      return SDL_APP_CONTINUE;
    }

    if (state->tonemap_pipeline) {
      SDL_BindGPUGraphicsPipeline(pass, state->tonemap_pipeline);

      SDL_GPUTextureSamplerBinding tex_bindings[2];
      SDL_zero(tex_bindings);
      tex_bindings[0].texture = state->hdr_target;
      tex_bindings[0].sampler = state->hdr_sampler;
      tex_bindings[1].texture = state->bloom_mips[0];
      tex_bindings[1].sampler = state->bloom_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 2);

      TonemapFragUniforms tonemap_u;
      SDL_zero(tonemap_u);
      tonemap_u.exposure = state->exposure;
      tonemap_u.bloom_intensity =
          (bloom_ok && state->bloom_enabled) ? state->bloom_intensity : 0.0f;
      SDL_PushGPUFragmentUniformData(cmd, 0, &tonemap_u, sizeof(tonemap_u));

      SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
    }

    SDL_EndGPURenderPass(pass);
  }

  /* ── Submit ───────────────────────────────────────────────────────── */
#ifdef FORGE_CAPTURE
  if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
    if (forge_capture_should_quit(&state->capture))
      return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
  }
#endif

  if (!SDL_SubmitGPUCommandBuffer(cmd)) {
    SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
  }

#ifdef FORGE_CAPTURE
  if (forge_capture_should_quit(&state->capture))
    return SDL_APP_SUCCESS;
#endif

  return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  (void)result;
  app_state *state = (app_state *)appstate;
  if (!state) return;

#ifdef FORGE_CAPTURE
  forge_capture_destroy(&state->capture, state->device);
#endif

  free_model_gpu(state->device, &state->box);
  free_model_gpu(state->device, &state->truck);

  if (state->sphere_vertex_buffer)
    SDL_ReleaseGPUBuffer(state->device, state->sphere_vertex_buffer);
  if (state->sphere_index_buffer)
    SDL_ReleaseGPUBuffer(state->device, state->sphere_index_buffer);
  if (state->grid_vertex_buffer)
    SDL_ReleaseGPUBuffer(state->device, state->grid_vertex_buffer);
  if (state->grid_index_buffer)
    SDL_ReleaseGPUBuffer(state->device, state->grid_index_buffer);

  if (state->white_texture)
    SDL_ReleaseGPUTexture(state->device, state->white_texture);
  if (state->sampler)
    SDL_ReleaseGPUSampler(state->device, state->sampler);
  if (state->hdr_sampler)
    SDL_ReleaseGPUSampler(state->device, state->hdr_sampler);
  if (state->bloom_sampler)
    SDL_ReleaseGPUSampler(state->device, state->bloom_sampler);
  if (state->shadow_sampler)
    SDL_ReleaseGPUSampler(state->device, state->shadow_sampler);

  if (state->hdr_target)
    SDL_ReleaseGPUTexture(state->device, state->hdr_target);
  if (state->depth_texture)
    SDL_ReleaseGPUTexture(state->device, state->depth_texture);

  /* Release shadow cube maps and shared depth buffer. */
  {
    int li;
    for (li = 0; li < MAX_POINT_LIGHTS; li++) {
      if (state->shadow_cubes[li])
        SDL_ReleaseGPUTexture(state->device, state->shadow_cubes[li]);
    }
  }
  if (state->shadow_depth)
    SDL_ReleaseGPUTexture(state->device, state->shadow_depth);

  release_bloom_mip_chain(state);

  if (state->tonemap_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->tonemap_pipeline);
  if (state->upsample_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->upsample_pipeline);
  if (state->downsample_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->downsample_pipeline);
  if (state->shadow_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
  if (state->emissive_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->emissive_pipeline);
  if (state->grid_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
  if (state->scene_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);

  SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
  SDL_DestroyWindow(state->window);
  SDL_DestroyGPUDevice(state->device);
  SDL_free(state);
}
