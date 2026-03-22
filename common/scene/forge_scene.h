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

/* Morph weight skip threshold */
#define FORGE_SCENE_MORPH_WEIGHT_EPSILON   0.001f

/* Light direction stability thresholds */
#define FORGE_SCENE_LIGHT_DIR_EPSILON      1e-6f
#define FORGE_SCENE_LIGHT_UP_PARALLEL_COS  0.999f

/* Textured pipeline sampler count: shadow map + material texture */
#define FORGE_SCENE_TEXTURED_FRAG_SAMPLER_COUNT 2

/* Debug line initial vertex capacity (vertices, not lines — 2 verts per line) */
#define FORGE_SCENE_DEBUG_VB_INITIAL_CAPACITY 4096

/* Minimum segments for debug circle/sphere arcs */
#define FORGE_SCENE_DEBUG_MIN_SEGMENTS 3

/* Axis-parallel cutoff for building orthonormal basis in debug circle */
#define FORGE_SCENE_DEBUG_AXIS_PARALLEL_COS 0.99f

/* UI atlas parameters */
#define FORGE_SCENE_ATLAS_PIXEL_HEIGHT 24.0f
#define FORGE_SCENE_ATLAS_PADDING      2
#define FORGE_SCENE_ASCII_START        32
#define FORGE_SCENE_ASCII_COUNT        95
#define FORGE_SCENE_UI_VB_CAPACITY     4096
#define FORGE_SCENE_UI_IB_CAPACITY     8192

/* Maximum materials per loaded model (BrainStem has 59 color-only materials) */
#define FORGE_SCENE_MODEL_MAX_MATERIALS 64

/* Maximum submeshes per loaded model (for centroid precomputation).
 * Used by transparency sorting to compute object-space centroids at load time
 * rather than walking vertex data every frame. */
#define FORGE_SCENE_MODEL_MAX_SUBMESHES 512

/* Maximum transparent draws that can be sorted per draw_model call.
 * Overflow BLEND submeshes fall back to immediate unsorted drawing. */
#define FORGE_SCENE_MAX_TRANSPARENT_DRAWS 256

/* Default alpha cutoff for MASK materials when the material lacks one */
#define FORGE_SCENE_DEFAULT_ALPHA_CUTOFF 0.5f

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

/* Interleaved vertex for textured scene geometry: position + normal + UV.
 * Matches the scene_textured shader vertex input layout (32 bytes). */
typedef struct ForgeSceneTexturedVertex {
    vec3 position;  /* object-space vertex position */
    vec3 normal;    /* object-space vertex normal   */
    vec2 uv;        /* texture coordinates          */
} ForgeSceneTexturedVertex;

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

/* Debug line vertex: world-space position + RGBA color (28 bytes).
 * Used by forge_scene_debug_line() and shape helpers. */
typedef struct ForgeSceneDebugVertex {
    float position[3]; /* world-space position */
    float color[4];    /* RGBA color           */
} ForgeSceneDebugVertex;

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

/* Textured scene fragment uniforms — Blinn-Phong with texture + atlas UV.
 * Must match scene_textured.frag.hlsl cbuffer SceneTexturedFragUniforms
 * (80 bytes).  Replaces base_color with uv_transform for texture sampling. */
typedef struct ForgeSceneTexturedFragUniforms {
    float uv_transform[4];   /* xy = UV offset, zw = UV scale (atlas remap) */
    float eye_pos[3];        /* camera position                      */
    float ambient;           /* ambient intensity                    */
    float light_dir[4];      /* directional light direction          */
    float light_color[3];    /* light RGB                            */
    float light_intensity;   /* light brightness                     */
    float shininess;         /* specular exponent                    */
    float specular_str;      /* specular strength                    */
    float _pad[2];           /* 16-byte alignment                    */
} ForgeSceneTexturedFragUniforms;

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

/* Instanced vertex uniforms: VP + light VP + node world (192 bytes).
 * Used by scene_instanced.vert.hlsl and scene_model_instanced.vert.hlsl.
 * The per-draw model matrix comes from the per-instance vertex buffer.
 * node_world applies the glTF node hierarchy transform before the instance
 * matrix — identity for simple meshes, non-identity for multi-node models. */
typedef struct ForgeSceneInstancedVertUniforms {
    mat4 vp;         /* camera view-projection                        */
    mat4 light_vp;   /* light view-projection                         */
    mat4 node_world; /* per-node local-to-model transform (often I)   */
} ForgeSceneInstancedVertUniforms;

/* Instanced shadow vertex uniforms: light VP + node world (128 bytes).
 * Used by scene_instanced_shadow.vert.hlsl and
 * scene_model_instanced_shadow.vert.hlsl. */
typedef struct ForgeSceneInstancedShadowVertUniforms {
    mat4 light_vp;   /* light view-projection               */
    mat4 node_world; /* per-node local-to-model transform    */
} ForgeSceneInstancedShadowVertUniforms;

/* Debug line vertex uniforms: VP only (64 bytes).
 * Must match debug.vert.hlsl cbuffer DebugUniforms. */
typedef struct ForgeSceneDebugVertUniforms {
    mat4 vp;  /* camera view-projection */
} ForgeSceneDebugVertUniforms;

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

/* A sortable draw command for back-to-front transparent rendering.
 * Collected during draw_model, sorted by view depth, then drawn. */
typedef struct ForgeSceneTransparentDraw {
    uint32_t node_index;    /* index into scene_data.nodes              */
    uint32_t submesh_index; /* global submesh index within the mesh     */
    float    sort_depth;    /* projected depth along camera forward     */
    mat4     final_world;   /* precomputed placement * node world       */
} ForgeSceneTransparentDraw;

/* Fragment uniforms for the alpha-masked shadow pass.  Smaller than the
 * full model fragment uniforms — only the fields needed for the discard. */
typedef struct ForgeSceneShadowMaskFragUniforms {
    float base_color_factor[4]; /* RGBA multiplier from material */
    float alpha_cutoff;         /* discard threshold             */
    float _pad[3];              /* pad to 16-byte alignment      */
} ForgeSceneShadowMaskFragUniforms;

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

    /* Object-space submesh centroids for transparency sorting.
     * Precomputed at load time by averaging vertex positions per submesh
     * so we avoid walking index data every frame during draw. */
    vec3     submesh_centroids[FORGE_SCENE_MODEL_MAX_SUBMESHES];
    uint32_t submesh_centroid_count;

    /* Stats (updated per draw_model call) */
    uint32_t draw_calls;
    uint32_t transparent_draw_calls; /* subset of draw_calls that were sorted */

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

    /* Object-space submesh centroids for transparency sorting */
    vec3     submesh_centroids[FORGE_SCENE_MODEL_MAX_SUBMESHES];
    uint32_t submesh_centroid_count;

    /* Stats */
    uint32_t draw_calls;
    uint32_t transparent_draw_calls;
    ForgeSceneVramStats vram;
} ForgeSceneSkinnedModel;

/* A loaded pipeline model with morph target (blend shape) support.
 * Uses standard 48-byte vertices (same as ForgeSceneModel) with per-vertex
 * position and normal deltas stored in GPU storage buffers.  The CPU blends
 * morph target deltas each frame and uploads the result — the vertex shader
 * adds deltas to base attributes via SV_VertexID indexing. */
typedef struct ForgeSceneMorphModel {
    /* Pipeline data (owns memory — freed by forge_scene_free_morph_model) */
    ForgePipelineScene       scene_data;   /* node hierarchy + world transforms */
    ForgePipelineMesh        mesh;         /* vertex/index data + morph deltas  */
    ForgePipelineMaterialSet materials;    /* PBR material descriptions         */
    ForgePipelineAnimFile    animations;   /* animation clips (morph weights)   */

    /* GPU resources */
    SDL_GPUBuffer *vertex_buffer;          /* uploaded 48-byte base vertices    */
    SDL_GPUBuffer *index_buffer;           /* uploaded uint32 indices           */
    SDL_GPUBuffer *morph_pos_buffer;       /* storage: vertex_count x float4 (16-byte stride) */
    SDL_GPUBuffer *morph_nrm_buffer;       /* storage: vertex_count x float4 (16-byte stride) */
    SDL_GPUTransferBuffer *morph_transfer_buffer; /* persistent upload staging  */

    /* Per-material textures */
    ForgeSceneModelTextures mat_textures[FORGE_SCENE_MODEL_MAX_MATERIALS];
    uint32_t                mat_texture_count;

    /* CPU-side blended deltas (uploaded to GPU each frame) */
    float *blended_pos_deltas;   /* vertex_count * 4 floats (16-byte stride) */
    float *blended_nrm_deltas;   /* vertex_count * 4 floats (16-byte stride) */

    /* Morph weight state (set by animation or manual override) */
    float    morph_weights[FORGE_PIPELINE_MAX_MORPH_TARGETS];
    uint32_t morph_target_count;  /* actual count from mesh */

    /* Animation state */
    float    anim_time;
    float    anim_speed;       /* default 1.0 */
    int      current_clip;
    bool     looping;          /* default true */
    bool     manual_weights;   /* true = skip animation, use morph_weights[] directly */

    /* Stats */
    uint32_t draw_calls;
    ForgeSceneVramStats vram;
} ForgeSceneMorphModel;

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

    /* ── Scene shaders + vertex layout (kept alive for create_pipeline) ── */
    SDL_GPUShader                  *scene_vs;       /* Blinn-Phong vertex shader   */
    SDL_GPUShader                  *scene_fs;       /* Blinn-Phong fragment shader  */
    SDL_GPUVertexBufferDescription  scene_vb_desc;  /* pos+normal buffer layout    */
    SDL_GPUVertexAttribute          scene_attrs[2]; /* position + normal attribs   */

    /* ── Internal pipelines ─────────────────────────────────────────── */
    SDL_GPUGraphicsPipeline *scene_pipeline;   /* Blinn-Phong (pos+normal)   */
    SDL_GPUGraphicsPipeline *shadow_pipeline;      /* depth-only shadow (24B stride) */
    SDL_GPUGraphicsPipeline *shadow_pipeline_pos;  /* depth-only shadow (12B stride) */
    SDL_GPUGraphicsPipeline *shadow_pipeline_tex;  /* depth-only shadow (32B stride) */
    SDL_GPUGraphicsPipeline *textured_pipeline;    /* Blinn-Phong + texture + atlas UV */
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

    /* ── Transparency sorting ───────────────────────────────────────── */
    bool transparency_sorting;  /* true = sort BLEND back-to-front (default) */

    /* ── Per-frame state ────────────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass      *model_copy_pass;   /* batched model data upload pass */
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

    /* ── Instanced mesh pipelines (lazy-initialized on first use) ──── */
    SDL_GPUGraphicsPipeline *instanced_pipeline;         /* Blinn-Phong, back cull */
    SDL_GPUGraphicsPipeline *instanced_shadow_pipeline;  /* depth-only, no cull    */
    bool                     instanced_pipelines_ready;

    /* ── Debug line resources (lazy-initialized on first use) ─────── */
    SDL_GPUGraphicsPipeline *debug_world_pipeline;    /* depth test ON        */
    SDL_GPUGraphicsPipeline *debug_overlay_pipeline;  /* depth test OFF       */
    SDL_GPUBuffer           *debug_vb;                /* GPU vertex buffer    */
    ForgeSceneDebugVertex   *debug_world_vertices;    /* CPU accumulation     */
    ForgeSceneDebugVertex   *debug_overlay_vertices;  /* CPU accumulation     */
    Uint32                   debug_world_count;       /* current world verts  */
    Uint32                   debug_overlay_count;     /* current overlay verts*/
    Uint32                   debug_world_capacity;    /* allocated world cap  */
    Uint32                   debug_overlay_capacity;  /* allocated overlay cap*/
    Uint32                   debug_vb_capacity;       /* GPU buffer capacity  */
    bool                     debug_ready;             /* lazy init flag       */

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
    SDL_GPUGraphicsPipeline *model_shadow_mask_pipeline; /* depth + alpha test for MASK shadows  */
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
    SDL_GPUGraphicsPipeline *skinned_shadow_mask_pipeline; /* depth + alpha test, 72-byte      */
    bool                     skinned_pipelines_ready;      /* lazy init flag                   */

    /* Morph target model pipelines (lazy-initialized on first morph model load) */
    SDL_GPUGraphicsPipeline *morph_pipeline;               /* cull back, depth write          */
    SDL_GPUGraphicsPipeline *morph_pipeline_blend;         /* alpha blend, no depth write     */
    SDL_GPUGraphicsPipeline *morph_pipeline_double;        /* cull none, depth write          */
    SDL_GPUGraphicsPipeline *morph_pipeline_blend_double;  /* alpha blend + cull none         */
    SDL_GPUGraphicsPipeline *morph_shadow_pipeline;        /* depth-only, 48-byte + storage   */
    bool                     morph_pipelines_ready;        /* lazy init flag                  */

    /* Instanced model pipelines (lazy-initialized on first instanced draw) */
    SDL_GPUGraphicsPipeline *model_instanced_pipeline;              /* cull back         */
    SDL_GPUGraphicsPipeline *model_instanced_pipeline_blend;        /* alpha blend       */
    SDL_GPUGraphicsPipeline *model_instanced_pipeline_double;       /* cull none         */
    SDL_GPUGraphicsPipeline *model_instanced_pipeline_blend_double; /* blend + cull none */
    SDL_GPUGraphicsPipeline *model_instanced_shadow_pipeline;       /* depth-only        */
    SDL_GPUGraphicsPipeline *model_instanced_shadow_mask_pipeline;  /* depth + alpha     */
    bool                     model_instanced_pipelines_ready;       /* lazy init flag    */
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

/* Draw a mesh into the shadow map using a position-only vertex buffer
 * (12-byte stride, tightly-packed float3).  Use this with ForgeShape
 * position buffers or any struct-of-arrays geometry.
 * Index buffer must contain 32-bit indices. */
static void forge_scene_draw_shadow_mesh_pos(ForgeScene *scene,
                                              SDL_GPUBuffer *pos_vb,
                                              SDL_GPUBuffer *ib,
                                              Uint32 index_count,
                                              mat4 model);

/* Draw a mesh into the shadow map using a ForgeSceneTexturedVertex buffer
 * (32-byte stride, position at offset 0).  Use this with textured meshes.
 * Index buffer must contain 32-bit indices. */
static void forge_scene_draw_shadow_textured_mesh(ForgeScene *scene,
                                                    SDL_GPUBuffer *vb,
                                                    SDL_GPUBuffer *ib,
                                                    Uint32 index_count,
                                                    mat4 model);

/* End the shadow pass. */
static void forge_scene_end_shadow_pass(ForgeScene *scene);

/* ── Main pass ──────────────────────────────────────────────── */

/* Begin the main color+depth pass.  Draws the sky background first. */
static void forge_scene_begin_main_pass(ForgeScene *scene);

/* Draw a lit mesh with Blinn-Phong shading and shadow using the default
 * pipeline (back-face culling, filled triangles).  base_color is RGBA [0..1]. */
static void forge_scene_draw_mesh(ForgeScene *scene,
                                   SDL_GPUBuffer *vb,
                                   SDL_GPUBuffer *ib,
                                   Uint32 index_count,
                                   mat4 model,
                                   const float base_color[4]);

/* Draw a lit mesh with a caller-provided pipeline.  Use this when you need
 * non-default rasterizer state (wireframe, double-sided, etc.).  Create
 * the pipeline with forge_scene_create_pipeline(). */
static void forge_scene_draw_mesh_ex(ForgeScene *scene,
                                      SDL_GPUGraphicsPipeline *pipeline,
                                      SDL_GPUBuffer *vb,
                                      SDL_GPUBuffer *ib,
                                      Uint32 index_count,
                                      mat4 model,
                                      const float base_color[4]);

/* Create a scene pipeline with custom cull and fill modes.  The pipeline
 * shares the same Blinn-Phong shaders, vertex layout, depth test, and
 * shadow map binding as the default — only rasterizer state differs.
 *
 * The caller owns the returned pipeline and must release it with
 * SDL_ReleaseGPUGraphicsPipeline() before destroying the scene.
 *
 * Common configurations:
 *   double-sided:  SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_FILL
 *   wireframe:     SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_LINE
 */
static SDL_GPUGraphicsPipeline *forge_scene_create_pipeline(
    ForgeScene *scene, SDL_GPUCullMode cull_mode, SDL_GPUFillMode fill_mode);

/* Draw a textured mesh with Blinn-Phong shading, shadow map, and atlas UV
 * remapping.  vb must contain ForgeSceneTexturedVertex data (32-byte stride:
 * position + normal + UV).  Index buffer must contain 32-bit indices.
 * uv_transform is (u_offset, v_offset, u_scale, v_scale) — pass (0,0,1,1)
 * for non-atlas textures.
 *
 * This convenience function re-binds the pipeline and samplers on every call.
 * For atlas rendering where many meshes share one texture, use the pair
 * forge_scene_bind_textured_resources() + forge_scene_draw_textured_mesh_no_bind()
 * to bind once and draw many. */
static void forge_scene_draw_textured_mesh(ForgeScene *scene,
                                            SDL_GPUBuffer *vb,
                                            SDL_GPUBuffer *ib,
                                            Uint32 index_count,
                                            mat4 model,
                                            SDL_GPUTexture *texture,
                                            SDL_GPUSampler *sampler,
                                            const float uv_transform[4]);

/* Bind the textured pipeline and fragment samplers (shadow map + material
 * texture).  Call once before a batch of forge_scene_draw_textured_mesh_no_bind()
 * calls that share the same texture and sampler. */
static bool forge_scene_bind_textured_resources(ForgeScene *scene,
                                                 SDL_GPUTexture *texture,
                                                 SDL_GPUSampler *sampler);

/* Draw a textured mesh WITHOUT re-binding pipeline or samplers — the caller
 * must have called forge_scene_bind_textured_resources() first.
 * vb must contain ForgeSceneTexturedVertex data (32-byte stride).
 * Index buffer must contain 32-bit indices. */
static void forge_scene_draw_textured_mesh_no_bind(ForgeScene *scene,
                                                     SDL_GPUBuffer *vb,
                                                     SDL_GPUBuffer *ib,
                                                     Uint32 index_count,
                                                     mat4 model,
                                                     const float uv_transform[4]);

