/* This is production-quality library code, not a lesson demo. See CLAUDE.md "Library quality standards". */
/*
 * forge_scene.h — Scene renderer library for forge-gpu
 *
 * A header-only library that packages the rendering stack built across GPU
 * lessons 01–39 into a single include: SDL GPU device/window/swapchain setup,
 * depth texture management with auto-resize, directional shadow map with PCF
 * sampling, Blinn-Phong lighting, procedural grid floor, quaternion FPS
 * camera with mouse/keyboard input, sky gradient background, shader creation
 * from SPIRV/DXIL bytecode, GPU buffer upload helpers, and forge UI
 * initialization with font atlas + rendering pipeline.
 *
 * One forge_scene_init() call replaces 500–600 lines of boilerplate per
 * lesson.  Physics, audio, and future GPU lessons include this header and
 * focus entirely on their subject matter, not rendering plumbing.
 *
 * Usage:
 *   // In exactly one .c file:
 *   #define FORGE_SCENE_IMPLEMENTATION
 *   #include "scene/forge_scene.h"
 *
 *   // In other files (if any):
 *   #include "scene/forge_scene.h"
 *
 * Depends on:
 *   common/math/forge_math.h   — vectors, matrices, quaternions
 *   common/ui/forge_ui.h       — TTF parsing, font atlas
 *   common/ui/forge_ui_ctx.h   — immediate-mode UI context
 *   SDL3/SDL.h                 — GPU API, windowing, input
 *
 * See: lessons/gpu/40-scene-renderer/ for a lesson teaching this library.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_SCENE_H
#define FORGE_SCENE_H

#include <SDL3/SDL.h>
#include <stddef.h>

#include "math/forge_math.h"
#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "ui/forge_ui_window.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

#ifdef FORGE_SCENE_MODEL_SUPPORT
#include "pipeline/forge_pipeline.h"
#endif

/* ── Constants ──────────────────────────────────────────────────────────── */

/* Default window dimensions (16:9).  All lessons use this size. */
#define FORGE_SCENE_WINDOW_WIDTH   1280
#define FORGE_SCENE_WINDOW_HEIGHT  720

/* Default camera parameters */
#define FORGE_SCENE_FOV_DEG            60.0f
#define FORGE_SCENE_NEAR_PLANE         0.1f
#define FORGE_SCENE_FAR_PLANE          200.0f
#define FORGE_SCENE_MOVE_SPEED         5.0f
#define FORGE_SCENE_MOUSE_SENSITIVITY  0.003f
#define FORGE_SCENE_PITCH_CLAMP        1.5f
#define FORGE_SCENE_MAX_DELTA_TIME     0.1f

/* Default shadow map parameters */
#define FORGE_SCENE_SHADOW_MAP_SIZE    2048
#define FORGE_SCENE_SHADOW_ORTHO_SIZE  15.0f
#define FORGE_SCENE_SHADOW_HEIGHT      20.0f
#define FORGE_SCENE_SHADOW_NEAR        0.1f
#define FORGE_SCENE_SHADOW_FAR         50.0f
#define FORGE_SCENE_SHADOW_BIAS_CONST  2.0f
#define FORGE_SCENE_SHADOW_BIAS_SLOPE  2.0f

/* Default grid parameters */
#define FORGE_SCENE_GRID_HALF_SIZE     20.0f
#define FORGE_SCENE_GRID_SPACING       1.0f
#define FORGE_SCENE_GRID_LINE_WIDTH    0.02f
#define FORGE_SCENE_GRID_FADE_DIST     30.0f

/* Default lighting */
#define FORGE_SCENE_AMBIENT            0.15f
#define FORGE_SCENE_SHININESS          32.0f
#define FORGE_SCENE_SPECULAR_STR       0.5f
#define FORGE_SCENE_LIGHT_INTENSITY    1.2f

/* Default aspect ratio fallback */
#define FORGE_SCENE_DEFAULT_ASPECT     (16.0f / 9.0f)

/* Light direction stability thresholds */
#define FORGE_SCENE_LIGHT_DIR_EPSILON      1e-6f
#define FORGE_SCENE_LIGHT_UP_PARALLEL_COS  0.999f

/* UI atlas parameters */
#define FORGE_SCENE_ATLAS_PIXEL_HEIGHT 24.0f
#define FORGE_SCENE_ATLAS_PADDING      2
#define FORGE_SCENE_ASCII_START        32
#define FORGE_SCENE_ASCII_COUNT        95
#define FORGE_SCENE_UI_VB_CAPACITY     4096
#define FORGE_SCENE_UI_IB_CAPACITY     8192

/* Maximum materials per loaded model (BrainStem has 59 color-only materials) */
#define FORGE_SCENE_MODEL_MAX_MATERIALS 64

#ifdef FORGE_SCENE_MODEL_SUPPORT
/* Joint buffer size for skinned models: MAX_JOINTS × sizeof(mat4).
 * Each mat4 is 64 bytes → 256 × 64 = 16384 bytes. */
#define FORGE_SCENE_JOINT_BUFFER_SIZE \
    (FORGE_PIPELINE_MAX_SKIN_JOINTS * (Uint32)sizeof(mat4))
#endif

/* Maximum path buffer size for texture path resolution */
#define FORGE_SCENE_PATH_BUF_SIZE 1024

/* ── Vertex types ───────────────────────────────────────────────────────── */

/* Interleaved vertex for scene geometry: position + normal.
 * Matches the scene and shadow shader vertex input layouts. */
typedef struct ForgeSceneVertex {
    vec3 position;  /* object-space vertex position */
    vec3 normal;    /* object-space vertex normal   */
} ForgeSceneVertex;

/* Position-only vertex for the grid floor quad. */
typedef struct ForgeSceneGridVertex {
    float position[3]; /* world-space position (12 bytes) */
} ForgeSceneGridVertex;

#ifdef FORGE_SCENE_MODEL_SUPPORT
/* Interleaved vertex for pipeline-processed model geometry:
 * position + normal + UV + tangent (48 bytes).
 * Matches ForgeSceneModelVertex layout expected by scene_model.vert.hlsl. */
typedef struct ForgeSceneModelVertex {
    float position[3];  /* 12 bytes — object-space position */
    float normal[3];    /* 12 bytes — object-space normal   */
    float uv[2];        /* 8 bytes  — texture coordinates   */
    float tangent[4];   /* 16 bytes — xyz=tangent, w=bitangent sign */
} ForgeSceneModelVertex;
#endif /* FORGE_SCENE_MODEL_SUPPORT */

/* ── Uniform structures ─────────────────────────────────────────────────── */

/* Scene vertex uniforms: 3 matrices (192 bytes).
 * Must match scene.vert.hlsl cbuffer SceneVertUniforms. */
typedef struct ForgeSceneVertUniforms {
    mat4 mvp;       /* model-view-projection */
    mat4 model;     /* world transform       */
    mat4 light_vp;  /* light VP * model      */
} ForgeSceneVertUniforms;

/* Scene fragment uniforms — Blinn-Phong lighting parameters.
 * Must match scene.frag.hlsl cbuffer SceneFragUniforms (80 bytes). */
typedef struct ForgeSceneFragUniforms {
    float base_color[4];     /* material RGBA                        */
    float eye_pos[3];        /* camera position                      */
    float ambient;           /* ambient intensity                    */
    float light_dir[4];      /* directional light direction          */
    float light_color[3];    /* light RGB                            */
    float light_intensity;   /* light brightness                     */
    float shininess;         /* specular exponent                    */
    float specular_str;      /* specular strength                    */
    float _pad[2];           /* 16-byte alignment                    */
} ForgeSceneFragUniforms;

/* Grid vertex uniforms: 2 matrices (128 bytes).
 * Must match grid.vert.hlsl cbuffer GridVertUniforms. */
typedef struct ForgeSceneGridVertUniforms {
    mat4 vp;        /* view-projection       */
    mat4 light_vp;  /* light view-projection */
} ForgeSceneGridVertUniforms;

/* Grid fragment uniforms — procedural grid + lighting (80 bytes).
 * Must match grid.frag.hlsl cbuffer GridFragUniforms. */
typedef struct ForgeSceneGridFragUniforms {
    float line_color[4];     /* grid line color                      */
    float bg_color[4];       /* background surface color             */
    float light_dir[3];      /* directional light direction          */
    float light_intensity;   /* light brightness                     */
    float eye_pos[3];        /* camera position                      */
    float grid_spacing;      /* world units between lines            */
    float line_width;        /* line thickness [0..0.5]              */
    float fade_distance;     /* distance where grid fades            */
    float ambient;           /* ambient intensity                    */
    float _pad;              /* 16-byte alignment                    */
} ForgeSceneGridFragUniforms;

/* Shadow pass vertex uniforms: 1 matrix (64 bytes).
 * Must match shadow.vert.hlsl cbuffer ShadowUniforms. */
typedef struct ForgeSceneShadowVertUniforms {
    mat4 light_vp;  /* light VP * model */
} ForgeSceneShadowVertUniforms;

/* UI vertex uniforms: orthographic projection (64 bytes). */
typedef struct ForgeSceneUiUniforms {
    mat4 projection;
} ForgeSceneUiUniforms;

#ifdef FORGE_SCENE_MODEL_SUPPORT
/* Model fragment uniforms — per-draw-call material + lighting (96 bytes).
 * Must match scene_model.frag.hlsl cbuffer FragUniforms. */
typedef struct ForgeSceneModelFragUniforms {
    float light_dir[4];         /* xyz = direction toward light              */
    float eye_pos[4];           /* xyz = camera position                     */
    float base_color_factor[4]; /* RGBA multiplier from material             */
    float emissive_factor[3];   /* RGB emission multiplier                   */
    float shadow_texel;         /* 1.0 / shadow_map_resolution              */
    float metallic_factor;      /* 0 = dielectric, 1 = metal                */
    float roughness_factor;     /* 0 = mirror, 1 = rough                    */
    float normal_scale;         /* normal map XY intensity                   */
    float occlusion_strength;   /* AO blend: 0 = none, 1 = full             */
    float shininess;            /* Blinn-Phong specular exponent             */
    float specular_str;         /* specular intensity multiplier             */
    float alpha_cutoff;         /* MASK mode threshold                       */
    float ambient;              /* ambient light intensity [0..1]            */
} ForgeSceneModelFragUniforms;

/* VRAM usage tracking for texture compression comparison. */
typedef struct ForgeSceneVramStats {
    uint64_t compressed_bytes;         /* actual bytes uploaded (compressed or raw) */
    uint64_t uncompressed_bytes;       /* what it would cost as RGBA8 with mipmaps */
    uint32_t compressed_texture_count; /* how many textures loaded as compressed */
    uint32_t total_texture_count;      /* total textures loaded for this model */
} ForgeSceneVramStats;

/* Per-material GPU textures for a loaded model. */
typedef struct ForgeSceneModelTextures {
    SDL_GPUTexture *base_color;          /* NULL -> white fallback          */
    SDL_GPUTexture *normal;              /* NULL -> flat normal fallback    */
    SDL_GPUTexture *metallic_roughness;  /* NULL -> white fallback (1.0)   */
    SDL_GPUTexture *occlusion;           /* NULL -> white fallback (1.0)   */
    SDL_GPUTexture *emissive;            /* NULL -> black fallback (0.0)   */
} ForgeSceneModelTextures;

/* A loaded pipeline model with GPU resources. */
typedef struct ForgeSceneModel {
    /* Pipeline data (owns memory — freed by forge_scene_free_model) */
    ForgePipelineScene       scene_data;  /* node hierarchy + world transforms */
    ForgePipelineMesh        mesh;        /* vertex/index data + submeshes     */
    ForgePipelineMaterialSet materials;   /* PBR material descriptions         */

    /* GPU resources */
    SDL_GPUBuffer *vertex_buffer;   /* uploaded 48-byte vertices */
    SDL_GPUBuffer *index_buffer;    /* uploaded uint32 indices   */

    /* Per-material textures (indexed by material_index from submesh) */
    ForgeSceneModelTextures mat_textures[FORGE_SCENE_MODEL_MAX_MATERIALS];
    uint32_t                mat_texture_count;

    /* Stats (updated per draw_model call) */
    uint32_t draw_calls;

    /* VRAM tracking for texture compression comparison.
     * Updated during texture loading — includes all material textures. */
    ForgeSceneVramStats vram;
} ForgeSceneModel;

/* A loaded skinned pipeline model with GPU resources and animation state.
 * Supports .fmesh v3 (72-byte vertices with joints/weights), .fskin
 * (skeleton hierarchy + inverse bind matrices), and .fanim (keyframes). */
typedef struct ForgeSceneSkinnedModel {
    /* Pipeline data (owns memory — freed by forge_scene_free_skinned_model) */
    ForgePipelineScene       scene_data;   /* node hierarchy + world transforms */
    ForgePipelineMesh        mesh;         /* vertex/index data + submeshes     */
    ForgePipelineMaterialSet materials;    /* PBR material descriptions         */
    ForgePipelineSkinSet     skins;        /* joint hierarchy + IBMs            */
    ForgePipelineAnimFile    animations;   /* animation clips                   */

    /* GPU resources */
    SDL_GPUBuffer *vertex_buffer;    /* uploaded 72-byte skinned vertices */
    SDL_GPUBuffer *index_buffer;     /* uploaded uint32 indices           */
    SDL_GPUBuffer *joint_buffer;     /* storage buffer: MAX_JOINTS × mat4 */
    SDL_GPUTransferBuffer *joint_transfer_buffer; /* persistent upload buffer */

    /* Per-material textures */
    ForgeSceneModelTextures mat_textures[FORGE_SCENE_MODEL_MAX_MATERIALS];
    uint32_t                mat_texture_count;

    /* Joint matrices (CPU-side, uploaded to joint_buffer each frame) */
    mat4     joint_matrices[FORGE_PIPELINE_MAX_SKIN_JOINTS];
    uint32_t active_joint_count;

    /* Cached indices (set at load time) */
    int      skinned_mesh_node; /* scene node with skin_index==0 and mesh_index>=0 */

    /* Animation state */
    float    anim_time;
    float    anim_speed;       /* default 1.0 */
    int      current_clip;
    bool     looping;          /* default true */

    /* Stats */
    uint32_t draw_calls;
    ForgeSceneVramStats vram;
} ForgeSceneSkinnedModel;

#endif /* FORGE_SCENE_MODEL_SUPPORT */

/* ── Configuration ──────────────────────────────────────────────────────── */

typedef struct ForgeSceneConfig {
    const char *window_title;

    /* Camera starting state */
    vec3  cam_start_pos;
    float cam_start_yaw;        /* radians */
    float cam_start_pitch;      /* radians */
    float move_speed;
    float mouse_sensitivity;

    /* Projection */
    float fov_deg;
    float near_plane;
    float far_plane;

    /* Lighting — light_dir points TOWARD the light source (positive Y = up) */
    vec3  light_dir;
    float light_intensity;
    float ambient;
    float shininess;
    float specular_str;
    float light_color[3];

    /* Shadow map */
    int   shadow_map_size;
    float shadow_ortho_size;
    float shadow_height;
    float shadow_near;
    float shadow_far;

    /* Grid floor */
    float grid_half_size;
    float grid_spacing;
    float grid_line_width;
    float grid_fade_dist;
    float grid_line_color[4];
    float grid_bg_color[4];

    /* Clear color (sky gradient overrides this, but used for depth-clear) */
    float clear_color[4];

    /* UI font (pass NULL to disable UI) */
    const char *font_path;
    float       font_size;
} ForgeSceneConfig;

/* ── Scene state ────────────────────────────────────────────────────────── */

typedef struct ForgeScene {
    SDL_Window    *window;
    SDL_GPUDevice *device;
    bool           window_claimed;    /* true after SDL_ClaimWindowForGPUDevice */

    /* ── Internal pipelines ─────────────────────────────────────────── */
    SDL_GPUGraphicsPipeline *scene_pipeline;   /* Blinn-Phong (pos+normal)   */
    SDL_GPUGraphicsPipeline *scene_pipeline_double; /* same, cull none        */
    SDL_GPUGraphicsPipeline *shadow_pipeline;  /* depth-only shadow pass     */
    SDL_GPUGraphicsPipeline *grid_pipeline;    /* procedural grid floor      */
    SDL_GPUGraphicsPipeline *sky_pipeline;     /* fullscreen sky gradient    */
    SDL_GPUGraphicsPipeline *ui_pipeline;      /* immediate-mode UI overlay  */

    /* ── Render-target textures ─────────────────────────────────────── */
    SDL_GPUTexture *depth_texture;             /* main pass depth (resizable) */
    SDL_GPUTexture *shadow_map;                /* shadow depth texture        */
    Uint32          depth_w, depth_h;          /* current depth dimensions    */

    /* ── Samplers ───────────────────────────────────────────────────── */
    SDL_GPUSampler *shadow_sampler;            /* nearest, clamp-to-edge      */
    SDL_GPUSampler *atlas_sampler;             /* linear, clamp-to-edge (UI)  */

    /* ── Grid geometry ──────────────────────────────────────────────── */
    SDL_GPUBuffer *grid_vb;
    SDL_GPUBuffer *grid_ib;

    /* ── UI resources ───────────────────────────────────────────────── */
    ForgeUiFont          ui_font;
    ForgeUiFontAtlas     ui_atlas;
    ForgeUiContext       ui_ctx;
    ForgeUiWindowContext ui_wctx;              /* draggable window support    */
    SDL_GPUTexture      *ui_atlas_texture;
    SDL_GPUBuffer       *ui_vb;
    SDL_GPUBuffer       *ui_ib;
    Uint32               ui_vb_capacity;       /* current GPU VB size (bytes) */
    Uint32               ui_ib_capacity;       /* current GPU IB size (bytes) */
    bool                 ui_enabled;           /* false if no font was loaded */

    /* ── Camera ─────────────────────────────────────────────────────── */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;
    bool  mouse_captured;
    float frame_scroll_delta;                 /* accumulated wheel input     */

    /* ── Timing ─────────────────────────────────────────────────────── */
    Uint64 last_ticks;
    float  dt;                                 /* delta time in seconds       */

    /* ── Light ──────────────────────────────────────────────────────── */
    vec3 light_dir;                            /* normalized, toward light    */
    mat4 light_vp;                             /* light view-projection       */

    /* ── Per-frame state ────────────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass      *skinned_copy_pass;   /* batched joint upload pass   */
    SDL_GPURenderPass    *pass;                /* current active render pass  */
    SDL_GPUTexture       *swapchain;
    Uint32                sw, sh;              /* swapchain dimensions        */
    mat4                  cam_vp;              /* camera view-projection      */
    float                 aspect;              /* current aspect ratio        */

    /* ── Cached config ──────────────────────────────────────────────── */
    ForgeSceneConfig      config;
    SDL_GPUTextureFormat  swapchain_fmt;
    SDL_GPUTextureFormat  depth_fmt;
    SDL_GPUTextureFormat  shadow_fmt;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif

#ifdef FORGE_SCENE_MODEL_SUPPORT
    /* Model rendering pipelines (lazy-initialized on first load) */
    SDL_GPUGraphicsPipeline *model_pipeline;             /* textured Blinn-Phong, cull back       */
    SDL_GPUGraphicsPipeline *model_pipeline_blend;       /* alpha blend, no depth write           */
    SDL_GPUGraphicsPipeline *model_pipeline_double;      /* cull none for double_sided            */
    SDL_GPUGraphicsPipeline *model_pipeline_blend_double; /* alpha blend + cull none              */
    SDL_GPUGraphicsPipeline *model_shadow_pipeline;      /* depth-only, 48-byte stride           */
    SDL_GPUSampler          *model_tex_sampler;      /* linear wrap for diffuse/emissive */
    SDL_GPUSampler          *model_normal_sampler;   /* linear wrap for normal maps      */
    SDL_GPUSampler          *model_shadow_cmp_sampler; /* comparison sampler for PCF     */
    SDL_GPUTexture          *model_white_texture;    /* 1x1 white (255,255,255,255)     */
    SDL_GPUTexture          *model_flat_normal;      /* 1x1 (128,128,255,255)           */
    SDL_GPUTexture          *model_black_texture;    /* 1x1 black (0,0,0,255)           */
    bool                     model_pipelines_ready;  /* lazy init flag                   */

    /* Skinned model pipelines (lazy-initialized on first skinned model load) */
    SDL_GPUGraphicsPipeline *skinned_pipeline;             /* cull back, depth write           */
    SDL_GPUGraphicsPipeline *skinned_pipeline_blend;       /* alpha blend, no depth write      */
    SDL_GPUGraphicsPipeline *skinned_pipeline_double;      /* cull none, depth write           */
    SDL_GPUGraphicsPipeline *skinned_pipeline_blend_double; /* alpha blend + cull none         */
    SDL_GPUGraphicsPipeline *skinned_shadow_pipeline;      /* depth-only, 72-byte stride       */
    bool                     skinned_pipelines_ready;      /* lazy init flag                   */
#endif /* FORGE_SCENE_MODEL_SUPPORT */
} ForgeScene;

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Return a configuration with sensible defaults.  Override specific fields
 * before passing to forge_scene_init. */
