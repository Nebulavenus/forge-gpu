/*
 * Lesson 26 — Procedural Sky (Hillaire)
 *
 * Why this lesson exists:
 *   Outdoor scenes need a sky.  A static skybox texture can't change
 *   time of day, and pre-baked lookup tables hide the underlying physics.
 *   This lesson implements Sébastien Hillaire's atmospheric model
 *   (EGSR 2020) with LUT-accelerated transmittance and multi-scattering,
 *   producing a physically-based sky that responds to sun angle in
 *   real time.
 *
 * What this lesson teaches:
 *   1. Per-pixel ray marching through Earth's atmosphere
 *   2. Rayleigh, Mie, and ozone scattering from physical constants
 *   3. The Beer-Lambert law for light extinction
 *   4. Phase functions (Rayleigh symmetric, Henyey-Greenstein forward)
 *   5. Compute-shader LUT precomputation (transmittance + multi-scatter)
 *   6. Ray matrix reconstruction for world-space view directions
 *   7. HDR rendering to a floating-point render target
 *   8. Jimenez dual-filter bloom (downsample + upsample)
 *   9. ACES filmic tone mapping with exposure control
 *  10. Quaternion fly camera in planet-centric coordinates
 *  11. Sun disc rendering with limb darkening
 *
 * Scene:
 *   A fullscreen quad where the fragment shader ray-marches through
 *   Earth's atmosphere.  No 3D geometry — the sky is computed entirely
 *   per pixel.  HDR output feeds into a Jimenez bloom pass (bright sun
 *   disc creates a natural glow), then ACES tone mapping compresses to
 *   displayable range.
 *
 * One-time compute passes (at startup):
 *   1. Transmittance LUT (256×64) — precompute optical transmittance
 *   2. Multi-scattering LUT (32×32) — precompute higher-order bounces
 *
 * Render passes (per frame):
 *   1. Sky pass → HDR render target (R16G16B16A16_FLOAT)
 *   2. Bloom downsample (5 passes) → bloom mip chain
 *   3. Bloom upsample (4 passes) → accumulated bloom
 *   4. Tonemap pass (HDR + bloom) → swapchain
 *
 * Controls:
 *   WASD / Space / C        — Fly camera (Space = up, C = down)
 *   LShift                  — 10× speed boost
 *   Mouse                   — Look around
 *   Left/Right arrows       — Sun azimuth
 *   Up/Down arrows          — Sun elevation
 *   T                       — Toggle auto sun rotation
 *   1/2/3                   — Tonemap: Clamp / Reinhard / ACES
 *   =/+                     — Increase exposure
 *   -                       — Decrease exposure
 *   B                       — Toggle bloom
 *   Escape                  — Release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include "math/forge_math.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecodes ────────────────────────────────────────────── */

/* Sky shaders — fullscreen quad vertex (ray matrix) + atmosphere fragment */
#include "shaders/compiled/sky_vert_spirv.h"
#include "shaders/compiled/sky_vert_dxil.h"
#include "shaders/compiled/sky_frag_spirv.h"
#include "shaders/compiled/sky_frag_dxil.h"

/* Compute shaders — one-time LUT generation */
#include "shaders/compiled/transmittance_lut_comp_spirv.h"
#include "shaders/compiled/transmittance_lut_comp_dxil.h"
#include "shaders/compiled/multiscatter_lut_comp_spirv.h"
#include "shaders/compiled/multiscatter_lut_comp_dxil.h"

/* Fullscreen vertex — shared by bloom downsample, upsample, and tonemap */
#include "shaders/compiled/fullscreen_vert_spirv.h"
#include "shaders/compiled/fullscreen_vert_dxil.h"

/* Bloom downsample — 13-tap Jimenez filter */
#include "shaders/compiled/bloom_downsample_frag_spirv.h"
#include "shaders/compiled/bloom_downsample_frag_dxil.h"

/* Bloom upsample — 9-tap tent filter */
#include "shaders/compiled/bloom_upsample_frag_spirv.h"
#include "shaders/compiled/bloom_upsample_frag_dxil.h"

/* Tone mapping — HDR + bloom → swapchain */
#include "shaders/compiled/tonemap_frag_spirv.h"
#include "shaders/compiled/tonemap_frag_dxil.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Window dimensions (16:9 standard for consistent screenshots). */
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Fullscreen quad — two triangles, no vertex buffer (SV_VertexID). */
#define FULLSCREEN_QUAD_VERTS 6

/* Camera parameters.
 * The camera works in kilometers (planet-centric coordinates).
 * R_GROUND = 6360 km, so 6360.001 = 1 meter above sea level. */
#define CAM_SPEED     0.2f    /* km/s base movement speed               */
#define CAM_SPEED_BOOST 10.0f /* multiplier when Shift is held          */
#define MOUSE_SENS    0.003f  /* radians per pixel of mouse movement    */
#define PITCH_CLAMP   1.5f    /* ~86 degrees, prevents camera flip      */
#define FOV_DEG       60      /* vertical field of view in degrees      */
/* Camera starting position — 1 meter above ground at equator.
 * In planet-centric coordinates: (0, R_ground + 0.001, 0). */
#define CAM_START_X   0.0f
#define CAM_START_Y   6360.001f
#define CAM_START_Z   0.0f

/* Sun defaults. */
#define SUN_ELEVATION_DEFAULT 0.5f   /* radians above horizon (~29 deg) */
#define SUN_AZIMUTH_DEFAULT   0.0f   /* radians from east               */
#define SUN_ELEVATION_SPEED   0.5f   /* radians/sec for arrow keys      */
#define SUN_AZIMUTH_SPEED     0.5f   /* radians/sec for arrow keys      */
#define SUN_AUTO_SPEED        0.01f  /* radians/sec for orbit rotation   */
#define SUN_ORBIT_TILT        1.2f   /* max elevation (~69 deg), full day/night cycle */
#define SUN_INTENSITY         20.0f  /* radiance multiplier             */

/* Atmosphere ray march defaults. */
#define NUM_VIEW_STEPS   32  /* outer ray march step count             */

/* Precomputed LUT dimensions. */
#define TRANSMITTANCE_LUT_W  256  /* transmittance LUT width in texels  */
#define TRANSMITTANCE_LUT_H   64  /* transmittance LUT height in texels */
#define MULTISCATTER_LUT_W    32  /* multi-scattering LUT width         */
#define MULTISCATTER_LUT_H    32  /* multi-scattering LUT height        */
#define COMPUTE_WORKGROUP      8  /* compute shader workgroup size      */

/* HDR render target format. */
#define HDR_FORMAT SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT

/* Bloom mip chain — 5 levels of progressive half-resolution.
 * For 1280x720: 640x360 → 320x180 → 160x90 → 80x45 → 40x22 */
#define BLOOM_MIP_COUNT 5

/* Bloom defaults. */
#define DEFAULT_BLOOM_INTENSITY 0.04f
#define DEFAULT_BLOOM_THRESHOLD 1.0f

/* Exposure control. */
#define DEFAULT_EXPOSURE 1.0f
#define EXPOSURE_STEP    0.1f
#define MIN_EXPOSURE     0.1f
#define MAX_EXPOSURE     20.0f

/* Tone mapping modes (matching shader constants). */
#define TONEMAP_CLAMP    0
#define TONEMAP_REINHARD 1
#define TONEMAP_ACES     2

/* Camera and sun starting orientation — sun near the horizon, camera
 * facing it for an appealing default sunset view. */
#define CAM_START_YAW         1.63f  /* face the sun at orbit start angle           */
#define SUN_ORBIT_START       3.08f  /* near π — sun ~4° above horizon, setting     */

/* Capture mode — fixed sun angle for consistent screenshots.
 * Matches the default orbit start for a warm sunset view. */
#define CAPTURE_SUN_ELEVATION  0.04f /* ~2 degrees — deep sunset, max warm scattering */
#define CAPTURE_SUN_AZIMUTH   SUN_ORBIT_START /* match orbit start angle            */
#define CAPTURE_CAM_YAW       CAM_START_YAW   /* face the sun at orbit start        */

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* ── Uniform structures ──────────────────────────────────────────────────── */

/* Sky vertex uniforms — ray matrix mapping NDC to world-space directions.
 * Built from camera basis vectors scaled by FOV/aspect, avoiding the
 * precision loss of inverse VP at planet-centric coordinates. */
typedef struct SkyVertUniforms {
  mat4 ray_matrix;   /* NDC → world-space ray direction (64 bytes) */
} SkyVertUniforms;   /* 64 bytes */

/* Sky fragment uniforms — camera, sun, and march parameters.
 * Must match the cbuffer layout in sky.frag.hlsl exactly.
 *
 * Layout (48 bytes):
 *   float3 cam_pos_km      (12 bytes)
 *   float  sun_intensity   ( 4 bytes)
 *   float3 sun_dir         (12 bytes)
 *   int    num_steps       ( 4 bytes)
 *   float2 resolution      ( 8 bytes)
 *   float2 _pad            ( 8 bytes) */
typedef struct SkyFragUniforms {
  float cam_pos_km[3];    /* camera position in km (planet-centric)   */
  float sun_intensity;     /* sun radiance multiplier                  */
  float sun_dir[3];        /* normalized direction toward the sun      */
  int   num_steps;         /* outer ray march step count               */
  float resolution[2];     /* window size in pixels                    */
  float _pad[2];           /* pad to 16-byte alignment          (8 bytes) */
} SkyFragUniforms;         /* 48 bytes */

/* Bloom downsample uniforms. */
typedef struct BloomDownsampleUniforms {
  float texel_size[2]; /* 1/source_width, 1/source_height (8 bytes) */
  float threshold;     /* brightness threshold              (4 bytes) */
  float use_karis;     /* 1.0 first pass, 0.0 rest          (4 bytes) */
} BloomDownsampleUniforms; /* 16 bytes */

/* Bloom upsample uniforms. */
typedef struct BloomUpsampleUniforms {
  float texel_size[2]; /* 1/source_width, 1/source_height (8 bytes) */
  float _pad[2];       /* pad to 16 bytes                  (8 bytes) */
} BloomUpsampleUniforms; /* 16 bytes */

/* Tone map fragment uniforms. */
typedef struct TonemapFragUniforms {
  float  exposure;       /* exposure multiplier     (4 bytes) */
  Uint32 tonemap_mode;   /* 0=clamp, 1=Reinh, 2=AC (4 bytes) */
  float  bloom_intensity; /* bloom contribution     (4 bytes) */
  float  _pad;           /* pad to 16 bytes         (4 bytes) */
} TonemapFragUniforms;   /* 16 bytes */

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct AppState {
  SDL_Window    *window;  /* main application window                        */
  SDL_GPUDevice *device;  /* GPU device handle (Vulkan/D3D12)               */

  /* Graphics pipelines — one per render pass type. */
  SDL_GPUGraphicsPipeline *sky_pipeline;        /* atmosphere ray march → HDR target    */
  SDL_GPUGraphicsPipeline *downsample_pipeline; /* 13-tap Jimenez bloom downsample      */
  SDL_GPUGraphicsPipeline *upsample_pipeline;   /* 9-tap tent upsample, additive blend  */
  SDL_GPUGraphicsPipeline *tonemap_pipeline;    /* HDR + bloom → swapchain (ACES)       */

  /* Compute pipelines — LUT generation (one-time at startup). */
  SDL_GPUComputePipeline *transmittance_compute;  /* transmittance LUT generator  */
  SDL_GPUComputePipeline *multiscatter_compute;   /* multi-scattering LUT generator */

  /* Precomputed LUT textures. */
  SDL_GPUTexture *transmittance_lut;  /* 256×64 transmittance (view_height × cos_zenith)  */
  SDL_GPUTexture *multiscatter_lut;   /* 32×32 multi-scattering (cos_sun × altitude)      */
  SDL_GPUSampler *lut_sampler;        /* linear/clamp sampler shared by both LUTs          */

  /* HDR render target — R16G16B16A16_FLOAT, both COLOR_TARGET and SAMPLER. */
  SDL_GPUTexture *hdr_target;   /* sky output texture (HDR floating point)  */
  SDL_GPUSampler *hdr_sampler;  /* linear/clamp sampler for tonemap input   */
  Uint32 hdr_width;             /* current HDR target width in pixels       */
  Uint32 hdr_height;            /* current HDR target height in pixels      */

  /* Bloom mip chain — 5 half-res HDR textures for downsample/upsample. */
  SDL_GPUTexture *bloom_mips[BLOOM_MIP_COUNT];    /* bloom mip textures [0..4]     */
  Uint32 bloom_widths[BLOOM_MIP_COUNT];            /* width of each bloom mip       */
  Uint32 bloom_heights[BLOOM_MIP_COUNT];           /* height of each bloom mip      */
  SDL_GPUSampler *bloom_sampler;                   /* linear/clamp sampler for bloom */

  /* Camera — quaternion fly camera in km-space (planet-centric). */
  vec3  cam_position;  /* world-space camera position in km                */
  float cam_yaw;       /* horizontal rotation in radians (0 = +Z)         */
  float cam_pitch;     /* vertical rotation in radians (clamped ±1.5)     */

  /* Sun direction — controlled by elevation + azimuth angles. */
  float sun_elevation;  /* radians above horizon (0 = horizon, π/2 = zenith)  */
  float sun_azimuth;    /* radians from east (0 = east, increases CCW)        */
  bool  sun_auto;       /* true = auto day/night cycle                        */
  float sun_orbit_angle;/* continuous orbit angle for day/night cycle (radians)*/

  /* HDR settings — switchable at runtime. */
  float  exposure;      /* brightness multiplier before tone mapping (>0)  */
  Uint32 tonemap_mode;  /* 0=clamp, 1=Reinhard, 2=ACES filmic             */

  /* Bloom settings. */
  bool  bloom_enabled;    /* true = bloom post-process active              */
  float bloom_intensity;  /* bloom contribution strength (0 = off)         */
  float bloom_threshold;  /* brightness cutoff for bloom extraction        */

  /* Timing and input. */
  Uint64 last_ticks;      /* timestamp of previous frame for delta time    */
  bool   mouse_captured;  /* true = mouse captured for FPS-style controls  */

#ifdef FORGE_CAPTURE
  ForgeCapture capture;   /* screenshot / GIF capture state                */
#endif
} AppState;