/* ── Instanced mesh drawing ─────────────────────────────────── */

/* Draw multiple instances of a mesh with Blinn-Phong shading.
 * instance_buffer contains mat4 transforms (64 bytes each, 4 vec4 columns).
 * Pipelines are lazy-initialized on first call. */
static void forge_scene_draw_mesh_instanced(ForgeScene *scene,
                                             SDL_GPUBuffer *vb,
                                             SDL_GPUBuffer *ib,
                                             Uint32 index_count,
                                             SDL_GPUBuffer *instance_buffer,
                                             Uint32 instance_count,
                                             const float base_color[4]);

/* Draw instanced mesh into the shadow map (depth-only). */
static void forge_scene_draw_shadow_mesh_instanced(ForgeScene *scene,
                                                    SDL_GPUBuffer *vb,
                                                    SDL_GPUBuffer *ib,
                                                    Uint32 index_count,
                                                    SDL_GPUBuffer *instance_buffer,
                                                    Uint32 instance_count);

/* ── Debug lines ────────────────────────────────────────────── */

/* Accumulate a debug line segment.  Call anytime before draw_debug_lines.
 * overlay=false: depth-tested (hidden behind geometry).
 * overlay=true:  always visible (drawn on top of everything). */
static void forge_scene_debug_line(ForgeScene *scene,
                                    vec3 a, vec3 b, vec4 color, bool overlay);

/* Wireframe box centered at center with given half-extents. */
static void forge_scene_debug_box(ForgeScene *scene,
                                   vec3 center, vec3 half_extents,
                                   vec4 color, bool overlay);

/* Circle in the plane perpendicular to axis. */
static void forge_scene_debug_circle(ForgeScene *scene,
                                      vec3 center, float radius, vec3 axis,
                                      vec4 color, int segments, bool overlay);

/* RGB axes (X=red, Y=green, Z=blue) at origin. */
static void forge_scene_debug_axes(ForgeScene *scene,
                                    vec3 origin, float length, bool overlay);

/* Wireframe sphere approximation (3 perpendicular circles). */
static void forge_scene_debug_sphere(ForgeScene *scene,
                                      vec3 center, float radius,
                                      vec4 color, int rings, bool overlay);

/* Upload accumulated debug vertices and draw them.  Call during the main
 * pass after scene geometry and before the grid.  Interrupts the render
 * pass internally for the upload, then resumes it.  Resets vertex counts. */
static void forge_scene_draw_debug_lines(ForgeScene *scene);

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

/* Draw multiple instances of a loaded model in the main pass.
 * instance_buffer contains mat4 transforms (64 bytes each).
 * Each submesh is drawn instance_count times per draw call.
 * Note: BLEND submeshes are not individually depth-sorted per instance. */
static void forge_scene_draw_model_instanced(ForgeScene *scene,
                                              ForgeSceneModel *model,
                                              SDL_GPUBuffer *instance_buffer,
                                              Uint32 instance_count);

/* Draw instanced model into the shadow map (depth-only). */
static void forge_scene_draw_model_shadows_instanced(ForgeScene *scene,
                                                      ForgeSceneModel *model,
                                                      SDL_GPUBuffer *instance_buffer,
                                                      Uint32 instance_count);

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

/* ── Morph target model API ─────────────────────────────── */

/* Load a morph target pipeline model (.fscene + .fmesh + optional .fmat + optional .fanim).
 * The .fmesh must have FLAG_MORPHS set (morph delta data appended).
 * fmat_path may be NULL (morph models may have no material file).
 * fanim_path may be NULL for static morph weights (manual control). */
static bool forge_scene_load_morph_model(
    ForgeScene *scene, ForgeSceneMorphModel *model,
    const char *fscene_path, const char *fmesh_path,
    const char *fmat_path,   const char *fanim_path,
    const char *base_dir);

/* Evaluate morph weight animation and blend deltas on the CPU,
 * then upload blended deltas to GPU storage buffers.
 * Must be called after forge_scene_begin_frame() (requires scene->cmd).
 * When model->manual_weights is true, skips animation evaluation and
 * uses morph_weights[] as-is. */
static void forge_scene_update_morph_animation(
    ForgeScene *scene, ForgeSceneMorphModel *model, float dt);

/* Draw morph model in the main pass. */
static void forge_scene_draw_morph_model(
    ForgeScene *scene, ForgeSceneMorphModel *model, mat4 placement);

/* Draw morph model into the shadow map. */
static void forge_scene_draw_morph_model_shadows(
    ForgeScene *scene, ForgeSceneMorphModel *model, mat4 placement);

/* Release all GPU and CPU resources for a morph model. */
static void forge_scene_free_morph_model(
    ForgeScene *scene, ForgeSceneMorphModel *model);

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

#ifdef FORGE_SCENE_MODEL_SUPPORT
/* Compute per-submesh centroids for transparency sorting.
 * Averages the position of every vertex in each LOD 0 submesh.
 * Results are written to out_centroids[0..submesh_count-1].
 * Handles malformed metadata gracefully (misaligned offsets, OOB indices,
 * overflowing index spans) by producing vec3(0,0,0) for bad submeshes. */
static void forge_scene_compute_centroids(
    const ForgePipelineMesh *mesh,
    vec3 *out_centroids, uint32_t max_submeshes);
#endif /* FORGE_SCENE_MODEL_SUPPORT */

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

#include "scene/shaders/compiled/scene_textured_vert_spirv.h"
#include "scene/shaders/compiled/scene_textured_vert_dxil.h"
#include "scene/shaders/compiled/scene_textured_vert_msl.h"
#include "scene/shaders/compiled/scene_textured_frag_spirv.h"
#include "scene/shaders/compiled/scene_textured_frag_dxil.h"
#include "scene/shaders/compiled/scene_textured_frag_msl.h"

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

#include "scene/shaders/compiled/scene_instanced_vert_spirv.h"
#include "scene/shaders/compiled/scene_instanced_vert_dxil.h"
#include "scene/shaders/compiled/scene_instanced_vert_msl.h"
#include "scene/shaders/compiled/scene_instanced_shadow_vert_spirv.h"
#include "scene/shaders/compiled/scene_instanced_shadow_vert_dxil.h"
#include "scene/shaders/compiled/scene_instanced_shadow_vert_msl.h"

#include "scene/shaders/compiled/debug_vert_spirv.h"
#include "scene/shaders/compiled/debug_vert_dxil.h"
#include "scene/shaders/compiled/debug_vert_msl.h"
#include "scene/shaders/compiled/debug_frag_spirv.h"
#include "scene/shaders/compiled/debug_frag_dxil.h"
#include "scene/shaders/compiled/debug_frag_msl.h"

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
#include "scene/shaders/compiled/scene_morph_vert_spirv.h"
#include "scene/shaders/compiled/scene_morph_vert_dxil.h"
#include "scene/shaders/compiled/scene_morph_vert_msl.h"
#include "scene/shaders/compiled/scene_morph_shadow_vert_spirv.h"
#include "scene/shaders/compiled/scene_morph_shadow_vert_dxil.h"
#include "scene/shaders/compiled/scene_morph_shadow_vert_msl.h"
#include "scene/shaders/compiled/shadow_mask_vert_spirv.h"
#include "scene/shaders/compiled/shadow_mask_vert_dxil.h"
#include "scene/shaders/compiled/shadow_mask_vert_msl.h"
#include "scene/shaders/compiled/shadow_mask_frag_spirv.h"
#include "scene/shaders/compiled/shadow_mask_frag_dxil.h"
#include "scene/shaders/compiled/shadow_mask_frag_msl.h"
#include "scene/shaders/compiled/shadow_mask_skinned_vert_spirv.h"
#include "scene/shaders/compiled/shadow_mask_skinned_vert_dxil.h"
#include "scene/shaders/compiled/shadow_mask_skinned_vert_msl.h"
#include "scene/shaders/compiled/scene_model_instanced_vert_spirv.h"
#include "scene/shaders/compiled/scene_model_instanced_vert_dxil.h"
#include "scene/shaders/compiled/scene_model_instanced_vert_msl.h"
#include "scene/shaders/compiled/scene_model_instanced_shadow_vert_spirv.h"
#include "scene/shaders/compiled/scene_model_instanced_shadow_vert_dxil.h"
#include "scene/shaders/compiled/scene_model_instanced_shadow_vert_msl.h"
#endif

/* ── D3D12 texture alignment constants ─────────────────────────────────── */

/* D3D12 requires texture upload row pitch aligned to 256 bytes
 * (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT).  We pad rows unconditionally
 * (harmless on Vulkan/Metal) so the D3D12 backend takes the fast copy
 * path instead of hitting its buggy realignment code. */