static inline ForgeSceneConfig forge_scene_default_config(const char *title);

/* Initialize the entire rendering stack.  Returns true on success.
 * Validates config fields first (sizes > 0, near < far, etc.) and returns
 * false with a descriptive log message if any value is invalid.
 * On failure, logs errors and returns false — call forge_scene_destroy
 * for cleanup even on failure. */
static bool forge_scene_init(ForgeScene *scene,
                             const ForgeSceneConfig *config,
                             int argc, char **argv);

/* Destroy all GPU resources and free memory.  Zeros the struct after cleanup,
 * so repeated calls are safe (idempotent). */
static void forge_scene_destroy(ForgeScene *scene);

/* Process an SDL event (quit, mouse capture, camera look).
 * Returns SDL_APP_CONTINUE normally, SDL_APP_SUCCESS on quit. */
static SDL_AppResult forge_scene_handle_event(ForgeScene *scene,
                                               SDL_Event *event);

/* Begin a new frame: compute delta time, update camera, acquire command
 * buffer and swapchain.  Returns false if the frame should be skipped
 * (window minimized or swapchain not ready). */
static bool forge_scene_begin_frame(ForgeScene *scene);

/* ── Shadow pass ────────────────────────────────────────────── */

/* Begin the depth-only shadow pass.  Draw shadow-casting geometry between
 * begin and end using forge_scene_draw_shadow_mesh. */
static void forge_scene_begin_shadow_pass(ForgeScene *scene);

/* Draw a mesh into the shadow map.  vb must contain ForgeSceneVertex data
 * (or any vertex with position at offset 0, 24-byte stride). */
static void forge_scene_draw_shadow_mesh(ForgeScene *scene,
                                          SDL_GPUBuffer *vb,
                                          SDL_GPUBuffer *ib,
                                          Uint32 index_count,
                                          mat4 model);

/* End the shadow pass. */
static void forge_scene_end_shadow_pass(ForgeScene *scene);

/* ── Main pass ──────────────────────────────────────────────── */

/* Begin the main color+depth pass.  Draws the sky background first. */
static void forge_scene_begin_main_pass(ForgeScene *scene);

/* Draw a lit mesh with Blinn-Phong shading and shadow.
 * base_color is RGBA [0..1]. */
static void forge_scene_draw_mesh(ForgeScene *scene,
                                   SDL_GPUBuffer *vb,
                                   SDL_GPUBuffer *ib,
                                   Uint32 index_count,
                                   mat4 model,
                                   const float base_color[4]);

/* Draw a lit mesh with both faces visible (no back-face culling).
 * Use for open geometry like uncapped cylinders or discs. */
static void forge_scene_draw_mesh_double_sided(ForgeScene *scene,
                                                SDL_GPUBuffer *vb,
                                                SDL_GPUBuffer *ib,
                                                Uint32 index_count,
                                                mat4 model,
                                                const float base_color[4]);

/* Draw the procedural grid floor. */
static void forge_scene_draw_grid(ForgeScene *scene);

/* End the main color+depth pass. */
static void forge_scene_end_main_pass(ForgeScene *scene);

/* ── UI pass ────────────────────────────────────────────────── */

/* Begin UI for this frame.  After calling this, use forge_scene_window_ui()
 * for draggable windows or forge_scene_ui() for direct context access.
 * mouse_x, mouse_y are pixel coordinates; mouse_down is left-button state. */
static void forge_scene_begin_ui(ForgeScene *scene,
                                  float mouse_x, float mouse_y,
                                  bool mouse_down);

/* Get the UI context for widget calls.  Valid between begin_ui/end_ui.
 * Returns NULL if UI is not enabled (no font_path configured). */
static inline ForgeUiContext *forge_scene_ui(ForgeScene *scene);

/* Get the window context for draggable windows.  Valid between
 * begin_ui/end_ui.  Returns NULL if UI is not enabled. */
static inline ForgeUiWindowContext *forge_scene_window_ui(ForgeScene *scene);

/* Finalize UI, upload draw data, and render in a separate pass. */
static void forge_scene_end_ui(ForgeScene *scene);

/* ── Frame end ──────────────────────────────────────────────── */

/* Submit the command buffer and handle capture.  Returns SDL_APP_CONTINUE
 * or SDL_APP_SUCCESS (when capture is complete). */
static SDL_AppResult forge_scene_end_frame(ForgeScene *scene);

/* ── Accessors ──────────────────────────────────────────────── */

static inline SDL_GPUDevice       *forge_scene_device(const ForgeScene *s);
static inline float                forge_scene_dt(const ForgeScene *s);
static inline mat4                 forge_scene_view_proj(const ForgeScene *s);
static inline mat4                 forge_scene_light_vp_mat(const ForgeScene *s);
static inline vec3                 forge_scene_cam_pos(const ForgeScene *s);
static inline Uint32               forge_scene_width(const ForgeScene *s);
static inline Uint32               forge_scene_height(const ForgeScene *s);
static inline SDL_GPUCommandBuffer*forge_scene_cmd(const ForgeScene *s);
static inline SDL_GPURenderPass   *forge_scene_main_pass(const ForgeScene *s);
static inline SDL_GPUTextureFormat forge_scene_swapchain_format(const ForgeScene *s);
static inline SDL_Window          *forge_scene_window(const ForgeScene *s);

/* ── Model loading (requires FORGE_SCENE_MODEL_SUPPORT) ────── */

#ifdef FORGE_SCENE_MODEL_SUPPORT
/* Load a pipeline-processed model (.fscene + .fmesh + .fmat).
 * base_dir is the directory containing model files — texture paths in
 * .fmat are resolved relative to this. */
static bool forge_scene_load_model(ForgeScene *scene,
                                    ForgeSceneModel *model,
                                    const char *fscene_path,
                                    const char *fmesh_path,
                                    const char *fmat_path,
                                    const char *base_dir);

/* Draw all visible nodes of a loaded model in the main pass.
 * placement is the world-space transform for the entire model. */
static void forge_scene_draw_model(ForgeScene *scene,
                                    ForgeSceneModel *model,
                                    mat4 placement);

/* Draw model into the shadow map (depth-only, no materials). */
static void forge_scene_draw_model_shadows(ForgeScene *scene,
                                            ForgeSceneModel *model,
                                            mat4 placement);

/* Release all GPU and CPU resources for a model. */
static void forge_scene_free_model(ForgeScene *scene,
                                    ForgeSceneModel *model);

/* Load a texture, preferring pipeline-compressed (.ftex) when a .meta.json
 * sidecar with compression info exists.  Falls back to SDL_LoadSurface +
 * GPU mipmaps otherwise.
 *
 * The .ftex format is chosen at build time by forge_texture_tool — `srgb`
 * and `is_normal_map` are used only for the uncompressed fallback path.
 *
 * Updates vram->compressed_bytes and vram->uncompressed_bytes. */
static SDL_GPUTexture *forge_scene_load_pipeline_texture(
    ForgeScene *scene, ForgeSceneVramStats *vram,
    const char *path, bool srgb, bool is_normal_map);

/* ── Skinned model API ───────────────────────────────────── */

/* Load a skinned pipeline model (.fscene + .fmesh + .fmat + .fskin + .fanim).
 * skin_path may be NULL if the model has no skeleton.
 * anim_path may be NULL for a static pose. */
static bool forge_scene_load_skinned_model(
    ForgeScene *scene, ForgeSceneSkinnedModel *model,
    const char *fscene_path, const char *fmesh_path,
    const char *fmat_path,   const char *fskin_path,
    const char *fanim_path,  const char *base_dir);

/* Advance animation time, evaluate keyframes, recompute joint matrices,
 * and upload joint data to the GPU.
 * Must be called after forge_scene_begin_frame() (requires scene->cmd).
 * Call once per frame before drawing. */
static void forge_scene_update_skinned_animation(
    ForgeScene *scene, ForgeSceneSkinnedModel *model, float dt);

/* Draw skinned model in the main pass. */
static void forge_scene_draw_skinned_model(
    ForgeScene *scene, ForgeSceneSkinnedModel *model, mat4 placement);

/* Draw skinned model into the shadow map. */
static void forge_scene_draw_skinned_model_shadows(
    ForgeScene *scene, ForgeSceneSkinnedModel *model, mat4 placement);

/* Release all GPU and CPU resources for a skinned model. */
static void forge_scene_free_skinned_model(
    ForgeScene *scene, ForgeSceneSkinnedModel *model);

#endif /* FORGE_SCENE_MODEL_SUPPORT */

/* ── Utility functions ──────────────────────────────────────── */

/* Create a GPU shader from pre-compiled SPIRV/DXIL bytecode or MSL source. */
static SDL_GPUShader *forge_scene_create_shader(
    ForgeScene *scene,
    SDL_GPUShaderStage stage,
    const unsigned char *spirv_code, unsigned int spirv_size,
    const unsigned char *dxil_code,  unsigned int dxil_size,
    const char *msl_code, unsigned int msl_size,
    int num_samplers, int num_storage_textures,
    int num_storage_buffers, int num_uniform_buffers);

/* Upload CPU data to a GPU buffer via a transfer buffer. */
static SDL_GPUBuffer *forge_scene_upload_buffer(
    ForgeScene *scene,
    SDL_GPUBufferUsageFlags usage,
    const void *data, Uint32 size);

/* Upload an SDL_Surface to the GPU as a texture with mipmaps.
 * srgb=true for diffuse/color textures, false for data textures. */
static SDL_GPUTexture *forge_scene_upload_texture(
    ForgeScene *scene, SDL_Surface *surface, bool srgb);

/* ══════════════════════════════════════════════════════════════════════════
 *                         IMPLEMENTATION
 * ══════════════════════════════════════════════════════════════════════════ */
#ifdef FORGE_SCENE_IMPLEMENTATION

/* ── Compiled shader bytecode ───────────────────────────────────────────── */

#include "scene/shaders/compiled/scene_vert_spirv.h"
#include "scene/shaders/compiled/scene_vert_dxil.h"
#include "scene/shaders/compiled/scene_vert_msl.h"
#include "scene/shaders/compiled/scene_frag_spirv.h"
#include "scene/shaders/compiled/scene_frag_dxil.h"
#include "scene/shaders/compiled/scene_frag_msl.h"

#include "scene/shaders/compiled/grid_vert_spirv.h"
#include "scene/shaders/compiled/grid_vert_dxil.h"
#include "scene/shaders/compiled/grid_vert_msl.h"
#include "scene/shaders/compiled/grid_frag_spirv.h"
#include "scene/shaders/compiled/grid_frag_dxil.h"
#include "scene/shaders/compiled/grid_frag_msl.h"

#include "scene/shaders/compiled/shadow_vert_spirv.h"
#include "scene/shaders/compiled/shadow_vert_dxil.h"
#include "scene/shaders/compiled/shadow_vert_msl.h"
#include "scene/shaders/compiled/shadow_frag_spirv.h"
#include "scene/shaders/compiled/shadow_frag_dxil.h"
#include "scene/shaders/compiled/shadow_frag_msl.h"

#include "scene/shaders/compiled/sky_vert_spirv.h"
#include "scene/shaders/compiled/sky_vert_dxil.h"
#include "scene/shaders/compiled/sky_vert_msl.h"
#include "scene/shaders/compiled/sky_frag_spirv.h"
#include "scene/shaders/compiled/sky_frag_dxil.h"
#include "scene/shaders/compiled/sky_frag_msl.h"

#include "scene/shaders/compiled/ui_vert_spirv.h"
#include "scene/shaders/compiled/ui_vert_dxil.h"
#include "scene/shaders/compiled/ui_vert_msl.h"
#include "scene/shaders/compiled/ui_frag_spirv.h"
#include "scene/shaders/compiled/ui_frag_dxil.h"
#include "scene/shaders/compiled/ui_frag_msl.h"

#ifdef FORGE_SCENE_MODEL_SUPPORT
#include "scene/shaders/compiled/scene_model_vert_spirv.h"
#include "scene/shaders/compiled/scene_model_vert_dxil.h"
#include "scene/shaders/compiled/scene_model_vert_msl.h"
#include "scene/shaders/compiled/scene_model_frag_spirv.h"
#include "scene/shaders/compiled/scene_model_frag_dxil.h"
#include "scene/shaders/compiled/scene_model_frag_msl.h"
#include "scene/shaders/compiled/scene_skinned_vert_spirv.h"
#include "scene/shaders/compiled/scene_skinned_vert_dxil.h"
#include "scene/shaders/compiled/scene_skinned_vert_msl.h"
#include "scene/shaders/compiled/scene_skinned_shadow_vert_spirv.h"
#include "scene/shaders/compiled/scene_skinned_shadow_vert_dxil.h"
#include "scene/shaders/compiled/scene_skinned_shadow_vert_msl.h"
#endif

/* ── Inline accessors ───────────────────────────────────────────────────── */

static inline ForgeSceneConfig forge_scene_default_config(const char *title)
{
    ForgeSceneConfig c;
    SDL_memset(&c, 0, sizeof(c));
    c.window_title     = title ? title : "Forge GPU";
    c.cam_start_pos    = vec3_create(0.0f, 4.0f, 12.0f);
    c.cam_start_yaw    = 0.0f;
    c.cam_start_pitch  = -0.2f;
    c.move_speed       = FORGE_SCENE_MOVE_SPEED;
    c.mouse_sensitivity = FORGE_SCENE_MOUSE_SENSITIVITY;
    c.fov_deg          = FORGE_SCENE_FOV_DEG;
    c.near_plane       = FORGE_SCENE_NEAR_PLANE;
    c.far_plane        = FORGE_SCENE_FAR_PLANE;

    /* Light direction points TOWARD the light (postmortem issue #4). */
    c.light_dir        = vec3_normalize(vec3_create(0.4f, 0.8f, 0.6f));
    c.light_intensity  = FORGE_SCENE_LIGHT_INTENSITY;
    c.ambient          = FORGE_SCENE_AMBIENT;
    c.shininess        = FORGE_SCENE_SHININESS;
    c.specular_str     = FORGE_SCENE_SPECULAR_STR;
    c.light_color[0]   = 1.0f;
    c.light_color[1]   = 0.95f;
    c.light_color[2]   = 0.9f;

    c.shadow_map_size  = FORGE_SCENE_SHADOW_MAP_SIZE;
    c.shadow_ortho_size = FORGE_SCENE_SHADOW_ORTHO_SIZE;
    c.shadow_height    = FORGE_SCENE_SHADOW_HEIGHT;
    c.shadow_near      = FORGE_SCENE_SHADOW_NEAR;
    c.shadow_far       = FORGE_SCENE_SHADOW_FAR;

    c.grid_half_size   = FORGE_SCENE_GRID_HALF_SIZE;
    c.grid_spacing     = FORGE_SCENE_GRID_SPACING;
    c.grid_line_width  = FORGE_SCENE_GRID_LINE_WIDTH;
    c.grid_fade_dist   = FORGE_SCENE_GRID_FADE_DIST;
    c.grid_line_color[0] = 0.4f;
    c.grid_line_color[1] = 0.4f;
    c.grid_line_color[2] = 0.5f;
    c.grid_line_color[3] = 1.0f;
    c.grid_bg_color[0] = 0.08f;
    c.grid_bg_color[1] = 0.08f;
    c.grid_bg_color[2] = 0.1f;
    c.grid_bg_color[3] = 1.0f;

    /* Clear color — visible contrast with grid (postmortem issue #6). */
    c.clear_color[0]   = 0.15f;
    c.clear_color[1]   = 0.15f;
    c.clear_color[2]   = 0.20f;
    c.clear_color[3]   = 1.0f;

    c.font_path = NULL;  /* UI disabled by default */
    c.font_size = FORGE_SCENE_ATLAS_PIXEL_HEIGHT;

    return c;
}

static inline ForgeUiContext *forge_scene_ui(ForgeScene *scene)
{
    if (!scene->ui_enabled) return NULL;
    return &scene->ui_ctx;
}

static inline ForgeUiWindowContext *forge_scene_window_ui(ForgeScene *scene)
{
    if (!scene->ui_enabled) return NULL;
    return &scene->ui_wctx;
}

static inline SDL_GPUDevice *forge_scene_device(const ForgeScene *s)
{
    return s->device;
}

static inline float forge_scene_dt(const ForgeScene *s)
{
    return s->dt;
}

static inline mat4 forge_scene_view_proj(const ForgeScene *s)
{
    return s->cam_vp;
}

static inline mat4 forge_scene_light_vp_mat(const ForgeScene *s)
{
    return s->light_vp;
}

static inline vec3 forge_scene_cam_pos(const ForgeScene *s)
{
    return s->cam_position;
}

static inline Uint32 forge_scene_width(const ForgeScene *s)
{
    return s->sw;
}

static inline Uint32 forge_scene_height(const ForgeScene *s)
{
    return s->sh;
}

static inline SDL_GPUCommandBuffer *forge_scene_cmd(const ForgeScene *s)
{
    return s->cmd;
}

static inline SDL_GPURenderPass *forge_scene_main_pass(const ForgeScene *s)
{
    return s->pass;
}

static inline SDL_GPUTextureFormat forge_scene_swapchain_format(
    const ForgeScene *s)
{
    return s->swapchain_fmt;
}

static inline SDL_Window *forge_scene_window(const ForgeScene *s)
{
    return s->window;
}

/* ── Shader helper ──────────────────────────────────────────────────────── */

static SDL_GPUShader *forge_scene_create_shader(
    ForgeScene *scene,
    SDL_GPUShaderStage stage,
    const unsigned char *spirv_code, unsigned int spirv_size,
    const unsigned char *dxil_code,  unsigned int dxil_size,
    const char *msl_code, unsigned int msl_size,
    int num_samplers, int num_storage_textures,
    int num_storage_buffers, int num_uniform_buffers)
{
    if (!scene || !scene->device) {
        SDL_Log("forge_scene: create_shader called with invalid scene/device");
        return NULL;
    }

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(scene->device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage                = stage;
    info.num_samplers         = num_samplers;
    info.num_storage_textures = num_storage_textures;
    info.num_storage_buffers  = num_storage_buffers;
    info.num_uniform_buffers  = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format     = SDL_GPU_SHADERFORMAT_SPIRV;
        info.entrypoint = "main";
        info.code       = spirv_code;
        info.code_size  = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format     = SDL_GPU_SHADERFORMAT_DXIL;
        info.entrypoint = "main";
        info.code       = dxil_code;
        info.code_size  = dxil_size;
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL) && msl_code && msl_size > 0) {
        info.format     = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";  /* spirv-cross renames main → main0 in MSL */
        info.code       = (const unsigned char *)msl_code;
        info.code_size  = msl_size;
    } else {
        SDL_Log("forge_scene: no supported shader format (need SPIRV, DXIL, or MSL)");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(scene->device, &info);
    if (!shader) {
        SDL_Log("forge_scene: SDL_CreateGPUShader (%s) failed: %s",
                stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vertex" : "fragment",
                SDL_GetError());
    }
    return shader;
}

/* ── Buffer upload helper ───────────────────────────────────────────────── */