/* ── Helper: create shader from SPIRV/DXIL bytecodes ──────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice   *device,
    SDL_GPUShaderStage stage,
    const Uint8     *spirv_code,
    size_t           spirv_size,
    const Uint8     *dxil_code,
    size_t           dxil_size,
    Uint32           num_samplers,
    Uint32           num_uniform_buffers
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

/* ── Helper: create compute pipeline from SPIRV/DXIL bytecodes ────────────── */

static SDL_GPUComputePipeline *create_compute_pipeline(
    SDL_GPUDevice       *device,
    const unsigned char *spirv_code,  size_t spirv_size,
    const unsigned char *dxil_code,   size_t dxil_size,
    int num_samplers,
    int num_readwrite_storage_textures,
    int num_uniform_buffers,
    int threadcount_x,
    int threadcount_y,
    int threadcount_z
) {
  SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

  SDL_GPUComputePipelineCreateInfo info;
  SDL_zero(info);
  info.entrypoint                     = "main";
  info.num_samplers                   = num_samplers;
  info.num_readwrite_storage_textures = num_readwrite_storage_textures;
  info.num_uniform_buffers            = num_uniform_buffers;
  info.threadcount_x                  = threadcount_x;
  info.threadcount_y                  = threadcount_y;
  info.threadcount_z                  = threadcount_z;

  if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
    info.code      = spirv_code;
    info.code_size = spirv_size;
  } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
    info.format    = SDL_GPU_SHADERFORMAT_DXIL;
    info.code      = dxil_code;
    info.code_size = dxil_size;
  } else {
    SDL_Log("No supported shader format for compute pipeline");
    return NULL;
  }

  SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(device, &info);
  if (!pipeline) {
    SDL_Log("Failed to create compute pipeline: %s", SDL_GetError());
  }
  return pipeline;
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

/* ── Helper: create bloom mip chain ───────────────────────────────────────── */

static bool create_bloom_mip_chain(AppState *state) {
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

/* Release all bloom mip textures. */
static void release_bloom_mip_chain(AppState *state) {
  for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
    if (state->bloom_mips[i]) {
      SDL_ReleaseGPUTexture(state->device, state->bloom_mips[i]);
      state->bloom_mips[i] = NULL;
    }
  }
}

/* ── Helper: spherical to Cartesian sun direction ─────────────────────────── */

/* Convert sun elevation (radians above horizon) and azimuth (radians from
 * east, increasing CCW viewed from above) to a normalized direction vector.
 *
 * Convention: Y is up (radial direction from planet center).
 *   elevation = 0  → sun at horizon
 *   elevation = π/2 → sun at zenith
 *   azimuth   = 0  → sun to the east (+X)
 *   azimuth   = π/2 → sun to the north (+Z) */