#define FORGE_SCENE_D3D12_PITCH_ALIGN     256

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

    Uint32 row_bytes_raw  = (Uint32)(tex_w * 4);
    Uint32 aligned_pitch  = (row_bytes_raw + FORGE_SCENE_D3D12_PITCH_ALIGN - 1)
                          & ~(FORGE_SCENE_D3D12_PITCH_ALIGN - 1);
    Uint32 total_bytes    = aligned_pitch * (Uint32)tex_h;

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

    const Uint8 *row_src = (const Uint8 *)converted->pixels;
    Uint8 *row_dst = (Uint8 *)mapped;
    for (Uint32 row = 0; row < (Uint32)tex_h; row++) {
        SDL_memcpy(row_dst + row * aligned_pitch,
                   row_src + row * converted->pitch,
                   row_bytes_raw);
        if (aligned_pitch > row_bytes_raw) {
            SDL_memset(row_dst + row * aligned_pitch + row_bytes_raw,
                       0, aligned_pitch - row_bytes_raw);
        }
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
    tex_src.pixels_per_row  = aligned_pitch / 4;
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

    Uint32 aligned_pitch = (w + FORGE_SCENE_D3D12_PITCH_ALIGN - 1)
                         & ~(FORGE_SCENE_D3D12_PITCH_ALIGN - 1);
    Uint32 total = aligned_pitch * h;
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
    const Uint8 *src_row = pixels;
    Uint8 *dst_row = (Uint8 *)mapped;
    for (Uint32 row = 0; row < h; row++) {
        SDL_memcpy(dst_row + row * aligned_pitch,
                   src_row + row * w,
                   w);
        if (aligned_pitch > w) {
            SDL_memset(dst_row + row * aligned_pitch + w,
                       0, aligned_pitch - w);
        }
    }
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
    src.pixels_per_row  = aligned_pitch;
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
    if (!forge_isfinite(config->fov_deg) || !forge_isfinite(config->near_plane) ||
        !forge_isfinite(config->far_plane) || !forge_isfinite(config->shadow_height) ||
        !forge_isfinite(config->shadow_ortho_size) || !forge_isfinite(config->shadow_near) ||
        !forge_isfinite(config->shadow_far) || !forge_isfinite(config->move_speed) ||
        !forge_isfinite(config->mouse_sensitivity) || !forge_isfinite(config->light_intensity) ||
        !forge_isfinite(config->ambient) || !forge_isfinite(config->shininess) ||
        !forge_isfinite(config->specular_str) || !forge_isfinite(config->grid_half_size) ||
        !forge_isfinite(config->grid_spacing) || !forge_isfinite(config->grid_line_width) ||
        !forge_isfinite(config->grid_fade_dist) || !forge_isfinite(config->font_size) ||
        !forge_isfinite(config->cam_start_pos.x) || !forge_isfinite(config->cam_start_pos.y) ||
        !forge_isfinite(config->cam_start_pos.z) || !forge_isfinite(config->cam_start_yaw) ||
        !forge_isfinite(config->cam_start_pitch) || !forge_isfinite(config->light_dir.x) ||
        !forge_isfinite(config->light_dir.y) || !forge_isfinite(config->light_dir.z)) {
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
 * cull mode and fill mode.  All other state is identical between the normal,
 * double-sided, and wireframe variants, so this helper prevents drift. */
static SDL_GPUGraphicsPipeline *forge_scene__create_scene_pipeline(
    ForgeScene *scene,
    SDL_GPUShader *vs, SDL_GPUShader *fs,
    SDL_GPUVertexInputState vertex_input,
    SDL_GPUCullMode cull_mode,
    SDL_GPUFillMode fill_mode)
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
    pi.rasterizer_state.fill_mode  = fill_mode;
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
    scene->transparency_sorting = true; /* sort BLEND back-to-front by default */

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

    scene->scene_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_vert_spirv, sizeof(scene_vert_spirv),
        scene_vert_dxil,  sizeof(scene_vert_dxil),
        scene_vert_msl, scene_vert_msl_size,
        0, 0, 0, 1);

    scene->scene_fs = forge_scene_create_shader(scene,
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

    SDL_GPUShader *textured_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_textured_vert_spirv, sizeof(scene_textured_vert_spirv),
        scene_textured_vert_dxil,  sizeof(scene_textured_vert_dxil),
        scene_textured_vert_msl, scene_textured_vert_msl_size,
        0, 0, 0, 1);

    SDL_GPUShader *textured_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_textured_frag_spirv, sizeof(scene_textured_frag_spirv),
        scene_textured_frag_dxil,  sizeof(scene_textured_frag_dxil),
        scene_textured_frag_msl, scene_textured_frag_msl_size,
        FORGE_SCENE_TEXTURED_FRAG_SAMPLER_COUNT, 0, 0, 1);

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

    if (!scene->scene_vs || !scene->scene_fs || !shadow_vs || !shadow_fs ||
        !textured_vs || !textured_fs ||
        !grid_vs || !grid_fs || !sky_vs || !sky_fs) {
        SDL_Log("forge_scene: one or more shaders failed");
        if (scene->scene_vs) {
            SDL_ReleaseGPUShader(scene->device, scene->scene_vs);
            scene->scene_vs = NULL;
        }
        if (scene->scene_fs) {
            SDL_ReleaseGPUShader(scene->device, scene->scene_fs);
            scene->scene_fs = NULL;
        }
        if (shadow_vs)   SDL_ReleaseGPUShader(scene->device, shadow_vs);
        if (shadow_fs)   SDL_ReleaseGPUShader(scene->device, shadow_fs);
        if (textured_vs) SDL_ReleaseGPUShader(scene->device, textured_vs);
        if (textured_fs) SDL_ReleaseGPUShader(scene->device, textured_fs);
        if (grid_vs)     SDL_ReleaseGPUShader(scene->device, grid_vs);
        if (grid_fs)     SDL_ReleaseGPUShader(scene->device, grid_fs);
        if (sky_vs)      SDL_ReleaseGPUShader(scene->device, sky_vs);
        if (sky_fs)      SDL_ReleaseGPUShader(scene->device, sky_fs);
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
        if (!scene->shadow_pipeline) {
            SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                    "(shadow_pipeline) failed: %s", SDL_GetError());
        }

        /* Position-only variant: 12-byte stride for struct-of-arrays buffers
         * (ForgeShape positions, or any tightly-packed float3 array). */
        SDL_GPUVertexBufferDescription vb_pos_only;
        SDL_zero(vb_pos_only);
        vb_pos_only.slot       = 0;
        vb_pos_only.pitch      = sizeof(float) * 3;  /* 12 bytes */
        vb_pos_only.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState pos_only_input;
        SDL_zero(pos_only_input);
        pos_only_input.vertex_buffer_descriptions = &vb_pos_only;
        pos_only_input.num_vertex_buffers         = 1;
        pos_only_input.vertex_attributes          = &pos_attr;
        pos_only_input.num_vertex_attributes      = 1;

        pi.vertex_input_state = pos_only_input;
        scene->shadow_pipeline_pos =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->shadow_pipeline_pos) {
            SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                    "(shadow_pipeline_pos) failed: %s", SDL_GetError());
        }

        /* Textured vertex variant: 32-byte stride for ForgeSceneTexturedVertex
         * (position at offset 0, followed by normal + UV — only position read). */
        SDL_GPUVertexBufferDescription vb_tex_shadow;
        SDL_zero(vb_tex_shadow);
        vb_tex_shadow.slot       = 0;
        vb_tex_shadow.pitch      = sizeof(ForgeSceneTexturedVertex);
        vb_tex_shadow.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexInputState tex_shadow_input;
        SDL_zero(tex_shadow_input);
        tex_shadow_input.vertex_buffer_descriptions = &vb_tex_shadow;
        tex_shadow_input.num_vertex_buffers         = 1;
        tex_shadow_input.vertex_attributes          = &pos_attr;
        tex_shadow_input.num_vertex_attributes      = 1;

        pi.vertex_input_state = tex_shadow_input;
        scene->shadow_pipeline_tex =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->shadow_pipeline_tex) {
            SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                    "(shadow_pipeline_tex) failed: %s", SDL_GetError());
        }
    }

    /* Default scene pipeline: Blinn-Phong with shadow map, back-face culling.
     * Applications needing other rasterizer state (wireframe, double-sided)
     * create their own pipelines via forge_scene_create_pipeline(). */
    /* Store vertex layout so forge_scene_create_pipeline() can reuse it */
    scene->scene_vb_desc = vb_full;
    SDL_memcpy(scene->scene_attrs, full_attrs, sizeof(full_attrs));

    scene->scene_pipeline = forge_scene__create_scene_pipeline(
        scene, scene->scene_vs, scene->scene_fs, full_input,
        SDL_GPU_CULLMODE_BACK, SDL_GPU_FILLMODE_FILL);

    /* Textured scene pipeline: Blinn-Phong with texture sampling and
     * atlas UV remapping.  ForgeSceneTexturedVertex = 32 bytes
     * (position + normal + UV).  Uses 2 fragment samplers: shadow + material. */
    {
        SDL_GPUVertexBufferDescription vb_tex;
        SDL_zero(vb_tex);
        vb_tex.slot       = 0;
        vb_tex.pitch      = sizeof(ForgeSceneTexturedVertex);
        vb_tex.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute tex_attrs[3];
        SDL_zero(tex_attrs);
        tex_attrs[0].location    = 0;
        tex_attrs[0].buffer_slot = 0;
        tex_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        tex_attrs[0].offset      = offsetof(ForgeSceneTexturedVertex, position);
        tex_attrs[1].location    = 1;
        tex_attrs[1].buffer_slot = 0;
        tex_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        tex_attrs[1].offset      = offsetof(ForgeSceneTexturedVertex, normal);
        tex_attrs[2].location    = 2;
        tex_attrs[2].buffer_slot = 0;
        tex_attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        tex_attrs[2].offset      = offsetof(ForgeSceneTexturedVertex, uv);

        SDL_GPUVertexInputState tex_input;
        SDL_zero(tex_input);
        tex_input.vertex_buffer_descriptions = &vb_tex;
        tex_input.num_vertex_buffers         = 1;
        tex_input.vertex_attributes          = tex_attrs;
        tex_input.num_vertex_attributes      = 3;

        scene->textured_pipeline = forge_scene__create_scene_pipeline(
            scene, textured_vs, textured_fs, tex_input,
            SDL_GPU_CULLMODE_BACK, SDL_GPU_FILLMODE_FILL);
        if (!scene->textured_pipeline) {
            SDL_Log("forge_scene: textured_pipeline creation failed: %s",
                    SDL_GetError());
        }
    }

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
        if (!scene->grid_pipeline) {
            SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                    "(grid_pipeline) failed: %s", SDL_GetError());
        }
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
        if (!scene->sky_pipeline) {
            SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                    "(sky_pipeline) failed: %s", SDL_GetError());
        }
    }

    /* Release non-scene shaders (pipelines keep internal copies).
     * Scene shaders (scene_vs, scene_fs) are kept alive in the struct
     * so forge_scene_create_pipeline() can create new pipelines later. */
    SDL_ReleaseGPUShader(scene->device, shadow_vs);
    SDL_ReleaseGPUShader(scene->device, shadow_fs);
    SDL_ReleaseGPUShader(scene->device, textured_vs);
    SDL_ReleaseGPUShader(scene->device, textured_fs);
    SDL_ReleaseGPUShader(scene->device, grid_vs);
    SDL_ReleaseGPUShader(scene->device, grid_fs);
    SDL_ReleaseGPUShader(scene->device, sky_vs);
    SDL_ReleaseGPUShader(scene->device, sky_fs);

    if (!scene->shadow_pipeline || !scene->shadow_pipeline_pos ||
        !scene->shadow_pipeline_tex ||
        !scene->textured_pipeline || !scene->scene_pipeline ||
        !scene->grid_pipeline || !scene->sky_pipeline) {
        SDL_Log("forge_scene: pipeline creation failed (see above)");
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
        if (!forge_isfinite(dir_len) || dir_len < FORGE_SCENE_LIGHT_DIR_EPSILON) {
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
    if (SDL_fabsf(vec3_dot(scene->light_dir, light_up)) > FORGE_SCENE_LIGHT_UP_PARALLEL_COS) {
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
    if (scene->model_copy_pass) {
        SDL_EndGPUCopyPass(scene->model_copy_pass);
        scene->model_copy_pass = NULL;
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
    if (!scene->pass || !scene->shadow_pipeline || !vb || !ib ||
        index_count == 0) return;

    /* Bind the default shadow pipeline — other shadow draw paths switch to
     * their own pipelines (pos-only, skinned) and do not restore this one,
     * so we must explicitly bind here to ensure the correct vertex stride. */
    SDL_BindGPUGraphicsPipeline(scene->pass, scene->shadow_pipeline);

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

static void forge_scene_draw_shadow_mesh_pos(ForgeScene *scene,
                                              SDL_GPUBuffer *pos_vb,
                                              SDL_GPUBuffer *ib,
                                              Uint32 index_count,
                                              mat4 model)
{
    if (!scene->pass || !scene->shadow_pipeline_pos ||
        !pos_vb || !ib || index_count == 0) return;

    /* Switch to the 12-byte-stride pipeline for position-only buffers */
    SDL_BindGPUGraphicsPipeline(scene->pass, scene->shadow_pipeline_pos);

    SDL_GPUBufferBinding vb_bind = { pos_vb, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    ForgeSceneShadowVertUniforms su;
    su.light_vp = mat4_multiply(scene->light_vp, model);
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

    SDL_DrawGPUIndexedPrimitives(scene->pass, index_count, 1, 0, 0, 0);
}

static void forge_scene_draw_shadow_textured_mesh(ForgeScene *scene,
                                                    SDL_GPUBuffer *vb,
                                                    SDL_GPUBuffer *ib,
                                                    Uint32 index_count,
                                                    mat4 model)
{
    if (!scene->pass || !scene->shadow_pipeline_tex ||
        !vb || !ib || index_count == 0) return;

    /* Switch to the 32-byte-stride pipeline for textured vertex buffers */
    SDL_BindGPUGraphicsPipeline(scene->pass, scene->shadow_pipeline_tex);

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

/* ── Instanced shadow mesh ─────────────────────────────────────────────── */

static void forge_scene__init_instanced_pipelines(ForgeScene *scene);

static void forge_scene_draw_shadow_mesh_instanced(ForgeScene *scene,
                                                    SDL_GPUBuffer *vb,
                                                    SDL_GPUBuffer *ib,
                                                    Uint32 index_count,
                                                    SDL_GPUBuffer *instance_buffer,
                                                    Uint32 instance_count)
{
    if (!scene->pass || !vb || !ib || !instance_buffer ||
        index_count == 0 || instance_count == 0) return;

    if (!scene->instanced_pipelines_ready)
        forge_scene__init_instanced_pipelines(scene);
    if (!scene->instanced_shadow_pipeline) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->instanced_shadow_pipeline);

    /* Slot 0: mesh vertices (only position at offset 0, 24-byte stride) */
    SDL_GPUBufferBinding vb_bind = { vb, 0 };
    /* Slot 1: instance transforms (mat4 = 64 bytes each) */
    SDL_GPUBufferBinding ib_inst = { instance_buffer, 0 };
    SDL_GPUBufferBinding vb_binds[2] = { vb_bind, ib_inst };
    SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);

    SDL_GPUBufferBinding idx_bind = { ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &idx_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Push light_vp + node_world (identity for simple meshes). */
    ForgeSceneInstancedShadowVertUniforms su;
    su.light_vp   = scene->light_vp;
    su.node_world = mat4_identity();
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

    SDL_DrawGPUIndexedPrimitives(scene->pass, index_count, instance_count,
                                  0, 0, 0);
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
    if (scene->model_copy_pass) {
        SDL_EndGPUCopyPass(scene->model_copy_pass);
        scene->model_copy_pass = NULL;
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

/* ── Draw with caller-provided pipeline ───────────────────────────────── */

static void forge_scene_draw_mesh_ex(ForgeScene *scene,
                                      SDL_GPUGraphicsPipeline *pipeline,
                                      SDL_GPUBuffer *vb,
                                      SDL_GPUBuffer *ib,
                                      Uint32 index_count,
                                      mat4 model,
                                      const float base_color[4])
{
    if (!pipeline) {
        SDL_Log("forge_scene_draw_mesh_ex: pipeline is NULL");
        return;
    }
    forge_scene__draw_mesh_internal(scene, pipeline,
                                    vb, ib, index_count, model, base_color);
}

/* ── Textured mesh drawing ────────────────────────────────────────────── */

/* NOTE: This function re-binds the pipeline and both fragment samplers on
 * every call for simplicity.  For atlas rendering where many meshes share
 * one texture, use forge_scene_bind_textured_resources() once followed by
 * forge_scene_draw_textured_mesh_no_bind() per mesh to avoid redundant binds. */
static void forge_scene_draw_textured_mesh(ForgeScene *scene,
                                            SDL_GPUBuffer *vb,
                                            SDL_GPUBuffer *ib,
                                            Uint32 index_count,
                                            mat4 model,
                                            SDL_GPUTexture *texture,
                                            SDL_GPUSampler *sampler,
                                            const float uv_transform[4])
{
    if (!vb || !ib || index_count == 0 || !uv_transform) return;
    if (!forge_scene_bind_textured_resources(scene, texture, sampler)) return;
    forge_scene_draw_textured_mesh_no_bind(scene, vb, ib, index_count,
                                           model, uv_transform);
}

/* ── Batched textured mesh drawing (bind once, draw many) ────────────── */

static bool forge_scene_bind_textured_resources(ForgeScene *scene,
                                                 SDL_GPUTexture *texture,
                                                 SDL_GPUSampler *sampler)
{
    if (!scene->pass || !scene->textured_pipeline || !texture || !sampler)
        return false;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->textured_pipeline);

    /* Fragment sampler slot 0: shadow map
     * Fragment sampler slot 1: material texture */
    SDL_GPUTextureSamplerBinding frag_binds[FORGE_SCENE_TEXTURED_FRAG_SAMPLER_COUNT];
    frag_binds[0].texture = scene->shadow_map;
    frag_binds[0].sampler = scene->shadow_sampler;
    frag_binds[1].texture = texture;
    frag_binds[1].sampler = sampler;
    SDL_BindGPUFragmentSamplers(scene->pass, 0, frag_binds,
                                SDL_arraysize(frag_binds));
    return true;
}

static void forge_scene_draw_textured_mesh_no_bind(ForgeScene *scene,
                                                     SDL_GPUBuffer *vb,
                                                     SDL_GPUBuffer *ib,
                                                     Uint32 index_count,
                                                     mat4 model,
                                                     const float uv_transform[4])
{
    if (!scene->pass || !scene->textured_pipeline || !vb || !ib ||
        index_count == 0 || !uv_transform)
        return;

    SDL_GPUBufferBinding vb_bind = { vb, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Vertex uniforms: same 3-matrix layout as the base scene pipeline */
    ForgeSceneVertUniforms vu;
    vu.mvp      = mat4_multiply(scene->cam_vp, model);
    vu.model    = model;
    vu.light_vp = mat4_multiply(scene->light_vp, model);
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

    /* Fragment uniforms: UV transform + lighting */
    ForgeSceneTexturedFragUniforms fu;
    SDL_memset(&fu, 0, sizeof(fu));
    fu.uv_transform[0]  = uv_transform[0];
    fu.uv_transform[1]  = uv_transform[1];
    fu.uv_transform[2]  = uv_transform[2];
    fu.uv_transform[3]  = uv_transform[3];
    fu.eye_pos[0]        = scene->cam_position.x;
    fu.eye_pos[1]        = scene->cam_position.y;
    fu.eye_pos[2]        = scene->cam_position.z;
    fu.ambient           = scene->config.ambient;
    fu.light_dir[0]      = scene->light_dir.x;
    fu.light_dir[1]      = scene->light_dir.y;
    fu.light_dir[2]      = scene->light_dir.z;
    fu.light_dir[3]      = 0.0f;
    fu.light_color[0]    = scene->config.light_color[0];
    fu.light_color[1]    = scene->config.light_color[1];
    fu.light_color[2]    = scene->config.light_color[2];
    fu.light_intensity   = scene->config.light_intensity;
    fu.shininess         = scene->config.shininess;
    fu.specular_str      = scene->config.specular_str;
    SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

    SDL_DrawGPUIndexedPrimitives(scene->pass, index_count, 1, 0, 0, 0);
}

/* ── Public pipeline creation ─────────────────────────────────────────── */

static SDL_GPUGraphicsPipeline *forge_scene_create_pipeline(
    ForgeScene *scene, SDL_GPUCullMode cull_mode, SDL_GPUFillMode fill_mode)
{
    if (!scene || !scene->scene_vs || !scene->scene_fs ||
        !scene->shadow_sampler || !scene->scene_pipeline) {
        SDL_Log("forge_scene_create_pipeline: scene not fully initialized");
        return NULL;
    }

    SDL_GPUVertexInputState vi;
    SDL_zero(vi);
    vi.vertex_buffer_descriptions = &scene->scene_vb_desc;
    vi.num_vertex_buffers         = 1;
    vi.vertex_attributes          = scene->scene_attrs;
    vi.num_vertex_attributes      = 2;

    SDL_GPUGraphicsPipeline *pipeline = forge_scene__create_scene_pipeline(
        scene, scene->scene_vs, scene->scene_fs, vi, cull_mode, fill_mode);
    if (!pipeline) {
        SDL_Log("forge_scene_create_pipeline: SDL_CreateGPUGraphicsPipeline "
                "failed: %s", SDL_GetError());
    }
    return pipeline;
}

/* ── Instanced mesh pipelines + draw ───────────────────────────────────── */

static void forge_scene__init_instanced_pipelines(ForgeScene *scene)
{
    if (scene->instanced_pipelines_ready) return;

    /* Create instanced vertex shader */
    SDL_GPUShader *inst_vs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_instanced_vert_spirv, scene_instanced_vert_spirv_size,
        scene_instanced_vert_dxil,  scene_instanced_vert_dxil_size,
        scene_instanced_vert_msl,   scene_instanced_vert_msl_size,
        0, 0, 0, 1);

    /* Create instanced shadow vertex shader */
    SDL_GPUShader *inst_shadow_vs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_instanced_shadow_vert_spirv, scene_instanced_shadow_vert_spirv_size,
        scene_instanced_shadow_vert_dxil,  scene_instanced_shadow_vert_dxil_size,
        scene_instanced_shadow_vert_msl,   scene_instanced_shadow_vert_msl_size,
        0, 0, 0, 1);

    /* Shadow fragment shader — SDL3 requires a non-NULL fragment shader even
     * for depth-only pipelines (num_color_targets == 0). */
    SDL_GPUShader *shadow_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl,   shadow_frag_msl_size,
        0, 0, 0, 0);

    if (!inst_vs || !inst_shadow_vs || !shadow_fs) {
        SDL_Log("forge_scene: instanced shader creation failed");
        if (inst_vs)        SDL_ReleaseGPUShader(scene->device, inst_vs);
        if (inst_shadow_vs) SDL_ReleaseGPUShader(scene->device, inst_shadow_vs);
        if (shadow_fs)      SDL_ReleaseGPUShader(scene->device, shadow_fs);
        return;
    }

    /* Vertex layout: slot 0 = per-vertex (24-byte), slot 1 = per-instance (64-byte) */
    SDL_GPUVertexBufferDescription vb_descs[2];
    SDL_zero(vb_descs);
    vb_descs[0].slot       = 0;
    vb_descs[0].pitch      = sizeof(ForgeSceneVertex);
    vb_descs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_descs[1].slot       = 1;
    vb_descs[1].pitch      = sizeof(mat4);
    vb_descs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
    vb_descs[1].instance_step_rate = 1;

    SDL_GPUVertexAttribute attrs[6];
    SDL_zero(attrs);
    /* Slot 0: position (float3) */
    attrs[0].location    = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset      = 0;
    /* Slot 0: normal (float3) */
    attrs[1].location    = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset      = sizeof(float) * 3;
    /* Slot 1: model_c0..c3 (4 × float4) */
    for (int i = 0; i < 4; i++) {
        attrs[2 + i].location    = (Uint32)(2 + i);
        attrs[2 + i].buffer_slot = 1;
        attrs[2 + i].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[2 + i].offset      = (Uint32)(sizeof(float) * 4 * i);
    }

    SDL_GPUVertexInputState vi;
    SDL_zero(vi);
    vi.vertex_buffer_descriptions = vb_descs;
    vi.num_vertex_buffers         = 2;
    vi.vertex_attributes          = attrs;
    vi.num_vertex_attributes      = 6;

    /* Main instanced pipeline: Blinn-Phong with shadow, back-face cull.
     * Reuses scene.frag.hlsl (same fragment shader as non-instanced). */
    scene->instanced_pipeline = forge_scene__create_scene_pipeline(
        scene, inst_vs, scene->scene_fs, vi,
        SDL_GPU_CULLMODE_BACK, SDL_GPU_FILLMODE_FILL);
    if (!scene->instanced_pipeline)
        SDL_Log("forge_scene: instanced pipeline creation failed: %s",
                SDL_GetError());

    /* Instanced shadow pipeline: depth-only, no cull, depth bias.
     * Shadow layout uses only position + instance matrix (5 attrs). */
    SDL_GPUVertexAttribute shadow_attrs[5];
    SDL_zero(shadow_attrs);
    shadow_attrs[0].location    = 0;
    shadow_attrs[0].buffer_slot = 0;
    shadow_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shadow_attrs[0].offset      = 0;
    for (int i = 0; i < 4; i++) {
        shadow_attrs[1 + i].location    = (Uint32)(1 + i);
        shadow_attrs[1 + i].buffer_slot = 1;
        shadow_attrs[1 + i].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        shadow_attrs[1 + i].offset      = (Uint32)(sizeof(float) * 4 * i);
    }

    SDL_GPUVertexInputState shadow_vi;
    SDL_zero(shadow_vi);
    shadow_vi.vertex_buffer_descriptions = vb_descs;
    shadow_vi.num_vertex_buffers         = 2;
    shadow_vi.vertex_attributes          = shadow_attrs;
    shadow_vi.num_vertex_attributes      = 5;

    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = inst_shadow_vs;
        pi.fragment_shader = shadow_fs;
        pi.vertex_input_state = shadow_vi;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor =
            FORGE_SCENE_SHADOW_BIAS_CONST;
        pi.rasterizer_state.depth_bias_slope_factor =
            FORGE_SCENE_SHADOW_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.num_color_targets         = 0;
        pi.target_info.depth_stencil_format      = scene->shadow_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->instanced_shadow_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->instanced_shadow_pipeline)
            SDL_Log("forge_scene: instanced shadow pipeline failed: %s",
                    SDL_GetError());
    }

    /* Release shaders — pipelines hold internal references */
    SDL_ReleaseGPUShader(scene->device, inst_vs);
    SDL_ReleaseGPUShader(scene->device, inst_shadow_vs);
    SDL_ReleaseGPUShader(scene->device, shadow_fs);

    /* Only mark ready if both pipelines succeeded.  On partial failure,
     * release the one that was created so the next call can retry cleanly. */
    if (scene->instanced_pipeline && scene->instanced_shadow_pipeline) {
        scene->instanced_pipelines_ready = true;
    } else {
        if (scene->instanced_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                                            scene->instanced_pipeline);
            scene->instanced_pipeline = NULL;
        }
        if (scene->instanced_shadow_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                                            scene->instanced_shadow_pipeline);
            scene->instanced_shadow_pipeline = NULL;
        }
    }
}