static SDL_GPUBuffer *forge_scene_upload_buffer(
    ForgeScene *scene,
    SDL_GPUBufferUsageFlags usage,
    const void *data, Uint32 size)
{
    if (!scene || !scene->device) {
        SDL_Log("forge_scene: upload_buffer called with invalid scene/device");
        return NULL;
    }
    if (size == 0) {
        SDL_Log("forge_scene: upload_buffer called with size 0");
        return NULL;
    }
    if (!data) {
        SDL_Log("forge_scene: upload_buffer called with NULL data (size=%u)",
                (unsigned)size);
        return NULL;
    }

    SDL_GPUBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = usage;
    buf_info.size  = size;

    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(scene->device, &buf_info);
    if (!buffer) {
        SDL_Log("forge_scene: SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = size;

    SDL_GPUTransferBuffer *xfer =
        SDL_CreateGPUTransferBuffer(scene->device, &xfer_info);
    if (!xfer) {
        SDL_Log("forge_scene: SDL_CreateGPUTransferBuffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUBuffer(scene->device, buffer);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(scene->device, xfer, false);
    if (!mapped) {
        SDL_Log("forge_scene: SDL_MapGPUTransferBuffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUBuffer(scene->device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(scene->device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(scene->device);
    if (!cmd) {
        SDL_Log("forge_scene: SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUBuffer(scene->device, buffer);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("forge_scene: SDL_BeginGPUCopyPass failed: %s",
                SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUBuffer(scene->device, buffer);
        return NULL;
    }

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size   = size;

    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUBuffer(scene->device, buffer);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
    return buffer;
}

/* ── Texture upload helper ──────────────────────────────────────────────── */

static SDL_GPUTexture *forge_scene_upload_texture(
    ForgeScene *scene, SDL_Surface *surface, bool srgb)
{
    if (!scene || !scene->device) {
        SDL_Log("forge_scene: upload_texture called with invalid scene/device");
        return NULL;
    }
    if (!surface) {
        SDL_Log("forge_scene: upload_texture called with NULL surface");
        return NULL;
    }

    SDL_Surface *converted =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    if (!converted) {
        SDL_Log("forge_scene: SDL_ConvertSurface failed: %s", SDL_GetError());
        return NULL;
    }

    int tex_w = converted->w;
    int tex_h = converted->h;
    int num_levels = (int)forge_log2f(
        (float)(tex_w > tex_h ? tex_w : tex_h)) + 1;

    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = srgb
        ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
        : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width                = (Uint32)tex_w;
    tex_info.height               = (Uint32)tex_h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = num_levels;

    SDL_GPUTexture *texture =
        SDL_CreateGPUTexture(scene->device, &tex_info);
    if (!texture) {
        SDL_Log("forge_scene: SDL_CreateGPUTexture failed: %s",
                SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    Uint32 total_bytes = (Uint32)(tex_w * tex_h * 4);

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *xfer =
        SDL_CreateGPUTransferBuffer(scene->device, &xfer_info);
    if (!xfer) {
        SDL_Log("forge_scene: texture transfer buffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTexture(scene->device, texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(scene->device, xfer, false);
    if (!mapped) {
        SDL_Log("forge_scene: texture map failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        SDL_DestroySurface(converted);
        return NULL;
    }

    Uint32 row_bytes = (Uint32)(tex_w * 4);
    const Uint8 *row_src = (const Uint8 *)converted->pixels;
    Uint8 *row_dst = (Uint8 *)mapped;
    for (Uint32 row = 0; row < (Uint32)tex_h; row++) {
        SDL_memcpy(row_dst + row * row_bytes,
                   row_src + row * converted->pitch,
                   row_bytes);
    }
    SDL_UnmapGPUTransferBuffer(scene->device, xfer);
    SDL_DestroySurface(converted);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(scene->device);
    if (!cmd) {
        SDL_Log("forge_scene: cmd for texture upload failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("forge_scene: copy pass for texture failed: %s",
                SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = xfer;
    tex_src.pixels_per_row  = (Uint32)tex_w;
    tex_src.rows_per_layer  = (Uint32)tex_h;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = texture;
    tex_dst.w       = (Uint32)tex_w;
    tex_dst.h       = (Uint32)tex_h;
    tex_dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_GenerateMipmapsForGPUTexture(cmd, texture);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("forge_scene: texture upload submit failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
    return texture;
}

/* ── Internal: create depth texture ─────────────────────────────────────── */

static SDL_GPUTexture *forge_scene__create_depth(SDL_GPUDevice *device,
                                                  Uint32 w, Uint32 h,
                                                  SDL_GPUTextureFormat fmt,
                                                  SDL_GPUTextureUsageFlags usage)
{
    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type                 = SDL_GPU_TEXTURETYPE_2D;
    ti.format               = fmt;
    ti.usage                = usage;
    ti.width                = w;
    ti.height               = h;
    ti.layer_count_or_depth = 1;
    ti.num_levels           = 1;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &ti);
    if (!tex) {
        SDL_Log("forge_scene: depth texture (%ux%u) failed: %s",
                w, h, SDL_GetError());
    }
    return tex;
}

/* ── Internal: upload atlas texture (R8_UNORM, no mipmaps) ──────────────── */

static SDL_GPUTexture *forge_scene__upload_atlas(SDL_GPUDevice *device,
                                                  const Uint8 *pixels,
                                                  Uint32 w, Uint32 h)
{
    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type                 = SDL_GPU_TEXTURETYPE_2D;
    ti.format               = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width                = w;
    ti.height               = h;
    ti.layer_count_or_depth = 1;
    ti.num_levels           = 1;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &ti);
    if (!tex) {
        SDL_Log("forge_scene: atlas texture failed: %s", SDL_GetError());
        return NULL;
    }

    Uint32 total = w * h;
    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total;

    SDL_GPUTransferBuffer *xfer =
        SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("forge_scene: atlas transfer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("forge_scene: atlas map failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(mapped, pixels, total);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("forge_scene: atlas cmd failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("forge_scene: atlas copy failed: %s", SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;
    src.pixels_per_row  = w;
    src.rows_per_layer  = h;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w       = w;
    dst.h       = h;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("forge_scene: atlas submit failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Config validation (internal) ─────────────────────────────────────── */

static bool forge_scene__validate_config(const ForgeSceneConfig *config)
{
    /* Reject NaN / Infinity in any floating-point config field.
     * NaN comparisons are always false, so range checks below would
     * silently pass — catch non-finite values up front. */
    if (!isfinite(config->fov_deg) || !isfinite(config->near_plane) ||
        !isfinite(config->far_plane) || !isfinite(config->shadow_height) ||
        !isfinite(config->shadow_ortho_size) || !isfinite(config->shadow_near) ||
        !isfinite(config->shadow_far) || !isfinite(config->move_speed) ||
        !isfinite(config->mouse_sensitivity) || !isfinite(config->light_intensity) ||
        !isfinite(config->ambient) || !isfinite(config->shininess) ||
        !isfinite(config->specular_str) || !isfinite(config->grid_half_size) ||
        !isfinite(config->grid_spacing) || !isfinite(config->grid_line_width) ||
        !isfinite(config->grid_fade_dist) || !isfinite(config->font_size) ||
        !isfinite(config->cam_start_pos.x) || !isfinite(config->cam_start_pos.y) ||
        !isfinite(config->cam_start_pos.z) || !isfinite(config->cam_start_yaw) ||
        !isfinite(config->cam_start_pitch) || !isfinite(config->light_dir.x) ||
        !isfinite(config->light_dir.y) || !isfinite(config->light_dir.z)) {
        SDL_Log("forge_scene: config contains NaN or Inf values");
        return false;
    }

    if (config->shadow_height <= 0.0f) {
        SDL_Log("forge_scene: shadow_height must be > 0 (got %.2f)",
                config->shadow_height);
        return false;
    }
    if (config->shadow_map_size <= 0) {
        SDL_Log("forge_scene: shadow_map_size must be > 0 (got %d)",
                config->shadow_map_size);
        return false;
    }
    if (config->fov_deg <= 0.0f || config->fov_deg >= 180.0f) {
        SDL_Log("forge_scene: fov_deg must be > 0 and < 180 (got %.2f)",
                config->fov_deg);
        return false;
    }
    if (config->near_plane <= 0.0f) {
        SDL_Log("forge_scene: near_plane must be > 0 (got %.4f)",
                config->near_plane);
        return false;
    }
    if (config->far_plane <= config->near_plane) {
        SDL_Log("forge_scene: far_plane must be > near_plane (got far=%.2f, near=%.2f)",
                config->far_plane, config->near_plane);
        return false;
    }
    if (config->shadow_near < 0.0f) {
        SDL_Log("forge_scene: shadow_near must be >= 0 (got %.4f)",
                config->shadow_near);
        return false;
    }
    if (config->shadow_far <= config->shadow_near) {
        SDL_Log("forge_scene: shadow_far must be > shadow_near (got far=%.2f, near=%.2f)",
                config->shadow_far, config->shadow_near);
        return false;
    }
    if (config->shadow_ortho_size <= 0.0f) {
        SDL_Log("forge_scene: shadow_ortho_size must be > 0 (got %.2f)",
                config->shadow_ortho_size);
        return false;
    }
    return true;
}

/* Internal: create a scene (Blinn-Phong + shadow) pipeline with the given
 * cull mode.  All other state is identical between the normal and double-
 * sided variants, so this helper prevents drift if the layout changes. */
static SDL_GPUGraphicsPipeline *forge_scene__create_scene_pipeline(
    ForgeScene *scene,
    SDL_GPUShader *vs, SDL_GPUShader *fs,
    SDL_GPUVertexInputState vertex_input,
    SDL_GPUCullMode cull_mode)
{
    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = scene->swapchain_fmt;

    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_zero(pi);
    pi.vertex_shader   = vs;
    pi.fragment_shader = fs;
    pi.vertex_input_state = vertex_input;
    pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    pi.rasterizer_state.cull_mode  = cull_mode;
    pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
    pi.depth_stencil_state.enable_depth_test  = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.depth_stencil_state.enable_stencil_test = false;
    pi.target_info.color_target_descriptions  = &ctd;
    pi.target_info.num_color_targets          = 1;
    pi.target_info.depth_stencil_format       = scene->depth_fmt;
    pi.target_info.has_depth_stencil_target   = true;
    return SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  forge_scene_init — Initialize the entire rendering stack
 * ══════════════════════════════════════════════════════════════════════════ */

static bool forge_scene_init(ForgeScene *scene,
                             const ForgeSceneConfig *config,
                             int argc, char **argv)
{
    if (!scene || !config) {
        SDL_Log("forge_scene: scene and config must be non-NULL");
        return false;
    }
    SDL_memset(scene, 0, sizeof(*scene));
    if (!forge_scene__validate_config(config)) return false;
    scene->config = *config;

    /* ── 1. SDL + window + device ────────────────────────────────── */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("forge_scene: SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    scene->window = SDL_CreateWindow(
        config->window_title,
        FORGE_SCENE_WINDOW_WIDTH, FORGE_SCENE_WINDOW_HEIGHT,
        SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!scene->window) {
        SDL_Log("forge_scene: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    scene->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV |
        SDL_GPU_SHADERFORMAT_DXIL |
        SDL_GPU_SHADERFORMAT_MSL,
        true, NULL);
    if (!scene->device) {
        SDL_Log("forge_scene: SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(scene->device, scene->window)) {
        SDL_Log("forge_scene: SDL_ClaimWindowForGPUDevice failed: %s",
                SDL_GetError());
        return false;
    }
    scene->window_claimed = true;

    /* SDR linear for correct gamma */
    if (SDL_WindowSupportsGPUSwapchainComposition(scene->device, scene->window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(scene->device, scene->window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("forge_scene: SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            return false;
        }
    }

    scene->swapchain_fmt =
        SDL_GetGPUSwapchainTextureFormat(scene->device, scene->window);

    /* ── 2. Depth format negotiation ─────────────────────────────── */

    /* Shadow depth needs SAMPLER usage for PCF sampling in fragment shaders */
    if (SDL_GPUTextureSupportsFormat(scene->device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        scene->shadow_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    } else if (SDL_GPUTextureSupportsFormat(scene->device,
            SDL_GPU_TEXTUREFORMAT_D16_UNORM, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        scene->shadow_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    } else {
        SDL_Log("forge_scene: no shadow depth format supports SAMPLER");
        return false;
    }

    /* Scene depth: used only as depth-stencil target, not sampled */
    if (SDL_GPUTextureSupportsFormat(scene->device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        scene->depth_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    } else if (SDL_GPUTextureSupportsFormat(scene->device,
            SDL_GPU_TEXTUREFORMAT_D16_UNORM, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        scene->depth_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    } else {
        SDL_Log("forge_scene: no supported depth format for scene target");
        return false;
    }

    /* ── 3. Create shaders ───────────────────────────────────────── */

    SDL_GPUShader *scene_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil,  sizeof(scene_vert_dxil),
        scene_vert_msl, scene_vert_msl_size,
        0, 0, 0, 1);

    SDL_GPUShader *scene_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_frag_spirv, sizeof(scene_frag_spirv),
        scene_frag_dxil,  sizeof(scene_frag_dxil),
        scene_frag_msl, scene_frag_msl_size,
        1, 0, 0, 1);

    SDL_GPUShader *shadow_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil,  sizeof(shadow_vert_dxil),
        shadow_vert_msl, shadow_vert_msl_size,
        0, 0, 0, 1);

    SDL_GPUShader *shadow_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl, shadow_frag_msl_size,
        0, 0, 0, 0);

    SDL_GPUShader *grid_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, sizeof(grid_vert_spirv),
        grid_vert_dxil,  sizeof(grid_vert_dxil),
        grid_vert_msl, grid_vert_msl_size,
        0, 0, 0, 1);

    SDL_GPUShader *grid_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, sizeof(grid_frag_spirv),
        grid_frag_dxil,  sizeof(grid_frag_dxil),
        grid_frag_msl, grid_frag_msl_size,
        1, 0, 0, 1);

    SDL_GPUShader *sky_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        sky_vert_spirv, sizeof(sky_vert_spirv),
        sky_vert_dxil,  sizeof(sky_vert_dxil),
        sky_vert_msl, sky_vert_msl_size,
        0, 0, 0, 0);

    SDL_GPUShader *sky_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        sky_frag_spirv, sizeof(sky_frag_spirv),
        sky_frag_dxil,  sizeof(sky_frag_dxil),
        sky_frag_msl, sky_frag_msl_size,
        0, 0, 0, 0);

    if (!scene_vs || !scene_fs || !shadow_vs || !shadow_fs ||
        !grid_vs || !grid_fs || !sky_vs || !sky_fs) {
        SDL_Log("forge_scene: one or more shaders failed");
        if (scene_vs)  SDL_ReleaseGPUShader(scene->device, scene_vs);
        if (scene_fs)  SDL_ReleaseGPUShader(scene->device, scene_fs);
        if (shadow_vs) SDL_ReleaseGPUShader(scene->device, shadow_vs);
        if (shadow_fs) SDL_ReleaseGPUShader(scene->device, shadow_fs);
        if (grid_vs)   SDL_ReleaseGPUShader(scene->device, grid_vs);
        if (grid_fs)   SDL_ReleaseGPUShader(scene->device, grid_fs);
        if (sky_vs)    SDL_ReleaseGPUShader(scene->device, sky_vs);
        if (sky_fs)    SDL_ReleaseGPUShader(scene->device, sky_fs);
        return false;
    }

    /* ── 4. Vertex input: position + normal (ForgeSceneVertex) ──── */

    SDL_GPUVertexBufferDescription vb_full;
    SDL_zero(vb_full);
    vb_full.slot       = 0;
    vb_full.pitch      = sizeof(ForgeSceneVertex);
    vb_full.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute full_attrs[2];
    SDL_zero(full_attrs);
    full_attrs[0].location    = 0;
    full_attrs[0].buffer_slot = 0;
    full_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    full_attrs[0].offset      = offsetof(ForgeSceneVertex, position);
    full_attrs[1].location    = 1;
    full_attrs[1].buffer_slot = 0;
    full_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    full_attrs[1].offset      = offsetof(ForgeSceneVertex, normal);

    SDL_GPUVertexInputState full_input;
    SDL_zero(full_input);
    full_input.vertex_buffer_descriptions = &vb_full;
    full_input.num_vertex_buffers         = 1;
    full_input.vertex_attributes          = full_attrs;
    full_input.num_vertex_attributes      = 2;

    /* Position-only input: same stride, reads only position. */
    SDL_GPUVertexAttribute pos_attr;
    SDL_zero(pos_attr);
    pos_attr.location    = 0;
    pos_attr.buffer_slot = 0;
    pos_attr.format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    pos_attr.offset      = offsetof(ForgeSceneVertex, position);

    SDL_GPUVertexInputState pos_input;
    SDL_zero(pos_input);
    pos_input.vertex_buffer_descriptions = &vb_full;
    pos_input.num_vertex_buffers         = 1;
    pos_input.vertex_attributes          = &pos_attr;
    pos_input.num_vertex_attributes      = 1;

    /* Grid input: 12-byte stride, position only. */
    SDL_GPUVertexBufferDescription vb_grid;
    SDL_zero(vb_grid);
    vb_grid.slot       = 0;
    vb_grid.pitch      = sizeof(ForgeSceneGridVertex);
    vb_grid.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute grid_attr;
    SDL_zero(grid_attr);
    grid_attr.location    = 0;
    grid_attr.buffer_slot = 0;
    grid_attr.format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    grid_attr.offset      = 0;

    SDL_GPUVertexInputState grid_input;
    SDL_zero(grid_input);
    grid_input.vertex_buffer_descriptions = &vb_grid;
    grid_input.num_vertex_buffers         = 1;
    grid_input.vertex_attributes          = &grid_attr;
    grid_input.num_vertex_attributes      = 1;

    /* Sky: no vertex input (fullscreen triangle from SV_VertexID). */
    SDL_GPUVertexInputState empty_input;
    SDL_zero(empty_input);

    /* ── 5. Create pipelines ─────────────────────────────────────── */

    /* Shadow pipeline: depth-only, no color target.
     * CULL_NONE ensures both faces contribute depth for tighter shadows.
     * Slope-scaled depth bias prevents shadow acne on curved surfaces. */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = shadow_vs;
        pi.fragment_shader = shadow_fs;
        pi.vertex_input_state = pos_input;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor =
            FORGE_SCENE_SHADOW_BIAS_CONST;
        pi.rasterizer_state.depth_bias_slope_factor =
            FORGE_SCENE_SHADOW_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.num_color_targets         = 0;
        pi.target_info.depth_stencil_format      = scene->shadow_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->shadow_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Scene pipelines: Blinn-Phong with shadow map.
     * Both variants share identical state except cull mode. */
    scene->scene_pipeline = forge_scene__create_scene_pipeline(
        scene, scene_vs, scene_fs, full_input, SDL_GPU_CULLMODE_BACK);
    scene->scene_pipeline_double = forge_scene__create_scene_pipeline(
        scene, scene_vs, scene_fs, full_input, SDL_GPU_CULLMODE_NONE);

    /* Grid pipeline: procedural grid with alpha blending for distance fade.
     * LESS_OR_EQUAL prevents z-fighting at Y=0. */
    {
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = scene->swapchain_fmt;
        ctd.blend_state.enable_blend = true;
        ctd.blend_state.src_color_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ctd.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        ctd.blend_state.src_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ctd.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = grid_vs;
        pi.fragment_shader = grid_fs;
        pi.vertex_input_state = grid_input;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->grid_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Sky pipeline: fullscreen triangle, no depth test (draws at z=0.9999),
     * renders BEHIND everything because depth write is off and the sky
     * vertex shader outputs z = 0.9999 (near far plane). */
    {
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = scene->swapchain_fmt;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = sky_vs;
        pi.fragment_shader = sky_fs;
        pi.vertex_input_state = empty_input;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->sky_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Release shaders (pipelines keep internal copies) */
    SDL_ReleaseGPUShader(scene->device, scene_vs);
    SDL_ReleaseGPUShader(scene->device, scene_fs);
    SDL_ReleaseGPUShader(scene->device, shadow_vs);
    SDL_ReleaseGPUShader(scene->device, shadow_fs);
    SDL_ReleaseGPUShader(scene->device, grid_vs);
    SDL_ReleaseGPUShader(scene->device, grid_fs);
    SDL_ReleaseGPUShader(scene->device, sky_vs);
    SDL_ReleaseGPUShader(scene->device, sky_fs);

    if (!scene->shadow_pipeline || !scene->scene_pipeline ||
        !scene->scene_pipeline_double ||
        !scene->grid_pipeline || !scene->sky_pipeline) {
        SDL_Log("forge_scene: one or more pipelines failed: %s",
                SDL_GetError());
        return false;
    }

    /* ── 6. Depth and shadow textures ────────────────────────────── */

    {
        int init_w = 0, init_h = 0;
        if (!SDL_GetWindowSizeInPixels(scene->window, &init_w, &init_h)) {
            SDL_Log("forge_scene: SDL_GetWindowSizeInPixels failed: %s",
                    SDL_GetError());
        }
        if (init_w <= 0 || init_h <= 0) {
            init_w = FORGE_SCENE_WINDOW_WIDTH;
            init_h = FORGE_SCENE_WINDOW_HEIGHT;
        }
        scene->depth_w = (Uint32)init_w;
        scene->depth_h = (Uint32)init_h;

        scene->depth_texture = forge_scene__create_depth(scene->device,
            scene->depth_w, scene->depth_h, scene->depth_fmt,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
        if (!scene->depth_texture) return false;

        scene->shadow_map = forge_scene__create_depth(scene->device,
            (Uint32)config->shadow_map_size, (Uint32)config->shadow_map_size,
            scene->shadow_fmt,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER);
        if (!scene->shadow_map) return false;
    }

    /* ── 7. Samplers ─────────────────────────────────────────────── */

    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter      = SDL_GPU_FILTER_NEAREST;
        si.mag_filter      = SDL_GPU_FILTER_NEAREST;
        si.mipmap_mode     = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        scene->shadow_sampler = SDL_CreateGPUSampler(scene->device, &si);
        if (!scene->shadow_sampler) {
            SDL_Log("forge_scene: shadow sampler failed: %s", SDL_GetError());
            return false;
        }
    }

    /* NOTE: atlas_sampler is created inside the UI init block (section 11)
     * so it is only allocated when UI is enabled. This keeps allocation
     * and cleanup symmetric (both gated on ui_enabled). */

    /* ── 8. Grid geometry ────────────────────────────────────────── */

    {
        float hs = config->grid_half_size;
        ForgeSceneGridVertex grid_verts[4] = {
            {{ -hs, 0.0f, -hs }},
            {{  hs, 0.0f, -hs }},
            {{  hs, 0.0f,  hs }},
            {{ -hs, 0.0f,  hs }}
        };
        Uint16 grid_idx[6] = { 0, 1, 2, 0, 2, 3 };

        scene->grid_vb = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_VERTEX,
            grid_verts, sizeof(grid_verts));
        scene->grid_ib = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_INDEX,
            grid_idx, sizeof(grid_idx));
        if (!scene->grid_vb || !scene->grid_ib) return false;
    }

    /* ── 9. Light direction and VP ───────────────────────────────── */

    {
        float dir_len = vec3_length(config->light_dir);
        if (!isfinite(dir_len) || dir_len < FORGE_SCENE_LIGHT_DIR_EPSILON) {
            SDL_Log("forge_scene: light_dir is zero or non-finite, using default");
            scene->light_dir = vec3_normalize(
                vec3_create(0.4f, 0.8f, 0.6f));
        } else {
            scene->light_dir = vec3_scale(config->light_dir, 1.0f / dir_len);
        }
    }

    /* Orthographic projection from the light's perspective */
    /* Place the light camera along light_dir (toward the light) so the
     * shadow map captures the scene from the same direction as the shader
     * lighting.  light_dir points toward the light source. */
    vec3 light_pos = vec3_scale(scene->light_dir, config->shadow_height);

    /* Choose an up vector that isn't parallel to light_dir.  If the light
     * points straight up or down, the default (0,1,0) up would collapse
     * the look-at basis — switch to (1,0,0) instead. */
    vec3 light_up = vec3_create(0.0f, 1.0f, 0.0f);
    if (fabsf(vec3_dot(scene->light_dir, light_up)) > FORGE_SCENE_LIGHT_UP_PARALLEL_COS) {
        light_up = vec3_create(1.0f, 0.0f, 0.0f);
    }
    mat4 light_view = mat4_look_at(
        light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        light_up);
    mat4 light_proj = mat4_orthographic(
        -config->shadow_ortho_size, config->shadow_ortho_size,
        -config->shadow_ortho_size, config->shadow_ortho_size,
        config->shadow_near, config->shadow_far);
    scene->light_vp = mat4_multiply(light_proj, light_view);

    /* ── 10. Camera ──────────────────────────────────────────────── */

    scene->cam_position   = config->cam_start_pos;
    scene->cam_yaw        = config->cam_start_yaw;
    scene->cam_pitch      = config->cam_start_pitch;
    scene->mouse_captured = false;
    scene->last_ticks     = SDL_GetPerformanceCounter();
    scene->dt             = 0.0f;

    /* ── 11. UI initialization (optional) ────────────────────────── */

    scene->ui_enabled = false;
    if (config->font_path) {
        if (!forge_ui_ttf_load(config->font_path, &scene->ui_font)) {
            SDL_Log("forge_scene: forge_ui_ttf_load failed for '%s'",
                    config->font_path);
            return false;
        }

        Uint32 codepoints[FORGE_SCENE_ASCII_COUNT];
        for (int i = 0; i < FORGE_SCENE_ASCII_COUNT; i++) {
            codepoints[i] = (Uint32)(FORGE_SCENE_ASCII_START + i);
        }

        float font_size = config->font_size > 0.0f
            ? config->font_size : FORGE_SCENE_ATLAS_PIXEL_HEIGHT;

        if (!forge_ui_atlas_build(&scene->ui_font, font_size,
                                  codepoints, FORGE_SCENE_ASCII_COUNT,
                                  FORGE_SCENE_ATLAS_PADDING,
                                  &scene->ui_atlas)) {
            SDL_Log("forge_scene: forge_ui_atlas_build failed");
            return false;
        }

        /* Upload atlas texture to GPU */
        scene->ui_atlas_texture = forge_scene__upload_atlas(
            scene->device,
            scene->ui_atlas.pixels,
            (Uint32)scene->ui_atlas.width,
            (Uint32)scene->ui_atlas.height);
        if (!scene->ui_atlas_texture) return false;

        /* Create atlas sampler (linear filtering for smooth font rendering).
         * Created here inside the UI block so allocation and cleanup are
         * both gated on ui_enabled — no leak if font_path is NULL. */
        {
            SDL_GPUSamplerCreateInfo si;
            SDL_zero(si);
            si.min_filter      = SDL_GPU_FILTER_LINEAR;
            si.mag_filter      = SDL_GPU_FILTER_LINEAR;
            si.mipmap_mode     = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            si.address_mode_u  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            si.address_mode_v  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            si.address_mode_w  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            scene->atlas_sampler = SDL_CreateGPUSampler(scene->device, &si);
            if (!scene->atlas_sampler) {
                SDL_Log("forge_scene: atlas sampler failed: %s",
                        SDL_GetError());
                return false;
            }
        }

        /* Initialize UI context */
        if (!forge_ui_ctx_init(&scene->ui_ctx, &scene->ui_atlas)) {
            SDL_Log("forge_scene: forge_ui_ctx_init failed");
            return false;
        }

        /* Initialize window context for draggable windows */
        if (!forge_ui_wctx_init(&scene->ui_wctx, &scene->ui_ctx)) {
            SDL_Log("forge_scene: forge_ui_wctx_init failed");
            return false;
        }

        /* Create UI GPU buffers (dynamic, resizable) */
        {
            SDL_GPUBufferCreateInfo bi;
            SDL_zero(bi);
            bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bi.size  = FORGE_SCENE_UI_VB_CAPACITY *
                       (Uint32)sizeof(ForgeUiVertex);
            scene->ui_vb = SDL_CreateGPUBuffer(scene->device, &bi);
            scene->ui_vb_capacity = bi.size;

            bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
            bi.size  = FORGE_SCENE_UI_IB_CAPACITY * (Uint32)sizeof(Uint32);
            scene->ui_ib = SDL_CreateGPUBuffer(scene->device, &bi);
            scene->ui_ib_capacity = bi.size;

            if (!scene->ui_vb || !scene->ui_ib) {
                SDL_Log("forge_scene: UI buffer creation failed: %s",
                        SDL_GetError());
                return false;
            }
        }

        /* Create UI pipeline */
        {
            SDL_GPUShader *ui_vs = forge_scene_create_shader(scene,
                SDL_GPU_SHADERSTAGE_VERTEX,
                ui_vert_spirv, sizeof(ui_vert_spirv),
                ui_vert_dxil,  sizeof(ui_vert_dxil),
                ui_vert_msl, ui_vert_msl_size,
                0, 0, 0, 1);

            SDL_GPUShader *ui_fs = forge_scene_create_shader(scene,
                SDL_GPU_SHADERSTAGE_FRAGMENT,
                ui_frag_spirv, sizeof(ui_frag_spirv),
                ui_frag_dxil,  sizeof(ui_frag_dxil),
                ui_frag_msl, ui_frag_msl_size,
                1, 0, 0, 0);

            if (!ui_vs || !ui_fs) {
                if (ui_vs) SDL_ReleaseGPUShader(scene->device, ui_vs);
                if (ui_fs) SDL_ReleaseGPUShader(scene->device, ui_fs);
                return false;
            }

            SDL_GPUVertexAttribute ui_attrs[4];
            SDL_zero(ui_attrs);
            ui_attrs[0].location    = 0;
            ui_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            ui_attrs[0].offset      = offsetof(ForgeUiVertex, pos_x);
            ui_attrs[1].location    = 1;
            ui_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            ui_attrs[1].offset      = offsetof(ForgeUiVertex, uv_u);
            ui_attrs[2].location    = 2;
            ui_attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            ui_attrs[2].offset      = offsetof(ForgeUiVertex, r);
            ui_attrs[3].location    = 3;
            ui_attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            ui_attrs[3].offset      = offsetof(ForgeUiVertex, b);

            SDL_GPUVertexBufferDescription ui_vbd;
            SDL_zero(ui_vbd);
            ui_vbd.pitch      = sizeof(ForgeUiVertex);
            ui_vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

            SDL_GPUColorTargetDescription ui_ctd;
            SDL_zero(ui_ctd);
            ui_ctd.format = scene->swapchain_fmt;
            ui_ctd.blend_state.enable_blend = true;
            ui_ctd.blend_state.src_color_blendfactor =
                SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            ui_ctd.blend_state.dst_color_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            ui_ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            ui_ctd.blend_state.src_alpha_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE;
            ui_ctd.blend_state.dst_alpha_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            ui_ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            ui_ctd.blend_state.color_write_mask =
                SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

            SDL_GPUGraphicsPipelineCreateInfo pi;
            SDL_zero(pi);
            pi.vertex_shader   = ui_vs;
            pi.fragment_shader = ui_fs;
            pi.vertex_input_state.vertex_buffer_descriptions = &ui_vbd;
            pi.vertex_input_state.num_vertex_buffers         = 1;
            pi.vertex_input_state.vertex_attributes          = ui_attrs;
            pi.vertex_input_state.num_vertex_attributes      = 4;
            pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
            pi.rasterizer_state.front_face =
                SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            pi.depth_stencil_state.enable_depth_test  = false;
            pi.depth_stencil_state.enable_depth_write = false;
            pi.target_info.color_target_descriptions = &ui_ctd;
            pi.target_info.num_color_targets         = 1;
            pi.target_info.has_depth_stencil_target  = false;

            scene->ui_pipeline =
                SDL_CreateGPUGraphicsPipeline(scene->device, &pi);

            SDL_ReleaseGPUShader(scene->device, ui_vs);
            SDL_ReleaseGPUShader(scene->device, ui_fs);

            if (!scene->ui_pipeline) {
                SDL_Log("forge_scene: UI pipeline failed: %s",
                        SDL_GetError());
                return false;
            }
        }

        scene->ui_enabled = true;
    }

    /* ── 12. Capture (optional) ──────────────────────────────────── */

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&scene->capture, argc, argv);
    if (scene->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&scene->capture,
                                scene->device, scene->window)) {
            SDL_Log("forge_scene: capture init failed");
            return false;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    return true;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Event handling
 * ══════════════════════════════════════════════════════════════════════════ */

static SDL_AppResult forge_scene_handle_event(ForgeScene *scene,
                                               SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat) break;
        if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
            if (scene->mouse_captured) {
                if (!SDL_SetWindowRelativeMouseMode(scene->window, false)) {
                    SDL_Log("forge_scene: SDL_SetWindowRelativeMouseMode "
                            "failed: %s", SDL_GetError());
                } else {
                    scene->mouse_captured = false;
                }
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!scene->mouse_captured) {
            /* Don't capture mouse if UI widget is hot or active
             * (postmortem issue #5: camera fights UI widgets). */
            if (scene->ui_enabled &&
                (scene->ui_ctx.hot != FORGE_UI_ID_NONE ||
                 scene->ui_ctx.active != FORGE_UI_ID_NONE)) {
                break;
            }
            if (!SDL_SetWindowRelativeMouseMode(scene->window, true)) {
                SDL_Log("forge_scene: SDL_SetWindowRelativeMouseMode "
                        "failed: %s", SDL_GetError());
            } else {
                scene->mouse_captured = true;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (scene->mouse_captured) {
            /* Check UI isn't active (postmortem issue #5). */
            if (scene->ui_enabled &&
                (scene->ui_ctx.hot != FORGE_UI_ID_NONE ||
                 scene->ui_ctx.active != FORGE_UI_ID_NONE)) {
                break;
            }
            /* Yaw decrements on positive xrel (CLAUDE.md FPS camera rule). */
            scene->cam_yaw   -= event->motion.xrel *
                                scene->config.mouse_sensitivity;
            scene->cam_pitch -= event->motion.yrel *
                                scene->config.mouse_sensitivity;
            if (scene->cam_pitch >  FORGE_SCENE_PITCH_CLAMP)
                scene->cam_pitch =  FORGE_SCENE_PITCH_CLAMP;
            if (scene->cam_pitch < -FORGE_SCENE_PITCH_CLAMP)
                scene->cam_pitch = -FORGE_SCENE_PITCH_CLAMP;
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        /* Accumulate scroll delta for UI scrolling. */
        scene->frame_scroll_delta += event->wheel.y;
        break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Frame lifecycle
 * ══════════════════════════════════════════════════════════════════════════ */

static bool forge_scene_begin_frame(ForgeScene *scene)
{
    /* ── Delta time ──────────────────────────────────────────────── */
    Uint64 now = SDL_GetPerformanceCounter();
    scene->dt = (float)(now - scene->last_ticks) /
                (float)SDL_GetPerformanceFrequency();
    scene->last_ticks = now;
    if (scene->dt > FORGE_SCENE_MAX_DELTA_TIME)
        scene->dt = FORGE_SCENE_MAX_DELTA_TIME;

    /* ── Camera movement (quaternion FPS camera — CLAUDE.md pattern) */
    quat cam_orient = quat_from_euler(scene->cam_yaw, scene->cam_pitch, 0.0f);
    vec3 forward = quat_forward(cam_orient);
    vec3 right   = quat_right(cam_orient);

    const bool *keys = SDL_GetKeyboardState(NULL);
    vec3 move = vec3_create(0.0f, 0.0f, 0.0f);

    if (keys[SDL_SCANCODE_W])      move = vec3_add(move, forward);
    if (keys[SDL_SCANCODE_S])      move = vec3_sub(move, forward);
    if (keys[SDL_SCANCODE_D])      move = vec3_add(move, right);
    if (keys[SDL_SCANCODE_A])      move = vec3_sub(move, right);
    if (keys[SDL_SCANCODE_SPACE])  move.y += 1.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) move.y -= 1.0f;

    if (vec3_length(move) > 0.001f) {
        move = vec3_scale(vec3_normalize(move),
                          scene->config.move_speed * scene->dt);
        scene->cam_position = vec3_add(scene->cam_position, move);
    }

    /* ── Camera matrices ─────────────────────────────────────────── */
    mat4 view = mat4_view_from_quat(scene->cam_position, cam_orient);

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(scene->window, &w, &h)) {
        SDL_Log("forge_scene: SDL_GetWindowSizeInPixels failed: %s",
                SDL_GetError());
    }
    scene->aspect = (w > 0 && h > 0) ? (float)w / (float)h
                                       : FORGE_SCENE_DEFAULT_ASPECT;
    mat4 proj = mat4_perspective(
        scene->config.fov_deg * FORGE_DEG2RAD, scene->aspect,
        scene->config.near_plane, scene->config.far_plane);
    scene->cam_vp = mat4_multiply(proj, view);

    /* ── Acquire command buffer and swapchain ─────────────────────── */
    scene->cmd = SDL_AcquireGPUCommandBuffer(scene->device);
    if (!scene->cmd) {
        SDL_Log("forge_scene: SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError());
        return false;
    }

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            scene->cmd, scene->window,
            &scene->swapchain, &scene->sw, &scene->sh)) {
        SDL_Log("forge_scene: SDL_WaitAndAcquireGPUSwapchainTexture "
                "failed: %s", SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        scene->cmd = NULL;
        return false;
    }
    if (!scene->swapchain) {
        /* Window minimized — skip frame */
        if (!SDL_CancelGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        scene->cmd = NULL;
        return false;
    }

    /* ── Resize depth texture if needed ───────────────────────────── */
    if (scene->sw != scene->depth_w || scene->sh != scene->depth_h) {
        SDL_GPUTexture *new_depth = forge_scene__create_depth(
            scene->device, scene->sw, scene->sh,
            scene->depth_fmt,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
        if (new_depth) {
            SDL_ReleaseGPUTexture(scene->device, scene->depth_texture);
            scene->depth_texture = new_depth;
            scene->depth_w = scene->sw;
            scene->depth_h = scene->sh;
        } else {
            SDL_Log("forge_scene: depth texture resize failed");
            /* Abort frame — mismatched depth/swapchain dimensions would cause
             * rendering errors.  Swapchain already acquired, so we must
             * submit (not cancel) the command buffer. */
            if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
                SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            scene->cmd = NULL;
            return false;
        }
    }

    return true;
}

/* ── Shadow pass ────────────────────────────────────────────────────────── */

static void forge_scene_begin_shadow_pass(ForgeScene *scene)
{
    /* Guard: a prior stage may have failed and cleared cmd */
    if (!scene->cmd) return;

    /* Close the batched skinned-model joint upload pass (if any) before
     * starting the first render pass.  All skinned updates must happen
     * before this point. */
    if (scene->skinned_copy_pass) {
        SDL_EndGPUCopyPass(scene->skinned_copy_pass);
        scene->skinned_copy_pass = NULL;
    }

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture     = scene->shadow_map;
    depth_target.clear_depth = 1.0f;
    depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op    = SDL_GPU_STOREOP_STORE;
    /* Explicitly set stencil ops (postmortem issue #2: SDL_zero leaves
     * stencil_load_op = LOAD = 0, which asserts with cycle=true). */
    depth_target.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    scene->pass = SDL_BeginGPURenderPass(
        scene->cmd, NULL, 0, &depth_target);
    if (!scene->pass) {
        SDL_Log("forge_scene: shadow render pass failed: %s",
                SDL_GetError());
        /* After swapchain acquisition we must submit, not cancel. */
        if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        scene->cmd = NULL;
        return;
    }

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->shadow_pipeline);

    SDL_GPUViewport shadow_vp = {
        0, 0,
        (float)scene->config.shadow_map_size,
        (float)scene->config.shadow_map_size,
        0.0f, 1.0f
    };
    SDL_SetGPUViewport(scene->pass, &shadow_vp);
}

static void forge_scene_draw_shadow_mesh(ForgeScene *scene,
                                          SDL_GPUBuffer *vb,
                                          SDL_GPUBuffer *ib,
                                          Uint32 index_count,
                                          mat4 model)
{
    if (!scene->pass || !vb || !ib || index_count == 0) return;

    SDL_GPUBufferBinding vb_bind = { vb, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    ForgeSceneShadowVertUniforms su;
    su.light_vp = mat4_multiply(scene->light_vp, model);
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

    SDL_DrawGPUIndexedPrimitives(scene->pass, index_count, 1, 0, 0, 0);
}

static void forge_scene_end_shadow_pass(ForgeScene *scene)
{
    if (scene->pass) {
        SDL_EndGPURenderPass(scene->pass);
        scene->pass = NULL;
    }
}

/* ── Main pass ──────────────────────────────────────────────────────────── */

static void forge_scene_begin_main_pass(ForgeScene *scene)
{
    if (!scene->cmd) return;

    /* Safety net: close the batched skinned joint upload pass if the caller
     * skipped the shadow pass.  begin_shadow_pass normally handles this,
     * but shadows are optional for skinned models. */
    if (scene->skinned_copy_pass) {
        SDL_EndGPUCopyPass(scene->skinned_copy_pass);
        scene->skinned_copy_pass = NULL;
    }

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture    = scene->swapchain;
    color_target.clear_color = (SDL_FColor){
        scene->config.clear_color[0],
        scene->config.clear_color[1],
        scene->config.clear_color[2],
        scene->config.clear_color[3]
    };
    color_target.load_op  = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture     = scene->depth_texture;
    depth_target.clear_depth = 1.0f;
    depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op    = SDL_GPU_STOREOP_STORE;
    /* Explicit stencil ops (postmortem issue #2). */
    depth_target.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    scene->pass = SDL_BeginGPURenderPass(
        scene->cmd, &color_target, 1, &depth_target);
    if (!scene->pass) {
        SDL_Log("forge_scene: main render pass failed: %s", SDL_GetError());
        /* After swapchain acquisition we must submit, not cancel. */
        if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        scene->cmd = NULL;
        return;
    }

    SDL_GPUViewport vp = {
        0, 0, (float)scene->sw, (float)scene->sh, 0.0f, 1.0f
    };
    SDL_SetGPUViewport(scene->pass, &vp);
    SDL_SetGPUScissor(scene->pass,
        &(SDL_Rect){ 0, 0, (int)scene->sw, (int)scene->sh });

    /* Draw sky background first (writes at z=0.9999, no depth write). */
    SDL_BindGPUGraphicsPipeline(scene->pass, scene->sky_pipeline);
    SDL_DrawGPUPrimitives(scene->pass, 3, 1, 0, 0);
}

/* Internal: shared draw logic for scene meshes (pipeline varies) */
static void forge_scene__draw_mesh_internal(ForgeScene *scene,
                                             SDL_GPUGraphicsPipeline *pipeline,
                                             SDL_GPUBuffer *vb,
                                             SDL_GPUBuffer *ib,
                                             Uint32 index_count,
                                             mat4 model,
                                             const float base_color[4])
{
    if (!scene->pass || !pipeline || !vb || !ib || index_count == 0) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, pipeline);

    /* Shadow map bound at fragment sampler slot 0 */
    SDL_GPUTextureSamplerBinding shadow_bind = {
        scene->shadow_map, scene->shadow_sampler
    };
    SDL_BindGPUFragmentSamplers(scene->pass, 0, &shadow_bind, 1);

    SDL_GPUBufferBinding vb_bind = { vb, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Vertex uniforms: MVP, model, light VP */
    ForgeSceneVertUniforms vu;
    vu.mvp      = mat4_multiply(scene->cam_vp, model);
    vu.model    = model;
    vu.light_vp = mat4_multiply(scene->light_vp, model);
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

    /* Fragment uniforms: lighting parameters */
    ForgeSceneFragUniforms fu;
    SDL_memset(&fu, 0, sizeof(fu));
    fu.base_color[0]   = base_color[0];
    fu.base_color[1]   = base_color[1];
    fu.base_color[2]   = base_color[2];
    fu.base_color[3]   = base_color[3];
    fu.eye_pos[0]      = scene->cam_position.x;
    fu.eye_pos[1]      = scene->cam_position.y;
    fu.eye_pos[2]      = scene->cam_position.z;
    fu.ambient         = scene->config.ambient;
    fu.light_dir[0]    = scene->light_dir.x;
    fu.light_dir[1]    = scene->light_dir.y;
    fu.light_dir[2]    = scene->light_dir.z;
    fu.light_dir[3]    = 0.0f;
    fu.light_color[0]  = scene->config.light_color[0];
    fu.light_color[1]  = scene->config.light_color[1];
    fu.light_color[2]  = scene->config.light_color[2];
    fu.light_intensity = scene->config.light_intensity;
    fu.shininess       = scene->config.shininess;
    fu.specular_str    = scene->config.specular_str;
    SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

    SDL_DrawGPUIndexedPrimitives(scene->pass, index_count, 1, 0, 0, 0);
}

static void forge_scene_draw_mesh(ForgeScene *scene,
                                   SDL_GPUBuffer *vb,
                                   SDL_GPUBuffer *ib,
                                   Uint32 index_count,
                                   mat4 model,
                                   const float base_color[4])
{
    forge_scene__draw_mesh_internal(scene, scene->scene_pipeline,
                                    vb, ib, index_count, model, base_color);
}

/* ── Double-sided mesh (cull none) ─────────────────────────────────────── */

static void forge_scene_draw_mesh_double_sided(ForgeScene *scene,
                                                SDL_GPUBuffer *vb,
                                                SDL_GPUBuffer *ib,
                                                Uint32 index_count,
                                                mat4 model,
                                                const float base_color[4])
{
    forge_scene__draw_mesh_internal(scene, scene->scene_pipeline_double,
                                    vb, ib, index_count, model, base_color);
}

/* ── Grid floor ────────────────────────────────────────────────────────── */

static void forge_scene_draw_grid(ForgeScene *scene)
{
    if (!scene->pass) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->grid_pipeline);

    /* Re-bind shadow map for grid fragment sampling */
    SDL_GPUTextureSamplerBinding shadow_bind = {
        scene->shadow_map, scene->shadow_sampler
    };
    SDL_BindGPUFragmentSamplers(scene->pass, 0, &shadow_bind, 1);

    /* Bind grid geometry */
    SDL_GPUBufferBinding vb_bind = { scene->grid_vb, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { scene->grid_ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Grid vertex uniforms: camera VP + light VP */
    ForgeSceneGridVertUniforms gvu;
    gvu.vp       = scene->cam_vp;
    gvu.light_vp = scene->light_vp;
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &gvu, sizeof(gvu));

    /* Grid fragment uniforms: lighting + grid appearance */
    ForgeSceneGridFragUniforms gfu;
    SDL_memset(&gfu, 0, sizeof(gfu));
    gfu.line_color[0]   = scene->config.grid_line_color[0];
    gfu.line_color[1]   = scene->config.grid_line_color[1];
    gfu.line_color[2]   = scene->config.grid_line_color[2];
    gfu.line_color[3]   = scene->config.grid_line_color[3];
    gfu.bg_color[0]     = scene->config.grid_bg_color[0];
    gfu.bg_color[1]     = scene->config.grid_bg_color[1];
    gfu.bg_color[2]     = scene->config.grid_bg_color[2];
    gfu.bg_color[3]     = scene->config.grid_bg_color[3];
    gfu.light_dir[0]    = scene->light_dir.x;
    gfu.light_dir[1]    = scene->light_dir.y;
    gfu.light_dir[2]    = scene->light_dir.z;
    gfu.light_intensity = scene->config.light_intensity;
    gfu.eye_pos[0]      = scene->cam_position.x;
    gfu.eye_pos[1]      = scene->cam_position.y;
    gfu.eye_pos[2]      = scene->cam_position.z;
    gfu.grid_spacing    = scene->config.grid_spacing;
    gfu.line_width      = scene->config.grid_line_width;
    gfu.fade_distance   = scene->config.grid_fade_dist;
    gfu.ambient         = scene->config.ambient;
    SDL_PushGPUFragmentUniformData(scene->cmd, 0, &gfu, sizeof(gfu));

    SDL_DrawGPUIndexedPrimitives(scene->pass, 6, 1, 0, 0, 0);
}

static void forge_scene_end_main_pass(ForgeScene *scene)
{
    if (scene->pass) {
        SDL_EndGPURenderPass(scene->pass);
        scene->pass = NULL;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  UI pass
 * ══════════════════════════════════════════════════════════════════════════ */

static void forge_scene_begin_ui(ForgeScene *scene,
                                  float mouse_x, float mouse_y,
                                  bool mouse_down)
{
    if (!scene->ui_enabled) return;

    forge_ui_ctx_begin(&scene->ui_ctx, mouse_x, mouse_y, mouse_down);

    /* Pass accumulated scroll delta from SDL_EVENT_MOUSE_WHEEL events. */
    scene->ui_ctx.scroll_delta = scene->frame_scroll_delta;
    scene->frame_scroll_delta  = 0.0f;

    /* Begin window context (determines hovered window from prev frame) */
    forge_ui_wctx_begin(&scene->ui_wctx);
}

static void forge_scene_end_ui(ForgeScene *scene)
{
    if (!scene->ui_enabled || !scene->cmd) return;

    /* Merge window draw lists into main context before ending */
    forge_ui_wctx_end(&scene->ui_wctx);
    forge_ui_ctx_end(&scene->ui_ctx);

    /* Skip if no draw data this frame */
    if (scene->ui_ctx.vertex_count == 0 ||
        scene->ui_ctx.index_count == 0) {
        return;
    }

    Uint32 vb_needed =
        (Uint32)scene->ui_ctx.vertex_count * (Uint32)sizeof(ForgeUiVertex);
    Uint32 ib_needed =
        (Uint32)scene->ui_ctx.index_count * (Uint32)sizeof(Uint32);

    /* ── Grow GPU buffers if needed (power-of-two sizing) ──────────── */

    if (vb_needed > scene->ui_vb_capacity) {
        Uint32 new_cap = scene->ui_vb_capacity;
        while (new_cap < vb_needed) new_cap *= 2;

        SDL_GPUBufferCreateInfo bi;
        SDL_zero(bi);
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bi.size  = new_cap;
        /* Allocate new buffer before releasing old — if creation fails,
         * the old buffer and capacity remain valid (no NULL-with-capacity). */
        SDL_GPUBuffer *new_vb = SDL_CreateGPUBuffer(scene->device, &bi);
        if (!new_vb) {
            SDL_Log("forge_scene: UI VB resize failed: %s", SDL_GetError());
            return;
        }
        SDL_ReleaseGPUBuffer(scene->device, scene->ui_vb);
        scene->ui_vb = new_vb;
        scene->ui_vb_capacity = new_cap;
    }

    if (ib_needed > scene->ui_ib_capacity) {
        Uint32 new_cap = scene->ui_ib_capacity;
        while (new_cap < ib_needed) new_cap *= 2;

        SDL_GPUBufferCreateInfo bi;
        SDL_zero(bi);
        bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        bi.size  = new_cap;
        /* Same allocate-then-release pattern for index buffer. */
        SDL_GPUBuffer *new_ib = SDL_CreateGPUBuffer(scene->device, &bi);
        if (!new_ib) {
            SDL_Log("forge_scene: UI IB resize failed: %s", SDL_GetError());
            return;
        }
        SDL_ReleaseGPUBuffer(scene->device, scene->ui_ib);
        scene->ui_ib = new_ib;
        scene->ui_ib_capacity = new_cap;
    }

    /* ── Upload vertex + index data via copy pass ──────────────────── */
    /* IMPORTANT: Copy pass must happen BEFORE the UI render pass, not
     * inside it (postmortem issue #3). */

    Uint32 total_upload = vb_needed + ib_needed;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_upload;

    SDL_GPUTransferBuffer *xfer =
        SDL_CreateGPUTransferBuffer(scene->device, &xfer_info);
    if (!xfer) {
        SDL_Log("forge_scene: UI transfer buffer failed: %s", SDL_GetError());
        return;
    }

    void *mapped = SDL_MapGPUTransferBuffer(scene->device, xfer, false);
    if (!mapped) {
        SDL_Log("forge_scene: UI map transfer buffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        return;
    }

    /* Vertex data at offset 0, index data immediately after */
    SDL_memcpy(mapped, scene->ui_ctx.vertices, vb_needed);
    SDL_memcpy((Uint8 *)mapped + vb_needed,
               scene->ui_ctx.indices, ib_needed);
    SDL_UnmapGPUTransferBuffer(scene->device, xfer);

    /* Copy pass to upload vertex + index data */
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(scene->cmd);
    if (!copy) {
        SDL_Log("forge_scene: UI copy pass failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        /* After swapchain acquisition we must submit, not cancel. */
        if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        scene->cmd = NULL;
        return;
    }

    {
        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = xfer;
        src.offset          = 0;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = scene->ui_vb;
        dst.offset = 0;
        dst.size   = vb_needed;

        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    }

    {
        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = xfer;
        src.offset          = vb_needed;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = scene->ui_ib;
        dst.offset = 0;
        dst.size   = ib_needed;

        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    }

    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(scene->device, xfer);

    /* ── UI render pass (separate from main scene pass) ────────────── */

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture  = scene->swapchain;
    color_target.load_op  = SDL_GPU_LOADOP_LOAD;   /* preserve scene content */
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *ui_pass =
        SDL_BeginGPURenderPass(scene->cmd, &color_target, 1, NULL);
    if (!ui_pass) {
        SDL_Log("forge_scene: UI render pass failed: %s", SDL_GetError());
        /* After swapchain acquisition we must submit, not cancel. */
        if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        scene->cmd = NULL;
        return;
    }

    SDL_BindGPUGraphicsPipeline(ui_pass, scene->ui_pipeline);

    /* Bind font atlas at fragment sampler slot 0 */
    SDL_GPUTextureSamplerBinding atlas_bind = {
        scene->ui_atlas_texture, scene->atlas_sampler
    };
    SDL_BindGPUFragmentSamplers(ui_pass, 0, &atlas_bind, 1);

    /* Bind UI vertex + index buffers */
    SDL_GPUBufferBinding vb_bind = { scene->ui_vb, 0 };
    SDL_BindGPUVertexBuffers(ui_pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { scene->ui_ib, 0 };
    SDL_BindGPUIndexBuffer(ui_pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Push orthographic projection matrix */
    ForgeSceneUiUniforms uu;
    uu.projection = mat4_orthographic(
        0.0f, (float)scene->sw, (float)scene->sh, 0.0f,
        -1.0f, 1.0f);
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &uu, sizeof(uu));

    SDL_DrawGPUIndexedPrimitives(ui_pass,
        (Uint32)scene->ui_ctx.index_count, 1, 0, 0, 0);

    SDL_EndGPURenderPass(ui_pass);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Frame end
 * ══════════════════════════════════════════════════════════════════════════ */

static SDL_AppResult forge_scene_end_frame(ForgeScene *scene)
{
    if (!scene->cmd) return SDL_APP_CONTINUE;

#ifdef FORGE_CAPTURE
    if (scene->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&scene->capture,
                                         scene->cmd, scene->swapchain)) {
            if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
                SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            scene->cmd = NULL;
            return SDL_APP_CONTINUE;
        }
        if (forge_capture_should_quit(&scene->capture)) {
            scene->cmd = NULL;
            return SDL_APP_SUCCESS;
        }
    } else
#endif
    {
        if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
            SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
            scene->cmd = NULL;
            return SDL_APP_FAILURE;
        }
    }

    scene->cmd = NULL;
    return SDL_APP_CONTINUE;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Cleanup
 * ══════════════════════════════════════════════════════════════════════════ */

static void forge_scene_destroy(ForgeScene *scene)
{
    if (!scene) return;

    if (!scene->device) {
        if (scene->window)
            SDL_DestroyWindow(scene->window);
        SDL_memset(scene, 0, sizeof(*scene));
        return;
    }

    /* Save device/window before we zero the struct at the end */
    SDL_Window *window = scene->window;
    SDL_GPUDevice *device = scene->device;

    /* Close any open copy/render passes and finalize in-flight command
     * buffer before tearing down GPU resources.  This handles the case
     * where the caller exits mid-frame (after begin_frame but before
     * end_frame). */
    if (scene->skinned_copy_pass) {
        SDL_EndGPUCopyPass(scene->skinned_copy_pass);
        scene->skinned_copy_pass = NULL;
    }
    if (scene->pass) {
        SDL_EndGPURenderPass(scene->pass);
        scene->pass = NULL;
    }
    if (scene->cmd) {
        /* If the swapchain was acquired (non-NULL), SDL requires submit,
         * not cancel.  Cancel is only valid before swapchain acquisition. */
        if (scene->swapchain) {
            if (!SDL_SubmitGPUCommandBuffer(scene->cmd)) {
                SDL_Log("forge_scene: SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
        } else {
            if (!SDL_CancelGPUCommandBuffer(scene->cmd)) {
                SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
        }
        scene->cmd = NULL;
    }

    /* Wait for all in-flight GPU work to complete */
    if (!SDL_WaitForGPUIdle(scene->device)) {
        SDL_Log("forge_scene: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&scene->capture, scene->device);
#endif

    /* UI resources — release based on pointer ownership, not ui_enabled,
     * so partial init failures don't leak GPU objects. */
    {
        if (scene->ui_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->ui_pipeline);
        if (scene->ui_vb)
            SDL_ReleaseGPUBuffer(scene->device, scene->ui_vb);
        if (scene->ui_ib)
            SDL_ReleaseGPUBuffer(scene->device, scene->ui_ib);
        if (scene->ui_atlas_texture)
            SDL_ReleaseGPUTexture(scene->device, scene->ui_atlas_texture);
        if (scene->atlas_sampler)
            SDL_ReleaseGPUSampler(scene->device, scene->atlas_sampler);

        forge_ui_wctx_free(&scene->ui_wctx);
        forge_ui_ctx_free(&scene->ui_ctx);
        forge_ui_atlas_free(&scene->ui_atlas);
        forge_ui_ttf_free(&scene->ui_font);
    }

    /* Grid buffers */
    if (scene->grid_vb)
        SDL_ReleaseGPUBuffer(scene->device, scene->grid_vb);
    if (scene->grid_ib)
        SDL_ReleaseGPUBuffer(scene->device, scene->grid_ib);

    /* Textures */
    if (scene->shadow_map)
        SDL_ReleaseGPUTexture(scene->device, scene->shadow_map);
    if (scene->depth_texture)
        SDL_ReleaseGPUTexture(scene->device, scene->depth_texture);

    /* Samplers */
    if (scene->shadow_sampler)
        SDL_ReleaseGPUSampler(scene->device, scene->shadow_sampler);

    /* Pipelines */
    if (scene->sky_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->sky_pipeline);
    if (scene->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->grid_pipeline);
    if (scene->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->scene_pipeline);
    if (scene->scene_pipeline_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->scene_pipeline_double);
    if (scene->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->shadow_pipeline);

#ifdef FORGE_SCENE_MODEL_SUPPORT
    /* Model pipelines and resources */
    if (scene->model_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline);
    if (scene->model_pipeline_blend)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline_blend);
    if (scene->model_pipeline_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline_double);
    if (scene->model_pipeline_blend_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline_blend_double);
    if (scene->model_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_shadow_pipeline);
    if (scene->model_tex_sampler)
        SDL_ReleaseGPUSampler(scene->device, scene->model_tex_sampler);
    if (scene->model_normal_sampler)
        SDL_ReleaseGPUSampler(scene->device, scene->model_normal_sampler);
    if (scene->model_shadow_cmp_sampler)
        SDL_ReleaseGPUSampler(scene->device, scene->model_shadow_cmp_sampler);
    if (scene->model_white_texture)
        SDL_ReleaseGPUTexture(scene->device, scene->model_white_texture);
    if (scene->model_flat_normal)
        SDL_ReleaseGPUTexture(scene->device, scene->model_flat_normal);
    if (scene->model_black_texture)
        SDL_ReleaseGPUTexture(scene->device, scene->model_black_texture);
    /* Skinned model pipelines */
    if (scene->skinned_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline);
    if (scene->skinned_pipeline_blend)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline_blend);
    if (scene->skinned_pipeline_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline_double);
    if (scene->skinned_pipeline_blend_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline_blend_double);
    if (scene->skinned_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_shadow_pipeline);
#endif

    /* Window — release claim first, then destroy.
     * Use locals because we zero the struct below. */
    if (scene->window_claimed)
        SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);

    /* Zero the entire struct so a second call is a safe no-op */
    SDL_memset(scene, 0, sizeof(*scene));
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Model loading and rendering (requires FORGE_SCENE_MODEL_SUPPORT)
 * ══════════════════════════════════════════════════════════════════════════ */

#ifdef FORGE_SCENE_MODEL_SUPPORT

/* ── Internal: create a 1x1 RGBA texture ────────────────────────────────── */

static SDL_GPUTexture *forge_scene__create_1x1(SDL_GPUDevice *device,
                                                 Uint8 r, Uint8 g,
                                                 Uint8 b, Uint8 a,
                                                 bool srgb)
{
    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type                 = SDL_GPU_TEXTURETYPE_2D;
    ti.format               = srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                                   : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width                = 1;
    ti.height               = 1;
    ti.layer_count_or_depth = 1;
    ti.num_levels           = 1;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &ti);
    if (!tex) {
        SDL_Log("forge_scene: SDL_CreateGPUTexture (1x1) failed: %s",
                SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = 4;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) { SDL_ReleaseGPUTexture(device, tex); return NULL; }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    Uint8 *p = (Uint8 *)mapped;
    p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        if (!SDL_CancelGPUCommandBuffer(cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;
    src.pixels_per_row  = 1;
    src.rows_per_layer  = 1;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = 1; dst.h = 1; dst.d = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("forge_scene: 1x1 texture submit failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Internal: load a texture from disk and upload to GPU ───────────────── */

static SDL_GPUTexture *forge_scene_load_texture(ForgeScene *scene,
                                                 const char *path,
                                                 bool srgb)
{
    if (!path || path[0] == '\0') return NULL;

    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("forge_scene: failed to load texture '%s': %s",
                path, SDL_GetError());
        return NULL;
    }

    SDL_GPUTexture *tex = forge_scene_upload_texture(scene, surface, srgb);
    SDL_DestroySurface(surface);
    return tex;
}

/* ── Compressed texture upload (BC7/BC5 from pre-transcoded mip data) ──── */

#ifdef FORGE_SCENE_MODEL_SUPPORT

/* Map ForgePipelineCompressedFormat to SDL_GPUTextureFormat. */
static SDL_GPUTextureFormat forge_scene__compressed_sdl_format(
    ForgePipelineCompressedFormat fmt)
{
    switch (fmt) {
    case FORGE_PIPELINE_COMPRESSED_BC7_SRGB:
        return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB;
    case FORGE_PIPELINE_COMPRESSED_BC7_UNORM:
        return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
    case FORGE_PIPELINE_COMPRESSED_BC5_UNORM:
        return SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
    default:
        return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
    }
}

/* Upload pre-transcoded compressed mip levels to the GPU.
 * Returns the created GPU texture, or NULL on failure. */
static SDL_GPUTexture *forge_scene_upload_compressed_texture(
    ForgeScene *scene,
    const ForgePipelineCompressedTexture *ctex)
{
    if (!scene || !scene->device || !ctex || !ctex->mips || ctex->mip_count == 0) {
        SDL_Log("forge_scene: upload_compressed_texture: invalid arguments");
        return NULL;
    }

    SDL_GPUTextureFormat sdl_fmt = forge_scene__compressed_sdl_format(ctex->format);

    /* Check format support before attempting creation */
    if (!SDL_GPUTextureSupportsFormat(scene->device, sdl_fmt,
                                       SDL_GPU_TEXTURETYPE_2D,
                                       SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        SDL_Log("forge_scene: GPU does not support compressed format %d",
                (int)sdl_fmt);
        return NULL;
    }

    /* Create the GPU texture with the compressed format */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = sdl_fmt;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = ctex->width;
    tex_info.height               = ctex->height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = ctex->mip_count;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(scene->device, &tex_info);
    if (!texture) {
        SDL_Log("forge_scene: compressed texture creation failed (%ux%u, "
                "fmt %d, mips %u): %s", ctex->width, ctex->height,
                (int)sdl_fmt, ctex->mip_count, SDL_GetError());
        return NULL;
    }

    /* D3D12 requires texture upload row pitch aligned to 256 bytes
     * (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) and mip offsets aligned to
     * 512 bytes (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT).  For BC formats
     * (4x4 blocks, 16 bytes/block), small mips have row pitches below 256.
     * We pad rows and align offsets unconditionally (harmless on Vulkan/Metal)
     * so the D3D12 backend takes the fast copy path instead of hitting its
     * buggy realignment code. */
#define FORGE_SCENE_D3D12_PITCH_ALIGN     256
#define FORGE_SCENE_D3D12_PLACEMENT_ALIGN 512
#define FORGE_SCENE_BC_BLOCK_SIZE         16  /* BC7 and BC5: 16 bytes per 4x4 block */
#define FORGE_SCENE_BC_BLOCK_DIM          4   /* 4x4 pixel blocks */

    /* Compute padded sizes per mip and total transfer buffer size */
    uint32_t offsets[FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS];
    uint32_t aligned_pitches[FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS];
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < ctex->mip_count && i < FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS; i++) {
        const ForgePipelineCompressedMip *mip = &ctex->mips[i];
        if (!mip->data || mip->data_size == 0) {
            offsets[i] = total_size;
            aligned_pitches[i] = 0;
            continue;
        }
        /* Align mip offset to 512 bytes (D3D12 placement alignment) */
        total_size = (total_size + FORGE_SCENE_D3D12_PLACEMENT_ALIGN - 1)
                   & ~(FORGE_SCENE_D3D12_PLACEMENT_ALIGN - 1);
        uint32_t blocks_x = (mip->width + FORGE_SCENE_BC_BLOCK_DIM - 1) / FORGE_SCENE_BC_BLOCK_DIM;
        uint32_t blocks_y = (mip->height + FORGE_SCENE_BC_BLOCK_DIM - 1) / FORGE_SCENE_BC_BLOCK_DIM;
        uint32_t row_pitch = blocks_x * FORGE_SCENE_BC_BLOCK_SIZE;
        uint32_t aligned_pitch = (row_pitch + FORGE_SCENE_D3D12_PITCH_ALIGN - 1)
                               & ~(FORGE_SCENE_D3D12_PITCH_ALIGN - 1);
        offsets[i] = total_size;
        aligned_pitches[i] = aligned_pitch;
        total_size += aligned_pitch * blocks_y;
    }
    if (total_size == 0) return texture;

    /* Create a single transfer buffer for all mips */
    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_size;

    SDL_GPUTransferBuffer *xfer =
        SDL_CreateGPUTransferBuffer(scene->device, &xfer_info);
    if (!xfer) {
        SDL_Log("forge_scene: compressed transfer buffer failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }

    /* Map and pack mip data with 256-byte row alignment */
    void *mapped = SDL_MapGPUTransferBuffer(scene->device, xfer, false);
    if (!mapped) {
        SDL_Log("forge_scene: compressed transfer map failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }

    for (uint32_t i = 0; i < ctex->mip_count && i < FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS; i++) {
        const ForgePipelineCompressedMip *mip = &ctex->mips[i];
        if (!mip->data || mip->data_size == 0) continue;

        uint32_t blocks_x = (mip->width + FORGE_SCENE_BC_BLOCK_DIM - 1) / FORGE_SCENE_BC_BLOCK_DIM;
        uint32_t blocks_y = (mip->height + FORGE_SCENE_BC_BLOCK_DIM - 1) / FORGE_SCENE_BC_BLOCK_DIM;
        uint32_t src_pitch = blocks_x * FORGE_SCENE_BC_BLOCK_SIZE;
        uint32_t expected_size = src_pitch * blocks_y;
        if (mip->data_size < expected_size) {
            SDL_Log("forge_scene: mip %u data_size %u < expected %u, skipping",
                    i, mip->data_size, expected_size);
            continue;
        }
        uint32_t dst_pitch = aligned_pitches[i];

        const uint8_t *src_row = (const uint8_t *)mip->data;
        uint8_t *dst_row = (uint8_t *)mapped + offsets[i];

        for (uint32_t row = 0; row < blocks_y; row++) {
            SDL_memcpy(dst_row, src_row, src_pitch);
            /* Zero padding bytes so we don't upload garbage */
            if (dst_pitch > src_pitch)
                SDL_memset(dst_row + src_pitch, 0, dst_pitch - src_pitch);
            src_row += src_pitch;
            dst_row += dst_pitch;
        }
    }
    SDL_UnmapGPUTransferBuffer(scene->device, xfer);

    /* Upload all mips in a single command buffer + copy pass */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(scene->device);
    if (!cmd) {
        SDL_Log("forge_scene: compressed cmd failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("forge_scene: compressed copy pass failed: %s", SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(cmd)) {
            SDL_Log("forge_scene: SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }

    for (uint32_t i = 0; i < ctex->mip_count && i < FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS; i++) {
        const ForgePipelineCompressedMip *mip = &ctex->mips[i];
        if (!mip->data || mip->data_size == 0) continue;

        /* Tell SDL the row pitch in pixels so it matches our padded layout */
        uint32_t pixels_per_padded_row =
            (aligned_pitches[i] / FORGE_SCENE_BC_BLOCK_SIZE) * FORGE_SCENE_BC_BLOCK_DIM;

        SDL_GPUTextureTransferInfo src;
        SDL_zero(src);
        src.transfer_buffer = xfer;
        src.offset          = offsets[i];
        src.pixels_per_row  = pixels_per_padded_row;
        src.rows_per_layer  = mip->height;

        SDL_GPUTextureRegion dst;
        SDL_zero(dst);
        dst.texture   = texture;
        dst.mip_level = i;
        dst.w = mip->width;
        dst.h = mip->height;
        dst.d = 1;

        SDL_UploadToGPUTexture(copy, &src, &dst, false);
    }

#undef FORGE_SCENE_D3D12_PITCH_ALIGN
#undef FORGE_SCENE_D3D12_PLACEMENT_ALIGN
#undef FORGE_SCENE_BC_BLOCK_SIZE
#undef FORGE_SCENE_BC_BLOCK_DIM

    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("forge_scene: compressed upload submit failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(scene->device, xfer);
        SDL_ReleaseGPUTexture(scene->device, texture);
        return NULL;
    }
    /* Safe to release now — SDL internally defers the free until the
     * submitted command buffer finishes executing on the GPU. */
    SDL_ReleaseGPUTransferBuffer(scene->device, xfer);

    return texture;
}

/* Estimate uncompressed VRAM cost: RGBA8 with full mip chain.
 * The mip chain sums to 4/3 * base size for power-of-two textures. */
static uint64_t forge_scene__estimate_uncompressed_vram(uint32_t w, uint32_t h)
{
    uint64_t total = 0;
    while (w >= 1 && h >= 1) {
        total += (uint64_t)w * h * 4; /* RGBA8 = 4 bytes/pixel */
        if (w == 1 && h == 1) break;
        if (w > 1) w /= 2;
        if (h > 1) h /= 2;
    }
    return total;
}

/* ── Pipeline-compressed texture loader ─────────────────────────────────── */

static SDL_GPUTexture *forge_scene_load_pipeline_texture(
    ForgeScene *scene, ForgeSceneVramStats *vram,
    const char *path, bool srgb, bool is_normal_map)
{
    (void)is_normal_map; /* format is baked into the .ftex file at build time */
    if (!path || path[0] == '\0') return NULL;
    if (!scene || !vram) return NULL;

    vram->total_texture_count++;

    /* Check for .meta.json sidecar with compression info */
    ForgePipelineCompressionInfo comp_info;
    bool has_sidecar = forge_pipeline_detect_sidecar(path, &comp_info);

    if (has_sidecar && comp_info.has_compression &&
        comp_info.compressed_file[0] != '\0') {

        /* Build full path to the .ftex file.
         * compressed_file is relative to the same directory as the image.
         * Extract the directory portion of `path`. */
        const char *last_sep = SDL_strrchr(path, '/');
        {
            const char *last_bsep = SDL_strrchr(path, '\\');
            if (last_bsep && (!last_sep || last_bsep > last_sep))
                last_sep = last_bsep;
        }
        size_t dir_len = last_sep ? (size_t)(last_sep - path) + 1 : 0;

        /* Replace .ktx2 extension with .ftex in the compressed_file name */
        char ftex_name[FORGE_PIPELINE_MAT_PATH_SIZE];
        SDL_strlcpy(ftex_name, comp_info.compressed_file, sizeof(ftex_name));
        {
            char *ext = SDL_strrchr(ftex_name, '.');
            if (ext) SDL_strlcpy(ext, ".ftex", sizeof(ftex_name) - (size_t)(ext - ftex_name));
        }

        char ftex_path[FORGE_SCENE_PATH_BUF_SIZE];
        if (dir_len > 0) {
            char dir_buf[FORGE_SCENE_PATH_BUF_SIZE];
            if (dir_len >= sizeof(dir_buf)) dir_len = sizeof(dir_buf) - 1;
            SDL_memcpy(dir_buf, path, dir_len);
            dir_buf[dir_len] = '\0';
            SDL_snprintf(ftex_path, sizeof(ftex_path), "%s%s", dir_buf, ftex_name);
        } else {
            SDL_strlcpy(ftex_path, ftex_name, sizeof(ftex_path));
        }

        /* Load pre-transcoded .ftex — no Basis transcoder needed */
        ForgePipelineCompressedTexture ctex;
        if (forge_pipeline_load_ftex(ftex_path, &ctex)) {
            /* Upload compressed blocks to GPU */
            SDL_GPUTexture *gpu_tex =
                forge_scene_upload_compressed_texture(scene, &ctex);

            if (gpu_tex) {
                /* Track VRAM usage */
                uint64_t compressed_size = 0;
                for (uint32_t m = 0; m < ctex.mip_count; m++) {
                    compressed_size += ctex.mips[m].data_size;
                }
                vram->compressed_bytes += compressed_size;
                vram->uncompressed_bytes +=
                    forge_scene__estimate_uncompressed_vram(
                        ctex.width, ctex.height);
                vram->compressed_texture_count++;

                forge_pipeline_free_compressed_texture(&ctex);
                return gpu_tex;
            }

            forge_pipeline_free_compressed_texture(&ctex);
            /* Fall through to fallback if upload failed */
        }
    }

    /* No compression available or transcoding failed — use the existing
     * SDL_LoadSurface + GPU mipmap generation path. */
    {
        SDL_Surface *surface = SDL_LoadSurface(path);
        if (!surface) {
            SDL_Log("forge_scene: failed to load texture '%s': %s",
                    path, SDL_GetError());
            return NULL;
        }

        /* Track VRAM before uploading (we have the surface dimensions) */
        uint64_t uncompressed = forge_scene__estimate_uncompressed_vram(
            (uint32_t)surface->w, (uint32_t)surface->h);
        vram->compressed_bytes   += uncompressed;
        vram->uncompressed_bytes += uncompressed;

        SDL_GPUTexture *tex = forge_scene_upload_texture(scene, surface, srgb);
        SDL_DestroySurface(surface);
        return tex;
    }
}

#endif /* FORGE_SCENE_MODEL_SUPPORT */

/* ── Init model pipelines (lazy, called on first load) ──────────────────── */

static bool forge_scene_init_model_pipelines(ForgeScene *scene)
{
    if (scene->model_pipelines_ready) return true;

    /* Create model shaders */
    SDL_GPUShader *model_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_model_vert_spirv, sizeof(scene_model_vert_spirv),
        scene_model_vert_dxil,  sizeof(scene_model_vert_dxil),
        scene_model_vert_msl, scene_model_vert_msl_size,
        0, 0, 0, 1);  /* 0 samplers, 0 storage, 0 storage_buf, 1 uniform */

    SDL_GPUShader *model_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_model_frag_spirv, sizeof(scene_model_frag_spirv),
        scene_model_frag_dxil,  sizeof(scene_model_frag_dxil),
        scene_model_frag_msl, scene_model_frag_msl_size,
        6, 0, 0, 1);  /* 6 samplers (5 texture + 1 shadow cmp), 1 uniform */

    /* Shadow uses existing shadow shader but with 48-byte stride */
    SDL_GPUShader *mshadow_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_vert_spirv, sizeof(shadow_vert_spirv),
        shadow_vert_dxil,  sizeof(shadow_vert_dxil),
        shadow_vert_msl, shadow_vert_msl_size,
        0, 0, 0, 1);

    SDL_GPUShader *mshadow_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl, shadow_frag_msl_size,
        0, 0, 0, 0);

    if (!model_vs || !model_fs || !mshadow_vs || !mshadow_fs) {
        SDL_Log("forge_scene: model shader creation failed");
        if (model_vs)   SDL_ReleaseGPUShader(scene->device, model_vs);
        if (model_fs)   SDL_ReleaseGPUShader(scene->device, model_fs);
        if (mshadow_vs) SDL_ReleaseGPUShader(scene->device, mshadow_vs);
        if (mshadow_fs) SDL_ReleaseGPUShader(scene->device, mshadow_fs);
        return false;
    }

    /* 48-byte vertex input: position + normal + uv + tangent */
    SDL_GPUVertexBufferDescription model_vbd;
    SDL_zero(model_vbd);
    model_vbd.slot       = 0;
    model_vbd.pitch      = sizeof(ForgeSceneModelVertex);
    model_vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute model_attrs[4];
    SDL_zero(model_attrs);
    model_attrs[0].location = 0; model_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; model_attrs[0].offset = 0;
    model_attrs[1].location = 1; model_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; model_attrs[1].offset = 12;
    model_attrs[2].location = 2; model_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; model_attrs[2].offset = 24;
    model_attrs[3].location = 3; model_attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; model_attrs[3].offset = 32;

    SDL_GPUVertexInputState model_input;
    SDL_zero(model_input);
    model_input.vertex_buffer_descriptions = &model_vbd;
    model_input.num_vertex_buffers         = 1;
    model_input.vertex_attributes          = model_attrs;
    model_input.num_vertex_attributes      = 4;

    /* Position-only input for shadow pipeline (48-byte stride, pos at offset 0) */
    SDL_GPUVertexAttribute shadow_pos_attr;
    SDL_zero(shadow_pos_attr);
    shadow_pos_attr.location = 0;
    shadow_pos_attr.format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shadow_pos_attr.offset   = 0;

    SDL_GPUVertexInputState shadow_input;
    SDL_zero(shadow_input);
    shadow_input.vertex_buffer_descriptions = &model_vbd;
    shadow_input.num_vertex_buffers         = 1;
    shadow_input.vertex_attributes          = &shadow_pos_attr;
    shadow_input.num_vertex_attributes      = 1;

    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = scene->swapchain_fmt;

    /* Opaque pipeline: cull back, depth test+write */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = model_vs;
        pi.fragment_shader    = model_fs;
        pi.vertex_input_state = model_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->model_pipeline = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Blend pipeline: alpha blend, no depth write */
    {
        SDL_GPUColorTargetDescription blend_ctd;
        SDL_zero(blend_ctd);
        blend_ctd.format = scene->swapchain_fmt;
        blend_ctd.blend_state.enable_blend = true;
        blend_ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend_ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend_ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend_ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = model_vs;
        pi.fragment_shader    = model_fs;
        pi.vertex_input_state = model_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions = &blend_ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->model_pipeline_blend = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Double-sided pipeline: cull none, depth test+write */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = model_vs;
        pi.fragment_shader    = model_fs;
        pi.vertex_input_state = model_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->model_pipeline_double = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Blend + double-sided pipeline: alpha blend, cull none, no depth write */
    {
        SDL_GPUColorTargetDescription blend_double_ctd;
        SDL_zero(blend_double_ctd);
        blend_double_ctd.format = scene->swapchain_fmt;
        blend_double_ctd.blend_state.enable_blend = true;
        blend_double_ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend_double_ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_double_ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend_double_ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend_double_ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_double_ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = model_vs;
        pi.fragment_shader    = model_fs;
        pi.vertex_input_state = model_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions = &blend_double_ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->model_pipeline_blend_double = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Shadow pipeline: depth-only, 48-byte stride, cull none, depth bias */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = mshadow_vs;
        pi.fragment_shader    = mshadow_fs;
        pi.vertex_input_state = shadow_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor = FORGE_SCENE_SHADOW_BIAS_CONST;
        pi.rasterizer_state.depth_bias_slope_factor    = FORGE_SCENE_SHADOW_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.num_color_targets        = 0;
        pi.target_info.depth_stencil_format     = scene->shadow_fmt;
        pi.target_info.has_depth_stencil_target = true;
        scene->model_shadow_pipeline = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Release shaders */
    SDL_ReleaseGPUShader(scene->device, model_vs);
    SDL_ReleaseGPUShader(scene->device, model_fs);
    SDL_ReleaseGPUShader(scene->device, mshadow_vs);
    SDL_ReleaseGPUShader(scene->device, mshadow_fs);

    if (!scene->model_pipeline || !scene->model_pipeline_blend ||
        !scene->model_pipeline_double || !scene->model_pipeline_blend_double ||
        !scene->model_shadow_pipeline) {
        SDL_Log("forge_scene: model pipeline creation failed: %s", SDL_GetError());
        goto init_model_fail;
    }

    /* Texture sampler: linear, wrap (for diffuse/emissive/mr/occlusion) */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_LINEAR;
        si.mag_filter     = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.max_anisotropy = 1;
        scene->model_tex_sampler = SDL_CreateGPUSampler(scene->device, &si);
        /* Same settings for normal maps */
        scene->model_normal_sampler = SDL_CreateGPUSampler(scene->device, &si);
    }

    /* Shadow comparison sampler for PCF */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_LINEAR;
        si.mag_filter     = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.enable_compare = true;
        si.compare_op     = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        scene->model_shadow_cmp_sampler = SDL_CreateGPUSampler(scene->device, &si);
    }

    if (!scene->model_tex_sampler || !scene->model_normal_sampler ||
        !scene->model_shadow_cmp_sampler) {
        SDL_Log("forge_scene: model sampler creation failed: %s", SDL_GetError());
        goto init_model_fail;
    }

    /* Fallback textures */
    scene->model_white_texture = forge_scene__create_1x1(
        scene->device, 255, 255, 255, 255, true);
    scene->model_flat_normal = forge_scene__create_1x1(
        scene->device, 128, 128, 255, 255, false);
    scene->model_black_texture = forge_scene__create_1x1(
        scene->device, 0, 0, 0, 255, true);

    if (!scene->model_white_texture || !scene->model_flat_normal ||
        !scene->model_black_texture) {
        SDL_Log("forge_scene: fallback texture creation failed");
        goto init_model_fail;
    }

    scene->model_pipelines_ready = true;
    return true;

init_model_fail:
    /* Release any partially-created resources so a retry does not leak */
    if (scene->model_pipeline)
        { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline); scene->model_pipeline = NULL; }
    if (scene->model_pipeline_blend)
        { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline_blend); scene->model_pipeline_blend = NULL; }
    if (scene->model_pipeline_double)
        { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline_double); scene->model_pipeline_double = NULL; }
    if (scene->model_pipeline_blend_double)
        { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_pipeline_blend_double); scene->model_pipeline_blend_double = NULL; }
    if (scene->model_shadow_pipeline)
        { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_shadow_pipeline); scene->model_shadow_pipeline = NULL; }
    if (scene->model_tex_sampler)
        { SDL_ReleaseGPUSampler(scene->device, scene->model_tex_sampler); scene->model_tex_sampler = NULL; }
    if (scene->model_normal_sampler)
        { SDL_ReleaseGPUSampler(scene->device, scene->model_normal_sampler); scene->model_normal_sampler = NULL; }
    if (scene->model_shadow_cmp_sampler)
        { SDL_ReleaseGPUSampler(scene->device, scene->model_shadow_cmp_sampler); scene->model_shadow_cmp_sampler = NULL; }
    if (scene->model_white_texture)
        { SDL_ReleaseGPUTexture(scene->device, scene->model_white_texture); scene->model_white_texture = NULL; }
    if (scene->model_flat_normal)
        { SDL_ReleaseGPUTexture(scene->device, scene->model_flat_normal); scene->model_flat_normal = NULL; }
    if (scene->model_black_texture)
        { SDL_ReleaseGPUTexture(scene->device, scene->model_black_texture); scene->model_black_texture = NULL; }
    return false;
}

/* ── Load model ─────────────────────────────────────────────────────────── */

/* Extract just the filename from a path (everything after the last / or \). */
static const char *forge_scene__basename(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

static bool forge_scene_load_model(ForgeScene *scene,
                                    ForgeSceneModel *model,
                                    const char *fscene_path,
                                    const char *fmesh_path,
                                    const char *fmat_path,
                                    const char *base_dir)
{
    if (!scene || !model) {
        SDL_Log("forge_scene: load_model: NULL scene or model");
        return false;
    }
    if (!fscene_path || !fmesh_path || !fmat_path || !base_dir) {
        SDL_Log("forge_scene: load_model: NULL path argument");
        return false;
    }
    if (base_dir[0] == '\0') {
        SDL_Log("forge_scene: load_model: empty base_dir");
        return false;
    }

    SDL_memset(model, 0, sizeof(*model));

    /* Lazy-init model pipelines */
    if (!forge_scene_init_model_pipelines(scene)) return false;

    /* Load pipeline data */
    if (!forge_pipeline_load_scene(fscene_path, &model->scene_data)) {
        SDL_Log("forge_scene: load_model: failed to load scene '%s'", fscene_path);
        return false;
    }
    if (!forge_pipeline_load_mesh(fmesh_path, &model->mesh)) {
        SDL_Log("forge_scene: load_model: failed to load mesh '%s'", fmesh_path);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }
    if (!forge_pipeline_load_materials(fmat_path, &model->materials)) {
        SDL_Log("forge_scene: load_model: failed to load materials '%s'", fmat_path);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }

    /* Validate asset compatibility — reject early rather than drawing
     * with a mismatched vertex layout or too many materials. */
    if (model->mesh.vertex_stride != sizeof(ForgeSceneModelVertex) ||
        (model->mesh.flags & FORGE_PIPELINE_FLAG_TANGENTS) == 0 ||
        (model->mesh.flags & FORGE_PIPELINE_FLAG_SKINNED) != 0) {
        SDL_Log("forge_scene: load_model: '%s' has unsupported vertex layout "
                "(stride=%u, flags=0x%x)", fmesh_path,
                model->mesh.vertex_stride, model->mesh.flags);
        forge_pipeline_free_materials(&model->materials);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }
    if (model->materials.material_count > FORGE_SCENE_MODEL_MAX_MATERIALS) {
        SDL_Log("forge_scene: load_model: '%s' has %u materials; max supported is %u",
                fmat_path, model->materials.material_count,
                (unsigned)FORGE_SCENE_MODEL_MAX_MATERIALS);
        forge_pipeline_free_materials(&model->materials);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }

    /* Upload vertex buffer */
    {
        uint64_t vb_size_64 =
            (uint64_t)model->mesh.vertex_count * model->mesh.vertex_stride;
        if (vb_size_64 > (uint64_t)UINT32_MAX) {
            SDL_Log("forge_scene: load_model: vertex buffer size exceeds "
                    "UINT32_MAX (%" SDL_PRIu64 " bytes)", vb_size_64);
            goto fail;
        }
        Uint32 vb_size = (Uint32)vb_size_64;
        model->vertex_buffer = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_VERTEX, model->mesh.vertices, vb_size);
        if (!model->vertex_buffer) {
            SDL_Log("forge_scene: load_model: vertex buffer upload failed");
            goto fail;
        }
    }

    /* Upload index buffer (LOD 0 — all indices) */
    {
        uint64_t total_indices = 0;
        for (uint32_t s = 0; s < model->mesh.submesh_count; s++) {
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, s);
            if (sub) total_indices += sub->index_count;
        }
        uint64_t ib_size_64 = total_indices * sizeof(uint32_t);
        if (ib_size_64 > (uint64_t)UINT32_MAX) {
            SDL_Log("forge_scene: load_model: index buffer size "
                    "exceeds UINT32_MAX (%" SDL_PRIu64 " bytes)", ib_size_64);
            goto fail;
        }
        if (total_indices > 0 && model->mesh.indices &&
            model->mesh.lod_count > 0) {
            /* Use the LOD 0 index data — starts at lods[0].index_offset */
            Uint32 lod0_offset = model->mesh.lods[0].index_offset;
            const uint8_t *idx_start =
                (const uint8_t *)model->mesh.indices + lod0_offset;
            Uint32 ib_size = (Uint32)ib_size_64;
            model->index_buffer = forge_scene_upload_buffer(scene,
                SDL_GPU_BUFFERUSAGE_INDEX, idx_start, ib_size);
        }
        if (!model->index_buffer) {
            SDL_Log("forge_scene: load_model: index buffer upload failed");
            goto fail;
        }
    }

    /* Load per-material textures.
     * Uses forge_scene_load_pipeline_texture which checks for .meta.json
     * sidecars with compression info and loads .ftex (pre-transcoded BC7/BC5)
     * when available, falling back to SDL_LoadSurface + GPU mipmaps otherwise. */
    {
        uint32_t mat_count = model->materials.material_count;
        /* mat_count is guaranteed <= FORGE_SCENE_MODEL_MAX_MATERIALS
         * by the validation check above. */
        model->mat_texture_count = mat_count;

        for (uint32_t i = 0; i < mat_count; i++) {
            const ForgePipelineMaterial *mat = &model->materials.materials[i];
            char path_buf[FORGE_SCENE_PATH_BUF_SIZE];

            /* Base color texture — .fmat stores a relative path; join
             * with base_dir to preserve subdirectory structure. */
            if (mat->base_color_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->base_color_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].base_color =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, true, false);
            }

            /* Normal texture (is_normal_map=true selects BC5) */
            if (mat->normal_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->normal_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].normal =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, false, true);
            }

            /* Metallic-roughness texture */
            if (mat->metallic_roughness_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir,
                                       mat->metallic_roughness_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].metallic_roughness =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, false, false);
            }

            /* Occlusion texture */
            if (mat->occlusion_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->occlusion_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].occlusion =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, false, false);
            }

            /* Emissive texture */
            if (mat->emissive_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->emissive_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].emissive =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, true, false);
            }
        }
    }

    SDL_Log("forge_scene: loaded model: %u nodes, %u meshes, %u materials, "
            "%u vertices, %u submeshes, %u/%u textures compressed",
            model->scene_data.node_count, model->scene_data.mesh_count,
            model->materials.material_count, model->mesh.vertex_count,
            model->mesh.submesh_count,
            model->vram.compressed_texture_count,
            model->vram.total_texture_count);
    return true;

fail:
    forge_scene_free_model(scene, model);
    return false;
}

/* ── Shared submesh draw helpers ──────────────────────────────────────── */

/* Bind the 6 texture+sampler slots used by all model pipelines.
 * Precondition: scene->pass must be non-NULL (callers validate this). */
static void forge_scene__bind_model_textures(
    ForgeScene *scene, const ForgeSceneModelTextures *textures)
{
    SDL_GPUTextureSamplerBinding tex_bindings[6];

    /* Slot 0: base color */
    tex_bindings[0].texture = (textures && textures->base_color)
        ? textures->base_color : scene->model_white_texture;
    tex_bindings[0].sampler = scene->model_tex_sampler;

    /* Slot 1: normal map */
    tex_bindings[1].texture = (textures && textures->normal)
        ? textures->normal : scene->model_flat_normal;
    tex_bindings[1].sampler = scene->model_normal_sampler;

    /* Slot 2: metallic-roughness */
    tex_bindings[2].texture = (textures && textures->metallic_roughness)
        ? textures->metallic_roughness : scene->model_white_texture;
    tex_bindings[2].sampler = scene->model_tex_sampler;

    /* Slot 3: occlusion */
    tex_bindings[3].texture = (textures && textures->occlusion)
        ? textures->occlusion : scene->model_white_texture;
    tex_bindings[3].sampler = scene->model_tex_sampler;

    /* Slot 4: emissive */
    tex_bindings[4].texture = (textures && textures->emissive)
        ? textures->emissive : scene->model_black_texture;
    tex_bindings[4].sampler = scene->model_tex_sampler;

    /* Slot 5: shadow map (comparison sampler) */
    tex_bindings[5].texture = scene->shadow_map;
    tex_bindings[5].sampler = scene->model_shadow_cmp_sampler;

    SDL_BindGPUFragmentSamplers(scene->pass, 0, tex_bindings, 6);
}

/* Fill fragment uniforms from scene config + material. */
static void forge_scene__fill_model_frag_uniforms(
    const ForgeScene *scene, const ForgePipelineMaterial *mat,
    ForgeSceneModelFragUniforms *fu)
{
    SDL_memset(fu, 0, sizeof(*fu));
    fu->light_dir[0] = scene->light_dir.x;
    fu->light_dir[1] = scene->light_dir.y;
    fu->light_dir[2] = scene->light_dir.z;
    fu->eye_pos[0]   = scene->cam_position.x;
    fu->eye_pos[1]   = scene->cam_position.y;
    fu->eye_pos[2]   = scene->cam_position.z;

    if (mat) {
        fu->base_color_factor[0] = mat->base_color_factor[0];
        fu->base_color_factor[1] = mat->base_color_factor[1];
        fu->base_color_factor[2] = mat->base_color_factor[2];
        fu->base_color_factor[3] = mat->base_color_factor[3];
        fu->emissive_factor[0]   = mat->emissive_factor[0];
        fu->emissive_factor[1]   = mat->emissive_factor[1];
        fu->emissive_factor[2]   = mat->emissive_factor[2];
        fu->metallic_factor      = mat->metallic_factor;
        fu->roughness_factor     = mat->roughness_factor;
        fu->normal_scale         = mat->normal_scale;
        fu->occlusion_strength   = mat->occlusion_strength;
        fu->alpha_cutoff = (mat->alpha_mode == FORGE_PIPELINE_ALPHA_MASK)
            ? mat->alpha_cutoff : 0.0f;
    } else {
        /* Default material: white, fully rough, no metal */
        fu->base_color_factor[0] = 1.0f;
        fu->base_color_factor[1] = 1.0f;
        fu->base_color_factor[2] = 1.0f;
        fu->base_color_factor[3] = 1.0f;
        fu->metallic_factor  = 0.0f;
        fu->roughness_factor = 1.0f;
        fu->normal_scale     = 1.0f;
        fu->occlusion_strength = 1.0f;
    }
    fu->shadow_texel = 1.0f / (float)scene->config.shadow_map_size;
    fu->shininess    = scene->config.shininess;
    fu->specular_str = scene->config.specular_str;
    fu->ambient      = scene->config.ambient;
}

/* ── Draw model ─────────────────────────────────────────────────────────── */

static void forge_scene_draw_model(ForgeScene *scene,
                                    ForgeSceneModel *model,
                                    mat4 placement)
{
    if (!scene || !model || !scene->pass) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0) return;

    model->draw_calls = 0;

    /* LOD 0 base index offset (in bytes) — we uploaded from this offset,
     * so GPU-side indices start at 0 relative to our uploaded buffer. */
    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    /* Bind vertex + index buffers once for all submeshes */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUGraphicsPipeline *last_pipeline = NULL; /* track to skip redundant binds */

    for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
        const ForgePipelineSceneNode *node = &model->scene_data.nodes[n];
        if (node->mesh_index < 0) continue;  /* transform-only node */

        /* Compose placement × node world transform */
        mat4 node_world;
        SDL_memcpy(&node_world, node->world_transform, sizeof(mat4));
        mat4 final_world = mat4_multiply(placement, node_world);

        /* Look up submesh range for this mesh */
        const ForgePipelineSceneMesh *smesh =
            forge_pipeline_scene_get_mesh(&model->scene_data,
                                          (uint32_t)node->mesh_index);
        if (!smesh) continue;

        for (uint32_t si = 0; si < smesh->submesh_count; si++) {
            uint32_t submesh_idx = smesh->first_submesh + si;
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, submesh_idx);
            if (!sub || sub->index_count == 0) continue;

            /* Select material */
            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count &&
                (uint32_t)sub->material_index < model->mat_texture_count) {
                mat = &model->materials.materials[sub->material_index];
                textures = &model->mat_textures[sub->material_index];
            }

            /* Select pipeline variant */
            SDL_GPUGraphicsPipeline *pipeline;
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND && mat->double_sided)
                pipeline = scene->model_pipeline_blend_double;
            else if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND)
                pipeline = scene->model_pipeline_blend;
            else if (mat && mat->double_sided)
                pipeline = scene->model_pipeline_double;
            else
                pipeline = scene->model_pipeline;

            if (pipeline != last_pipeline) {
                SDL_BindGPUGraphicsPipeline(scene->pass, pipeline);
                last_pipeline = pipeline;
            }

            forge_scene__bind_model_textures(scene, textures);

            /* Vertex uniforms */
            ForgeSceneVertUniforms vu;
            vu.mvp      = mat4_multiply(scene->cam_vp, final_world);
            vu.model    = final_world;
            vu.light_vp = mat4_multiply(scene->light_vp, final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

            /* Fragment uniforms */
            ForgeSceneModelFragUniforms fu;
            forge_scene__fill_model_frag_uniforms(scene, mat, &fu);
            SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

            /* Draw indexed: offset into our uploaded index buffer */
            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
            model->draw_calls++;
        }
    }
}

/* ── Draw model shadows ─────────────────────────────────────────────────── */

static void forge_scene_draw_model_shadows(ForgeScene *scene,
                                            ForgeSceneModel *model,
                                            mat4 placement)
{
    if (!scene || !model || !scene->pass) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->model_shadow_pipeline);

    /* Bind vertex + index buffers */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
        const ForgePipelineSceneNode *node = &model->scene_data.nodes[n];
        if (node->mesh_index < 0) continue;

        mat4 node_world;
        SDL_memcpy(&node_world, node->world_transform, sizeof(mat4));
        mat4 final_world = mat4_multiply(placement, node_world);

        const ForgePipelineSceneMesh *smesh =
            forge_pipeline_scene_get_mesh(&model->scene_data,
                                          (uint32_t)node->mesh_index);
        if (!smesh) continue;

        for (uint32_t si = 0; si < smesh->submesh_count; si++) {
            uint32_t submesh_idx = smesh->first_submesh + si;
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, submesh_idx);
            if (!sub || sub->index_count == 0) continue;

            /* Skip non-opaque submeshes — transparent materials should not
             * cast shadows, and alpha-masked materials need a mask-aware
             * shadow shader (planned for Lesson 44). */
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count) {
                int amode = model->materials.materials[sub->material_index].alpha_mode;
                if (amode == FORGE_PIPELINE_ALPHA_BLEND ||
                    amode == FORGE_PIPELINE_ALPHA_MASK)
                    continue;
            }

            ForgeSceneShadowVertUniforms su;
            su.light_vp = mat4_multiply(scene->light_vp, final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
        }
    }
}

/* ── Free model ─────────────────────────────────────────────────────────── */

static void forge_scene_free_model(ForgeScene *scene,
                                    ForgeSceneModel *model)
{
    if (!model) return;
    if (!scene || !scene->device) {
        /* Can't release GPU resources without a device */
        forge_pipeline_free_scene(&model->scene_data);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_materials(&model->materials);
        SDL_memset(model, 0, sizeof(*model));
        return;
    }

    /* Release GPU buffers */
    if (model->vertex_buffer)
        SDL_ReleaseGPUBuffer(scene->device, model->vertex_buffer);
    if (model->index_buffer)
        SDL_ReleaseGPUBuffer(scene->device, model->index_buffer);

    /* Release per-material textures (skip fallbacks owned by scene) */
    for (uint32_t i = 0; i < model->mat_texture_count; i++) {
        ForgeSceneModelTextures *t = &model->mat_textures[i];
        if (t->base_color && t->base_color != scene->model_white_texture)
            SDL_ReleaseGPUTexture(scene->device, t->base_color);
        if (t->normal && t->normal != scene->model_flat_normal)
            SDL_ReleaseGPUTexture(scene->device, t->normal);
        if (t->metallic_roughness && t->metallic_roughness != scene->model_white_texture)
            SDL_ReleaseGPUTexture(scene->device, t->metallic_roughness);
        if (t->occlusion && t->occlusion != scene->model_white_texture)
            SDL_ReleaseGPUTexture(scene->device, t->occlusion);
        if (t->emissive && t->emissive != scene->model_black_texture)
            SDL_ReleaseGPUTexture(scene->device, t->emissive);
    }

    /* Free pipeline data */
    forge_pipeline_free_scene(&model->scene_data);
    forge_pipeline_free_mesh(&model->mesh);
    forge_pipeline_free_materials(&model->materials);

    SDL_memset(model, 0, sizeof(*model));
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Skinned model support
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Init skinned pipelines (lazy, called on first skinned model load) ── */

static bool forge_scene_init_skinned_pipelines(ForgeScene *scene)
{
    if (scene->skinned_pipelines_ready) return true;

    /* Ensure model pipelines are ready (we reuse samplers, fallback textures) */
    if (!forge_scene_init_model_pipelines(scene)) return false;

    /* Create skinned vertex shader — 1 storage buffer (joints), 1 uniform */
    SDL_GPUShader *skinned_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_skinned_vert_spirv, sizeof(scene_skinned_vert_spirv),
        scene_skinned_vert_dxil,  sizeof(scene_skinned_vert_dxil),
        scene_skinned_vert_msl, scene_skinned_vert_msl_size,
        0, 0, 1, 1);  /* 0 samplers, 0 storage_tex, 1 storage_buf, 1 uniform */

    /* Reuse model fragment shader */
    SDL_GPUShader *skinned_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_model_frag_spirv, sizeof(scene_model_frag_spirv),
        scene_model_frag_dxil,  sizeof(scene_model_frag_dxil),
        scene_model_frag_msl, scene_model_frag_msl_size,
        6, 0, 0, 1);

    /* Skinned shadow vertex shader — 1 storage buffer, 1 uniform */
    SDL_GPUShader *skinned_shadow_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_skinned_shadow_vert_spirv, sizeof(scene_skinned_shadow_vert_spirv),
        scene_skinned_shadow_vert_dxil,  sizeof(scene_skinned_shadow_vert_dxil),
        scene_skinned_shadow_vert_msl, scene_skinned_shadow_vert_msl_size,
        0, 0, 1, 1);

    /* Reuse existing shadow fragment shader */
    SDL_GPUShader *skinned_shadow_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl, shadow_frag_msl_size,
        0, 0, 0, 0);

    if (!skinned_vs || !skinned_fs || !skinned_shadow_vs || !skinned_shadow_fs) {
        SDL_Log("forge_scene: skinned shader creation failed");
        if (skinned_vs)        SDL_ReleaseGPUShader(scene->device, skinned_vs);
        if (skinned_fs)        SDL_ReleaseGPUShader(scene->device, skinned_fs);
        if (skinned_shadow_vs) SDL_ReleaseGPUShader(scene->device, skinned_shadow_vs);
        if (skinned_shadow_fs) SDL_ReleaseGPUShader(scene->device, skinned_shadow_fs);
        return false;
    }

    /* 72-byte vertex input: pos + normal + uv + tangent + joints + weights */
    SDL_GPUVertexBufferDescription skinned_vbd;
    SDL_zero(skinned_vbd);
    skinned_vbd.slot       = 0;
    skinned_vbd.pitch      = FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN;
    skinned_vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute skinned_attrs[6];
    SDL_zero(skinned_attrs);
    skinned_attrs[0].location = 0; skinned_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;  skinned_attrs[0].offset = 0;
    skinned_attrs[1].location = 1; skinned_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;  skinned_attrs[1].offset = 12;
    skinned_attrs[2].location = 2; skinned_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;  skinned_attrs[2].offset = 24;
    skinned_attrs[3].location = 3; skinned_attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;  skinned_attrs[3].offset = 32;
    skinned_attrs[4].location = 4; skinned_attrs[4].format = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4; skinned_attrs[4].offset = 48;
    skinned_attrs[5].location = 5; skinned_attrs[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;  skinned_attrs[5].offset = 56;

    SDL_GPUVertexInputState skinned_input;
    SDL_zero(skinned_input);
    skinned_input.vertex_buffer_descriptions = &skinned_vbd;
    skinned_input.num_vertex_buffers         = 1;
    skinned_input.vertex_attributes          = skinned_attrs;
    skinned_input.num_vertex_attributes      = 6;

    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = scene->swapchain_fmt;

    /* Opaque pipeline: cull back, depth test+write */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = skinned_vs;
        pi.fragment_shader    = skinned_fs;
        pi.vertex_input_state = skinned_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->skinned_pipeline = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Blend pipeline: alpha blend, no depth write */
    {
        SDL_GPUColorTargetDescription blend_ctd;
        SDL_zero(blend_ctd);
        blend_ctd.format = scene->swapchain_fmt;
        blend_ctd.blend_state.enable_blend = true;
        blend_ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend_ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend_ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend_ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = skinned_vs;
        pi.fragment_shader    = skinned_fs;
        pi.vertex_input_state = skinned_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions = &blend_ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->skinned_pipeline_blend = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Double-sided pipeline: cull none, depth test+write */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = skinned_vs;
        pi.fragment_shader    = skinned_fs;
        pi.vertex_input_state = skinned_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions = &ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->skinned_pipeline_double = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Blend + double-sided */
    {
        SDL_GPUColorTargetDescription blend_double_ctd;
        SDL_zero(blend_double_ctd);
        blend_double_ctd.format = scene->swapchain_fmt;
        blend_double_ctd.blend_state.enable_blend = true;
        blend_double_ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend_double_ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_double_ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend_double_ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend_double_ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_double_ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = skinned_vs;
        pi.fragment_shader    = skinned_fs;
        pi.vertex_input_state = skinned_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.target_info.color_target_descriptions = &blend_double_ctd;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.depth_stencil_format      = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->skinned_pipeline_blend_double = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Shadow pipeline: depth-only, 72-byte stride, cull none, depth bias */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = skinned_shadow_vs;
        pi.fragment_shader    = skinned_shadow_fs;
        pi.vertex_input_state = skinned_input;
        pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor = FORGE_SCENE_SHADOW_BIAS_CONST;
        pi.rasterizer_state.depth_bias_slope_factor    = FORGE_SCENE_SHADOW_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op        = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.num_color_targets        = 0;
        pi.target_info.depth_stencil_format     = scene->shadow_fmt;
        pi.target_info.has_depth_stencil_target = true;
        scene->skinned_shadow_pipeline = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Release shaders */
    SDL_ReleaseGPUShader(scene->device, skinned_vs);
    SDL_ReleaseGPUShader(scene->device, skinned_fs);
    SDL_ReleaseGPUShader(scene->device, skinned_shadow_vs);
    SDL_ReleaseGPUShader(scene->device, skinned_shadow_fs);

    if (!scene->skinned_pipeline || !scene->skinned_pipeline_blend ||
        !scene->skinned_pipeline_double || !scene->skinned_pipeline_blend_double ||
        !scene->skinned_shadow_pipeline) {
        SDL_Log("forge_scene: skinned pipeline creation failed: %s", SDL_GetError());
        if (scene->skinned_pipeline)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline); scene->skinned_pipeline = NULL; }
        if (scene->skinned_pipeline_blend)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline_blend); scene->skinned_pipeline_blend = NULL; }
        if (scene->skinned_pipeline_double)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline_double); scene->skinned_pipeline_double = NULL; }
        if (scene->skinned_pipeline_blend_double)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_pipeline_blend_double); scene->skinned_pipeline_blend_double = NULL; }
        if (scene->skinned_shadow_pipeline)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_shadow_pipeline); scene->skinned_shadow_pipeline = NULL; }
        return false;
    }

    scene->skinned_pipelines_ready = true;
    return true;
}