static vec3 sun_direction_from_angles(float elevation, float azimuth) {
  float cos_el = SDL_cosf(elevation);
  vec3 dir;
  dir.x = cos_el * SDL_cosf(azimuth);
  dir.y = SDL_sinf(elevation);
  dir.z = cos_el * SDL_sinf(azimuth);
  return dir;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*                              SDL CALLBACKS                               */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppInit ──────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  /* Step 1 — Initialize SDL video subsystem. */
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* Step 2 — Create GPU device with debug enabled. */
  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
  if (!device) {
    SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* Step 3 — Create window. */
  SDL_Window *window =
      SDL_CreateWindow("Lesson 26 \xe2\x80\x94 Procedural Sky (Hillaire)", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

  /* Step 5 — Request SDR_LINEAR for correct sRGB gamma handling. */
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

  SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

  /* Step 6 — Allocate AppState. */
  AppState *state = SDL_calloc(1, sizeof(AppState));
  if (!state) {
    SDL_Log("Failed to allocate AppState");
    SDL_ReleaseWindowFromGPUDevice(device, window);
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

  /* Step 8 — Create HDR render target. */
  state->hdr_width = (Uint32)draw_w;
  state->hdr_height = (Uint32)draw_h;
  state->hdr_target = create_hdr_target(device, state->hdr_width, state->hdr_height);
  if (!state->hdr_target) {
    goto init_fail;
  }

  /* Step 9 — Create bloom mip chain. */
  if (!create_bloom_mip_chain(state)) {
    goto init_fail;
  }

  /* Step 10 — Create samplers. */
  {
    /* HDR sampler — linear filtering, clamp to edge. */
    SDL_GPUSamplerCreateInfo samp_info;
    SDL_zero(samp_info);
    samp_info.min_filter = SDL_GPU_FILTER_LINEAR;
    samp_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    state->hdr_sampler = SDL_CreateGPUSampler(device, &samp_info);
    if (!state->hdr_sampler) {
      SDL_Log("Failed to create HDR sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }
  {
    /* Bloom sampler — linear filtering, clamp to edge. */
    SDL_GPUSamplerCreateInfo samp_info;
    SDL_zero(samp_info);
    samp_info.min_filter = SDL_GPU_FILTER_LINEAR;
    samp_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    state->bloom_sampler = SDL_CreateGPUSampler(device, &samp_info);
    if (!state->bloom_sampler) {
      SDL_Log("Failed to create bloom sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }
  {
    /* LUT sampler — linear filtering, clamp to edge (shared by both LUTs). */
    SDL_GPUSamplerCreateInfo samp_info;
    SDL_zero(samp_info);
    samp_info.min_filter = SDL_GPU_FILTER_LINEAR;
    samp_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    state->lut_sampler = SDL_CreateGPUSampler(device, &samp_info);
    if (!state->lut_sampler) {
      SDL_Log("Failed to create LUT sampler: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 11 — Create LUT textures for precomputed atmosphere data.
   * Both use R16G16B16A16_FLOAT, COMPUTE_STORAGE_WRITE for the compute
   * shader to write, and SAMPLER for the fragment shader to read. */
  {
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = HDR_FORMAT;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
                   | SDL_GPU_TEXTUREUSAGE_SAMPLER;

    /* Transmittance LUT: 256×64. */
    tex_info.width = TRANSMITTANCE_LUT_W;
    tex_info.height = TRANSMITTANCE_LUT_H;
    state->transmittance_lut = SDL_CreateGPUTexture(device, &tex_info);
    if (!state->transmittance_lut) {
      SDL_Log("Failed to create transmittance LUT: %s", SDL_GetError());
      goto init_fail;
    }

    /* Multi-scattering LUT: 32×32. */
    tex_info.width = MULTISCATTER_LUT_W;
    tex_info.height = MULTISCATTER_LUT_H;
    state->multiscatter_lut = SDL_CreateGPUTexture(device, &tex_info);
    if (!state->multiscatter_lut) {
      SDL_Log("Failed to create multi-scattering LUT: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 12 — Create compute pipelines for LUT generation. */
  {
    /* Transmittance compute: 0 samplers, 1 RW storage texture, 0 uniforms.
     * LUT dimensions are compile-time constants in the shader. */
    state->transmittance_compute = create_compute_pipeline(
        device,
        transmittance_lut_comp_spirv, sizeof(transmittance_lut_comp_spirv),
        transmittance_lut_comp_dxil, sizeof(transmittance_lut_comp_dxil),
        0, 1, 0,
        COMPUTE_WORKGROUP, COMPUTE_WORKGROUP, 1);
    if (!state->transmittance_compute) {
      goto init_fail;
    }

    /* Multi-scattering compute: 1 sampler, 1 RW storage texture, 0 uniforms.
     * LUT dimensions are compile-time constants in the shader. */
    state->multiscatter_compute = create_compute_pipeline(
        device,
        multiscatter_lut_comp_spirv, sizeof(multiscatter_lut_comp_spirv),
        multiscatter_lut_comp_dxil, sizeof(multiscatter_lut_comp_dxil),
        1, 1, 0,
        COMPUTE_WORKGROUP, COMPUTE_WORKGROUP, 1);
    if (!state->multiscatter_compute) {
      goto init_fail;
    }
  }

  /* Step 13 — One-time LUT generation via compute dispatch.
   * Two separate compute passes on one command buffer.  SDL GPU does NOT
   * implicitly synchronize reads/writes within a single compute pass,
   * so the multi-scattering pass (which reads the transmittance LUT)
   * must be a separate pass from the transmittance write pass. */
  {
    SDL_GPUCommandBuffer *lut_cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!lut_cmd) {
      SDL_Log("Failed to acquire command buffer for LUT generation: %s", SDL_GetError());
      goto init_fail;
    }

    /* Pass 1: Generate transmittance LUT.
     * No uniforms needed — LUT dimensions are compile-time constants. */
    {
      SDL_GPUStorageTextureReadWriteBinding storage_binding;
      SDL_zero(storage_binding);
      storage_binding.texture = state->transmittance_lut;
      storage_binding.mip_level = 0;
      storage_binding.layer = 0;
      storage_binding.cycle = false;

      SDL_GPUComputePass *pass = SDL_BeginGPUComputePass(
          lut_cmd, &storage_binding, 1, NULL, 0);
      if (!pass) {
        SDL_Log("Failed to begin transmittance compute pass: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(lut_cmd)) {
          SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        goto init_fail;
      }

      SDL_BindGPUComputePipeline(pass, state->transmittance_compute);

      Uint32 groups_x = (TRANSMITTANCE_LUT_W + COMPUTE_WORKGROUP - 1) / COMPUTE_WORKGROUP;
      Uint32 groups_y = (TRANSMITTANCE_LUT_H + COMPUTE_WORKGROUP - 1) / COMPUTE_WORKGROUP;
      SDL_DispatchGPUCompute(pass, groups_x, groups_y, 1);
      SDL_EndGPUComputePass(pass);
    }

    /* Pass 2: Generate multi-scattering LUT.
     * This pass reads the transmittance LUT (as a sampled texture)
     * and writes the multi-scattering LUT.
     * No uniforms needed — LUT dimensions are compile-time constants. */
    {
      SDL_GPUStorageTextureReadWriteBinding storage_binding;
      SDL_zero(storage_binding);
      storage_binding.texture = state->multiscatter_lut;
      storage_binding.mip_level = 0;
      storage_binding.layer = 0;
      storage_binding.cycle = false;

      SDL_GPUComputePass *pass = SDL_BeginGPUComputePass(
          lut_cmd, &storage_binding, 1, NULL, 0);
      if (!pass) {
        SDL_Log("Failed to begin multi-scattering compute pass: %s", SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(lut_cmd)) {
          SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        goto init_fail;
      }

      SDL_BindGPUComputePipeline(pass, state->multiscatter_compute);

      /* Bind the transmittance LUT as a sampled texture for reading. */
      SDL_GPUTextureSamplerBinding sampler_binding;
      SDL_zero(sampler_binding);
      sampler_binding.texture = state->transmittance_lut;
      sampler_binding.sampler = state->lut_sampler;
      SDL_BindGPUComputeSamplers(pass, 0, &sampler_binding, 1);

      Uint32 groups_x = (MULTISCATTER_LUT_W + COMPUTE_WORKGROUP - 1) / COMPUTE_WORKGROUP;
      Uint32 groups_y = (MULTISCATTER_LUT_H + COMPUTE_WORKGROUP - 1) / COMPUTE_WORKGROUP;
      SDL_DispatchGPUCompute(pass, groups_x, groups_y, 1);
      SDL_EndGPUComputePass(pass);
    }

    if (!SDL_SubmitGPUCommandBuffer(lut_cmd)) {
      SDL_Log("SDL_SubmitGPUCommandBuffer (LUT generation) failed: %s", SDL_GetError());
      goto init_fail;
    }

    /* Wait for the GPU to finish LUT generation before continuing.
     * The LUTs must be ready before the first frame renders. */
    if (!SDL_WaitForGPUIdle(device)) {
      SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
      goto init_fail;
    }
    SDL_Log("  LUT generation complete (transmittance %dx%d, multiscatter %dx%d)",
            TRANSMITTANCE_LUT_W, TRANSMITTANCE_LUT_H,
            MULTISCATTER_LUT_W, MULTISCATTER_LUT_H);
  }

  /* Step 14 — Create the sky pipeline.
   * Renders to the HDR target.  No vertex buffer — fullscreen quad via
   * SV_VertexID.  The sky vertex shader has 1 uniform buffer (ray_matrix),
   * the fragment shader has 2 samplers (LUTs) and 1 uniform buffer. */
  {
    SDL_GPUShader *vert = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        sky_vert_spirv, sizeof(sky_vert_spirv),
        sky_vert_dxil, sizeof(sky_vert_dxil),
        0, 1); /* 0 samplers, 1 uniform buffer (ray_matrix) */

    SDL_GPUShader *frag = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        sky_frag_spirv, sizeof(sky_frag_spirv),
        sky_frag_dxil, sizeof(sky_frag_dxil),
        2, 1); /* 2 samplers (transmittance + multiscatter LUTs), 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create sky shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pipe_info;
    SDL_zero(pipe_info);
    pipe_info.vertex_shader = vert;
    pipe_info.fragment_shader = frag;
    pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipe_info.target_info.color_target_descriptions = &color_desc;
    pipe_info.target_info.num_color_targets = 1;
    pipe_info.target_info.has_depth_stencil_target = false;

    state->sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!state->sky_pipeline) {
      SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 15 — Create the bloom downsample pipeline. */
  {
    SDL_GPUShader *vert = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil),
        0, 0); /* no samplers, no uniforms in vertex */

    SDL_GPUShader *frag = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        bloom_downsample_frag_spirv, sizeof(bloom_downsample_frag_spirv),
        bloom_downsample_frag_dxil, sizeof(bloom_downsample_frag_dxil),
        1, 1); /* 1 sampler, 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create bloom downsample shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT;

    SDL_GPUGraphicsPipelineCreateInfo pipe_info;
    SDL_zero(pipe_info);
    pipe_info.vertex_shader = vert;
    pipe_info.fragment_shader = frag;
    pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipe_info.target_info.color_target_descriptions = &color_desc;
    pipe_info.target_info.num_color_targets = 1;
    pipe_info.target_info.has_depth_stencil_target = false;

    state->downsample_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!state->downsample_pipeline) {
      SDL_Log("Failed to create bloom downsample pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 16 — Create the bloom upsample pipeline.
   * Additive blending (ONE + ONE) so upsampled contribution accumulates. */
  {
    SDL_GPUShader *vert = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil),
        0, 0);

    SDL_GPUShader *frag = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        bloom_upsample_frag_spirv, sizeof(bloom_upsample_frag_spirv),
        bloom_upsample_frag_dxil, sizeof(bloom_upsample_frag_dxil),
        1, 1); /* 1 sampler, 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create bloom upsample shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = HDR_FORMAT;
    /* Additive blend: output = src * ONE + dst * ONE. */
    color_desc.blend_state.enable_blend = true;
    color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo pipe_info;
    SDL_zero(pipe_info);
    pipe_info.vertex_shader = vert;
    pipe_info.fragment_shader = frag;
    pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipe_info.target_info.color_target_descriptions = &color_desc;
    pipe_info.target_info.num_color_targets = 1;
    pipe_info.target_info.has_depth_stencil_target = false;

    state->upsample_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);

    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!state->upsample_pipeline) {
      SDL_Log("Failed to create bloom upsample pipeline: %s", SDL_GetError());
      goto init_fail;
    }
  }

  /* Step 17 — Create the tone mapping pipeline. */
  {
    SDL_GPUShader *vert = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        fullscreen_vert_spirv, sizeof(fullscreen_vert_spirv),
        fullscreen_vert_dxil, sizeof(fullscreen_vert_dxil),
        0, 0);

    SDL_GPUShader *frag = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        tonemap_frag_spirv, sizeof(tonemap_frag_spirv),
        tonemap_frag_dxil, sizeof(tonemap_frag_dxil),
        2, 1); /* 2 samplers (HDR + bloom), 1 uniform buffer */

    if (!vert || !frag) {
      SDL_Log("Failed to create tonemap shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      goto init_fail;
    }

    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    color_desc.format = swapchain_format;

    SDL_GPUGraphicsPipelineCreateInfo pipe_info;
    SDL_zero(pipe_info);
    pipe_info.vertex_shader = vert;
    pipe_info.fragment_shader = frag;
    pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
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

  /* Step 18 — Initialize camera and sun state. */
  state->cam_position = (vec3){ CAM_START_X, CAM_START_Y, CAM_START_Z };
  state->cam_yaw = CAM_START_YAW;
  state->cam_pitch = 0.0f;

  state->sun_elevation = SUN_ELEVATION_DEFAULT;
  state->sun_azimuth = SUN_AZIMUTH_DEFAULT;
  state->sun_auto = true;
  state->sun_orbit_angle = SUN_ORBIT_START;

  state->exposure = DEFAULT_EXPOSURE;
  state->tonemap_mode = TONEMAP_ACES;
  state->bloom_enabled = true;
  state->bloom_intensity = DEFAULT_BLOOM_INTENSITY;
  state->bloom_threshold = DEFAULT_BLOOM_THRESHOLD;

  state->last_ticks = SDL_GetPerformanceCounter();

  /* Step 19 — Capture the mouse for FPS-style controls. */
  if (!SDL_SetWindowRelativeMouseMode(window, true)) {
    SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
  }
  state->mouse_captured = true;

#ifdef FORGE_CAPTURE
  /* If capture mode, set a known sun angle for consistent screenshots. */
  if (state->capture.mode != FORGE_CAPTURE_NONE) {
    state->sun_elevation = CAPTURE_SUN_ELEVATION;
    state->sun_azimuth = CAPTURE_SUN_AZIMUTH;
    state->cam_yaw = CAPTURE_CAM_YAW;
    state->sun_auto = false;
    forge_capture_init(&state->capture, state->device, state->window);
  }
#endif

  SDL_Log("Lesson 26 — Procedural Sky (Hillaire) initialized");
  SDL_Log("  Camera: (%.3f, %.3f, %.3f) km", state->cam_position.x,
          state->cam_position.y, state->cam_position.z);
  SDL_Log("  Controls: WASD + mouse = fly, arrows = sun, T = auto-sun, 1/2/3 = tonemap");

  *appstate = state;
  return SDL_APP_CONTINUE;

init_fail:
  *appstate = state;
  return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  AppState *state = (AppState *)appstate;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;

  case SDL_EVENT_KEY_DOWN:
    /* Only process non-repeat key presses for toggles. */
    if (!event->key.repeat) {
      switch (event->key.scancode) {
      case SDL_SCANCODE_ESCAPE:
        if (state->mouse_captured) {
          /* First escape releases the mouse. */
          if (!SDL_SetWindowRelativeMouseMode(state->window, false)) {
            SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
          }
          state->mouse_captured = false;
        } else {
          return SDL_APP_SUCCESS; /* Second escape quits. */
        }
        break;

      /* Tone mapping mode selection. */
      case SDL_SCANCODE_1:
        state->tonemap_mode = TONEMAP_CLAMP;
        SDL_Log("Tone mapping: Clamp");
        break;
      case SDL_SCANCODE_2:
        state->tonemap_mode = TONEMAP_REINHARD;
        SDL_Log("Tone mapping: Reinhard");
        break;
      case SDL_SCANCODE_3:
        state->tonemap_mode = TONEMAP_ACES;
        SDL_Log("Tone mapping: ACES filmic");
        break;

      /* Bloom toggle. */
      case SDL_SCANCODE_B:
        state->bloom_enabled = !state->bloom_enabled;
        SDL_Log("Bloom: %s", state->bloom_enabled ? "ON" : "OFF");
        break;

      /* Auto sun toggle. */
      case SDL_SCANCODE_T:
        state->sun_auto = !state->sun_auto;
        SDL_Log("Auto sun rotation: %s", state->sun_auto ? "ON" : "OFF");
        break;

      default:
        break;
      }
    }

    /* Exposure controls (allow repeat). */
    if (event->key.scancode == SDL_SCANCODE_EQUALS) {
      state->exposure += EXPOSURE_STEP;
      if (state->exposure > MAX_EXPOSURE) state->exposure = MAX_EXPOSURE;
      SDL_Log("Exposure: %.1f", state->exposure);
    } else if (event->key.scancode == SDL_SCANCODE_MINUS) {
      state->exposure -= EXPOSURE_STEP;
      if (state->exposure < MIN_EXPOSURE) state->exposure = MIN_EXPOSURE;
      SDL_Log("Exposure: %.1f", state->exposure);
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    /* Click to recapture the mouse. */
    if (!state->mouse_captured) {
      if (!SDL_SetWindowRelativeMouseMode(state->window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
      }
      state->mouse_captured = true;
    }
    break;

  case SDL_EVENT_MOUSE_MOTION:
    /* Only process mouse movement when captured. */
    if (state->mouse_captured) {
      state->cam_yaw -= event->motion.xrel * MOUSE_SENS;
      state->cam_pitch -= event->motion.yrel * MOUSE_SENS;

      /* Clamp pitch to prevent camera from flipping. */
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
  AppState *state = (AppState *)appstate;

  /* ── Delta time ─────────────────────────────────────────────────── */
  Uint64 now = SDL_GetPerformanceCounter();
  float dt = (float)(now - state->last_ticks) / (float)SDL_GetPerformanceFrequency();
  state->last_ticks = now;
  if (dt > MAX_FRAME_DT) dt = MAX_FRAME_DT;

  /* ── Process held keys for movement ─────────────────────────────── */
  {
    const bool *keys = SDL_GetKeyboardState(NULL);

    /* Camera movement — WASD + Space/C for up/down. */
    float speed = CAM_SPEED * dt;
    if (keys[SDL_SCANCODE_LSHIFT]) speed *= CAM_SPEED_BOOST;

    /* Build orientation quaternion from yaw/pitch. */
    quat orientation = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);

    /* Get local directions from the quaternion. */
    vec3 forward = quat_forward(orientation);
    vec3 right = quat_right(orientation);
    vec3 up = { 0.0f, 1.0f, 0.0f }; /* world up for consistent vertical movement */

    if (keys[SDL_SCANCODE_W]) {
      state->cam_position.x += forward.x * speed;
      state->cam_position.y += forward.y * speed;
      state->cam_position.z += forward.z * speed;
    }
    if (keys[SDL_SCANCODE_S]) {
      state->cam_position.x -= forward.x * speed;
      state->cam_position.y -= forward.y * speed;
      state->cam_position.z -= forward.z * speed;
    }
    if (keys[SDL_SCANCODE_D]) {
      state->cam_position.x += right.x * speed;
      state->cam_position.y += right.y * speed;
      state->cam_position.z += right.z * speed;
    }
    if (keys[SDL_SCANCODE_A]) {
      state->cam_position.x -= right.x * speed;
      state->cam_position.y -= right.y * speed;
      state->cam_position.z -= right.z * speed;
    }
    if (keys[SDL_SCANCODE_SPACE]) {
      state->cam_position.x += up.x * speed;
      state->cam_position.y += up.y * speed;
      state->cam_position.z += up.z * speed;
    }
    if (keys[SDL_SCANCODE_C]) {
      state->cam_position.x -= up.x * speed;
      state->cam_position.y -= up.y * speed;
      state->cam_position.z -= up.z * speed;
    }

    /* Sun direction — arrow keys for manual control. */
    if (keys[SDL_SCANCODE_UP]) {
      state->sun_elevation += SUN_ELEVATION_SPEED * dt;
    }
    if (keys[SDL_SCANCODE_DOWN]) {
      state->sun_elevation -= SUN_ELEVATION_SPEED * dt;
    }
    if (keys[SDL_SCANCODE_RIGHT]) {
      state->sun_azimuth += SUN_AZIMUTH_SPEED * dt;
    }
    if (keys[SDL_SCANCODE_LEFT]) {
      state->sun_azimuth -= SUN_AZIMUTH_SPEED * dt;
    }

    /* Clamp sun elevation to [-π/2, π/2]. */
    if (state->sun_elevation > FORGE_PI * 0.5f) state->sun_elevation = FORGE_PI * 0.5f;
    if (state->sun_elevation < -FORGE_PI * 0.5f) state->sun_elevation = -FORGE_PI * 0.5f;
  }

  /* Auto day/night cycle — orbit the sun in a full circle so it rises,
   * passes overhead, sets, goes through night, and rises again.
   * The orbit angle drives elevation via sin() and azimuth via cos(),
   * giving a natural arc from east (sunrise) to west (sunset). */
  if (state->sun_auto) {
    state->sun_orbit_angle += SUN_AUTO_SPEED * dt;
    state->sun_elevation = SDL_sinf(state->sun_orbit_angle) * SUN_ORBIT_TILT;
    state->sun_azimuth   = state->sun_orbit_angle;
  }

  /* ── Build ray matrix for sky rendering ──────────────────────────
   * Instead of inv_vp (which loses precision when the camera is 6360 km
   * from the origin), we build a matrix that maps NDC directly to
   * world-space ray directions using camera basis vectors and FOV.
   *
   * For each pixel at NDC (nx, ny):
   *   ray_dir = nx * (aspect * tan(fov/2) * right)
   *           + ny * (tan(fov/2) * up)
   *           + forward
   *
   * This avoids the catastrophic precision loss from subtracting
   * huge world positions (≈6360 km) in the fragment shader.
   * ─────────────────────────────────────────────────────────────── */
  quat orientation = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);

  int draw_w = 0, draw_h = 0;
  if (!SDL_GetWindowSizeInPixels(state->window, &draw_w, &draw_h)) {
    draw_w = WINDOW_WIDTH;
    draw_h = WINDOW_HEIGHT;
  }
  float aspect = (float)draw_w / (float)draw_h;

  /* Camera basis vectors from quaternion orientation. */
  vec3 cam_right   = quat_right(orientation);
  vec3 cam_up      = quat_up(orientation);
  vec3 cam_forward = quat_forward(orientation);

  /* Scale right/up by the projection's FOV and aspect ratio so that
   * NDC [-1,1] maps to the correct angular range. */
  float half_fov_tan = SDL_tanf((float)FOV_DEG * FORGE_DEG2RAD * 0.5f);
  float sx = aspect * half_fov_tan;
  float sy = half_fov_tan;

  /* Build the ray matrix.  When the vertex shader computes
   * mul(ray_matrix, float4(ndc.x, ndc.y, 1.0, 1.0)):
   *   col0 * ndc.x  =  sx * right * ndc.x
   *   col1 * ndc.y  =  sy * up * ndc.y
   *   col2 * 1.0    =  (0,0,0,0)           — unused
   *   col3 * 1.0    =  (forward, 1)         — constant forward + w=1
   * Result: (sx*right*nx + sy*up*ny + forward, 1) */
  mat4 ray_matrix = { 0 };
  /* Column 0: scaled right direction */
  ray_matrix.m[0]  = sx * cam_right.x;
  ray_matrix.m[1]  = sx * cam_right.y;
  ray_matrix.m[2]  = sx * cam_right.z;
  /* Column 1: scaled up direction */
  ray_matrix.m[4]  = sy * cam_up.x;
  ray_matrix.m[5]  = sy * cam_up.y;
  ray_matrix.m[6]  = sy * cam_up.z;
  /* Column 2: zero (ndc.z = 1 but we don't use it for direction) */
  /* Column 3: forward direction + w=1 */
  ray_matrix.m[12] = cam_forward.x;
  ray_matrix.m[13] = cam_forward.y;
  ray_matrix.m[14] = cam_forward.z;
  ray_matrix.m[15] = 1.0f;

  /* Handle window resize — recreate HDR target and bloom chain. */
  if ((Uint32)draw_w != state->hdr_width || (Uint32)draw_h != state->hdr_height) {
    Uint32 new_w = (Uint32)draw_w;
    Uint32 new_h = (Uint32)draw_h;

    SDL_GPUTexture *new_hdr = create_hdr_target(state->device, new_w, new_h);
    if (!new_hdr) {
      SDL_Log("Resize: failed to create HDR target, keeping old size");
    } else {
      if (state->hdr_target) {
        SDL_ReleaseGPUTexture(state->device, state->hdr_target);
      }
      state->hdr_target = new_hdr;
      state->hdr_width = new_w;
      state->hdr_height = new_h;

      release_bloom_mip_chain(state);
      if (!create_bloom_mip_chain(state)) {
        SDL_Log("Resize: failed to create bloom mip chain");
      }
    }
  }

  /* Compute sun direction from elevation and azimuth. */
  vec3 sun_dir = sun_direction_from_angles(state->sun_elevation, state->sun_azimuth);

  /* ── Acquire command buffer and swapchain ────────────────────────── */
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
  if (!cmd) {
    SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_CONTINUE;
  }

  SDL_GPUTexture *swapchain = NULL;
  Uint32 sc_w, sc_h;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state->window, &swapchain, &sc_w, &sc_h)) {
    SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
      SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }
    return SDL_APP_CONTINUE;
  }
  if (!swapchain) {
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
      SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    }
    return SDL_APP_CONTINUE;
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 1 — SKY RENDERING → HDR target
   *
   * Ray-march through Earth's atmosphere for each pixel.  The vertex
   * shader reconstructs world-space view rays via the ray matrix.
   * LUT textures provide transmittance and multi-scattering data.
   * ════════════════════════════════════════════════════════════════════ */
  {
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = state->hdr_target;
    color_target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    if (!pass) {
      SDL_Log("Failed to begin sky render pass: %s", SDL_GetError());
      if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      }
      return SDL_APP_CONTINUE;
    }

    SDL_BindGPUGraphicsPipeline(pass, state->sky_pipeline);

    /* Bind LUT textures for the fragment shader. */
    SDL_GPUTextureSamplerBinding lut_bindings[2];
    SDL_zero(lut_bindings);
    lut_bindings[0].texture = state->transmittance_lut;
    lut_bindings[0].sampler = state->lut_sampler;
    lut_bindings[1].texture = state->multiscatter_lut;
    lut_bindings[1].sampler = state->lut_sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, lut_bindings, 2);

    /* Push vertex uniforms — ray matrix that maps NDC to world-space
     * view directions (replaces inv_vp to avoid float precision loss). */
    SkyVertUniforms vert_u;
    vert_u.ray_matrix = ray_matrix;
    SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

    /* Push fragment uniforms — camera, sun, march parameters. */
    SkyFragUniforms frag_u;
    frag_u.cam_pos_km[0] = state->cam_position.x;
    frag_u.cam_pos_km[1] = state->cam_position.y;
    frag_u.cam_pos_km[2] = state->cam_position.z;
    frag_u.sun_intensity = SUN_INTENSITY;
    frag_u.sun_dir[0] = sun_dir.x;
    frag_u.sun_dir[1] = sun_dir.y;
    frag_u.sun_dir[2] = sun_dir.z;
    frag_u.num_steps = NUM_VIEW_STEPS;
    frag_u.resolution[0] = (float)state->hdr_width;
    frag_u.resolution[1] = (float)state->hdr_height;
    frag_u._pad[0] = 0.0f;
    frag_u._pad[1] = 0.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

    SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 2 — BLOOM DOWNSAMPLE (5 passes)
   *
   * Progressive half-resolution filtering.  First pass reads from the
   * HDR target; subsequent passes read from the previous bloom mip.
   * First pass also applies brightness threshold and Karis averaging
   * to suppress firefly artifacts from the bright sun disc.
   * ════════════════════════════════════════════════════════════════════ */
  bool bloom_ok = state->bloom_enabled;

  if (bloom_ok) {
    for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
      SDL_GPUColorTargetInfo color_target;
      SDL_zero(color_target);
      color_target.texture = state->bloom_mips[i];
      color_target.load_op = SDL_GPU_LOADOP_CLEAR;
      color_target.store_op = SDL_GPU_STOREOP_STORE;
      color_target.clear_color.r = 0.0f;
      color_target.clear_color.g = 0.0f;
      color_target.clear_color.b = 0.0f;
      color_target.clear_color.a = 1.0f;

      SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
      if (!pass) {
        SDL_Log("Failed to begin bloom downsample pass %d: %s", i, SDL_GetError());
        bloom_ok = false;
        break;
      }

      SDL_BindGPUGraphicsPipeline(pass, state->downsample_pipeline);

      /* Source is HDR target for pass 0, previous bloom mip otherwise. */
      SDL_GPUTextureSamplerBinding src_binding;
      SDL_zero(src_binding);
      if (i == 0) {
        src_binding.texture = state->hdr_target;
      } else {
        src_binding.texture = state->bloom_mips[i - 1];
      }
      src_binding.sampler = state->bloom_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, &src_binding, 1);

      /* Texel size of the SOURCE texture. */
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
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 3 — BLOOM UPSAMPLE (4 passes, additive blend)
   *
   * Each pass reads from bloom_mips[i+1] (smaller) and additively
   * blends into bloom_mips[i] (larger).  LOAD op preserves existing
   * downsample data.
   * ════════════════════════════════════════════════════════════════════ */
  if (bloom_ok) {
    for (int i = BLOOM_MIP_COUNT - 2; i >= 0; i--) {
      SDL_GPUColorTargetInfo color_target;
      SDL_zero(color_target);
      color_target.texture = state->bloom_mips[i];
      color_target.load_op = SDL_GPU_LOADOP_LOAD;
      color_target.store_op = SDL_GPU_STOREOP_STORE;

      SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
      if (!pass) {
        SDL_Log("Failed to begin bloom upsample pass %d: %s", i, SDL_GetError());
        bloom_ok = false;
        break;
      }

      SDL_BindGPUGraphicsPipeline(pass, state->upsample_pipeline);

      /* Source: the smaller mip we're upsampling from. */
      SDL_GPUTextureSamplerBinding src_binding;
      SDL_zero(src_binding);
      src_binding.texture = state->bloom_mips[i + 1];
      src_binding.sampler = state->bloom_sampler;
      SDL_BindGPUFragmentSamplers(pass, 0, &src_binding, 1);

      /* Texel size of the SOURCE (smaller) texture. */
      BloomUpsampleUniforms us_u;
      SDL_zero(us_u);
      us_u.texel_size[0] = 1.0f / (float)state->bloom_widths[i + 1];
      us_u.texel_size[1] = 1.0f / (float)state->bloom_heights[i + 1];
      SDL_PushGPUFragmentUniformData(cmd, 0, &us_u, sizeof(us_u));

      SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
      SDL_EndGPURenderPass(pass);
    }
  }

  /* ════════════════════════════════════════════════════════════════════
   * PASS 4 — TONE MAP → swapchain
   *
   * Combines HDR sky with bloom, applies exposure and tone mapping,
   * writes to the sRGB swapchain.
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

    SDL_BindGPUGraphicsPipeline(pass, state->tonemap_pipeline);

    /* Bind HDR target (slot 0) and bloom result (slot 1).
     * If bloom mips are NULL (e.g. allocation failed during resize),
     * bind the HDR target as a dummy to avoid undefined behavior. */
    SDL_GPUTextureSamplerBinding tex_bindings[2];
    SDL_zero(tex_bindings);
    tex_bindings[0].texture = state->hdr_target;
    tex_bindings[0].sampler = state->hdr_sampler;
    tex_bindings[1].texture = state->bloom_mips[0]
                                  ? state->bloom_mips[0] : state->hdr_target;
    tex_bindings[1].sampler = state->bloom_sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 2);

    TonemapFragUniforms tonemap_u;
    SDL_zero(tonemap_u);
    tonemap_u.exposure = state->exposure;
    tonemap_u.tonemap_mode = state->tonemap_mode;
    tonemap_u.bloom_intensity = (bloom_ok && state->bloom_enabled)
                                    ? state->bloom_intensity : 0.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &tonemap_u, sizeof(tonemap_u));

    SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTS, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
  }

  /* ── Submit ───────────────────────────────────────────────────────── */
#ifdef FORGE_CAPTURE
  if (forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
    if (forge_capture_should_quit(&state->capture)) {
      return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
  }
#endif

  /* Submit the command buffer. */
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

  AppState *state = (AppState *)appstate;
  if (!state)
    return;

#ifdef FORGE_CAPTURE
  forge_capture_destroy(&state->capture, state->device);
#endif

  /* Release in reverse creation order. */
  if (state->tonemap_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->tonemap_pipeline);
  if (state->upsample_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->upsample_pipeline);
  if (state->downsample_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->downsample_pipeline);
  if (state->sky_pipeline)
    SDL_ReleaseGPUGraphicsPipeline(state->device, state->sky_pipeline);

  if (state->multiscatter_compute)
    SDL_ReleaseGPUComputePipeline(state->device, state->multiscatter_compute);
  if (state->transmittance_compute)
    SDL_ReleaseGPUComputePipeline(state->device, state->transmittance_compute);

  if (state->bloom_sampler)
    SDL_ReleaseGPUSampler(state->device, state->bloom_sampler);
  if (state->lut_sampler)
    SDL_ReleaseGPUSampler(state->device, state->lut_sampler);
  if (state->hdr_sampler)
    SDL_ReleaseGPUSampler(state->device, state->hdr_sampler);

  release_bloom_mip_chain(state);

  if (state->multiscatter_lut)
    SDL_ReleaseGPUTexture(state->device, state->multiscatter_lut);
  if (state->transmittance_lut)
    SDL_ReleaseGPUTexture(state->device, state->transmittance_lut);
  if (state->hdr_target)
    SDL_ReleaseGPUTexture(state->device, state->hdr_target);

  SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
  SDL_DestroyWindow(state->window);
  SDL_DestroyGPUDevice(state->device);
  SDL_free(state);
}