static void forge_scene_draw_mesh_instanced(ForgeScene *scene,
                                             SDL_GPUBuffer *vb,
                                             SDL_GPUBuffer *ib,
                                             Uint32 index_count,
                                             SDL_GPUBuffer *instance_buffer,
                                             Uint32 instance_count,
                                             const float base_color[4])
{
    if (!scene->pass || !vb || !ib || !instance_buffer ||
        index_count == 0 || instance_count == 0) return;

    if (!scene->instanced_pipelines_ready)
        forge_scene__init_instanced_pipelines(scene);
    if (!scene->instanced_pipeline) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->instanced_pipeline);

    /* Shadow map at fragment sampler slot 0 */
    SDL_GPUTextureSamplerBinding shadow_bind = {
        scene->shadow_map, scene->shadow_sampler
    };
    SDL_BindGPUFragmentSamplers(scene->pass, 0, &shadow_bind, 1);

    /* Slot 0: mesh vertices, Slot 1: instance transforms */
    SDL_GPUBufferBinding vb_binds[2] = {
        { vb, 0 }, { instance_buffer, 0 }
    };
    SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);

    SDL_GPUBufferBinding idx_bind = { ib, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &idx_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Vertex uniforms: VP + light_vp + node_world (192 bytes).
     * Simple meshes have no node hierarchy, so node_world = identity. */
    ForgeSceneInstancedVertUniforms vu;
    vu.vp         = scene->cam_vp;
    vu.light_vp   = scene->light_vp;
    vu.node_world = mat4_identity();
    SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

    /* Fragment uniforms: same Blinn-Phong as non-instanced */
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

    SDL_DrawGPUIndexedPrimitives(scene->pass, index_count, instance_count,
                                  0, 0, 0);
}

/* ── Debug lines ───────────────────────────────────────────────────────── */

/* Internal: ensure debug CPU array has room for count more vertices.
 * Returns true on success, false if allocation failed. */
static bool forge_scene__debug_ensure_capacity(
    ForgeSceneDebugVertex **array, Uint32 *capacity, Uint32 current, Uint32 count)
{
    Uint32 needed = current + count;
    if (needed < current) return false; /* overflow */
    if (needed <= *capacity) return true;

    Uint32 new_cap = *capacity ? *capacity : FORGE_SCENE_DEBUG_VB_INITIAL_CAPACITY;
    while (new_cap < needed) {
        Uint32 next = new_cap * 2;
        if (next <= new_cap) return false; /* overflow */
        new_cap = next;
    }

    ForgeSceneDebugVertex *new_arr = (ForgeSceneDebugVertex *)SDL_realloc(
        *array, new_cap * sizeof(ForgeSceneDebugVertex));
    if (!new_arr) {
        SDL_Log("forge_scene: debug vertex realloc failed (%u vertices)", new_cap);
        return false;
    }
    *array    = new_arr;
    *capacity = new_cap;
    return true;
}

static void forge_scene_debug_line(ForgeScene *scene,
                                    vec3 a, vec3 b, vec4 color, bool overlay)
{
    if (!scene) return;

    ForgeSceneDebugVertex *arr;
    Uint32 *count;
    if (overlay) {
        if (!forge_scene__debug_ensure_capacity(
                &scene->debug_overlay_vertices, &scene->debug_overlay_capacity,
                scene->debug_overlay_count, 2))
            return;
        arr   = scene->debug_overlay_vertices;
        count = &scene->debug_overlay_count;
    } else {
        if (!forge_scene__debug_ensure_capacity(
                &scene->debug_world_vertices, &scene->debug_world_capacity,
                scene->debug_world_count, 2))
            return;
        arr   = scene->debug_world_vertices;
        count = &scene->debug_world_count;
    }

    ForgeSceneDebugVertex va;
    va.position[0] = a.x; va.position[1] = a.y; va.position[2] = a.z;
    va.color[0] = color.x; va.color[1] = color.y; va.color[2] = color.z; va.color[3] = color.w;
    ForgeSceneDebugVertex vb_vert;
    vb_vert.position[0] = b.x; vb_vert.position[1] = b.y; vb_vert.position[2] = b.z;
    vb_vert.color[0] = color.x; vb_vert.color[1] = color.y; vb_vert.color[2] = color.z; vb_vert.color[3] = color.w;
    arr[*count]     = va;
    arr[*count + 1] = vb_vert;
    *count += 2;
}

static void forge_scene_debug_box(ForgeScene *scene,
                                   vec3 center, vec3 half_extents,
                                   vec4 color, bool overlay)
{
    float hx = half_extents.x, hy = half_extents.y, hz = half_extents.z;
    /* 8 corners */
    vec3 c[8];
    c[0] = vec3_create(center.x - hx, center.y - hy, center.z - hz);
    c[1] = vec3_create(center.x + hx, center.y - hy, center.z - hz);
    c[2] = vec3_create(center.x + hx, center.y + hy, center.z - hz);
    c[3] = vec3_create(center.x - hx, center.y + hy, center.z - hz);
    c[4] = vec3_create(center.x - hx, center.y - hy, center.z + hz);
    c[5] = vec3_create(center.x + hx, center.y - hy, center.z + hz);
    c[6] = vec3_create(center.x + hx, center.y + hy, center.z + hz);
    c[7] = vec3_create(center.x - hx, center.y + hy, center.z + hz);
    /* 12 edges */
    int edges[][2] = {
        {0,1},{1,2},{2,3},{3,0},  /* near face (z - hz)  */
        {4,5},{5,6},{6,7},{7,4},  /* far face  (z + hz)  */
        {0,4},{1,5},{2,6},{3,7},  /* connecting edges    */
    };
    for (int i = 0; i < 12; i++)
        forge_scene_debug_line(scene, c[edges[i][0]], c[edges[i][1]],
                               color, overlay);
}

static void forge_scene_debug_circle(ForgeScene *scene,
                                      vec3 center, float radius, vec3 axis,
                                      vec4 color, int segments, bool overlay)
{
    if (segments < FORGE_SCENE_DEBUG_MIN_SEGMENTS)
        segments = FORGE_SCENE_DEBUG_MIN_SEGMENTS;

    /* Build an orthonormal basis from axis.
     * Guard against zero-length axis to avoid degenerate normalize. */
    if (vec3_length(axis) < FORGE_SCENE_LIGHT_DIR_EPSILON)
        axis = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 n = vec3_normalize(axis);
    vec3 up = (SDL_fabsf(n.y) < FORGE_SCENE_DEBUG_AXIS_PARALLEL_COS)
        ? vec3_create(0.0f, 1.0f, 0.0f)
        : vec3_create(1.0f, 0.0f, 0.0f);
    vec3 u = vec3_normalize(vec3_cross(up, n));
    vec3 v = vec3_cross(n, u);

    float step = 2.0f * FORGE_PI / (float)segments;
    vec3 prev;
    for (int i = 0; i <= segments; i++) {
        float angle = step * (float)i;
        float cs = SDL_cosf(angle) * radius;
        float sn = SDL_sinf(angle) * radius;
        vec3 p = vec3_add(center,
            vec3_add(vec3_scale(u, cs), vec3_scale(v, sn)));
        if (i > 0)
            forge_scene_debug_line(scene, prev, p, color, overlay);
        prev = p;
    }
}

static void forge_scene_debug_axes(ForgeScene *scene,
                                    vec3 origin, float length, bool overlay)
{
    vec4 red   = vec4_create(1.0f, 0.2f, 0.2f, 1.0f);
    vec4 green = vec4_create(0.2f, 1.0f, 0.2f, 1.0f);
    vec4 blue  = vec4_create(0.3f, 0.3f, 1.0f, 1.0f);
    forge_scene_debug_line(scene, origin,
        vec3_add(origin, vec3_create(length, 0, 0)), red, overlay);
    forge_scene_debug_line(scene, origin,
        vec3_add(origin, vec3_create(0, length, 0)), green, overlay);
    forge_scene_debug_line(scene, origin,
        vec3_add(origin, vec3_create(0, 0, length)), blue, overlay);
}

static void forge_scene_debug_sphere(ForgeScene *scene,
                                      vec3 center, float radius,
                                      vec4 color, int rings, bool overlay)
{
    if (rings < FORGE_SCENE_DEBUG_MIN_SEGMENTS)
        rings = FORGE_SCENE_DEBUG_MIN_SEGMENTS;
    /* Three perpendicular great circles */
    vec3 x_axis = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 y_axis = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 z_axis = vec3_create(0.0f, 0.0f, 1.0f);
    forge_scene_debug_circle(scene, center, radius, x_axis, color, rings, overlay);
    forge_scene_debug_circle(scene, center, radius, y_axis, color, rings, overlay);
    forge_scene_debug_circle(scene, center, radius, z_axis, color, rings, overlay);
}

/* Internal: lazy-init debug line pipelines and GPU buffer */
static void forge_scene__init_debug_pipelines(ForgeScene *scene)
{
    if (scene->debug_ready) return;

    SDL_GPUShader *debug_vs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_VERTEX,
        debug_vert_spirv, debug_vert_spirv_size,
        debug_vert_dxil,  debug_vert_dxil_size,
        debug_vert_msl,   debug_vert_msl_size,
        0, 0, 0, 1);

    SDL_GPUShader *debug_fs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_FRAGMENT,
        debug_frag_spirv, debug_frag_spirv_size,
        debug_frag_dxil,  debug_frag_dxil_size,
        debug_frag_msl,   debug_frag_msl_size,
        0, 0, 0, 0);

    if (!debug_vs || !debug_fs) {
        SDL_Log("forge_scene: debug shader creation failed");
        if (debug_vs) SDL_ReleaseGPUShader(scene->device, debug_vs);
        if (debug_fs) SDL_ReleaseGPUShader(scene->device, debug_fs);
        return;
    }

    /* Vertex layout: position (float3) + color (float4) = 28 bytes */
    SDL_GPUVertexBufferDescription vb_desc;
    SDL_zero(vb_desc);
    vb_desc.slot       = 0;
    vb_desc.pitch      = sizeof(ForgeSceneDebugVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute debug_attrs[2];
    SDL_zero(debug_attrs);
    debug_attrs[0].location    = 0;
    debug_attrs[0].buffer_slot = 0;
    debug_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    debug_attrs[0].offset      = 0;
    debug_attrs[1].location    = 1;
    debug_attrs[1].buffer_slot = 0;
    debug_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    debug_attrs[1].offset      = sizeof(float) * 3;

    SDL_GPUVertexInputState vi;
    SDL_zero(vi);
    vi.vertex_buffer_descriptions = &vb_desc;
    vi.num_vertex_buffers         = 1;
    vi.vertex_attributes          = debug_attrs;
    vi.num_vertex_attributes      = 2;

    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = scene->swapchain_fmt;

    /* World pipeline: depth test ON, depth write ON */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = debug_vs;
        pi.fragment_shader = debug_fs;
        pi.vertex_input_state = vi;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_LINELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions  = &ctd;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.depth_stencil_format       = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target   = true;
        scene->debug_world_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->debug_world_pipeline)
            SDL_Log("forge_scene: debug world pipeline failed: %s",
                    SDL_GetError());
    }

    /* Overlay pipeline: depth test OFF, depth write OFF */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = debug_vs;
        pi.fragment_shader = debug_fs;
        pi.vertex_input_state = vi;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_LINELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_ALWAYS;
        pi.depth_stencil_state.enable_depth_test  = false;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.color_target_descriptions  = &ctd;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.depth_stencil_format       = scene->depth_fmt;
        pi.target_info.has_depth_stencil_target   = true;
        scene->debug_overlay_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->debug_overlay_pipeline)
            SDL_Log("forge_scene: debug overlay pipeline failed: %s",
                    SDL_GetError());
    }

    SDL_ReleaseGPUShader(scene->device, debug_vs);
    SDL_ReleaseGPUShader(scene->device, debug_fs);

    /* Only mark ready if both pipelines succeeded.  On partial failure,
     * release the one that was created so the next call can retry. */
    if (scene->debug_world_pipeline && scene->debug_overlay_pipeline) {
        scene->debug_ready = true;
    } else {
        if (scene->debug_world_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                                            scene->debug_world_pipeline);
            scene->debug_world_pipeline = NULL;
        }
        if (scene->debug_overlay_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                                            scene->debug_overlay_pipeline);
            scene->debug_overlay_pipeline = NULL;
        }
    }
}

static void forge_scene_draw_debug_lines(ForgeScene *scene)
{
    if (!scene || !scene->cmd || !scene->pass) return;

    Uint32 world_count   = scene->debug_world_count;
    Uint32 overlay_count = scene->debug_overlay_count;
    Uint32 total_count   = world_count + overlay_count;
    if (total_count == 0) return;
    if (total_count < world_count) goto reset; /* overflow */

    if (!scene->debug_ready)
        forge_scene__init_debug_pipelines(scene);
    if (!scene->debug_world_pipeline && !scene->debug_overlay_pipeline) goto reset;

    /* Grow GPU buffer if needed */
    if (total_count > UINT32_MAX / (Uint32)sizeof(ForgeSceneDebugVertex))
        goto reset; /* overflow */
    Uint32 needed_bytes = total_count * (Uint32)sizeof(ForgeSceneDebugVertex);
    if (needed_bytes > scene->debug_vb_capacity) {
        /* Allocate-then-swap: create the new buffer before releasing the old
         * one, so scene->debug_vb never points at freed memory on failure. */
        SDL_GPUBufferCreateInfo bci;
        SDL_zero(bci);
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size  = needed_bytes;
        SDL_GPUBuffer *new_vb = SDL_CreateGPUBuffer(scene->device, &bci);
        if (!new_vb) {
            SDL_Log("forge_scene: debug VB creation failed: %s", SDL_GetError());
            goto reset;
        }
        if (scene->debug_vb)
            SDL_ReleaseGPUBuffer(scene->device, scene->debug_vb);
        scene->debug_vb = new_vb;
        scene->debug_vb_capacity = needed_bytes;
    }

    /* End current render pass to do the upload */
    if (scene->pass) {
        SDL_EndGPURenderPass(scene->pass);
        scene->pass = NULL;
    }

    /* Upload via transfer buffer */
    bool upload_ok = false;
    {
        SDL_GPUTransferBufferCreateInfo tbci;
        SDL_zero(tbci);
        tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbci.size  = needed_bytes;
        SDL_GPUTransferBuffer *tb =
            SDL_CreateGPUTransferBuffer(scene->device, &tbci);
        if (!tb) {
            SDL_Log("forge_scene: debug transfer buffer failed: %s",
                    SDL_GetError());
            goto reopen;
        }

        void *mapped = SDL_MapGPUTransferBuffer(scene->device, tb, false);
        if (!mapped) {
            SDL_Log("forge_scene: debug transfer map failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(scene->device, tb);
            goto reopen;
        }

        /* Copy world vertices first, then overlay */
        if (world_count > 0)
            SDL_memcpy(mapped, scene->debug_world_vertices,
                       world_count * sizeof(ForgeSceneDebugVertex));
        if (overlay_count > 0)
            SDL_memcpy((Uint8 *)mapped + world_count * sizeof(ForgeSceneDebugVertex),
                       scene->debug_overlay_vertices,
                       overlay_count * sizeof(ForgeSceneDebugVertex));

        SDL_UnmapGPUTransferBuffer(scene->device, tb);

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(scene->cmd);
        if (copy) {
            SDL_GPUTransferBufferLocation src = { tb, 0 };
            SDL_GPUBufferRegion dst = { scene->debug_vb, 0, needed_bytes };
            SDL_UploadToGPUBuffer(copy, &src, &dst, false);
            SDL_EndGPUCopyPass(copy);
            upload_ok = true;
        } else {
            SDL_Log("forge_scene: debug copy pass failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(scene->device, tb);
            goto reopen;
        }
        SDL_ReleaseGPUTransferBuffer(scene->device, tb);
    }

reopen:
    /* Re-open the main render pass with LOAD (preserve color + depth) */
    {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture  = scene->swapchain;
        color_target.load_op  = SDL_GPU_LOADOP_LOAD;
        color_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depth_target;
        SDL_zero(depth_target);
        depth_target.texture     = scene->depth_texture;
        depth_target.load_op     = SDL_GPU_LOADOP_LOAD;
        depth_target.store_op    = SDL_GPU_STOREOP_STORE;
        depth_target.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
        depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        scene->pass = SDL_BeginGPURenderPass(
            scene->cmd, &color_target, 1, &depth_target);
        if (!scene->pass) {
            SDL_Log("forge_scene: debug pass reopen failed: %s",
                    SDL_GetError());
            goto reset;
        }

        SDL_GPUViewport vp = {
            0, 0, (float)scene->sw, (float)scene->sh, 0.0f, 1.0f
        };
        SDL_SetGPUViewport(scene->pass, &vp);
        SDL_SetGPUScissor(scene->pass,
            &(SDL_Rect){ 0, 0, (int)scene->sw, (int)scene->sh });
    }

    /* Draw world lines (depth-tested) — only if upload succeeded */
    if (upload_ok && world_count > 0 && scene->debug_world_pipeline) {
        SDL_BindGPUGraphicsPipeline(scene->pass, scene->debug_world_pipeline);
        SDL_GPUBufferBinding vb_bind = { scene->debug_vb, 0 };
        SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);

        ForgeSceneDebugVertUniforms du;
        du.vp = scene->cam_vp;
        SDL_PushGPUVertexUniformData(scene->cmd, 0, &du, sizeof(du));

        SDL_DrawGPUPrimitives(scene->pass, world_count, 1, 0, 0);
    }

    /* Draw overlay lines (always on top) — only if upload succeeded */
    if (upload_ok && overlay_count > 0 && scene->debug_overlay_pipeline) {
        SDL_BindGPUGraphicsPipeline(scene->pass, scene->debug_overlay_pipeline);
        SDL_GPUBufferBinding vb_bind = { scene->debug_vb, 0 };
        SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);

        ForgeSceneDebugVertUniforms du;
        du.vp = scene->cam_vp;
        SDL_PushGPUVertexUniformData(scene->cmd, 0, &du, sizeof(du));

        SDL_DrawGPUPrimitives(scene->pass, overlay_count, 1, world_count, 0);
    }

reset:
    scene->debug_world_count   = 0;
    scene->debug_overlay_count = 0;
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
    if (scene->model_copy_pass) {
        SDL_EndGPUCopyPass(scene->model_copy_pass);
        scene->model_copy_pass = NULL;
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
    if (scene->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->shadow_pipeline);
    if (scene->shadow_pipeline_pos)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->shadow_pipeline_pos);
    if (scene->shadow_pipeline_tex)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->shadow_pipeline_tex);
    if (scene->textured_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->textured_pipeline);

    /* Instanced mesh pipelines */
    if (scene->instanced_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->instanced_pipeline);
    if (scene->instanced_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->instanced_shadow_pipeline);

    /* Debug line resources */
    if (scene->debug_world_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->debug_world_pipeline);
    if (scene->debug_overlay_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->debug_overlay_pipeline);
    if (scene->debug_vb)
        SDL_ReleaseGPUBuffer(scene->device, scene->debug_vb);
    SDL_free(scene->debug_world_vertices);
    SDL_free(scene->debug_overlay_vertices);

    /* Scene shaders (kept alive for forge_scene_create_pipeline) */
    if (scene->scene_vs)
        SDL_ReleaseGPUShader(scene->device, scene->scene_vs);
    if (scene->scene_fs)
        SDL_ReleaseGPUShader(scene->device, scene->scene_fs);

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
    if (scene->model_shadow_mask_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_shadow_mask_pipeline);
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
    if (scene->morph_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline);
    if (scene->morph_pipeline_blend)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline_blend);
    if (scene->morph_pipeline_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline_double);
    if (scene->morph_pipeline_blend_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline_blend_double);
    if (scene->morph_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_shadow_pipeline);
    if (scene->skinned_shadow_mask_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_shadow_mask_pipeline);

    /* Instanced model pipelines */
    if (scene->model_instanced_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_instanced_pipeline);
    if (scene->model_instanced_pipeline_blend)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_instanced_pipeline_blend);
    if (scene->model_instanced_pipeline_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_instanced_pipeline_double);
    if (scene->model_instanced_pipeline_blend_double)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_instanced_pipeline_blend_double);
    if (scene->model_instanced_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_instanced_shadow_pipeline);
    if (scene->model_instanced_shadow_mask_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_instanced_shadow_mask_pipeline);
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

    /* D3D12 mip offsets must be aligned to 512 bytes
     * (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT).  For BC formats (4x4
     * blocks, 16 bytes/block), small mips have row pitches below 256.
     * Row pitch alignment (FORGE_SCENE_D3D12_PITCH_ALIGN) is defined at
     * file scope and shared with the uncompressed upload functions. */
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

    /* Shadow mask pipeline: depth + alpha test for MASK materials.
     * Uses the shadow_mask shaders which sample the base color texture
     * and discard fragments below the material's alpha_cutoff. */
    SDL_GPUShader *smask_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        shadow_mask_vert_spirv, sizeof(shadow_mask_vert_spirv),
        shadow_mask_vert_dxil,  sizeof(shadow_mask_vert_dxil),
        shadow_mask_vert_msl, shadow_mask_vert_msl_size,
        0, 0, 0, 1);  /* 0 samplers, 0 storage tex, 0 storage buf, 1 uniform */

    SDL_GPUShader *smask_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_mask_frag_spirv, sizeof(shadow_mask_frag_spirv),
        shadow_mask_frag_dxil,  sizeof(shadow_mask_frag_dxil),
        shadow_mask_frag_msl, shadow_mask_frag_msl_size,
        1, 0, 0, 1);  /* 1 sampler (base color), 0 storage, 0 buf, 1 uniform */

    if (smask_vs && smask_fs) {
        /* Vertex input: position + normal (unused) + uv from 48-byte stride */
        SDL_GPUVertexAttribute smask_attrs[3];
        SDL_zero(smask_attrs);
        smask_attrs[0].location = 0;
        smask_attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        smask_attrs[0].offset   = 0;   /* position */
        smask_attrs[1].location = 1;
        smask_attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        smask_attrs[1].offset   = 12;  /* normal (passed through but unused) */
        smask_attrs[2].location = 2;
        smask_attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        smask_attrs[2].offset   = 24;  /* uv */

        SDL_GPUVertexInputState smask_input;
        SDL_zero(smask_input);
        smask_input.vertex_buffer_descriptions = &model_vbd;
        smask_input.num_vertex_buffers         = 1;
        smask_input.vertex_attributes          = smask_attrs;
        smask_input.num_vertex_attributes      = 3;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = smask_vs;
        pi.fragment_shader    = smask_fs;
        pi.vertex_input_state = smask_input;
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
        scene->model_shadow_mask_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->model_shadow_mask_pipeline) {
            SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                    "(model_shadow_mask_pipeline) failed: %s",
                    SDL_GetError());
        }
    }
    if (smask_vs) SDL_ReleaseGPUShader(scene->device, smask_vs);
    if (smask_fs) SDL_ReleaseGPUShader(scene->device, smask_fs);

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
    if (scene->model_shadow_mask_pipeline)
        { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->model_shadow_mask_pipeline); scene->model_shadow_mask_pipeline = NULL; }
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