/* ── Load skinned model ──────────────────────────────────────────────────── */

static bool forge_scene_load_skinned_model(
    ForgeScene *scene, ForgeSceneSkinnedModel *model,
    const char *fscene_path, const char *fmesh_path,
    const char *fmat_path,   const char *fskin_path,
    const char *fanim_path,  const char *base_dir)
{
    if (!scene || !model) {
        SDL_Log("forge_scene: load_skinned_model: NULL scene or model");
        return false;
    }
    if (!fscene_path || !fmesh_path || !fmat_path || !base_dir) {
        SDL_Log("forge_scene: load_skinned_model: NULL required path");
        return false;
    }
    if (base_dir[0] == '\0') {
        SDL_Log("forge_scene: load_skinned_model: empty base_dir");
        return false;
    }

    SDL_memset(model, 0, sizeof(*model));
    model->skinned_mesh_node = -1; /* -1 = no skinned mesh node (set during skin load) */
    model->anim_speed = 1.0f;
    model->looping = true;

    /* Lazy-init skinned pipelines */
    if (!forge_scene_init_skinned_pipelines(scene)) return false;

    /* Load pipeline data */
    if (!forge_pipeline_load_scene(fscene_path, &model->scene_data)) {
        SDL_Log("forge_scene: load_skinned_model: failed to load scene '%s'",
                fscene_path);
        return false;
    }
    if (!forge_pipeline_load_mesh(fmesh_path, &model->mesh)) {
        SDL_Log("forge_scene: load_skinned_model: failed to load mesh '%s'",
                fmesh_path);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }
    if (!forge_pipeline_load_materials(fmat_path, &model->materials)) {
        SDL_Log("forge_scene: load_skinned_model: failed to load materials '%s'",
                fmat_path);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }

    /* Validate: must be skinned 72-byte vertices */
    if (model->mesh.vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN ||
        (model->mesh.flags & FORGE_PIPELINE_FLAG_SKINNED) == 0 ||
        (model->mesh.flags & FORGE_PIPELINE_FLAG_TANGENTS) == 0) {
        SDL_Log("forge_scene: load_skinned_model: '%s' has unsupported vertex "
                "layout (stride=%u, flags=0x%x) — expected skinned+tangent",
                fmesh_path, model->mesh.vertex_stride, model->mesh.flags);
        forge_pipeline_free_materials(&model->materials);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }
    if (model->materials.material_count > FORGE_SCENE_MODEL_MAX_MATERIALS) {
        SDL_Log("forge_scene: load_skinned_model: too many materials (%u, max %u)",
                model->materials.material_count,
                (unsigned)FORGE_SCENE_MODEL_MAX_MATERIALS);
        forge_pipeline_free_materials(&model->materials);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }

    /* Load skins (required if path provided — caller explicitly asked for skinning) */
    if (fskin_path) {
        if (!forge_pipeline_load_skins(fskin_path, &model->skins)) {
            SDL_Log("forge_scene: load_skinned_model: failed to load skins '%s'",
                    fskin_path);
            goto fail;
        } else if (model->skins.skin_count != 1) {
            SDL_Log("forge_scene: load_skinned_model: expected exactly 1 skin, "
                    "got %u in '%s'", model->skins.skin_count, fskin_path);
            goto fail;
        } else {
            /* Validate single skinned mesh node — the runtime builds one
             * joint palette and reuses it for all draws, so multiple skinned
             * mesh nodes with different world transforms would deform
             * incorrectly. */
            uint32_t skinned_mesh_nodes = 0;
            model->skinned_mesh_node = -1;
            for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
                const ForgePipelineSceneNode *nd = &model->scene_data.nodes[n];
                if (nd->mesh_index < 0) continue;

                if (nd->skin_index < 0) {
                    /* Rigid mesh node in a skinned model — the draw loops
                     * route all mesh nodes through skinned pipelines, so a
                     * rigid node would be deformed by the joint palette. */
                    SDL_Log("forge_scene: load_skinned_model: node %u has "
                            "mesh_index %d but skin_index %d — mixed "
                            "rigid+skinned models not supported",
                            n, nd->mesh_index, nd->skin_index);
                    goto fail;
                }
                if (nd->skin_index != 0) {
                    SDL_Log("forge_scene: load_skinned_model: node %u has "
                            "skin_index %d, but single-palette runtime "
                            "requires skin_index=0",
                            n, nd->skin_index);
                    goto fail;
                }
                model->skinned_mesh_node = (int)n;
                skinned_mesh_nodes++;
            }
            if (skinned_mesh_nodes != 1) {
                SDL_Log("forge_scene: load_skinned_model: expected 1 skinned mesh "
                        "node, got %u (0 = no skinned geometry, >1 = multi-palette "
                        "not supported)", skinned_mesh_nodes);
                goto fail;
            }
        }
    }

    /* Load animations (required if path provided — caller explicitly asked for it) */
    if (fanim_path) {
        if (!forge_pipeline_load_animation(fanim_path, &model->animations)) {
            SDL_Log("forge_scene: load_skinned_model: failed to load anim '%s'",
                    fanim_path);
            goto fail;
        }
    }

    /* Upload vertex buffer */
    {
        uint64_t vb_size_64 =
            (uint64_t)model->mesh.vertex_count * model->mesh.vertex_stride;
        if (vb_size_64 > (uint64_t)UINT32_MAX) {
            SDL_Log("forge_scene: load_skinned_model: vertex buffer size "
                    "exceeds UINT32_MAX (%" SDL_PRIu64 " bytes)", vb_size_64);
            goto fail;
        }
        Uint32 vb_size = (Uint32)vb_size_64;
        model->vertex_buffer = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_VERTEX, model->mesh.vertices, vb_size);
        if (!model->vertex_buffer) {
            SDL_Log("forge_scene: load_skinned_model: vertex buffer upload failed");
            goto fail;
        }
    }

    /* Upload index buffer (LOD 0) */
    {
        uint64_t total_indices = 0;
        for (uint32_t s = 0; s < model->mesh.submesh_count; s++) {
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, s);
            if (sub) total_indices += sub->index_count;
        }
        uint64_t ib_size_64 = total_indices * sizeof(uint32_t);
        if (ib_size_64 > (uint64_t)UINT32_MAX) {
            SDL_Log("forge_scene: load_skinned_model: index buffer size "
                    "exceeds UINT32_MAX (%" SDL_PRIu64 " bytes)", ib_size_64);
            goto fail;
        }
        if (total_indices > 0 && model->mesh.indices &&
            model->mesh.lod_count > 0) {
            Uint32 lod0_offset = model->mesh.lods[0].index_offset;
            const uint8_t *idx_start =
                (const uint8_t *)model->mesh.indices + lod0_offset;
            Uint32 ib_size = (Uint32)ib_size_64;
            model->index_buffer = forge_scene_upload_buffer(scene,
                SDL_GPU_BUFFERUSAGE_INDEX, idx_start, ib_size);
        }
        if (!model->index_buffer) {
            SDL_Log("forge_scene: load_skinned_model: index buffer upload failed");
            goto fail;
        }
    }

    /* Create joint storage buffer (identity-filled initially).
     * Reuses forge_scene_upload_buffer() instead of manual transfer. */
    {
        for (uint32_t i = 0; i < FORGE_PIPELINE_MAX_SKIN_JOINTS; i++)
            model->joint_matrices[i] = mat4_identity();

        model->joint_buffer = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            model->joint_matrices, FORGE_SCENE_JOINT_BUFFER_SIZE);
        if (!model->joint_buffer) {
            SDL_Log("forge_scene: load_skinned_model: joint buffer upload failed");
            goto fail;
        }
    }

    /* Create persistent transfer buffer for per-frame joint uploads.
     * Allocated once here, reused each frame in update_skinned_animation. */
    {
        SDL_GPUTransferBufferCreateInfo tbci;
        SDL_zero(tbci);
        tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbci.size  = FORGE_SCENE_JOINT_BUFFER_SIZE;
        model->joint_transfer_buffer =
            SDL_CreateGPUTransferBuffer(scene->device, &tbci);
        if (!model->joint_transfer_buffer) {
            SDL_Log("forge_scene: load_skinned_model: joint transfer buffer "
                    "creation failed: %s", SDL_GetError());
            goto fail;
        }
    }

    /* Load per-material textures — pass &model->vram directly for
     * VRAM tracking (no temporary wrapper needed). */
    {
        uint32_t mat_count = model->materials.material_count;
        model->mat_texture_count = mat_count;

        for (uint32_t i = 0; i < mat_count; i++) {
            const ForgePipelineMaterial *mat = &model->materials.materials[i];
            char path_buf[FORGE_SCENE_PATH_BUF_SIZE];

            if (mat->base_color_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->base_color_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].base_color =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, true, false);
            }
            if (mat->normal_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->normal_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].normal =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, false, true);
            }
            if (mat->metallic_roughness_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->metallic_roughness_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].metallic_roughness =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, false, false);
            }
            if (mat->occlusion_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->occlusion_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].occlusion =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, false, false);
            }
            if (mat->emissive_texture[0]) {
                int len = SDL_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                                       base_dir, mat->emissive_texture);
                if (len >= 0 && len < (int)sizeof(path_buf))
                    model->mat_textures[i].emissive =
                        forge_scene_load_pipeline_texture(
                            scene, &model->vram, path_buf, true, false);
            }
        }
    }

    SDL_Log("forge_scene: loaded skinned model: %u nodes, %u meshes, "
            "%u materials, %u vertices, %u submeshes, %u skins, %u clips, "
            "%u/%u textures compressed",
            model->scene_data.node_count, model->scene_data.mesh_count,
            model->materials.material_count, model->mesh.vertex_count,
            model->mesh.submesh_count,
            model->skins.skin_count,
            model->animations.clip_count,
            model->vram.compressed_texture_count,
            model->vram.total_texture_count);
    return true;

fail:
    forge_scene_free_skinned_model(scene, model);
    return false;
}

/* ── Update skinned animation ────────────────────────────────────────────── */

static void forge_scene_update_skinned_animation(
    ForgeScene *scene, ForgeSceneSkinnedModel *model, float dt)
{
    if (!scene || !model) return;

    /* Advance animation time */
    if (model->animations.clip_count > 0 &&
        model->current_clip >= 0 &&
        (uint32_t)model->current_clip < model->animations.clip_count) {

        model->anim_time += dt * model->anim_speed;

        /* Wrap time to prevent float precision loss after long sessions.
         * forge_pipeline_anim_apply does its own fmodf, but keeping
         * anim_time bounded avoids UI display issues and precision drift. */
        {
            float dur = model->animations.clips[model->current_clip].duration;
            if (model->looping && dur > FORGE_PIPELINE_ANIM_EPSILON) {
                model->anim_time = SDL_fmodf(model->anim_time, dur);
                if (model->anim_time < 0.0f) model->anim_time += dur;
            }
        }

        /* Apply animation to scene nodes */
        forge_pipeline_anim_apply(
            &model->animations.clips[model->current_clip],
            model->scene_data.nodes, model->scene_data.node_count,
            model->anim_time, model->looping);

        /* Recompute world transforms */
        forge_pipeline_scene_compute_world_transforms(
            model->scene_data.nodes, model->scene_data.node_count,
            model->scene_data.roots, model->scene_data.root_count,
            model->scene_data.children, model->scene_data.child_count);
    }

    /* Compute joint matrices — uses first skin only (validated at load time).
     * skinned_mesh_node was cached during load to avoid per-frame linear scan. */
    if (model->skins.skin_count > 0) {
        model->active_joint_count = forge_pipeline_compute_joint_matrices(
            &model->skins.skins[0],
            model->scene_data.nodes, model->scene_data.node_count,
            model->skinned_mesh_node,
            model->joint_matrices, FORGE_PIPELINE_MAX_SKIN_JOINTS);
    }

    /* Upload joint matrices to GPU via persistent transfer buffer.
     * Uses cycle=true to avoid GPU stalls when the previous frame's
     * upload is still in flight. Lazily opens a single copy pass on
     * scene->skinned_copy_pass that is shared across all skinned models
     * and ended in forge_scene_begin_shadow_pass. */
    if (model->joint_buffer && model->joint_transfer_buffer &&
        model->active_joint_count > 0) {
        if (!scene->cmd) {
            SDL_Log("forge_scene: update_skinned_animation: scene->cmd "
                    "is NULL, joint upload skipped (call after "
                    "forge_scene_begin_frame)");
        } else {
            Uint32 upload_size =
                model->active_joint_count * (Uint32)sizeof(mat4);

            void *mapped = SDL_MapGPUTransferBuffer(
                scene->device, model->joint_transfer_buffer, true);
            if (mapped) {
                SDL_memcpy(mapped, model->joint_matrices, upload_size);
                SDL_UnmapGPUTransferBuffer(scene->device,
                                           model->joint_transfer_buffer);

                /* Lazily open a batched copy pass for all skinned models */
                if (!scene->skinned_copy_pass) {
                    scene->skinned_copy_pass =
                        SDL_BeginGPUCopyPass(scene->cmd);
                    if (!scene->skinned_copy_pass) {
                        SDL_Log("forge_scene: joint buffer copy pass "
                                "failed: %s", SDL_GetError());
                    }
                }
                if (scene->skinned_copy_pass) {
                    SDL_GPUTransferBufferLocation src;
                    SDL_zero(src);
                    src.transfer_buffer = model->joint_transfer_buffer;
                    SDL_GPUBufferRegion dst;
                    SDL_zero(dst);
                    dst.buffer = model->joint_buffer;
                    dst.size   = upload_size;
                    SDL_UploadToGPUBuffer(
                        scene->skinned_copy_pass, &src, &dst, true);
                }
            } else {
                SDL_Log("forge_scene: SDL_MapGPUTransferBuffer failed: %s",
                        SDL_GetError());
            }
        }
    }
}