/* ── Centroid computation helper ──────────────────────────────────────── */

static void forge_scene_compute_centroids(
    const ForgePipelineMesh *mesh,
    vec3 *out_centroids, uint32_t max_submeshes)
{
    if (!mesh || !out_centroids) return;
    if (!mesh->vertices || !mesh->indices) return;
    if (mesh->lod_count == 0 || !mesh->lods) return;

    uint32_t sc = mesh->submesh_count;
    if (sc > max_submeshes) sc = max_submeshes;

    const uint8_t  *verts   = (const uint8_t *)mesh->vertices;
    uint32_t        stride  = mesh->vertex_stride;
    uint32_t lod0_off       = mesh->lods[0].index_offset;
    uint32_t lod0_idx_count = mesh->lods[0].index_count;

    /* Guard against undersized/misaligned stride or misaligned index offset */
    if (stride < (uint32_t)(3 * sizeof(float)) ||
        (stride % sizeof(float)) != 0 ||
        (lod0_off % sizeof(uint32_t)) != 0) {
        for (uint32_t s = 0; s < sc; s++)
            out_centroids[s] = vec3_create(0, 0, 0);
        return;
    }

    /* Rebase indices to the LOD 0 start so first_idx is relative */
    const uint32_t *indices = (const uint32_t *)(
        (const uint8_t *)mesh->indices + lod0_off);

    for (uint32_t s = 0; s < sc; s++) {
        const ForgePipelineSubmesh *sub =
            forge_pipeline_lod_submesh(mesh, 0, s);
        if (!sub || sub->index_count == 0) {
            out_centroids[s] = vec3_create(0, 0, 0);
            continue;
        }
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        if (sub->index_offset < lod0_off ||
            ((sub->index_offset - lod0_off) % sizeof(uint32_t)) != 0) {
            out_centroids[s] = vec3_create(0, 0, 0);
            continue;
        }
        uint32_t first_idx =
            (sub->index_offset - lod0_off) / sizeof(uint32_t);
        if (first_idx >= lod0_idx_count ||
            sub->index_count > (lod0_idx_count - first_idx)) {
            out_centroids[s] = vec3_create(0, 0, 0);
            continue;
        }
        uint32_t valid_samples = 0;
        for (uint32_t i = 0; i < sub->index_count; i++) {
            uint32_t vi = indices[first_idx + i];
            if (vi >= mesh->vertex_count) continue;
            const float *pos =
                (const float *)(verts + (uint64_t)vi * stride);
            cx += pos[0]; cy += pos[1]; cz += pos[2];
            valid_samples++;
        }
        if (valid_samples == 0) {
            out_centroids[s] = vec3_create(0, 0, 0);
            continue;
        }
        float inv = 1.0f / (float)valid_samples;
        out_centroids[s] = vec3_create(cx * inv, cy * inv, cz * inv);
    }
}

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

    /* Precompute per-submesh centroids for transparency sorting */
    model->submesh_centroid_count = model->mesh.submesh_count;
    if (model->submesh_centroid_count > FORGE_SCENE_MODEL_MAX_SUBMESHES)
        model->submesh_centroid_count = FORGE_SCENE_MODEL_MAX_SUBMESHES;
    forge_scene_compute_centroids(&model->mesh, model->submesh_centroids,
                                   FORGE_SCENE_MODEL_MAX_SUBMESHES);

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

/* ── Transparent draw sort ───────────────────────────────────────────────── */

/* Comparison function for SDL_qsort: back-to-front (farther draws first). */
static int forge_scene__transparent_cmp(const void *a, const void *b)
{
    const ForgeSceneTransparentDraw *ta = (const ForgeSceneTransparentDraw *)a;
    const ForgeSceneTransparentDraw *tb = (const ForgeSceneTransparentDraw *)b;
    float da = ta->sort_depth;
    float db = tb->sort_depth;
    if (da > db) return -1; /* farther first */
    if (da < db) return  1;
    /* Deterministic tie-break: node index, then submesh index */
    if (ta->node_index < tb->node_index) return -1;
    if (ta->node_index > tb->node_index) return  1;
    if (ta->submesh_index < tb->submesh_index) return -1;
    if (ta->submesh_index > tb->submesh_index) return  1;
    return 0;
}

/* ── Draw model (two-pass: opaque first, then sorted transparent) ───────── */

static void forge_scene_draw_model(ForgeScene *scene,
                                    ForgeSceneModel *model,
                                    mat4 placement)
{
    if (!scene || !model || !scene->pass) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0) return;

    model->draw_calls = 0;
    model->transparent_draw_calls = 0;

    /* Precompute camera forward for view-depth sorting (invariant per call) */
    quat cam_q = quat_from_euler(scene->cam_yaw, scene->cam_pitch, 0.0f);
    vec3 cam_fwd = quat_forward(cam_q);

    /* LOD 0 base index offset (in bytes) — we uploaded from this offset,
     * so GPU-side indices start at 0 relative to our uploaded buffer. */
    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    /* Bind vertex + index buffers once for all submeshes */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUGraphicsPipeline *last_pipeline = NULL;

    /* Collect BLEND submeshes for deferred sorted drawing.
     * ~19 KB on the stack (256 × 76 bytes) — acceptable for desktop. */
    ForgeSceneTransparentDraw blend_draws[FORGE_SCENE_MAX_TRANSPARENT_DRAWS];
    uint32_t blend_count = 0;

    /* ── Pass 1: draw opaque + MASK submeshes immediately ──────────── */

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

            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count &&
                (uint32_t)sub->material_index < model->mat_texture_count) {
                mat = &model->materials.materials[sub->material_index];
                textures = &model->mat_textures[sub->material_index];
            }

            /* Defer BLEND submeshes to pass 2 (unless sorting is disabled) */
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND) {
                if (scene->transparency_sorting) {
                    if (blend_count < FORGE_SCENE_MAX_TRANSPARENT_DRAWS) {
                        ForgeSceneTransparentDraw *td =
                            &blend_draws[blend_count++];
                        td->node_index    = n;
                        td->submesh_index = submesh_idx;
                        td->final_world   = final_world;

                        vec3 centroid =
                            (submesh_idx < model->submesh_centroid_count)
                            ? model->submesh_centroids[submesh_idx]
                            : vec3_create(0, 0, 0);
                        vec4 wc4 = mat4_multiply_vec4(final_world,
                            vec4_create(centroid.x, centroid.y,
                                        centroid.z, 1.0f));
                        vec3 wc = vec3_create(wc4.x, wc4.y, wc4.z);
                        td->sort_depth = vec3_dot(
                            vec3_sub(wc, scene->cam_position), cam_fwd);
                        continue; /* deferred to pass 2 */
                    }
                    /* Queue full — fall through to draw unsorted */
                }
                /* Sorting disabled or queue full: draw immediately */
            }

            /* Select pipeline variant */
            SDL_GPUGraphicsPipeline *pipeline;
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND &&
                mat->double_sided)
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

            ForgeSceneVertUniforms vu;
            vu.mvp      = mat4_multiply(scene->cam_vp, final_world);
            vu.model    = final_world;
            vu.light_vp = mat4_multiply(scene->light_vp, final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

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

    /* ── Pass 2: sort and draw BLEND submeshes back-to-front ───────── */

    if (blend_count > 0) {
        SDL_qsort(blend_draws, blend_count,
                  sizeof(ForgeSceneTransparentDraw),
                  forge_scene__transparent_cmp);

        for (uint32_t i = 0; i < blend_count; i++) {
            const ForgeSceneTransparentDraw *td = &blend_draws[i];
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, td->submesh_index);
            if (!sub || sub->index_count == 0) continue;

            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count &&
                (uint32_t)sub->material_index < model->mat_texture_count) {
                mat = &model->materials.materials[sub->material_index];
                textures = &model->mat_textures[sub->material_index];
            }

            SDL_GPUGraphicsPipeline *pipeline;
            if (mat && mat->double_sided)
                pipeline = scene->model_pipeline_blend_double;
            else
                pipeline = scene->model_pipeline_blend;

            if (pipeline != last_pipeline) {
                SDL_BindGPUGraphicsPipeline(scene->pass, pipeline);
                last_pipeline = pipeline;
            }

            forge_scene__bind_model_textures(scene, textures);

            ForgeSceneVertUniforms vu;
            vu.mvp      = mat4_multiply(scene->cam_vp, td->final_world);
            vu.model    = td->final_world;
            vu.light_vp = mat4_multiply(scene->light_vp, td->final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

            ForgeSceneModelFragUniforms fu;
            forge_scene__fill_model_frag_uniforms(scene, mat, &fu);
            SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
            model->draw_calls++;
            model->transparent_draw_calls++;
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

    /* Bind vertex + index buffers */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    /* Track which pipeline is bound to avoid redundant switches */
    SDL_GPUGraphicsPipeline *last_shadow_pipe = NULL;

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

            /* Determine alpha mode for this submesh */
            int amode = FORGE_PIPELINE_ALPHA_OPAQUE;
            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count) {
                mat = &model->materials.materials[sub->material_index];
                amode = mat->alpha_mode;
                if ((uint32_t)sub->material_index < model->mat_texture_count)
                    textures = &model->mat_textures[sub->material_index];
            }

            /* Skip BLEND — transparent materials should not cast shadows */
            if (amode == FORGE_PIPELINE_ALPHA_BLEND)
                continue;

            ForgeSceneShadowVertUniforms su;
            su.light_vp = mat4_multiply(scene->light_vp, final_world);

            if (amode == FORGE_PIPELINE_ALPHA_MASK &&
                scene->model_shadow_mask_pipeline) {
                /* MASK: use shadow mask pipeline with alpha test */
                if (last_shadow_pipe != scene->model_shadow_mask_pipeline) {
                    SDL_BindGPUGraphicsPipeline(scene->pass,
                        scene->model_shadow_mask_pipeline);
                    last_shadow_pipe = scene->model_shadow_mask_pipeline;
                }
                SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

                /* Bind base color texture for alpha sampling */
                SDL_GPUTextureSamplerBinding mask_tex;
                mask_tex.texture = (textures && textures->base_color)
                    ? textures->base_color : scene->model_white_texture;
                mask_tex.sampler = scene->model_tex_sampler;
                SDL_BindGPUFragmentSamplers(scene->pass, 0, &mask_tex, 1);

                /* Push alpha cutoff uniform */
                ForgeSceneShadowMaskFragUniforms mfu;
                SDL_zero(mfu);
                if (mat) {
                    SDL_memcpy(mfu.base_color_factor,
                               mat->base_color_factor, 4 * sizeof(float));
                    mfu.alpha_cutoff = mat->alpha_cutoff;
                } else {
                    mfu.base_color_factor[0] = 1.0f;
                    mfu.base_color_factor[1] = 1.0f;
                    mfu.base_color_factor[2] = 1.0f;
                    mfu.base_color_factor[3] = 1.0f;
                    mfu.alpha_cutoff = FORGE_SCENE_DEFAULT_ALPHA_CUTOFF;
                }
                SDL_PushGPUFragmentUniformData(scene->cmd, 0,
                                               &mfu, sizeof(mfu));
            } else {
                /* OPAQUE: use standard depth-only shadow pipeline */
                if (last_shadow_pipe != scene->model_shadow_pipeline) {
                    SDL_BindGPUGraphicsPipeline(scene->pass,
                        scene->model_shadow_pipeline);
                    last_shadow_pipe = scene->model_shadow_pipeline;
                }
                SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));
            }

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
        }
    }
}

/* ── Instanced model pipelines + draw ───────────────────────────────────── */