/* ── Draw skinned model ──────────────────────────────────────────────────── */

static void forge_scene_draw_skinned_model(
    ForgeScene *scene, ForgeSceneSkinnedModel *model, mat4 placement)
{
    if (!scene || !model || !scene->pass) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0) return;

    model->draw_calls = 0;

    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    /* Bind vertex + index buffers */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Bind joint storage buffer at vertex stage slot 0 */
    SDL_BindGPUVertexStorageBuffers(scene->pass, 0,
                                    &model->joint_buffer, 1);

    SDL_GPUGraphicsPipeline *last_pipeline = NULL;

    for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
        const ForgePipelineSceneNode *node = &model->scene_data.nodes[n];
        if (node->mesh_index < 0) continue;

        /* placement * node_world positions the mesh; the joint matrices
         * contain inv(mesh_world), so the two cancel in the final chain. */
        mat4 node_world;
        SDL_memcpy(&node_world, node->world_transform, sizeof(mat4));
        mat4 final_world = mat4_multiply(placement, node_world);

        const ForgePipelineSceneMesh *smesh =
            forge_pipeline_scene_get_mesh(&model->scene_data,
                                          (uint32_t)node->mesh_index);
        if (!smesh) continue;

        for (uint32_t si = 0; si < smesh->submesh_count; si++) {
            uint32_t submesh_idx = smesh->first_submesh + si;
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, submesh_idx);
            if (!sub || sub->index_count == 0) continue;

            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count &&
                (uint32_t)sub->material_index < model->mat_texture_count) {
                mat = &model->materials.materials[sub->material_index];
                textures = &model->mat_textures[sub->material_index];
            }

            /* Select pipeline variant */
            SDL_GPUGraphicsPipeline *pipeline;
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND && mat->double_sided)
                pipeline = scene->skinned_pipeline_blend_double;
            else if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND)
                pipeline = scene->skinned_pipeline_blend;
            else if (mat && mat->double_sided)
                pipeline = scene->skinned_pipeline_double;
            else
                pipeline = scene->skinned_pipeline;

            if (pipeline != last_pipeline) {
                SDL_BindGPUGraphicsPipeline(scene->pass, pipeline);
                last_pipeline = pipeline;
            }

            forge_scene__bind_model_textures(scene, textures);

            /* Vertex uniforms */
            ForgeSceneVertUniforms vu;
            vu.mvp      = mat4_multiply(scene->cam_vp, final_world);
            vu.model    = final_world;
            vu.light_vp = mat4_multiply(scene->light_vp, final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

            /* Fragment uniforms */
            ForgeSceneModelFragUniforms fu;
            forge_scene__fill_model_frag_uniforms(scene, mat, &fu);
            SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
            model->draw_calls++;
        }
    }
}

/* ── Draw skinned model shadows ──────────────────────────────────────────── */

static void forge_scene_draw_skinned_model_shadows(
    ForgeScene *scene, ForgeSceneSkinnedModel *model, mat4 placement)
{
    if (!scene || !model || !scene->pass) return;
    if (!scene->skinned_shadow_pipeline) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->skinned_shadow_pipeline);

    /* Bind vertex + index buffers */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Bind joint storage buffer */
    SDL_BindGPUVertexStorageBuffers(scene->pass, 0,
                                    &model->joint_buffer, 1);

    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
        const ForgePipelineSceneNode *node = &model->scene_data.nodes[n];
        if (node->mesh_index < 0) continue;

        mat4 node_world;
        SDL_memcpy(&node_world, node->world_transform, sizeof(mat4));
        mat4 final_world = mat4_multiply(placement, node_world);

        const ForgePipelineSceneMesh *smesh =
            forge_pipeline_scene_get_mesh(&model->scene_data,
                                          (uint32_t)node->mesh_index);
        if (!smesh) continue;

        for (uint32_t si = 0; si < smesh->submesh_count; si++) {
            uint32_t submesh_idx = smesh->first_submesh + si;
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, submesh_idx);
            if (!sub || sub->index_count == 0) continue;

            /* Skip non-opaque submeshes */
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count) {
                int amode = model->materials.materials[sub->material_index].alpha_mode;
                if (amode == FORGE_PIPELINE_ALPHA_BLEND ||
                    amode == FORGE_PIPELINE_ALPHA_MASK)
                    continue;
            }

            ForgeSceneShadowVertUniforms su;
            su.light_vp = mat4_multiply(scene->light_vp, final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
        }
    }
}