static void forge_scene__init_model_instanced_pipelines(ForgeScene *scene)
{
    if (scene->model_instanced_pipelines_ready) return;

    /* Ensure base model resources (samplers, fallback textures) are ready.
     * Do not set the ready flag — allows retry on next call. */
    if (!scene->model_pipelines_ready) return;

    /* Create instanced model vertex shader */
    SDL_GPUShader *inst_vs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_model_instanced_vert_spirv, scene_model_instanced_vert_spirv_size,
        scene_model_instanced_vert_dxil,  scene_model_instanced_vert_dxil_size,
        scene_model_instanced_vert_msl,   scene_model_instanced_vert_msl_size,
        0, 0, 0, 1);

    /* Create instanced model shadow vertex shader */
    SDL_GPUShader *inst_shadow_vs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_VERTEX,
        scene_model_instanced_shadow_vert_spirv,
        scene_model_instanced_shadow_vert_spirv_size,
        scene_model_instanced_shadow_vert_dxil,
        scene_model_instanced_shadow_vert_dxil_size,
        scene_model_instanced_shadow_vert_msl,
        scene_model_instanced_shadow_vert_msl_size,
        0, 0, 0, 1);

    /* Shadow fragment shader — SDL3 requires non-NULL even for depth-only. */
    SDL_GPUShader *shadow_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl,   shadow_frag_msl_size,
        0, 0, 0, 0);

    if (!inst_vs || !inst_shadow_vs || !shadow_fs) {
        SDL_Log("forge_scene: instanced model shader creation failed");
        if (inst_vs)        SDL_ReleaseGPUShader(scene->device, inst_vs);
        if (inst_shadow_vs) SDL_ReleaseGPUShader(scene->device, inst_shadow_vs);
        if (shadow_fs)      SDL_ReleaseGPUShader(scene->device, shadow_fs);
        return;
    }

    /* Reuse the model fragment shader from the regular model pipelines.
     * We need a reference to it — create a fresh one for pipeline creation. */
    SDL_GPUShader *model_fs = forge_scene_create_shader(
        scene, SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_model_frag_spirv, scene_model_frag_spirv_size,
        scene_model_frag_dxil,  scene_model_frag_dxil_size,
        scene_model_frag_msl,   scene_model_frag_msl_size,
        6, 0, 0, 1);  /* 6 samplers: shadow_cmp + base + normal + mr + occ + emissive */

    if (!model_fs) {
        SDL_Log("forge_scene: instanced model fragment shader failed");
        SDL_ReleaseGPUShader(scene->device, inst_vs);
        SDL_ReleaseGPUShader(scene->device, inst_shadow_vs);
        SDL_ReleaseGPUShader(scene->device, shadow_fs);
        return;
    }

    /* Vertex layout: slot 0 = model vertex (48-byte), slot 1 = instance (64-byte) */
    SDL_GPUVertexBufferDescription vb_descs[2];
    SDL_zero(vb_descs);
    vb_descs[0].slot       = 0;
    vb_descs[0].pitch      = sizeof(ForgeSceneModelVertex);
    vb_descs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_descs[1].slot       = 1;
    vb_descs[1].pitch      = sizeof(mat4);
    vb_descs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
    vb_descs[1].instance_step_rate = 1;

    SDL_GPUVertexAttribute attrs[8];
    SDL_zero(attrs);
    /* Slot 0: position (float3) */
    attrs[0].location = 0; attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = 0;
    /* Slot 0: normal (float3) */
    attrs[1].location = 1; attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = sizeof(float) * 3;
    /* Slot 0: uv (float2) */
    attrs[2].location = 2; attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset = sizeof(float) * 6;
    /* Slot 0: tangent (float4) */
    attrs[3].location = 3; attrs[3].buffer_slot = 0;
    attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attrs[3].offset = sizeof(float) * 8;
    /* Slot 1: instance model_c0..c3 (4 × float4) */
    for (int i = 0; i < 4; i++) {
        attrs[4 + i].location    = (Uint32)(4 + i);
        attrs[4 + i].buffer_slot = 1;
        attrs[4 + i].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[4 + i].offset      = (Uint32)(sizeof(float) * 4 * i);
    }

    SDL_GPUVertexInputState vi;
    SDL_zero(vi);
    vi.vertex_buffer_descriptions = vb_descs;
    vi.num_vertex_buffers         = 2;
    vi.vertex_attributes          = attrs;
    vi.num_vertex_attributes      = 8;

    /* Helper: create a model instanced pipeline with given blend/cull state */
    #define FORGE__CREATE_MODEL_INST_PIPELINE(out, cull, has_blend, write_depth) do { \
        SDL_GPUColorTargetDescription ctd;                                           \
        SDL_zero(ctd);                                                               \
        ctd.format = scene->swapchain_fmt;                                           \
        if (has_blend) {                                                             \
            ctd.blend_state.enable_blend = true;                                     \
            ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;   \
            ctd.blend_state.dst_color_blendfactor =                                  \
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;                             \
            ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;                    \
            ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;         \
            ctd.blend_state.dst_alpha_blendfactor =                                  \
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;                             \
            ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;                    \
        }                                                                            \
        SDL_GPUGraphicsPipelineCreateInfo pi;                                        \
        SDL_zero(pi);                                                                \
        pi.vertex_shader   = inst_vs;                                                \
        pi.fragment_shader = model_fs;                                               \
        pi.vertex_input_state = vi;                                                  \
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;                      \
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;                      \
        pi.rasterizer_state.cull_mode  = (cull);                                     \
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;        \
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;          \
        pi.depth_stencil_state.enable_depth_test  = true;                            \
        pi.depth_stencil_state.enable_depth_write = (write_depth);                   \
        pi.depth_stencil_state.enable_stencil_test = false;                          \
        pi.target_info.color_target_descriptions  = &ctd;                            \
        pi.target_info.num_color_targets          = 1;                               \
        pi.target_info.depth_stencil_format       = scene->depth_fmt;                \
        pi.target_info.has_depth_stencil_target   = true;                            \
        (out) = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);                   \
        if (!(out)) SDL_Log("forge_scene: model instanced pipeline failed: %s",      \
                            SDL_GetError());                                          \
    } while (0)

    FORGE__CREATE_MODEL_INST_PIPELINE(scene->model_instanced_pipeline,
        SDL_GPU_CULLMODE_BACK, false, true);
    FORGE__CREATE_MODEL_INST_PIPELINE(scene->model_instanced_pipeline_blend,
        SDL_GPU_CULLMODE_BACK, true, false);
    FORGE__CREATE_MODEL_INST_PIPELINE(scene->model_instanced_pipeline_double,
        SDL_GPU_CULLMODE_NONE, false, true);
    FORGE__CREATE_MODEL_INST_PIPELINE(scene->model_instanced_pipeline_blend_double,
        SDL_GPU_CULLMODE_NONE, true, false);

    #undef FORGE__CREATE_MODEL_INST_PIPELINE

    /* Shadow pipelines for instanced models */
    /* Shadow vertex layout: position + uv + instance matrix (6 attrs) */
    SDL_GPUVertexAttribute shadow_attrs[6];
    SDL_zero(shadow_attrs);
    shadow_attrs[0].location = 0; shadow_attrs[0].buffer_slot = 0;
    shadow_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    shadow_attrs[0].offset = 0;
    shadow_attrs[1].location = 1; shadow_attrs[1].buffer_slot = 0;
    shadow_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    shadow_attrs[1].offset = sizeof(float) * 6;  /* uv at offset 24 */
    for (int i = 0; i < 4; i++) {
        shadow_attrs[2 + i].location    = (Uint32)(2 + i);
        shadow_attrs[2 + i].buffer_slot = 1;
        shadow_attrs[2 + i].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        shadow_attrs[2 + i].offset      = (Uint32)(sizeof(float) * 4 * i);
    }

    SDL_GPUVertexInputState shadow_vi;
    SDL_zero(shadow_vi);
    shadow_vi.vertex_buffer_descriptions = vb_descs;
    shadow_vi.num_vertex_buffers         = 2;
    shadow_vi.vertex_attributes          = shadow_attrs;
    shadow_vi.num_vertex_attributes      = 6;

    /* Opaque shadow pipeline */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = inst_shadow_vs;
        pi.fragment_shader = shadow_fs;
        pi.vertex_input_state = shadow_vi;
        pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor =
            FORGE_SCENE_SHADOW_BIAS_CONST;
        pi.rasterizer_state.depth_bias_slope_factor =
            FORGE_SCENE_SHADOW_BIAS_SLOPE;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.enable_stencil_test = false;
        pi.target_info.num_color_targets         = 0;
        pi.target_info.depth_stencil_format      = scene->shadow_fmt;
        pi.target_info.has_depth_stencil_target  = true;
        scene->model_instanced_shadow_pipeline =
            SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
        if (!scene->model_instanced_shadow_pipeline)
            SDL_Log("forge_scene: model instanced shadow pipeline failed: %s",
                    SDL_GetError());
    }

    /* MASK shadow pipeline — reuse shadow_mask.frag.hlsl.
     * The instanced shadow VS outputs UV for alpha test. */
    {
        /* Get the shadow mask fragment shader (same as non-instanced) */
        SDL_GPUShader *mask_fs = forge_scene_create_shader(
            scene, SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_mask_frag_spirv, shadow_mask_frag_spirv_size,
            shadow_mask_frag_dxil,  shadow_mask_frag_dxil_size,
            shadow_mask_frag_msl,   shadow_mask_frag_msl_size,
            1, 0, 0, 1);

        if (mask_fs) {
            SDL_GPUGraphicsPipelineCreateInfo pi;
            SDL_zero(pi);
            pi.vertex_shader   = inst_shadow_vs;
            pi.fragment_shader = mask_fs;
            pi.vertex_input_state = shadow_vi;
            pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
            pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            pi.rasterizer_state.enable_depth_bias = true;
            pi.rasterizer_state.depth_bias_constant_factor =
                FORGE_SCENE_SHADOW_BIAS_CONST;
            pi.rasterizer_state.depth_bias_slope_factor =
                FORGE_SCENE_SHADOW_BIAS_SLOPE;
            pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
            pi.depth_stencil_state.enable_depth_test  = true;
            pi.depth_stencil_state.enable_depth_write = true;
            pi.depth_stencil_state.enable_stencil_test = false;
            pi.target_info.num_color_targets         = 0;
            pi.target_info.depth_stencil_format      = scene->shadow_fmt;
            pi.target_info.has_depth_stencil_target  = true;
            scene->model_instanced_shadow_mask_pipeline =
                SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
            if (!scene->model_instanced_shadow_mask_pipeline)
                SDL_Log("forge_scene: model instanced shadow mask pipeline "
                        "failed: %s", SDL_GetError());
            SDL_ReleaseGPUShader(scene->device, mask_fs);
        }
    }

    SDL_ReleaseGPUShader(scene->device, inst_vs);
    SDL_ReleaseGPUShader(scene->device, inst_shadow_vs);
    SDL_ReleaseGPUShader(scene->device, model_fs);
    SDL_ReleaseGPUShader(scene->device, shadow_fs);

    /* Only mark ready if all required pipelines succeeded.  On failure,
     * release any partial pipelines so the next call can retry. */
    if (scene->model_instanced_pipeline &&
        scene->model_instanced_pipeline_blend &&
        scene->model_instanced_pipeline_double &&
        scene->model_instanced_pipeline_blend_double &&
        scene->model_instanced_shadow_pipeline) {
        scene->model_instanced_pipelines_ready = true;
    } else {
        /* Release any variant that did get created */
        if (scene->model_instanced_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                scene->model_instanced_pipeline);
            scene->model_instanced_pipeline = NULL;
        }
        if (scene->model_instanced_pipeline_blend) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                scene->model_instanced_pipeline_blend);
            scene->model_instanced_pipeline_blend = NULL;
        }
        if (scene->model_instanced_pipeline_double) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                scene->model_instanced_pipeline_double);
            scene->model_instanced_pipeline_double = NULL;
        }
        if (scene->model_instanced_pipeline_blend_double) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                scene->model_instanced_pipeline_blend_double);
            scene->model_instanced_pipeline_blend_double = NULL;
        }
        if (scene->model_instanced_shadow_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                scene->model_instanced_shadow_pipeline);
            scene->model_instanced_shadow_pipeline = NULL;
        }
        if (scene->model_instanced_shadow_mask_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(scene->device,
                scene->model_instanced_shadow_mask_pipeline);
            scene->model_instanced_shadow_mask_pipeline = NULL;
        }
    }
}

static void forge_scene_draw_model_instanced(ForgeScene *scene,
                                              ForgeSceneModel *model,
                                              SDL_GPUBuffer *instance_buffer,
                                              Uint32 instance_count)
{
    if (!scene || !model || !scene->pass || !instance_buffer) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0 || instance_count == 0) return;

    if (!scene->model_instanced_pipelines_ready)
        forge_scene__init_model_instanced_pipelines(scene);
    if (!scene->model_instanced_pipeline) return;

    model->draw_calls = 0;
    model->transparent_draw_calls = 0;

    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    /* Bind slot 0: model vertices, slot 1: instance transforms */
    SDL_GPUBufferBinding vb_binds[2] = {
        { model->vertex_buffer, 0 }, { instance_buffer, 0 }
    };
    SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);

    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUGraphicsPipeline *last_pipeline = NULL;

    for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
        const ForgePipelineSceneNode *node = &model->scene_data.nodes[n];
        if (node->mesh_index < 0) continue;

        /* Extract node's world transform from the hierarchy */
        mat4 node_world;
        SDL_memcpy(&node_world, node->world_transform, sizeof(mat4));

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
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND &&
                mat->double_sided)
                pipeline = scene->model_instanced_pipeline_blend_double;
            else if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND)
                pipeline = scene->model_instanced_pipeline_blend;
            else if (mat && mat->double_sided)
                pipeline = scene->model_instanced_pipeline_double;
            else
                pipeline = scene->model_instanced_pipeline;

            if (pipeline != last_pipeline) {
                SDL_BindGPUGraphicsPipeline(scene->pass, pipeline);
                last_pipeline = pipeline;
                /* Re-bind vertex buffers after pipeline switch */
                SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);
                SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                                       SDL_GPU_INDEXELEMENTSIZE_32BIT);
            }

            forge_scene__bind_model_textures(scene, textures);

            /* Vertex uniforms: VP + light_vp + node_world (192 bytes).
             * node_world applies the glTF node hierarchy transform per submesh.
             * The per-instance model matrix is applied in the shader. */
            ForgeSceneInstancedVertUniforms vu;
            vu.vp         = scene->cam_vp;
            vu.light_vp   = scene->light_vp;
            vu.node_world = node_world;
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

            ForgeSceneModelFragUniforms fu;
            forge_scene__fill_model_frag_uniforms(scene, mat, &fu);
            SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, instance_count, first_index, 0, 0);
            model->draw_calls++;
        }
    }
}

static void forge_scene_draw_model_shadows_instanced(ForgeScene *scene,
                                                      ForgeSceneModel *model,
                                                      SDL_GPUBuffer *instance_buffer,
                                                      Uint32 instance_count)
{
    if (!scene || !model || !scene->pass || !instance_buffer) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0 || instance_count == 0) return;

    if (!scene->model_instanced_pipelines_ready)
        forge_scene__init_model_instanced_pipelines(scene);
    if (!scene->model_instanced_shadow_pipeline) return;

    uint32_t lod0_base_offset = model->mesh.lods[0].index_offset;

    /* Bind slot 0: model vertices, slot 1: instance transforms */
    SDL_GPUBufferBinding vb_binds[2] = {
        { model->vertex_buffer, 0 }, { instance_buffer, 0 }
    };
    SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);

    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUGraphicsPipeline *last_shadow_pipe = NULL;

    for (uint32_t n = 0; n < model->scene_data.node_count; n++) {
        const ForgePipelineSceneNode *node = &model->scene_data.nodes[n];
        if (node->mesh_index < 0) continue;

        mat4 node_world;
        SDL_memcpy(&node_world, node->world_transform, sizeof(mat4));

        const ForgePipelineSceneMesh *smesh =
            forge_pipeline_scene_get_mesh(&model->scene_data,
                                          (uint32_t)node->mesh_index);
        if (!smesh) continue;

        for (uint32_t si = 0; si < smesh->submesh_count; si++) {
            uint32_t submesh_idx = smesh->first_submesh + si;
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, submesh_idx);
            if (!sub || sub->index_count == 0) continue;

            int amode = FORGE_PIPELINE_ALPHA_OPAQUE;
            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count) {
                mat = &model->materials.materials[sub->material_index];
                amode = mat->alpha_mode;
                if ((uint32_t)sub->material_index < model->mat_texture_count)
                    textures = &model->mat_textures[sub->material_index];
            }

            /* Skip BLEND — transparent materials should not cast shadows */
            if (amode == FORGE_PIPELINE_ALPHA_BLEND)
                continue;

            /* Push light_vp + node_world per submesh */
            ForgeSceneInstancedShadowVertUniforms su;
            su.light_vp   = scene->light_vp;
            su.node_world = node_world;

            if (amode == FORGE_PIPELINE_ALPHA_MASK &&
                scene->model_instanced_shadow_mask_pipeline) {
                if (last_shadow_pipe !=
                    scene->model_instanced_shadow_mask_pipeline) {
                    SDL_BindGPUGraphicsPipeline(scene->pass,
                        scene->model_instanced_shadow_mask_pipeline);
                    last_shadow_pipe =
                        scene->model_instanced_shadow_mask_pipeline;
                    SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);
                    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                                           SDL_GPU_INDEXELEMENTSIZE_32BIT);
                }
                SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

                SDL_GPUTextureSamplerBinding mask_tex;
                mask_tex.texture = (textures && textures->base_color)
                    ? textures->base_color : scene->model_white_texture;
                mask_tex.sampler = scene->model_tex_sampler;
                SDL_BindGPUFragmentSamplers(scene->pass, 0, &mask_tex, 1);

                ForgeSceneShadowMaskFragUniforms mfu;
                SDL_zero(mfu);
                if (mat) {
                    SDL_memcpy(mfu.base_color_factor,
                               mat->base_color_factor, 4 * sizeof(float));
                    mfu.alpha_cutoff = mat->alpha_cutoff;
                } else {
                    mfu.base_color_factor[0] = 1.0f;
                    mfu.base_color_factor[1] = 1.0f;
                    mfu.base_color_factor[2] = 1.0f;
                    mfu.base_color_factor[3] = 1.0f;
                    mfu.alpha_cutoff = FORGE_SCENE_DEFAULT_ALPHA_CUTOFF;
                }
                SDL_PushGPUFragmentUniformData(scene->cmd, 0,
                                               &mfu, sizeof(mfu));
            } else {
                if (last_shadow_pipe !=
                    scene->model_instanced_shadow_pipeline) {
                    SDL_BindGPUGraphicsPipeline(scene->pass,
                        scene->model_instanced_shadow_pipeline);
                    last_shadow_pipe = scene->model_instanced_shadow_pipeline;
                    SDL_BindGPUVertexBuffers(scene->pass, 0, vb_binds, 2);
                    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                                           SDL_GPU_INDEXELEMENTSIZE_32BIT);
                }
                SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));
            }

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, instance_count, first_index, 0, 0);
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

    /* Skinned shadow mask pipeline: depth + alpha test for MASK materials.
     * Uses the skinned shadow mask vertex shader (applies skinning + passes UV)
     * and the same shadow_mask fragment shader as the non-skinned variant. */
    {
        SDL_GPUShader *sk_smask_vs = forge_scene_create_shader(scene,
            SDL_GPU_SHADERSTAGE_VERTEX,
            shadow_mask_skinned_vert_spirv, sizeof(shadow_mask_skinned_vert_spirv),
            shadow_mask_skinned_vert_dxil,  sizeof(shadow_mask_skinned_vert_dxil),
            shadow_mask_skinned_vert_msl, shadow_mask_skinned_vert_msl_size,
            0, 0, 1, 1);  /* 0 samplers, 0 storage_tex, 1 storage_buf, 1 uniform */

        SDL_GPUShader *sk_smask_fs = forge_scene_create_shader(scene,
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_mask_frag_spirv, sizeof(shadow_mask_frag_spirv),
            shadow_mask_frag_dxil,  sizeof(shadow_mask_frag_dxil),
            shadow_mask_frag_msl, shadow_mask_frag_msl_size,
            1, 0, 0, 1);

        if (sk_smask_vs && sk_smask_fs) {
            SDL_GPUGraphicsPipelineCreateInfo pi;
            SDL_zero(pi);
            pi.vertex_shader      = sk_smask_vs;
            pi.fragment_shader    = sk_smask_fs;
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
            scene->skinned_shadow_mask_pipeline =
                SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
            if (!scene->skinned_shadow_mask_pipeline) {
                SDL_Log("forge_scene: SDL_CreateGPUGraphicsPipeline "
                        "(skinned_shadow_mask_pipeline) failed: %s",
                        SDL_GetError());
            }
        }
        if (sk_smask_vs) SDL_ReleaseGPUShader(scene->device, sk_smask_vs);
        if (sk_smask_fs) SDL_ReleaseGPUShader(scene->device, sk_smask_fs);
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
        if (scene->skinned_shadow_mask_pipeline)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->skinned_shadow_mask_pipeline); scene->skinned_shadow_mask_pipeline = NULL; }
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

    /* Precompute per-submesh centroids for transparency sorting.
     * These are bind-pose (object-space) centroids — joint transforms are
     * not applied.  For most skinned meshes the transparent submeshes are
     * small relative to the model, so bind-pose centroids give a reasonable
     * sort key without the cost of per-frame skinned readback. */
    model->submesh_centroid_count = model->mesh.submesh_count;
    if (model->submesh_centroid_count > FORGE_SCENE_MODEL_MAX_SUBMESHES)
        model->submesh_centroid_count = FORGE_SCENE_MODEL_MAX_SUBMESHES;
    forge_scene_compute_centroids(&model->mesh, model->submesh_centroids,
                                   FORGE_SCENE_MODEL_MAX_SUBMESHES);

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
     * scene->model_copy_pass that is shared across all skinned models
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
                if (!scene->model_copy_pass) {
                    scene->model_copy_pass =
                        SDL_BeginGPUCopyPass(scene->cmd);
                    if (!scene->model_copy_pass) {
                        SDL_Log("forge_scene: joint buffer copy pass "
                                "failed: %s", SDL_GetError());
                    }
                }
                if (scene->model_copy_pass) {
                    SDL_GPUTransferBufferLocation src;
                    SDL_zero(src);
                    src.transfer_buffer = model->joint_transfer_buffer;
                    SDL_GPUBufferRegion dst;
                    SDL_zero(dst);
                    dst.buffer = model->joint_buffer;
                    dst.size   = upload_size;
                    SDL_UploadToGPUBuffer(
                        scene->model_copy_pass, &src, &dst, true);
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
    model->transparent_draw_calls = 0;

    /* Precompute camera forward for view-depth sorting (invariant per call) */
    quat cam_q = quat_from_euler(scene->cam_yaw, scene->cam_pitch, 0.0f);
    vec3 cam_fwd = quat_forward(cam_q);

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

    /* Collect BLEND submeshes for deferred sorted drawing.
     * ~19 KB on the stack (256 × 76 bytes) — acceptable for desktop. */
    ForgeSceneTransparentDraw blend_draws[FORGE_SCENE_MAX_TRANSPARENT_DRAWS];
    uint32_t blend_count = 0;

    /* ── Pass 1: draw opaque + MASK submeshes immediately ──────────── */

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

            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count &&
                (uint32_t)sub->material_index < model->mat_texture_count) {
                mat = &model->materials.materials[sub->material_index];
                textures = &model->mat_textures[sub->material_index];
            }

            /* Defer BLEND submeshes to pass 2 (unless sorting is disabled) */
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND) {
                if (scene->transparency_sorting) {
                    if (blend_count < FORGE_SCENE_MAX_TRANSPARENT_DRAWS) {
                        ForgeSceneTransparentDraw *td =
                            &blend_draws[blend_count++];
                        td->node_index    = n;
                        td->submesh_index = submesh_idx;
                        td->final_world   = final_world;

                        vec3 centroid =
                            (submesh_idx < model->submesh_centroid_count)
                            ? model->submesh_centroids[submesh_idx]
                            : vec3_create(0, 0, 0);
                        vec4 wc4 = mat4_multiply_vec4(final_world,
                            vec4_create(centroid.x, centroid.y,
                                        centroid.z, 1.0f));
                        vec3 wc = vec3_create(wc4.x, wc4.y, wc4.z);
                        td->sort_depth = vec3_dot(
                            vec3_sub(wc, scene->cam_position), cam_fwd);
                        continue; /* deferred to pass 2 */
                    }
                    /* Queue full — fall through to draw unsorted */
                }
            }

            /* Select pipeline variant */
            SDL_GPUGraphicsPipeline *pipeline;
            if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND &&
                mat->double_sided)
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

            ForgeSceneVertUniforms vu;
            vu.mvp      = mat4_multiply(scene->cam_vp, final_world);
            vu.model    = final_world;
            vu.light_vp = mat4_multiply(scene->light_vp, final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

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

    /* ── Pass 2: sort and draw BLEND submeshes back-to-front ───────── */

    if (blend_count > 0) {
        SDL_qsort(blend_draws, blend_count,
                  sizeof(ForgeSceneTransparentDraw),
                  forge_scene__transparent_cmp);

        for (uint32_t i = 0; i < blend_count; i++) {
            const ForgeSceneTransparentDraw *td = &blend_draws[i];
            const ForgePipelineSubmesh *sub =
                forge_pipeline_lod_submesh(&model->mesh, 0, td->submesh_index);
            if (!sub || sub->index_count == 0) continue;

            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count &&
                (uint32_t)sub->material_index < model->mat_texture_count) {
                mat = &model->materials.materials[sub->material_index];
                textures = &model->mat_textures[sub->material_index];
            }

            SDL_GPUGraphicsPipeline *pipeline;
            if (mat && mat->double_sided)
                pipeline = scene->skinned_pipeline_blend_double;
            else
                pipeline = scene->skinned_pipeline_blend;

            if (pipeline != last_pipeline) {
                SDL_BindGPUGraphicsPipeline(scene->pass, pipeline);
                last_pipeline = pipeline;
            }

            forge_scene__bind_model_textures(scene, textures);

            ForgeSceneVertUniforms vu;
            vu.mvp      = mat4_multiply(scene->cam_vp, td->final_world);
            vu.model    = td->final_world;
            vu.light_vp = mat4_multiply(scene->light_vp, td->final_world);
            SDL_PushGPUVertexUniformData(scene->cmd, 0, &vu, sizeof(vu));

            ForgeSceneModelFragUniforms fu;
            forge_scene__fill_model_frag_uniforms(scene, mat, &fu);
            SDL_PushGPUFragmentUniformData(scene->cmd, 0, &fu, sizeof(fu));

            uint32_t first_index =
                (sub->index_offset - lod0_base_offset) / sizeof(uint32_t);
            SDL_DrawGPUIndexedPrimitives(scene->pass,
                sub->index_count, 1, first_index, 0, 0);
            model->draw_calls++;
            model->transparent_draw_calls++;
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
    SDL_GPUGraphicsPipeline *last_shadow_pipe = NULL;

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

            int amode = FORGE_PIPELINE_ALPHA_OPAQUE;
            const ForgePipelineMaterial *mat = NULL;
            const ForgeSceneModelTextures *textures = NULL;
            if (sub->material_index >= 0 &&
                (uint32_t)sub->material_index < model->materials.material_count) {
                mat = &model->materials.materials[sub->material_index];
                amode = mat->alpha_mode;
                if ((uint32_t)sub->material_index < model->mat_texture_count)
                    textures = &model->mat_textures[sub->material_index];
            }

            if (amode == FORGE_PIPELINE_ALPHA_BLEND)
                continue;

            ForgeSceneShadowVertUniforms su;
            su.light_vp = mat4_multiply(scene->light_vp, final_world);

            if (amode == FORGE_PIPELINE_ALPHA_MASK &&
                scene->skinned_shadow_mask_pipeline) {
                if (last_shadow_pipe != scene->skinned_shadow_mask_pipeline) {
                    SDL_BindGPUGraphicsPipeline(scene->pass,
                        scene->skinned_shadow_mask_pipeline);
                    last_shadow_pipe = scene->skinned_shadow_mask_pipeline;
                }
                SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));

                SDL_GPUTextureSamplerBinding mask_tex;
                mask_tex.texture = (textures && textures->base_color)
                    ? textures->base_color : scene->model_white_texture;
                mask_tex.sampler = scene->model_tex_sampler;
                SDL_BindGPUFragmentSamplers(scene->pass, 0, &mask_tex, 1);

                ForgeSceneShadowMaskFragUniforms mfu;
                SDL_zero(mfu);
                if (mat) {
                    SDL_memcpy(mfu.base_color_factor,
                               mat->base_color_factor, 4 * sizeof(float));
                    mfu.alpha_cutoff = mat->alpha_cutoff;
                } else {
                    mfu.base_color_factor[0] = 1.0f;
                    mfu.base_color_factor[1] = 1.0f;
                    mfu.base_color_factor[2] = 1.0f;
                    mfu.base_color_factor[3] = 1.0f;
                    mfu.alpha_cutoff = FORGE_SCENE_DEFAULT_ALPHA_CUTOFF;
                }
                SDL_PushGPUFragmentUniformData(scene->cmd, 0,
                                               &mfu, sizeof(mfu));
            } else {
                if (last_shadow_pipe != scene->skinned_shadow_pipeline) {
                    SDL_BindGPUGraphicsPipeline(scene->pass,
                        scene->skinned_shadow_pipeline);
                    last_shadow_pipe = scene->skinned_shadow_pipeline;
                }
                SDL_PushGPUVertexUniformData(scene->cmd, 0, &su, sizeof(su));
            }

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

/* ══════════════════════════════════════════════════════════════════════════
 *  Morph target model support
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Init morph pipelines (lazy, called on first morph model load) ────── */

static bool forge_scene_init_morph_pipelines(ForgeScene *scene)
{
    if (scene->morph_pipelines_ready) return true;

    /* Ensure model pipelines are ready (we reuse samplers, fallback textures) */
    if (!forge_scene_init_model_pipelines(scene)) return false;

    /* Create morph vertex shader — 2 storage buffers (pos + nrm deltas), 1 uniform */
    SDL_GPUShader *morph_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_morph_vert_spirv, sizeof(scene_morph_vert_spirv),
        scene_morph_vert_dxil,  sizeof(scene_morph_vert_dxil),
        scene_morph_vert_msl,   scene_morph_vert_msl_size,
        0, 0, 2, 1);  /* 0 samplers, 0 storage_tex, 2 storage_buf, 1 uniform */

    /* Reuse model fragment shader */
    SDL_GPUShader *morph_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        scene_model_frag_spirv, sizeof(scene_model_frag_spirv),
        scene_model_frag_dxil,  sizeof(scene_model_frag_dxil),
        scene_model_frag_msl, scene_model_frag_msl_size,
        6, 0, 0, 1);

    /* Morph shadow vertex shader — 1 storage buffer (pos deltas only), 1 uniform */
    SDL_GPUShader *morph_shadow_vs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_VERTEX,
        scene_morph_shadow_vert_spirv, sizeof(scene_morph_shadow_vert_spirv),
        scene_morph_shadow_vert_dxil,  sizeof(scene_morph_shadow_vert_dxil),
        scene_morph_shadow_vert_msl, scene_morph_shadow_vert_msl_size,
        0, 0, 1, 1);

    /* Reuse existing shadow fragment shader */
    SDL_GPUShader *morph_shadow_fs = forge_scene_create_shader(scene,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        shadow_frag_spirv, sizeof(shadow_frag_spirv),
        shadow_frag_dxil,  sizeof(shadow_frag_dxil),
        shadow_frag_msl, shadow_frag_msl_size,
        0, 0, 0, 0);

    if (!morph_vs || !morph_fs || !morph_shadow_vs || !morph_shadow_fs) {
        SDL_Log("forge_scene: morph shader creation failed");
        if (morph_vs)        SDL_ReleaseGPUShader(scene->device, morph_vs);
        if (morph_fs)        SDL_ReleaseGPUShader(scene->device, morph_fs);
        if (morph_shadow_vs) SDL_ReleaseGPUShader(scene->device, morph_shadow_vs);
        if (morph_shadow_fs) SDL_ReleaseGPUShader(scene->device, morph_shadow_fs);
        return false;
    }

    /* 48-byte vertex input: pos + normal + uv + tangent (same as scene_model) */
    SDL_GPUVertexBufferDescription morph_vbd;
    SDL_zero(morph_vbd);
    morph_vbd.slot       = 0;
    morph_vbd.pitch      = FORGE_PIPELINE_VERTEX_STRIDE_TAN;
    morph_vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute morph_attrs[4];
    SDL_zero(morph_attrs);
    morph_attrs[0].location = 0; morph_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; morph_attrs[0].offset = 0;
    morph_attrs[1].location = 1; morph_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; morph_attrs[1].offset = 12;
    morph_attrs[2].location = 2; morph_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; morph_attrs[2].offset = 24;
    morph_attrs[3].location = 3; morph_attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; morph_attrs[3].offset = 32;

    SDL_GPUVertexInputState morph_input;
    SDL_zero(morph_input);
    morph_input.vertex_buffer_descriptions = &morph_vbd;
    morph_input.num_vertex_buffers         = 1;
    morph_input.vertex_attributes          = morph_attrs;
    morph_input.num_vertex_attributes      = 4;

    /* Shadow pass uses only position (offset 0) but needs same stride */
    SDL_GPUVertexAttribute morph_shadow_attrs[1];
    SDL_zero(morph_shadow_attrs);
    morph_shadow_attrs[0].location = 0;
    morph_shadow_attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    morph_shadow_attrs[0].offset   = 0;

    SDL_GPUVertexInputState morph_shadow_input;
    SDL_zero(morph_shadow_input);
    morph_shadow_input.vertex_buffer_descriptions = &morph_vbd;
    morph_shadow_input.num_vertex_buffers         = 1;
    morph_shadow_input.vertex_attributes          = morph_shadow_attrs;
    morph_shadow_input.num_vertex_attributes      = 1;

    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = scene->swapchain_fmt;

    /* Opaque pipeline: cull back, depth test+write */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = morph_vs;
        pi.fragment_shader    = morph_fs;
        pi.vertex_input_state = morph_input;
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
        scene->morph_pipeline = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
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
        pi.vertex_shader      = morph_vs;
        pi.fragment_shader    = morph_fs;
        pi.vertex_input_state = morph_input;
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
        scene->morph_pipeline_blend = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Double-sided pipeline: cull none, depth test+write */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = morph_vs;
        pi.fragment_shader    = morph_fs;
        pi.vertex_input_state = morph_input;
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
        scene->morph_pipeline_double = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
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
        pi.vertex_shader      = morph_vs;
        pi.fragment_shader    = morph_fs;
        pi.vertex_input_state = morph_input;
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
        scene->morph_pipeline_blend_double = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Shadow pipeline: depth-only, 48-byte stride + 1 storage buffer, cull none, depth bias */
    {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader      = morph_shadow_vs;
        pi.fragment_shader    = morph_shadow_fs;
        pi.vertex_input_state = morph_shadow_input;
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
        scene->morph_shadow_pipeline = SDL_CreateGPUGraphicsPipeline(scene->device, &pi);
    }

    /* Release shaders */
    SDL_ReleaseGPUShader(scene->device, morph_vs);
    SDL_ReleaseGPUShader(scene->device, morph_fs);
    SDL_ReleaseGPUShader(scene->device, morph_shadow_vs);
    SDL_ReleaseGPUShader(scene->device, morph_shadow_fs);

    if (!scene->morph_pipeline || !scene->morph_pipeline_blend ||
        !scene->morph_pipeline_double || !scene->morph_pipeline_blend_double ||
        !scene->morph_shadow_pipeline) {
        SDL_Log("forge_scene: morph pipeline creation failed: %s", SDL_GetError());
        if (scene->morph_pipeline)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline); scene->morph_pipeline = NULL; }
        if (scene->morph_pipeline_blend)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline_blend); scene->morph_pipeline_blend = NULL; }
        if (scene->morph_pipeline_double)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline_double); scene->morph_pipeline_double = NULL; }
        if (scene->morph_pipeline_blend_double)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_pipeline_blend_double); scene->morph_pipeline_blend_double = NULL; }
        if (scene->morph_shadow_pipeline)
            { SDL_ReleaseGPUGraphicsPipeline(scene->device, scene->morph_shadow_pipeline); scene->morph_shadow_pipeline = NULL; }
        return false;
    }

    scene->morph_pipelines_ready = true;
    return true;
}

/* ── Load morph model ────────────────────────────────────────────────────── */