/* ── Free skinned model ──────────────────────────────────────────────────── */

static void forge_scene_free_skinned_model(
    ForgeScene *scene, ForgeSceneSkinnedModel *model)
{
    if (!model) return;

    if (scene && scene->device) {
        /* Release GPU buffers */
        if (model->vertex_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->vertex_buffer);
        if (model->index_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->index_buffer);
        if (model->joint_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->joint_buffer);
        if (model->joint_transfer_buffer)
            SDL_ReleaseGPUTransferBuffer(scene->device,
                                         model->joint_transfer_buffer);

        /* Release per-material textures (skip fallbacks owned by scene) */
        for (uint32_t i = 0; i < model->mat_texture_count; i++) {
            ForgeSceneModelTextures *t = &model->mat_textures[i];
            if (t->base_color && t->base_color != scene->model_white_texture)
                SDL_ReleaseGPUTexture(scene->device, t->base_color);
            if (t->normal && t->normal != scene->model_flat_normal)
                SDL_ReleaseGPUTexture(scene->device, t->normal);
            if (t->metallic_roughness && t->metallic_roughness != scene->model_white_texture)
                SDL_ReleaseGPUTexture(scene->device, t->metallic_roughness);
            if (t->occlusion && t->occlusion != scene->model_white_texture)
                SDL_ReleaseGPUTexture(scene->device, t->occlusion);
            if (t->emissive && t->emissive != scene->model_black_texture)
                SDL_ReleaseGPUTexture(scene->device, t->emissive);
        }
    }

    /* Free pipeline data */
    forge_pipeline_free_scene(&model->scene_data);
    forge_pipeline_free_mesh(&model->mesh);
    forge_pipeline_free_materials(&model->materials);
    forge_pipeline_free_skins(&model->skins);
    forge_pipeline_free_animation(&model->animations);

    SDL_memset(model, 0, sizeof(*model));
}

#endif /* FORGE_SCENE_MODEL_SUPPORT */

#endif /* FORGE_SCENE_IMPLEMENTATION */
#endif /* FORGE_SCENE_H */