static bool forge_scene_load_morph_model(
    ForgeScene *scene, ForgeSceneMorphModel *model,
    const char *fscene_path, const char *fmesh_path,
    const char *fmat_path,   const char *fanim_path,
    const char *base_dir)
{
    if (!scene || !model) {
        SDL_Log("forge_scene: load_morph_model: NULL scene or model");
        return false;
    }
    if (!fscene_path || !fmesh_path || !base_dir) {
        SDL_Log("forge_scene: load_morph_model: NULL required path");
        return false;
    }
    if (base_dir[0] == '\0') {
        SDL_Log("forge_scene: load_morph_model: empty base_dir");
        return false;
    }

    SDL_memset(model, 0, sizeof(*model));
    model->anim_speed = 1.0f;
    model->looping = true;

    /* Lazy-init morph pipelines */
    if (!forge_scene_init_morph_pipelines(scene)) return false;

    /* Load pipeline data */
    if (!forge_pipeline_load_scene(fscene_path, &model->scene_data)) {
        SDL_Log("forge_scene: load_morph_model: failed to load scene '%s'",
                fscene_path);
        return false;
    }

    if (!forge_pipeline_load_mesh(fmesh_path, &model->mesh)) {
        SDL_Log("forge_scene: load_morph_model: failed to load mesh '%s'",
                fmesh_path);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }

    /* Validate: must have morph data and 48-byte base vertices */
    if (!forge_pipeline_has_morph_data(&model->mesh)) {
        SDL_Log("forge_scene: load_morph_model: '%s' has no morph data "
                "(flags=0x%x)", fmesh_path, model->mesh.flags);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }
    if (model->mesh.vertex_stride != FORGE_PIPELINE_VERTEX_STRIDE_TAN) {
        SDL_Log("forge_scene: load_morph_model: '%s' has unsupported stride %u "
                "(expected %u)", fmesh_path, model->mesh.vertex_stride,
                (unsigned)FORGE_PIPELINE_VERTEX_STRIDE_TAN);
        forge_pipeline_free_mesh(&model->mesh);
        forge_pipeline_free_scene(&model->scene_data);
        SDL_memset(model, 0, sizeof(*model));
        return false;
    }

    /* Load materials (optional — morph models may have no material file) */
    if (fmat_path) {
        if (!forge_pipeline_load_materials(fmat_path, &model->materials)) {
            SDL_Log("forge_scene: load_morph_model: failed to load materials '%s'",
                    fmat_path);
            goto fail;
        }
        if (model->materials.material_count > FORGE_SCENE_MODEL_MAX_MATERIALS) {
            SDL_Log("forge_scene: load_morph_model: too many materials (%u, max %u)",
                    model->materials.material_count,
                    (unsigned)FORGE_SCENE_MODEL_MAX_MATERIALS);
            goto fail;
        }
    }

    /* Load animations (optional — morph weights can be set manually) */
    if (fanim_path) {
        if (!forge_pipeline_load_animation(fanim_path, &model->animations)) {
            SDL_Log("forge_scene: load_morph_model: failed to load anim '%s'",
                    fanim_path);
            goto fail;
        }
    }

    /* Cache morph target count and default weights */
    model->morph_target_count = model->mesh.morph_target_count;
    for (uint32_t i = 0; i < model->morph_target_count; i++)
        model->morph_weights[i] = model->mesh.morph_targets[i].default_weight;

    /* Upload vertex buffer */
    {
        uint64_t vb_size_64 =
            (uint64_t)model->mesh.vertex_count * model->mesh.vertex_stride;
        if (vb_size_64 > (uint64_t)UINT32_MAX) {
            SDL_Log("forge_scene: load_morph_model: vertex buffer overflow");
            goto fail;
        }
        Uint32 vb_size = (Uint32)vb_size_64;
        model->vertex_buffer = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_VERTEX, model->mesh.vertices, vb_size);
        if (!model->vertex_buffer) {
            SDL_Log("forge_scene: load_morph_model: vertex buffer upload failed");
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
            SDL_Log("forge_scene: load_morph_model: index buffer overflow");
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
            SDL_Log("forge_scene: load_morph_model: index buffer upload failed");
            goto fail;
        }
    }

    /* Allocate CPU-side blended delta arrays.
     * Use 4 floats per vertex (16-byte stride) to match the GPU's
     * StructuredBuffer<float4> which has ArrayStride 16 in both SPIRV and DXIL.
     * The 4th float per element is padding (always 0). */
    {
        size_t delta_floats = (size_t)model->mesh.vertex_count * 4;
        size_t delta_bytes  = delta_floats * sizeof(float);
        model->blended_pos_deltas = (float *)SDL_calloc(1, delta_bytes);
        model->blended_nrm_deltas = (float *)SDL_calloc(1, delta_bytes);
        if (!model->blended_pos_deltas || !model->blended_nrm_deltas) {
            SDL_Log("forge_scene: load_morph_model: delta alloc failed");
            goto fail;
        }
    }

    /* Create morph delta storage buffers (zero-filled initially).
     * Use 16-byte stride (4 floats) per element to match SPIRV's
     * StructuredBuffer<float4> stride of 16 bytes. */
    {
        uint64_t buf_size_64 =
            (uint64_t)model->mesh.vertex_count * 4 * sizeof(float);
        if (buf_size_64 > (uint64_t)UINT32_MAX) {
            SDL_Log("forge_scene: load_morph_model: delta buffer size "
                    "exceeds UINT32_MAX (%" SDL_PRIu64 " bytes)", buf_size_64);
            goto fail;
        }
        Uint32 buf_size = (Uint32)buf_size_64;
        model->morph_pos_buffer = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            model->blended_pos_deltas, buf_size);
        model->morph_nrm_buffer = forge_scene_upload_buffer(scene,
            SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            model->blended_nrm_deltas, buf_size);
        if (!model->morph_pos_buffer || !model->morph_nrm_buffer) {
            SDL_Log("forge_scene: load_morph_model: delta buffer upload failed");
            goto fail;
        }
    }

    /* Create persistent transfer buffer for per-frame delta uploads.
     * Sized for both position + normal deltas in sequence (16 bytes each). */
    {
        uint64_t single_size_64 =
            (uint64_t)model->mesh.vertex_count * 4 * sizeof(float);
        if (single_size_64 > (uint64_t)(UINT32_MAX / 2)) {
            SDL_Log("forge_scene: load_morph_model: transfer buffer size "
                    "exceeds UINT32_MAX");
            goto fail;
        }
        Uint32 single_buf_size = (Uint32)single_size_64;
        SDL_GPUTransferBufferCreateInfo tbci;
        SDL_zero(tbci);
        tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbci.size  = single_buf_size * 2;  /* pos + nrm */
        model->morph_transfer_buffer =
            SDL_CreateGPUTransferBuffer(scene->device, &tbci);
        if (!model->morph_transfer_buffer) {
            SDL_Log("forge_scene: load_morph_model: transfer buffer creation "
                    "failed: %s", SDL_GetError());
            goto fail;
        }
    }

    /* Load per-material textures */
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

    SDL_Log("forge_scene: loaded morph model: %u vertices, %u submeshes, "
            "%u morph targets, %u clips",
            model->mesh.vertex_count, model->mesh.submesh_count,
            model->morph_target_count, model->animations.clip_count);
    return true;

fail:
    forge_scene_free_morph_model(scene, model);
    return false;
}

/* ── Update morph animation ──────────────────────────────────────────────── */

static void forge_scene_update_morph_animation(
    ForgeScene *scene, ForgeSceneMorphModel *model, float dt)
{
    if (!scene || !model) return;

    /* ── Evaluate morph weights from animation ──────────────────────── */
    if (!model->manual_weights &&
        model->animations.clip_count > 0 &&
        model->current_clip >= 0 &&
        (uint32_t)model->current_clip < model->animations.clip_count) {

        model->anim_time += dt * model->anim_speed;

        const ForgePipelineAnimation *clip =
            &model->animations.clips[model->current_clip];
        float dur = clip->duration;
        if (model->looping && dur > FORGE_PIPELINE_ANIM_EPSILON) {
            model->anim_time = SDL_fmodf(model->anim_time, dur);
            if (model->anim_time < 0.0f) model->anim_time += dur;
        }

        /* Reset weights to authored defaults before evaluating channels so
         * stale values from prior frames do not persist when channels don't
         * write every component. Uses default_weight (not 0.0f) to preserve
         * authored rest poses — see GitHub issue #316. */
        for (uint32_t w = 0; w < model->morph_target_count; w++)
            model->morph_weights[w] = model->mesh.morph_targets[w].default_weight;

        /* Find morph weight channels and sample them.
         * forge_pipeline_anim_apply skips MORPH_WEIGHTS channels, so we
         * evaluate them here by searching the clip's channels directly. */
        for (uint32_t ci = 0; ci < clip->channel_count; ci++) {
            const ForgePipelineAnimChannel *ch = &clip->channels[ci];
            if (ch->target_path != FORGE_PIPELINE_ANIM_MORPH_WEIGHTS)
                continue;
            if (ch->sampler_index >= clip->sampler_count)
                continue;

            const ForgePipelineAnimSampler *samp =
                &clip->samplers[ch->sampler_index];
            if (samp->keyframe_count == 0 || !samp->timestamps || !samp->values)
                continue;

            /* Binary search for the keyframe bracket.
             * anim_time is already wrapped by fmodf above, so no
             * additional wrapping needed here. */
            float t = model->anim_time;

            /* Clamp to valid range */
            /* src_nc = stride in the keyframe value array (may differ
             * from morph_target_count).  dst_nc = how many weights we
             * actually write — clamped to the model's target count. */
            uint32_t src_nc = (uint32_t)samp->value_components;
            uint32_t dst_nc = src_nc < model->morph_target_count
                            ? src_nc : model->morph_target_count;

            if (t <= samp->timestamps[0]) {
                /* Before first keyframe — use first values */
                for (uint32_t w = 0; w < dst_nc; w++)
                    model->morph_weights[w] = samp->values[w];
            } else if (t >= samp->timestamps[samp->keyframe_count - 1]) {
                /* After last keyframe — use last values */
                uint32_t last = (uint32_t)(samp->keyframe_count - 1) * src_nc;
                for (uint32_t w = 0; w < dst_nc; w++)
                    model->morph_weights[w] = samp->values[last + w];
            } else if (samp->keyframe_count >= 2) {
                /* Binary search — needs at least 2 keyframes */
                uint32_t lo = 0, hi = samp->keyframe_count - 2;
                while (lo < hi) {
                    uint32_t mid = (lo + hi) / 2;
                    if (samp->timestamps[mid + 1] <= t)
                        lo = mid + 1;
                    else
                        hi = mid;
                }
                float t0 = samp->timestamps[lo];
                float t1 = samp->timestamps[lo + 1];
                float alpha = (t1 > t0)
                    ? (t - t0) / (t1 - t0) : 0.0f;

                for (uint32_t w = 0; w < dst_nc; w++) {
                    float v0 = samp->values[lo * src_nc + w];
                    float v1 = samp->values[(lo + 1) * src_nc + w];
                    model->morph_weights[w] = v0 + alpha * (v1 - v0);
                }
            }
        }
    }

    /* ── CPU-blend morph deltas ──────────────────────────────────────── */
    if (model->morph_target_count > 0 && model->blended_pos_deltas &&
        model->blended_nrm_deltas) {
        uint32_t vc = model->mesh.vertex_count;

        /* Clear blended arrays (4 floats per vertex for 16-byte stride) */
        SDL_memset(model->blended_pos_deltas, 0, vc * 4 * sizeof(float));
        SDL_memset(model->blended_nrm_deltas, 0, vc * 4 * sizeof(float));

        /* Accumulate weighted deltas from each target.
         * Source deltas are tightly packed (3 floats per vertex).
         * Destination uses 16-byte stride (4 floats per vertex) to match
         * the GPU's StructuredBuffer<float4> stride of 16 bytes. */
        for (uint32_t ti = 0; ti < model->morph_target_count; ti++) {
            float w = model->morph_weights[ti];
            if (w < FORGE_SCENE_MORPH_WEIGHT_EPSILON &&
                w > -FORGE_SCENE_MORPH_WEIGHT_EPSILON) continue; /* skip near-zero */

            const ForgePipelineMorphTarget *mt = &model->mesh.morph_targets[ti];
            if (mt->position_deltas) {
                for (uint32_t v = 0; v < vc; v++) {
                    model->blended_pos_deltas[v * 4 + 0] += w * mt->position_deltas[v * 3 + 0];
                    model->blended_pos_deltas[v * 4 + 1] += w * mt->position_deltas[v * 3 + 1];
                    model->blended_pos_deltas[v * 4 + 2] += w * mt->position_deltas[v * 3 + 2];
                }
            }
            if (mt->normal_deltas) {
                for (uint32_t v = 0; v < vc; v++) {
                    model->blended_nrm_deltas[v * 4 + 0] += w * mt->normal_deltas[v * 3 + 0];
                    model->blended_nrm_deltas[v * 4 + 1] += w * mt->normal_deltas[v * 3 + 1];
                    model->blended_nrm_deltas[v * 4 + 2] += w * mt->normal_deltas[v * 3 + 2];
                }
            }
        }
    }

    /* ── Upload blended deltas to GPU ───────────────────────────────── */
    if (model->morph_pos_buffer && model->morph_nrm_buffer &&
        model->morph_transfer_buffer && model->mesh.vertex_count > 0) {
        if (!scene->cmd) {
            SDL_Log("forge_scene: update_morph_animation: scene->cmd is NULL "
                    "(call after forge_scene_begin_frame)");
        } else {
            /* 16-byte stride per element (4 floats) to match SPIRV ArrayStride.
             * Size validated at load time — cast is safe here. */
            Uint32 single_size =
                (Uint32)((uint64_t)model->mesh.vertex_count * 4 * sizeof(float));

            void *mapped = SDL_MapGPUTransferBuffer(
                scene->device, model->morph_transfer_buffer, true);
            if (mapped) {
                /* Pack pos + nrm sequentially in the transfer buffer */
                SDL_memcpy(mapped, model->blended_pos_deltas, single_size);
                SDL_memcpy((uint8_t *)mapped + single_size,
                           model->blended_nrm_deltas, single_size);
                SDL_UnmapGPUTransferBuffer(scene->device,
                                           model->morph_transfer_buffer);

                /* Reuse the shared model_copy_pass for morph uploads */
                if (!scene->model_copy_pass) {
                    scene->model_copy_pass =
                        SDL_BeginGPUCopyPass(scene->cmd);
                    if (!scene->model_copy_pass) {
                        SDL_Log("forge_scene: morph delta copy pass "
                                "failed: %s", SDL_GetError());
                    }
                }
                if (scene->model_copy_pass) {
                    /* Upload position deltas */
                    SDL_GPUTransferBufferLocation pos_src;
                    SDL_zero(pos_src);
                    pos_src.transfer_buffer = model->morph_transfer_buffer;
                    pos_src.offset = 0;
                    SDL_GPUBufferRegion pos_dst;
                    SDL_zero(pos_dst);
                    pos_dst.buffer = model->morph_pos_buffer;
                    pos_dst.size   = single_size;
                    SDL_UploadToGPUBuffer(
                        scene->model_copy_pass, &pos_src, &pos_dst, true);

                    /* Upload normal deltas */
                    SDL_GPUTransferBufferLocation nrm_src;
                    SDL_zero(nrm_src);
                    nrm_src.transfer_buffer = model->morph_transfer_buffer;
                    nrm_src.offset = single_size;
                    SDL_GPUBufferRegion nrm_dst;
                    SDL_zero(nrm_dst);
                    nrm_dst.buffer = model->morph_nrm_buffer;
                    nrm_dst.size   = single_size;
                    SDL_UploadToGPUBuffer(
                        scene->model_copy_pass, &nrm_src, &nrm_dst, true);
                }
            } else {
                SDL_Log("forge_scene: morph transfer buffer map failed: %s",
                        SDL_GetError());
            }
        }
    }
}

/* ── Draw morph model ────────────────────────────────────────────────────── */

static void forge_scene_draw_morph_model(
    ForgeScene *scene, ForgeSceneMorphModel *model, mat4 placement)
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

    /* Bind morph delta storage buffers at vertex stage slots 0 and 1 */
    SDL_GPUBuffer *morph_bufs[2] = {
        model->morph_pos_buffer, model->morph_nrm_buffer
    };
    SDL_BindGPUVertexStorageBuffers(scene->pass, 0, morph_bufs, 2);

    SDL_GPUGraphicsPipeline *last_pipeline = NULL;

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
                pipeline = scene->morph_pipeline_blend_double;
            else if (mat && mat->alpha_mode == FORGE_PIPELINE_ALPHA_BLEND)
                pipeline = scene->morph_pipeline_blend;
            else if (mat && mat->double_sided)
                pipeline = scene->morph_pipeline_double;
            else
                pipeline = scene->morph_pipeline;

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

/* ── Draw morph model shadows ────────────────────────────────────────────── */

static void forge_scene_draw_morph_model_shadows(
    ForgeScene *scene, ForgeSceneMorphModel *model, mat4 placement)
{
    if (!scene || !model || !scene->pass) return;
    if (!scene->morph_shadow_pipeline) return;
    if (!model->vertex_buffer || !model->index_buffer) return;
    if (model->mesh.lod_count == 0) return;

    SDL_BindGPUGraphicsPipeline(scene->pass, scene->morph_shadow_pipeline);

    /* Bind vertex + index buffers */
    SDL_GPUBufferBinding vb_bind = { model->vertex_buffer, 0 };
    SDL_BindGPUVertexBuffers(scene->pass, 0, &vb_bind, 1);
    SDL_GPUBufferBinding ib_bind = { model->index_buffer, 0 };
    SDL_BindGPUIndexBuffer(scene->pass, &ib_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Bind position delta storage buffer only (shadow needs no normals) */
    SDL_BindGPUVertexStorageBuffers(scene->pass, 0,
                                    &model->morph_pos_buffer, 1);

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

/* ── Free morph model ────────────────────────────────────────────────────── */

static void forge_scene_free_morph_model(
    ForgeScene *scene, ForgeSceneMorphModel *model)
{
    if (!model) return;

    if (scene && scene->device) {
        /* Release GPU buffers */
        if (model->vertex_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->vertex_buffer);
        if (model->index_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->index_buffer);
        if (model->morph_pos_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->morph_pos_buffer);
        if (model->morph_nrm_buffer)
            SDL_ReleaseGPUBuffer(scene->device, model->morph_nrm_buffer);
        if (model->morph_transfer_buffer)
            SDL_ReleaseGPUTransferBuffer(scene->device,
                                         model->morph_transfer_buffer);

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

    /* Free CPU-side delta arrays */
    SDL_free(model->blended_pos_deltas);
    SDL_free(model->blended_nrm_deltas);

    /* Free pipeline data */
    forge_pipeline_free_scene(&model->scene_data);
    forge_pipeline_free_mesh(&model->mesh);
    forge_pipeline_free_materials(&model->materials);
    forge_pipeline_free_animation(&model->animations);

    SDL_memset(model, 0, sizeof(*model));
}

#endif /* FORGE_SCENE_MODEL_SUPPORT */

#endif /* FORGE_SCENE_IMPLEMENTATION */
#endif /* FORGE_SCENE_H */
