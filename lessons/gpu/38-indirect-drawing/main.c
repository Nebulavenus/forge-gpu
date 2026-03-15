/*
 * Lesson 38 — Indirect Drawing
 *
 * Demonstrates GPU-driven rendering using SDL_DrawGPUIndexedPrimitivesIndirect.
 * A compute shader performs frustum culling and writes indirect draw commands
 * — the CPU never decides what to draw. The lesson features a dual-camera
 * split-screen: the left half shows the main camera's view rendered with
 * indirect drawing (only visible objects), and the right half shows an
 * overhead debug camera with all objects colored green (visible) or red
 * (culled), plus the main camera's frustum drawn as wireframe lines.
 *
 * Key concepts:
 *   - Indirect drawing with SDL_DrawGPUIndexedPrimitivesIndirect
 *   - Compute shader frustum culling (Gribb-Hartmann plane extraction)
 *   - Per-object storage buffer transforms via instance vertex buffer pattern
 *   - Sphere-vs-frustum visibility testing
 *   - Dual-camera split-screen with viewport/scissor
 *   - GPU buffer usage flags (INDIRECT, COMPUTE_STORAGE_WRITE, etc.)
 *
 * Scene: CesiumMilkTruck at center (regular draw calls), 200 BoxTextured
 * instances scattered across a ~40x40 unit area (indirect draw calls).
 *
 * Controls:
 *   WASD / Mouse  — move/look (main camera)
 *   Space / Shift — fly up/down
 *   F             — toggle frustum culling on/off
 *   D             — toggle debug view visibility
 *   Escape        — release mouse cursor
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>    /* offsetof */
#include <string.h>    /* memset   */
#include <stdio.h>     /* snprintf */
#include <math.h>      /* sqrtf    */

#include "math/forge_math.h"
#include "gltf/forge_gltf.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecode ─────────────────────────────────────────── */

/* Compute: frustum culling — reads object data, writes indirect commands
 * and per-object visibility flags */
#include "shaders/compiled/frustum_cull_comp_spirv.h"
#include "shaders/compiled/frustum_cull_comp_dxil.h"
#include "shaders/compiled/frustum_cull_comp_msl.h"

/* Indirect box rendering — vertex reads object data from storage buffer,
 * fragment does Blinn-Phong with diffuse texture + shadow map */
#include "shaders/compiled/indirect_box_vert_spirv.h"
#include "shaders/compiled/indirect_box_vert_dxil.h"
#include "shaders/compiled/indirect_box_vert_msl.h"
#include "shaders/compiled/indirect_box_frag_spirv.h"
#include "shaders/compiled/indirect_box_frag_dxil.h"
#include "shaders/compiled/indirect_box_frag_msl.h"

/* Indirect shadow pass — vertex reads object data from storage buffer,
 * fragment is a no-op (depth-only) */
#include "shaders/compiled/indirect_shadow_vert_spirv.h"
#include "shaders/compiled/indirect_shadow_vert_dxil.h"
#include "shaders/compiled/indirect_shadow_vert_msl.h"
#include "shaders/compiled/indirect_shadow_frag_spirv.h"
#include "shaders/compiled/indirect_shadow_frag_dxil.h"
#include "shaders/compiled/indirect_shadow_frag_msl.h"

/* Debug view — renders all boxes with green/red coloring based on
 * visibility buffer from the compute pass */
#include "shaders/compiled/debug_box_vert_spirv.h"
#include "shaders/compiled/debug_box_vert_dxil.h"
#include "shaders/compiled/debug_box_vert_msl.h"
#include "shaders/compiled/debug_box_frag_spirv.h"
#include "shaders/compiled/debug_box_frag_dxil.h"
#include "shaders/compiled/debug_box_frag_msl.h"

/* Frustum wireframe — draws the main camera's frustum as yellow lines
 * in the debug view */
#include "shaders/compiled/frustum_lines_vert_spirv.h"
#include "shaders/compiled/frustum_lines_vert_dxil.h"
#include "shaders/compiled/frustum_lines_vert_msl.h"
#include "shaders/compiled/frustum_lines_frag_spirv.h"
#include "shaders/compiled/frustum_lines_frag_dxil.h"
#include "shaders/compiled/frustum_lines_frag_msl.h"

/* Grid floor — procedural anti-aliased grid with shadow receiving */
#include "shaders/compiled/grid_vert_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_vert_msl.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_frag_dxil.h"
#include "shaders/compiled/grid_frag_msl.h"

/* Truck scene — standard Blinn-Phong with diffuse texture + shadow map,
 * rendered with regular (non-indirect) draw calls */
#include "shaders/compiled/truck_scene_vert_spirv.h"
#include "shaders/compiled/truck_scene_vert_dxil.h"
#include "shaders/compiled/truck_scene_vert_msl.h"
#include "shaders/compiled/truck_scene_frag_spirv.h"
#include "shaders/compiled/truck_scene_frag_dxil.h"
#include "shaders/compiled/truck_scene_frag_msl.h"

/* ── Constants ────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH       1280
#define WINDOW_HEIGHT      720
#define NUM_BOXES          200
#define SHADOW_MAP_SIZE    2048
#define SHADOW_DEPTH_FMT   SDL_GPU_TEXTUREFORMAT_D32_FLOAT
#define DEPTH_FMT          SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Camera */
#define FOV_DEG            60.0f
#define NEAR_PLANE         0.1f
#define FAR_PLANE          200.0f
#define MOVE_SPEED         8.0f
#define MOUSE_SENSITIVITY  0.003f
#define PITCH_CLAMP        1.5f
#define MAX_DELTA_TIME     0.1f

/* Debug camera — fixed overhead view */
#define DEBUG_CAM_HEIGHT   45.0f
#define DEBUG_CAM_BACK     35.0f
#define DEBUG_FOV_DEG      70.0f

/* Initial main camera position — elevated, looking at center */
#define CAM_START_X        15.0f
#define CAM_START_Y        8.0f
#define CAM_START_Z        15.0f
#define CAM_START_YAW_DEG  45.0f
#define CAM_START_PITCH_DEG -20.0f

/* Lighting */
#define LIGHT_DIR_X        0.6f
#define LIGHT_DIR_Y        1.0f
#define LIGHT_DIR_Z        0.4f

/* Scene lighting parameters */
#define SCENE_AMBIENT      0.12f
#define SCENE_SHININESS    64.0f
#define SCENE_SPECULAR_STR 0.4f

/* Grid */
#define GRID_HALF_SIZE     50.0f
#define GRID_NUM_VERTS     4
#define GRID_NUM_INDICES   6
#define GRID_SPACING       1.0f
#define GRID_LINE_WIDTH    0.02f
#define GRID_FADE_DIST     40.0f
#define GRID_AMBIENT       0.15f
#define GRID_SHININESS     32.0f
#define GRID_SPECULAR_STR  0.2f

/* Grid colors (linear space) */
#define GRID_LINE_R        0.068f
#define GRID_LINE_G        0.534f
#define GRID_LINE_B        0.932f
#define GRID_BG_R          0.014f
#define GRID_BG_G          0.014f
#define GRID_BG_B          0.045f

/* Clear color (dark background) */
#define CLEAR_R            0.02f
#define CLEAR_G            0.02f
#define CLEAR_B            0.03f

/* Shadow */
#define SHADOW_BIAS        0.002f
#define SHADOW_ORTHO_HALF  30.0f
#define SHADOW_LIGHT_DIST  40.0f

/* Compute workgroup size — must match [numthreads(64, 1, 1)] in HLSL */
#define WORKGROUP_SIZE     64

/* Box placement layout */
#define BOX_GRID_SPACING   2.8f
#define BOX_GROUND_Y       0.5f
#define BOX_ROTATION_STEP  0.4f
#define PLANE_NORM_EPS     0.0001f

/* Model paths */
#define BOX_MODEL_PATH     "assets/models/BoxTextured/BoxTextured.gltf"
#define TRUCK_MODEL_PATH   "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define PATH_BUFFER_SIZE   512

/* Texture */
#define BYTES_PER_PIXEL    4
#define WHITE_TEX_DIM      1
#define WHITE_TEX_LAYERS   1
#define WHITE_TEX_LEVELS   1
#define WHITE_RGBA         255
#define MAX_LOD            1000.0f

/* Frustum wireframe */
#define FRUSTUM_LINE_VERTS 24  /* 12 edges x 2 vertices each */
#define FRUSTUM_PLANES     6
#define FRUSTUM_CORNERS    8

/* Bounding sphere scale for a unit cube: sqrt(3)/2 ~ 0.866 */
#define CUBE_BSPHERE_SCALE 0.866f

/* Box scatter area */
#define BOX_AREA_SIZE      40.0f
#define BOX_GRID_COLS      15
#define BOX_MIN_CLEARANCE  3.0f
#define BOX_PUSH_DIST      4.0f
#define BOX_STACK_MOD      7
#define BOX_STACK_Y        1.5f
#define BOX_BASE_SCALE     0.8f
#define BOX_SCALE_VAR      0.2f

/* ── Shader resource counts ───────────────────────────────────────────── */
/* Every pipeline's resource binding counts are defined here so there are
 * no magic numbers in create_shader() / create_compute_pipeline_helper()
 * calls.  The counts must match the HLSL register declarations exactly. */

/* Compute pipeline: frustum culling */
#define COMP_NUM_SAMPLERS                   0
#define COMP_NUM_READONLY_STORAGE_TEXTURES  0
#define COMP_NUM_READONLY_STORAGE_BUFFERS   1  /* object data (transforms + bounds) */
#define COMP_NUM_READWRITE_STORAGE_TEXTURES 0
#define COMP_NUM_READWRITE_STORAGE_BUFFERS  2  /* indirect commands + visibility flags */
#define COMP_NUM_UNIFORM_BUFFERS            1  /* frustum planes + control flags */

/* Indirect box vertex: reads object data from storage buffer */
#define IBOX_VERT_NUM_SAMPLERS         0
#define IBOX_VERT_NUM_STORAGE_TEXTURES 0
#define IBOX_VERT_NUM_STORAGE_BUFFERS  1  /* object data */
#define IBOX_VERT_NUM_UNIFORM_BUFFERS  1  /* VP + light VP */

/* Indirect box fragment: Blinn-Phong with diffuse + shadow map */
#define IBOX_FRAG_NUM_SAMPLERS         2  /* diffuse + shadow */
#define IBOX_FRAG_NUM_STORAGE_TEXTURES 0
#define IBOX_FRAG_NUM_STORAGE_BUFFERS  0
#define IBOX_FRAG_NUM_UNIFORM_BUFFERS  1  /* lighting params */

/* Indirect shadow vertex: reads object data for transform */
#define ISHADOW_VERT_NUM_SAMPLERS         0
#define ISHADOW_VERT_NUM_STORAGE_TEXTURES 0
#define ISHADOW_VERT_NUM_STORAGE_BUFFERS  1  /* object data */
#define ISHADOW_VERT_NUM_UNIFORM_BUFFERS  1  /* light VP */

/* Indirect shadow fragment: depth-only, no resources */
#define ISHADOW_FRAG_NUM_SAMPLERS         0
#define ISHADOW_FRAG_NUM_STORAGE_TEXTURES 0
#define ISHADOW_FRAG_NUM_STORAGE_BUFFERS  0
#define ISHADOW_FRAG_NUM_UNIFORM_BUFFERS  0

/* Debug box vertex: reads object data for transforms */
#define DBOX_VERT_NUM_SAMPLERS         0
#define DBOX_VERT_NUM_STORAGE_TEXTURES 0
#define DBOX_VERT_NUM_STORAGE_BUFFERS  1  /* object data */
#define DBOX_VERT_NUM_UNIFORM_BUFFERS  1  /* debug VP */

/* Debug box fragment: reads visibility buffer for green/red coloring */
#define DBOX_FRAG_NUM_SAMPLERS         0
#define DBOX_FRAG_NUM_STORAGE_TEXTURES 0
#define DBOX_FRAG_NUM_STORAGE_BUFFERS  1  /* visibility flags */
#define DBOX_FRAG_NUM_UNIFORM_BUFFERS  0

/* Frustum lines vertex: simple MVP transform */
#define FLINE_VERT_NUM_SAMPLERS         0
#define FLINE_VERT_NUM_STORAGE_TEXTURES 0
#define FLINE_VERT_NUM_STORAGE_BUFFERS  0
#define FLINE_VERT_NUM_UNIFORM_BUFFERS  1  /* debug VP */

/* Frustum lines fragment: uses vertex color, no resources */
#define FLINE_FRAG_NUM_SAMPLERS         0
#define FLINE_FRAG_NUM_STORAGE_TEXTURES 0
#define FLINE_FRAG_NUM_STORAGE_BUFFERS  0
#define FLINE_FRAG_NUM_UNIFORM_BUFFERS  0

/* Grid vertex: VP matrix only */
#define GRID_VERT_NUM_SAMPLERS         0
#define GRID_VERT_NUM_STORAGE_TEXTURES 0
#define GRID_VERT_NUM_STORAGE_BUFFERS  0
#define GRID_VERT_NUM_UNIFORM_BUFFERS  1

/* Grid fragment: shadow map sampler + lighting params */
#define GRID_FRAG_NUM_SAMPLERS         1  /* shadow map */
#define GRID_FRAG_NUM_STORAGE_TEXTURES 0
#define GRID_FRAG_NUM_STORAGE_BUFFERS  0
#define GRID_FRAG_NUM_UNIFORM_BUFFERS  1

/* Truck scene vertex: VP + light VP */
#define TRUCK_VERT_NUM_SAMPLERS         0
#define TRUCK_VERT_NUM_STORAGE_TEXTURES 0
#define TRUCK_VERT_NUM_STORAGE_BUFFERS  0
#define TRUCK_VERT_NUM_UNIFORM_BUFFERS  1

/* Truck scene fragment: diffuse + shadow samplers + lighting */
#define TRUCK_FRAG_NUM_SAMPLERS         2  /* diffuse + shadow */
#define TRUCK_FRAG_NUM_STORAGE_TEXTURES 0
#define TRUCK_FRAG_NUM_STORAGE_BUFFERS  0
#define TRUCK_FRAG_NUM_UNIFORM_BUFFERS  1

/* ── Vertex layouts ───────────────────────────────────────────────────── */

/* SceneVertex matches ForgeGltfVertex layout: position + normal + UV.
 * Used by both box and truck meshes loaded from glTF files. */
typedef struct SceneVertex {
    vec3 position;   /* 12 bytes — model-space position */
    vec3 normal;     /* 12 bytes — outward surface normal */
    vec2 uv;         /* 8 bytes  — texture coordinates */
} SceneVertex;       /* 32 bytes total */

/* LineVertex for frustum wireframe and the split-screen divider line.
 * Per-vertex color allows different wireframe elements to have distinct
 * colors without changing shader state. */
typedef struct LineVertex {
    vec3 position;   /* 12 bytes — world-space position */
    vec4 color;      /* 16 bytes — RGBA per-vertex color */
} LineVertex;        /* 28 bytes total */

/* ── Per-object GPU data ──────────────────────────────────────────────── */
/* Must match the HLSL ObjectData struct exactly.  Stored in a storage
 * buffer read by both the compute shader (for culling) and the vertex
 * shaders (for per-instance transforms).  Each field is documented with
 * its byte offset for cross-referencing with the HLSL layout.
 *
 * Total: 112 bytes per object, 16-byte aligned. */
typedef struct ObjectGPUData {
    mat4     model;              /* offset  0: 64 bytes — model-to-world transform */
    float    color[4];           /* offset 64: 16 bytes — base color multiplier (RGBA) */
    float    bounding_sphere[4]; /* offset 80: 16 bytes — xyz=center(world), w=radius */
    Uint32   num_indices;        /* offset 96: 4 bytes  — index count for this object */
    Uint32   first_index;        /* offset 100: 4 bytes — first index in the shared index buffer */
    Sint32   vertex_offset;      /* offset 104: 4 bytes — added to each index before fetching vertex */
    Uint32   _pad;               /* offset 108: 4 bytes — align struct to 16 bytes */
} ObjectGPUData;                 /* 112 bytes */

/* ── Instance data for the truck (L13 pattern — 4xvec4 model matrix columns) ── */
/* The truck uses traditional instanced rendering (not indirect drawing)
 * because it's a single object at the scene center.  The instance buffer
 * holds one mat4 per truck instance. */
typedef struct TruckInstanceData {
    mat4 model;                  /* 64 bytes — model-to-world transform */
} TruckInstanceData;

/* ── Uniform structures ──────────────────────────────────────────────── */

/* Indirect box / truck vertex uniforms: the view-projection matrix for
 * the current camera and the light's VP for shadow map projection. */
typedef struct VertUniforms {
    mat4 vp;                     /* camera view-projection */
    mat4 light_vp;               /* light view-projection for shadow coords */
} VertUniforms;                  /* 128 bytes */

/* Shadow pass: only the light VP is needed since the vertex shader
 * transforms directly to light clip space. */
typedef struct ShadowUniforms {
    mat4 light_vp;               /* light view-projection */
} ShadowUniforms;                /* 64 bytes */

/* Box fragment uniforms: Blinn-Phong lighting parameters and shadow
 * configuration.  Padded to 16-byte alignment for GPU uniform rules. */
typedef struct BoxFragUniforms {
    float light_dir[4];          /* xyz = normalized direction, w = pad */
    float eye_pos[4];            /* xyz = camera world position, w = pad */
    float shadow_texel;          /* 1.0 / shadow_map_size for PCF offset */
    float shininess;             /* specular exponent */
    float ambient;               /* ambient light intensity */
    float specular_str;          /* specular strength multiplier */
} BoxFragUniforms;               /* 48 bytes */

/* Truck fragment uniforms — matches truck_scene.frag.hlsl cbuffer layout.
 * Includes base_color and has_texture flag so each truck primitive can
 * use either a diffuse texture or a flat color. */
typedef struct TruckFragUniforms {
    float base_color[4];         /* RGBA material color */
    float light_dir[4];          /* xyz = normalized direction, w = pad */
    float eye_pos[4];            /* xyz = camera world position, w = pad */
    float shadow_texel;          /* 1.0 / shadow_map_size */
    float shininess;             /* specular exponent */
    float ambient;               /* ambient light intensity */
    float specular_str;          /* specular strength multiplier */
    Uint32 has_texture;          /* 1 = sample diffuse texture, 0 = use base_color */
    float _pad[3];               /* pad to 16-byte boundary */
} TruckFragUniforms;             /* 80 bytes */

/* Grid vertex uniforms: just the view-projection matrix. */
typedef struct GridVertUniforms {
    mat4 vp;                     /* camera view-projection */
} GridVertUniforms;              /* 64 bytes */

/* Grid fragment uniforms: procedural grid pattern + Blinn-Phong lighting
 * with shadow map sampling.  Must match the HLSL cbuffer layout exactly. */
typedef struct GridFragUniforms {
    float line_color[4];         /* RGBA grid line color */
    float bg_color[4];           /* RGBA grid background color */
    float light_dir[4];          /* xyz = light direction, w = pad */
    float eye_pos[4];            /* xyz = camera position, w = pad */
    mat4  light_vp;              /* light VP for shadow projection */
    float grid_spacing;          /* world units between grid lines */
    float line_width;            /* line thickness [0..0.5] */
    float fade_distance;         /* distance where grid fades to background */
    float ambient;               /* ambient light intensity */
    float shininess;             /* specular exponent */
    float specular_str;          /* specular strength multiplier */
    float shadow_texel;          /* 1.0 / shadow_map_size */
    float _pad;                  /* 16-byte alignment padding */
} GridFragUniforms;              /* 160 bytes */

/* Compute shader uniforms: the 6 frustum planes extracted from the main
 * camera's VP matrix, plus control flags for toggling culling. */
typedef struct CullUniforms {
    float frustum_planes[6][4];  /* 6 planes, each (nx, ny, nz, d) */
    Uint32 num_objects;          /* number of objects to process */
    Uint32 enable_culling;       /* 1 = cull, 0 = draw everything */
    float  _pad[2];              /* 16-byte alignment */
} CullUniforms;                  /* 112 bytes */

/* Debug view vertex uniforms: the overhead camera's VP matrix. */
typedef struct DebugVertUniforms {
    mat4 vp;                     /* debug camera view-projection */
} DebugVertUniforms;             /* 64 bytes */

/* Line vertex uniforms: VP matrix for frustum wireframe and divider. */
typedef struct LineVertUniforms {
    mat4 vp;                     /* camera view-projection */
} LineVertUniforms;              /* 64 bytes */

/* ── GPU-side model data ──────────────────────────────────────────────── */
/* Mirrors the L13 pattern: each parsed glTF primitive becomes a GpuPrimitive
 * with its own vertex/index buffers, and each material becomes a GpuMaterial
 * with an optional diffuse texture. */

typedef struct GpuPrimitive {
    SDL_GPUBuffer          *vertex_buffer;   /* per-vertex position/normal/UV */
    SDL_GPUBuffer          *index_buffer;    /* triangle indices */
    Uint32                  index_count;     /* number of indices */
    Uint32                  vertex_count;    /* number of vertices */
    int                     material_index;  /* index into ModelData.materials */
    SDL_GPUIndexElementSize index_type;      /* 16-bit or 32-bit indices */
    bool                    has_uvs;         /* whether UVs are present */
} GpuPrimitive;

typedef struct GpuMaterial {
    float          base_color[4];  /* RGBA material color from glTF */
    SDL_GPUTexture *texture;       /* NULL = use placeholder white texture */
    bool           has_texture;    /* whether a diffuse texture was loaded */
} GpuMaterial;

typedef struct ModelData {
    ForgeGltfScene  scene;          /* parsed glTF scene (CPU-side) */
    ForgeArena      gltf_arena;     /* arena backing glTF allocations */
    GpuPrimitive   *primitives;     /* GPU primitive array */
    int             primitive_count; /* number of primitives */
    GpuMaterial    *materials;       /* GPU material array */
    int             material_count;  /* number of materials */
} ModelData;

/* ── Application state ────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window              *window;                    /* main application window */
    SDL_GPUDevice           *device;                    /* GPU device for all rendering */

    /* ── Pipelines ──────────────────────────────────────────────────── */
    SDL_GPUComputePipeline  *cull_pipeline;            /* frustum cull compute */
    SDL_GPUGraphicsPipeline *indirect_box_pipeline;    /* indirect-drawn boxes (main view) */
    SDL_GPUGraphicsPipeline *indirect_shadow_pipeline; /* indirect-drawn boxes (shadow pass) */
    SDL_GPUGraphicsPipeline *debug_box_pipeline;       /* all boxes with visibility coloring */
    SDL_GPUGraphicsPipeline *frustum_line_pipeline;    /* frustum wireframe lines */
    SDL_GPUGraphicsPipeline *grid_pipeline;            /* procedural grid floor */
    SDL_GPUGraphicsPipeline *truck_pipeline;           /* truck Blinn-Phong */
    SDL_GPUGraphicsPipeline *truck_shadow_pipeline;    /* truck shadow depth */

    /* ── Shared textures and samplers ───────────────────────────────── */
    SDL_GPUTexture          *depth_texture;            /* main pass depth (window-sized) */
    SDL_GPUTexture          *shadow_depth_texture;     /* shadow map (SHADOW_MAP_SIZE) */
    SDL_GPUTexture          *white_texture;            /* 1x1 white placeholder */
    SDL_GPUSampler          *sampler;                  /* linear wrap for diffuse textures */
    SDL_GPUSampler          *shadow_sampler;           /* comparison sampler for shadow PCF */
    Uint32                   depth_w, depth_h;         /* current depth texture dimensions */

    /* ── Models ─────────────────────────────────────────────────────── */
    ModelData                box_model;                /* BoxTextured glTF */
    ModelData                truck_model;              /* CesiumMilkTruck glTF */

    /* ── GPU buffers for indirect drawing ───────────────────────────── */
    SDL_GPUBuffer           *object_data_buf;          /* ObjectGPUData[NUM_BOXES] storage */
    SDL_GPUBuffer           *indirect_buf;             /* indirect draw commands (compute writes) */
    SDL_GPUBuffer           *visibility_buf;           /* per-object visibility flags (compute writes) */
    SDL_GPUBuffer           *instance_id_buf;          /* [0, 1, ..., 199] vertex buffer for SV_InstanceID */
    SDL_GPUBuffer           *frustum_line_buf;         /* frustum wireframe vertices (24 LineVertex) */

    /* ── Truck instance buffer (L13 pattern) ────────────────────────── */
    SDL_GPUBuffer           *truck_instance_buf;       /* one TruckInstanceData per mesh node */
    int                      truck_instance_count;     /* number of mesh-bearing nodes */
    int                      truck_node_to_inst[64];   /* node index → instance index (-1 = no mesh) */

    /* ── Grid ───────────────────────────────────────────────────────── */
    SDL_GPUBuffer           *grid_vertex_buf;          /* 4-vertex ground quad */
    SDL_GPUBuffer           *grid_index_buf;           /* 6 indices for 2 triangles */

    /* ── Object placement data (CPU copy for bounding sphere computation) ── */
    ObjectGPUData            objects[NUM_BOXES];         /* CPU copy of per-object data (transforms, bounds) */

    /* ── Divider line buffer (split-screen separator) ───────────────── */
    SDL_GPUBuffer           *divider_line_buf;         /* 2 LineVertex for vertical line */

    /* ── Camera state ───────────────────────────────────────────────── */
    vec3  cam_position;                                /* world-space camera position */
    float cam_yaw;                                     /* horizontal rotation (radians) */
    float cam_pitch;                                   /* vertical rotation (radians, clamped) */

    /* ── Timing ─────────────────────────────────────────────────────── */
    Uint64 last_ticks;                                 /* SDL_GetTicks() from previous frame */

    /* ── Input state ────────────────────────────────────────────────── */
    bool mouse_captured;                               /* true = relative mouse mode active */

    /* ── Toggle flags ───────────────────────────────────────────────── */
    bool culling_enabled;                              /* F key: enable/disable frustum culling */
    bool debug_view_enabled;                           /* D key: show/hide debug split-screen */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;                              /* screenshot/GIF capture state */
#endif
} app_state;

/* ── Depth texture helper ─────────────────────────────────────────────── */
/* Creates a D32_FLOAT depth texture at the given dimensions.  Used for
 * both the main pass depth buffer and the shadow map. */

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device,
                                             Uint32 w, Uint32 h,
                                             SDL_GPUTextureFormat fmt,
                                             SDL_GPUTextureUsageFlags usage)
{
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = fmt;
    info.usage                = usage;
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

/* ── Shader helper ────────────────────────────────────────────────────── */
/* Creates a vertex or fragment shader from pre-compiled SPIRV, DXIL, or MSL
 * bytecode, choosing the format supported by the current GPU device. */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice       *device,
    SDL_GPUShaderStage   stage,
    const unsigned char *spirv_code,  unsigned int spirv_size,
    const unsigned char *dxil_code,   unsigned int dxil_size,
    const char *msl_code, unsigned int msl_size,
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
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL) && msl_code && msl_size > 0) {
        info.format     = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
        info.code       = (const unsigned char *)msl_code;
        info.code_size  = msl_size;
    } else {
        SDL_Log("No supported shader format (need SPIRV, DXIL, or MSL)");
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

/* ── Compute pipeline helper ──────────────────────────────────────────── */
/* Creates a compute pipeline from pre-compiled SPIRV/DXIL bytecode or MSL source.
 * The resource binding counts are taken from the COMP_NUM_* constants
 * which must match the HLSL register declarations. */

static SDL_GPUComputePipeline *create_compute_pipeline_helper(
    SDL_GPUDevice *device,
    const unsigned char *spirv_code, unsigned int spirv_size,
    const unsigned char *dxil_code, unsigned int dxil_size,
    const char          *msl_code,   unsigned int msl_size)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUComputePipelineCreateInfo info;
    SDL_zero(info);
    info.entrypoint                    = "main";
    info.num_samplers                  = COMP_NUM_SAMPLERS;
    info.num_readonly_storage_textures = COMP_NUM_READONLY_STORAGE_TEXTURES;
    info.num_readonly_storage_buffers  = COMP_NUM_READONLY_STORAGE_BUFFERS;
    info.num_readwrite_storage_textures = COMP_NUM_READWRITE_STORAGE_TEXTURES;
    info.num_readwrite_storage_buffers = COMP_NUM_READWRITE_STORAGE_BUFFERS;
    info.num_uniform_buffers           = COMP_NUM_UNIFORM_BUFFERS;
    info.threadcount_x = WORKGROUP_SIZE;
    info.threadcount_y = 1;
    info.threadcount_z = 1;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL) && msl_code && msl_size > 0) {
        info.format     = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
        info.code       = (const unsigned char *)msl_code;
        info.code_size  = msl_size;
    } else {
        SDL_Log("No supported compute shader format");
        return NULL;
    }

    SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (!pipeline) {
        SDL_Log("Failed to create compute pipeline: %s", SDL_GetError());
    }
    return pipeline;
}

/* ── GPU buffer upload helper ─────────────────────────────────────────── */
/* Creates a GPU buffer with the given usage flags, then uploads data via
 * a transfer buffer.  This is the standard SDL GPU upload pattern:
 * create buffer -> create transfer -> map -> memcpy -> unmap -> copy pass. */

static SDL_GPUBuffer *upload_gpu_buffer(SDL_GPUDevice *device,
                                        SDL_GPUBufferUsageFlags usage,
                                        const void *data, Uint32 size)
{
    SDL_GPUBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = usage;
    buf_info.size  = size;

    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
    if (!buffer) {
        SDL_Log("Failed to create GPU buffer: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = size;

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
    dst.size   = size;

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

/* ── Texture loading helper ───────────────────────────────────────────── */
/* Loads an image file from disk, converts to R8G8B8A8_UNORM_SRGB, creates
 * a GPU texture with mipmaps, and uploads via a transfer buffer.  Returns
 * NULL on failure with an error logged. */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }
    SDL_Log("Loaded texture: %dx%d from '%s'", surface->w, surface->h, path);

    /* Convert to ABGR8888 (SDL's name for R8G8B8A8 bytes in memory). */
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

    /* Copy row-by-row to handle pitch differences between SDL surface
     * and the tightly-packed GPU transfer buffer layout. */
    Uint32 dest_row_bytes = (Uint32)(tex_w * BYTES_PER_PIXEL);
    const Uint8 *row_src = (const Uint8 *)converted->pixels;
    Uint8 *row_dst = (Uint8 *)mapped;
    for (Uint32 row = 0; row < (Uint32)tex_h; row++) {
        SDL_memcpy(row_dst + row * dest_row_bytes,
                   row_src + row * converted->pitch,
                   dest_row_bytes);
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
    tex_src.pixels_per_row  = (Uint32)tex_w;
    tex_src.rows_per_layer  = (Uint32)tex_h;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = texture;
    tex_dst.w       = (Uint32)tex_w;
    tex_dst.h       = (Uint32)tex_h;
    tex_dst.d       = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    /* Generate mipmaps from the uploaded base level. */
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

/* ── 1x1 white placeholder texture ────────────────────────────────────── */
/* Used when a glTF material has no diffuse texture — the shader samples
 * this and gets (1,1,1,1), so base_color alone controls the appearance. */

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device)
{
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = WHITE_TEX_DIM;
    tex_info.height               = WHITE_TEX_DIM;
    tex_info.layer_count_or_depth = WHITE_TEX_LAYERS;
    tex_info.num_levels           = WHITE_TEX_LEVELS;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create white texture: %s", SDL_GetError());
        return NULL;
    }

    Uint8 white_pixel[BYTES_PER_PIXEL] = {
        WHITE_RGBA, WHITE_RGBA, WHITE_RGBA, WHITE_RGBA
    };

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = sizeof(white_pixel);

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

/* ── Free GPU-side model resources ────────────────────────────────────── */
/* Releases all GPU buffers and textures associated with a loaded model.
 * Tracks already-released textures to avoid double-free when multiple
 * materials share the same texture image. */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model)
{
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
        /* Track released textures to avoid double-free when multiple
         * materials reference the same underlying GPU texture. */
        SDL_GPUTexture *released[FORGE_GLTF_MAX_IMAGES];
        int released_count = 0;
        SDL_memset(released, 0, sizeof(released));

        for (int i = 0; i < model->material_count; i++) {
            SDL_GPUTexture *tex = model->materials[i].texture;
            if (!tex) continue;

            bool already = false;
            for (int j = 0; j < released_count; j++) {
                if (released[j] == tex) { already = true; break; }
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

/* ── Upload parsed glTF scene to GPU ──────────────────────────────────── */
/* Transfers all primitives (vertex + index buffers) and material textures
 * to GPU memory.  The white_texture parameter is stored for render-time
 * fallback but not used during upload itself. */

static bool upload_model_to_gpu(SDL_GPUDevice *device, ModelData *model,
                                SDL_GPUTexture *white_texture)
{
    ForgeGltfScene *scene = &model->scene;

    /* ── Upload primitives (vertex + index buffers) ─────────────────── */
    model->primitive_count = scene->primitive_count;
    model->primitives = (GpuPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(GpuPrimitive));
    if (!model->primitives) {
        SDL_Log("Failed to allocate GPU primitives");
        return false;
    }

    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *src = &scene->primitives[i];
        GpuPrimitive *dst = &model->primitives[i];

        dst->material_index = src->material_index;
        dst->index_count = src->index_count;
        dst->vertex_count = src->vertex_count;
        dst->has_uvs = src->has_uvs;

        if (src->vertices && src->vertex_count > 0) {
            Uint32 vb_size = src->vertex_count * (Uint32)sizeof(ForgeGltfVertex);
            dst->vertex_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_VERTEX, src->vertices, vb_size);
            if (!dst->vertex_buffer) {
                free_model_gpu(device, model);
                return false;
            }
        }

        if (src->indices && src->index_count > 0) {
            Uint32 ib_size = src->index_count * src->index_stride;
            dst->index_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_INDEX, src->indices, ib_size);
            if (!dst->index_buffer) {
                free_model_gpu(device, model);
                return false;
            }

            dst->index_type = (src->index_stride == 2)
                ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                : SDL_GPU_INDEXELEMENTSIZE_32BIT;
        }
    }

    /* ── Load material textures ─────────────────────────────────────── */
    model->material_count = scene->material_count;
    model->materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!model->materials) {
        SDL_Log("Failed to allocate GPU materials");
        free_model_gpu(device, model);
        return false;
    }

    /* Track loaded textures to avoid loading the same image file twice
     * when multiple materials reference it. */
    SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
    const char *loaded_paths[FORGE_GLTF_MAX_IMAGES];
    int loaded_count = 0;
    SDL_memset(loaded_textures, 0, sizeof(loaded_textures));
    SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

    for (int i = 0; i < scene->material_count; i++) {
        const ForgeGltfMaterial *src = &scene->materials[i];
        GpuMaterial *dst = &model->materials[i];

        dst->base_color[0] = src->base_color[0];
        dst->base_color[1] = src->base_color[1];
        dst->base_color[2] = src->base_color[2];
        dst->base_color[3] = src->base_color[3];
        dst->has_texture = src->has_texture;
        dst->texture = NULL;

        if (src->has_texture && src->texture_path[0] != '\0') {
            /* Check if this texture was already loaded by a previous material. */
            bool found = false;
            for (int j = 0; j < loaded_count; j++) {
                if (loaded_paths[j] &&
                    SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
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

        SDL_Log("  Material %d: '%s' color=(%.2f,%.2f,%.2f) tex=%s",
                i, src->name,
                dst->base_color[0], dst->base_color[1], dst->base_color[2],
                dst->has_texture ? "yes" : "no");
    }

    (void)white_texture;   /* Used at render time, not during upload */
    return true;
}

/* ── Frustum plane extraction (Gribb-Hartmann method) ─────────────────── */
/* Extracts 6 frustum planes from a view-projection matrix.  Each plane is
 * stored as (nx, ny, nz, d) where dot(pos, n) + d gives the signed
 * distance to the plane.  Planes are normalized so distances are in world
 * units.
 *
 * IMPORTANT: Uses Vulkan [0,1] depth convention.
 *   Near  = row2          (NOT row3+row2 like OpenGL [-1,1])
 *   Far   = row3 - row2
 *
 * The mat4 is stored column-major (4 columns of 4 floats).  When we copy
 * to a flat array m[16]:
 *   m[0..3]   = column 0
 *   m[4..7]   = column 1
 *   m[8..11]  = column 2
 *   m[12..15] = column 3
 * So row i = { m[i], m[i+4], m[i+8], m[i+12] }. */

static void extract_frustum_planes(mat4 vp, float planes[6][4])
{
    float m[16];
    SDL_memcpy(m, &vp, sizeof(m));

    /* Left:   row3 + row0 */
    planes[0][0] = m[3]  + m[0];
    planes[0][1] = m[7]  + m[4];
    planes[0][2] = m[11] + m[8];
    planes[0][3] = m[15] + m[12];

    /* Right:  row3 - row0 */
    planes[1][0] = m[3]  - m[0];
    planes[1][1] = m[7]  - m[4];
    planes[1][2] = m[11] - m[8];
    planes[1][3] = m[15] - m[12];

    /* Bottom: row3 + row1 */
    planes[2][0] = m[3]  + m[1];
    planes[2][1] = m[7]  + m[5];
    planes[2][2] = m[11] + m[9];
    planes[2][3] = m[15] + m[13];

    /* Top:    row3 - row1 */
    planes[3][0] = m[3]  - m[1];
    planes[3][1] = m[7]  - m[5];
    planes[3][2] = m[11] - m[9];
    planes[3][3] = m[15] - m[13];

    /* Near:   row2 only (Vulkan [0,1] depth — NOT row3+row2) */
    planes[4][0] = m[2];
    planes[4][1] = m[6];
    planes[4][2] = m[10];
    planes[4][3] = m[14];

    /* Far:    row3 - row2 */
    planes[5][0] = m[3]  - m[2];
    planes[5][1] = m[7]  - m[6];
    planes[5][2] = m[11] - m[10];
    planes[5][3] = m[15] - m[14];

    /* Normalize each plane so the normal has unit length.  This makes
     * the signed distance computation produce world-space distances,
     * which is required for correct sphere-vs-plane testing. */
    for (int i = 0; i < FRUSTUM_PLANES; i++) {
        float len = sqrtf(planes[i][0] * planes[i][0] +
                          planes[i][1] * planes[i][1] +
                          planes[i][2] * planes[i][2]);
        if (len > PLANE_NORM_EPS) {
            planes[i][0] /= len;
            planes[i][1] /= len;
            planes[i][2] /= len;
            planes[i][3] /= len;
        }
    }
}

/* ── Compute frustum corners ──────────────────────────────────────────── */
/* Computes the 8 world-space corners of a frustum by inverting the VP
 * matrix and transforming the 8 NDC cube corners back to world space.
 * Uses Vulkan [0,1] depth: z=0 is the near plane, z=1 is the far plane.
 *
 * Corner order:
 *   0-3: near plane (bottom-left, bottom-right, top-right, top-left)
 *   4-7: far plane  (same winding) */

static void compute_frustum_corners(mat4 vp, vec3 out_corners[8])
{
    mat4 inv_vp = mat4_inverse(vp);

    /* NDC corners: (x, y, z, w) where z=0 is near, z=1 is far.
     * x,y range from -1 to +1 in Vulkan NDC. */
    const float ndc[8][4] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},  /* near bottom-left  */
        { 1.0f, -1.0f, 0.0f, 1.0f},  /* near bottom-right */
        { 1.0f,  1.0f, 0.0f, 1.0f},  /* near top-right    */
        {-1.0f,  1.0f, 0.0f, 1.0f},  /* near top-left     */
        {-1.0f, -1.0f, 1.0f, 1.0f},  /* far bottom-left   */
        { 1.0f, -1.0f, 1.0f, 1.0f},  /* far bottom-right  */
        { 1.0f,  1.0f, 1.0f, 1.0f},  /* far top-right     */
        {-1.0f,  1.0f, 1.0f, 1.0f},  /* far top-left      */
    };

    for (int i = 0; i < FRUSTUM_CORNERS; i++) {
        vec4 clip = vec4_create(ndc[i][0], ndc[i][1], ndc[i][2], ndc[i][3]);
        vec4 world = mat4_multiply_vec4(inv_vp, clip);

        /* Perspective divide: convert from homogeneous to Cartesian coords */
        float inv_w = 1.0f / world.w;
        out_corners[i] = vec3_create(world.x * inv_w,
                                     world.y * inv_w,
                                     world.z * inv_w);
    }
}

/* ── Build frustum wireframe line vertices ─────────────────────────────── */
/* Takes 8 frustum corners and produces 24 LineVertex values (12 edges,
 * each with 2 endpoints).  The edges connect:
 *   - Near quad:      0-1, 1-2, 2-3, 3-0
 *   - Far quad:       4-5, 5-6, 6-7, 7-4
 *   - Near-to-far:    0-4, 1-5, 2-6, 3-7
 * All vertices are bright yellow for high visibility against the scene. */

static void build_frustum_line_vertices(const vec3 corners[8],
                                        LineVertex out[FRUSTUM_LINE_VERTS])
{
    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},   /* near quad */
        {4, 5}, {5, 6}, {6, 7}, {7, 4},   /* far quad  */
        {0, 4}, {1, 5}, {2, 6}, {3, 7},   /* connecting edges */
    };

    const vec4 yellow = vec4_create(1.0f, 0.9f, 0.0f, 1.0f);

    for (int i = 0; i < 12; i++) {
        out[i * 2 + 0].position = corners[edges[i][0]];
        out[i * 2 + 0].color    = yellow;
        out[i * 2 + 1].position = corners[edges[i][1]];
        out[i * 2 + 1].color    = yellow;
    }
}
/* ── SDL_AppInit ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── 1. Initialise SDL ────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device ─────────────────────────────────────────── */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
        true,   /* debug mode */
        NULL    /* no backend preference */
    );
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create window & claim swapchain ───────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        "Forge GPU - 38 Indirect Drawing",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE);
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

    /* ── 4. Request an sRGB swapchain (SDR_LINEAR) ────────────────────── */
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

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── 5. Allocate + zero app_state ─────────────────────────────────── */
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window = window;
    state->device = device;
    state->culling_enabled = true;
    state->debug_view_enabled = true;

    /* ── 6. Create depth textures ─────────────────────────────────────── */
    int win_w = 0, win_h = 0;
    if (!SDL_GetWindowSizeInPixels(window, &win_w, &win_h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        SDL_free(state);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->depth_w = (Uint32)win_w;
    state->depth_h = (Uint32)win_h;

    /* Main scene depth buffer */
    state->depth_texture = create_depth_texture(device,
        (Uint32)win_w, (Uint32)win_h,
        DEPTH_FMT,
        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    if (!state->depth_texture) {
        SDL_free(state);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Shadow map depth buffer — needs SAMPLER usage so we can read it
     * in the lighting pass via a comparison sampler. */
    {
        SDL_GPUTextureCreateInfo si;
        SDL_zero(si);
        si.type                 = SDL_GPU_TEXTURETYPE_2D;
        si.format               = SHADOW_DEPTH_FMT;
        si.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                                  SDL_GPU_TEXTUREUSAGE_SAMPLER;
        si.width                = SHADOW_MAP_SIZE;
        si.height               = SHADOW_MAP_SIZE;
        si.layer_count_or_depth = 1;
        si.num_levels           = 1;

        state->shadow_depth_texture = SDL_CreateGPUTexture(device, &si);
        if (!state->shadow_depth_texture) {
            SDL_Log("Failed to create shadow depth texture: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTexture(device, state->depth_texture);
            SDL_free(state);
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 7. Create samplers ───────────────────────────────────────────── */

    /* Linear-wrap sampler for diffuse textures */
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
        si.min_lod        = 0.0f;
        si.max_lod        = MAX_LOD;

        state->sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->sampler) {
            SDL_Log("Failed to create sampler: %s", SDL_GetError());
            SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
            SDL_ReleaseGPUTexture(device, state->depth_texture);
            SDL_free(state);
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* Shadow comparison sampler — returns 0/1 based on depth comparison,
     * hardware-filtered for soft shadow edges (PCF). */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter      = SDL_GPU_FILTER_LINEAR;
        si.mag_filter      = SDL_GPU_FILTER_LINEAR;
        si.address_mode_u  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w  = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.compare_op      = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        si.enable_compare  = true;

        state->shadow_sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->shadow_sampler) {
            SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
            SDL_ReleaseGPUSampler(device, state->sampler);
            SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
            SDL_ReleaseGPUTexture(device, state->depth_texture);
            SDL_free(state);
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 8. Create 1x1 white placeholder texture ─────────────────────── */
    state->white_texture = create_white_texture(device);
    if (!state->white_texture) {
        SDL_Log("Failed to create white placeholder texture");
        SDL_ReleaseGPUSampler(device, state->shadow_sampler);
        SDL_ReleaseGPUSampler(device, state->sampler);
        SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
        SDL_ReleaseGPUTexture(device, state->depth_texture);
        SDL_free(state);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 9. Create all shaders (14 total) ─────────────────────────────── */

    /* Indirect box shaders — GPU-driven rendering with storage buffer reads */
    SDL_GPUShader *ibox_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        indirect_box_vert_spirv, indirect_box_vert_spirv_size,
        indirect_box_vert_dxil,  indirect_box_vert_dxil_size,
        indirect_box_vert_msl,   indirect_box_vert_msl_size,
        IBOX_VERT_NUM_SAMPLERS, IBOX_VERT_NUM_STORAGE_TEXTURES,
        IBOX_VERT_NUM_STORAGE_BUFFERS, IBOX_VERT_NUM_UNIFORM_BUFFERS);
    if (!ibox_vert) goto fail_cleanup;

    SDL_GPUShader *ibox_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        indirect_box_frag_spirv, indirect_box_frag_spirv_size,
        indirect_box_frag_dxil,  indirect_box_frag_dxil_size,
        indirect_box_frag_msl,   indirect_box_frag_msl_size,
        IBOX_FRAG_NUM_SAMPLERS, IBOX_FRAG_NUM_STORAGE_TEXTURES,
        IBOX_FRAG_NUM_STORAGE_BUFFERS, IBOX_FRAG_NUM_UNIFORM_BUFFERS);
    if (!ibox_frag) {
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    /* Indirect shadow shaders — depth-only pass for shadow map */
    SDL_GPUShader *ishadow_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        indirect_shadow_vert_spirv, indirect_shadow_vert_spirv_size,
        indirect_shadow_vert_dxil,  indirect_shadow_vert_dxil_size,
        indirect_shadow_vert_msl,   indirect_shadow_vert_msl_size,
        ISHADOW_VERT_NUM_SAMPLERS, ISHADOW_VERT_NUM_STORAGE_TEXTURES,
        ISHADOW_VERT_NUM_STORAGE_BUFFERS, ISHADOW_VERT_NUM_UNIFORM_BUFFERS);
    if (!ishadow_vert) {
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    SDL_GPUShader *ishadow_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        indirect_shadow_frag_spirv, indirect_shadow_frag_spirv_size,
        indirect_shadow_frag_dxil,  indirect_shadow_frag_dxil_size,
        indirect_shadow_frag_msl,   indirect_shadow_frag_msl_size,
        ISHADOW_FRAG_NUM_SAMPLERS, ISHADOW_FRAG_NUM_STORAGE_TEXTURES,
        ISHADOW_FRAG_NUM_STORAGE_BUFFERS, ISHADOW_FRAG_NUM_UNIFORM_BUFFERS);
    if (!ishadow_frag) {
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    /* Debug box shaders — wireframe/colored debug visualization */
    SDL_GPUShader *dbox_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        debug_box_vert_spirv, debug_box_vert_spirv_size,
        debug_box_vert_dxil,  debug_box_vert_dxil_size,
        debug_box_vert_msl,   debug_box_vert_msl_size,
        DBOX_VERT_NUM_SAMPLERS, DBOX_VERT_NUM_STORAGE_TEXTURES,
        DBOX_VERT_NUM_STORAGE_BUFFERS, DBOX_VERT_NUM_UNIFORM_BUFFERS);
    if (!dbox_vert) {
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    SDL_GPUShader *dbox_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        debug_box_frag_spirv, debug_box_frag_spirv_size,
        debug_box_frag_dxil,  debug_box_frag_dxil_size,
        debug_box_frag_msl,   debug_box_frag_msl_size,
        DBOX_FRAG_NUM_SAMPLERS, DBOX_FRAG_NUM_STORAGE_TEXTURES,
        DBOX_FRAG_NUM_STORAGE_BUFFERS, DBOX_FRAG_NUM_UNIFORM_BUFFERS);
    if (!dbox_frag) {
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    /* Frustum line shaders — wireframe frustum visualization */
    SDL_GPUShader *fline_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        frustum_lines_vert_spirv, frustum_lines_vert_spirv_size,
        frustum_lines_vert_dxil,  frustum_lines_vert_dxil_size,
        frustum_lines_vert_msl,   frustum_lines_vert_msl_size,
        FLINE_VERT_NUM_SAMPLERS, FLINE_VERT_NUM_STORAGE_TEXTURES,
        FLINE_VERT_NUM_STORAGE_BUFFERS, FLINE_VERT_NUM_UNIFORM_BUFFERS);
    if (!fline_vert) {
        SDL_ReleaseGPUShader(device, dbox_frag);
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    SDL_GPUShader *fline_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        frustum_lines_frag_spirv, frustum_lines_frag_spirv_size,
        frustum_lines_frag_dxil,  frustum_lines_frag_dxil_size,
        frustum_lines_frag_msl,   frustum_lines_frag_msl_size,
        FLINE_FRAG_NUM_SAMPLERS, FLINE_FRAG_NUM_STORAGE_TEXTURES,
        FLINE_FRAG_NUM_STORAGE_BUFFERS, FLINE_FRAG_NUM_UNIFORM_BUFFERS);
    if (!fline_frag) {
        SDL_ReleaseGPUShader(device, fline_vert);
        SDL_ReleaseGPUShader(device, dbox_frag);
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    /* Grid shaders — procedural anti-aliased grid floor */
    SDL_GPUShader *grid_vs = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        grid_vert_spirv, grid_vert_spirv_size,
        grid_vert_dxil,  grid_vert_dxil_size,
        grid_vert_msl,   grid_vert_msl_size,
        GRID_VERT_NUM_SAMPLERS, GRID_VERT_NUM_STORAGE_TEXTURES,
        GRID_VERT_NUM_STORAGE_BUFFERS, GRID_VERT_NUM_UNIFORM_BUFFERS);
    if (!grid_vs) {
        SDL_ReleaseGPUShader(device, fline_frag);
        SDL_ReleaseGPUShader(device, fline_vert);
        SDL_ReleaseGPUShader(device, dbox_frag);
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    SDL_GPUShader *grid_fs = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        grid_frag_spirv, grid_frag_spirv_size,
        grid_frag_dxil,  grid_frag_dxil_size,
        grid_frag_msl,   grid_frag_msl_size,
        GRID_FRAG_NUM_SAMPLERS, GRID_FRAG_NUM_STORAGE_TEXTURES,
        GRID_FRAG_NUM_STORAGE_BUFFERS, GRID_FRAG_NUM_UNIFORM_BUFFERS);
    if (!grid_fs) {
        SDL_ReleaseGPUShader(device, grid_vs);
        SDL_ReleaseGPUShader(device, fline_frag);
        SDL_ReleaseGPUShader(device, fline_vert);
        SDL_ReleaseGPUShader(device, dbox_frag);
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    /* Truck scene shaders — instanced rendering with per-instance mat4 */
    SDL_GPUShader *truck_vert = create_shader(device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        truck_scene_vert_spirv, truck_scene_vert_spirv_size,
        truck_scene_vert_dxil,  truck_scene_vert_dxil_size,
        truck_scene_vert_msl,   truck_scene_vert_msl_size,
        TRUCK_VERT_NUM_SAMPLERS, TRUCK_VERT_NUM_STORAGE_TEXTURES,
        TRUCK_VERT_NUM_STORAGE_BUFFERS, TRUCK_VERT_NUM_UNIFORM_BUFFERS);
    if (!truck_vert) {
        SDL_ReleaseGPUShader(device, grid_fs);
        SDL_ReleaseGPUShader(device, grid_vs);
        SDL_ReleaseGPUShader(device, fline_frag);
        SDL_ReleaseGPUShader(device, fline_vert);
        SDL_ReleaseGPUShader(device, dbox_frag);
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    SDL_GPUShader *truck_frag = create_shader(device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        truck_scene_frag_spirv, truck_scene_frag_spirv_size,
        truck_scene_frag_dxil,  truck_scene_frag_dxil_size,
        truck_scene_frag_msl,   truck_scene_frag_msl_size,
        TRUCK_FRAG_NUM_SAMPLERS, TRUCK_FRAG_NUM_STORAGE_TEXTURES,
        TRUCK_FRAG_NUM_STORAGE_BUFFERS, TRUCK_FRAG_NUM_UNIFORM_BUFFERS);
    if (!truck_frag) {
        SDL_ReleaseGPUShader(device, truck_vert);
        SDL_ReleaseGPUShader(device, grid_fs);
        SDL_ReleaseGPUShader(device, grid_vs);
        SDL_ReleaseGPUShader(device, fline_frag);
        SDL_ReleaseGPUShader(device, fline_vert);
        SDL_ReleaseGPUShader(device, dbox_frag);
        SDL_ReleaseGPUShader(device, dbox_vert);
        SDL_ReleaseGPUShader(device, ishadow_frag);
        SDL_ReleaseGPUShader(device, ishadow_vert);
        SDL_ReleaseGPUShader(device, ibox_frag);
        SDL_ReleaseGPUShader(device, ibox_vert);
        goto fail_cleanup;
    }

    /* ── 10. Create compute pipeline (frustum culling) ────────────────── */
    state->cull_pipeline = create_compute_pipeline_helper(device,
        frustum_cull_comp_spirv, frustum_cull_comp_spirv_size,
        frustum_cull_comp_dxil,  frustum_cull_comp_dxil_size,
        frustum_cull_comp_msl,   frustum_cull_comp_msl_size);
    if (!state->cull_pipeline) {
        SDL_Log("Failed to create cull compute pipeline");
        goto fail_release_shaders;
    }

    /* ── 11. Create graphics pipelines (7 total) ──────────────────────── */

    /* ── 11a. Indirect box pipeline ───────────────────────────────────
     * GPU-driven rendering: the vertex shader reads per-object data from
     * a storage buffer, indexed by an instance ID from vertex buffer slot 1.
     * Two vertex buffer slots:
     *   Slot 0: mesh vertices (position, normal, UV) — per-vertex rate
     *   Slot 1: instance IDs (uint32) — per-instance rate               */
    {
        SDL_GPUVertexAttribute attrs[4];
        SDL_zero(attrs);

        /* Per-vertex: position (float3) */
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = offsetof(SceneVertex, position);

        /* Per-vertex: normal (float3) */
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset      = offsetof(SceneVertex, normal);

        /* Per-vertex: UV (float2) */
        attrs[2].location    = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset      = offsetof(SceneVertex, uv);

        /* Per-instance: object ID (uint32) — indexes into the storage buffer */
        attrs[3].location    = 3;
        attrs[3].buffer_slot = 1;
        attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
        attrs[3].offset      = 0;

        SDL_GPUVertexBufferDescription vb_descs[2];
        SDL_zero(vb_descs);
        vb_descs[0].slot               = 0;
        vb_descs[0].pitch              = sizeof(SceneVertex);
        vb_descs[0].input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_descs[0].instance_step_rate = 0;
        vb_descs[1].slot               = 1;
        vb_descs[1].pitch              = sizeof(Uint32);
        vb_descs[1].input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vb_descs[1].instance_step_rate = 0;

        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = ibox_vert;
        pi.fragment_shader = ibox_frag;

        pi.vertex_input_state.vertex_attributes        = attrs;
        pi.vertex_input_state.num_vertex_attributes    = 4;
        pi.vertex_input_state.vertex_buffer_descriptions = vb_descs;
        pi.vertex_input_state.num_vertex_buffers       = 2;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.has_depth_stencil_target  = true;
        pi.target_info.depth_stencil_format      = DEPTH_FMT;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;

        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->indirect_box_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->indirect_box_pipeline) {
            SDL_Log("Failed to create indirect box pipeline: %s",
                    SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 11b. Indirect shadow pipeline ────────────────────────────────
     * Depth-only pass for the shadow map. Same two-slot vertex input as
     * the indirect box pipeline (mesh + instance IDs), but renders only
     * to the shadow depth target — no color attachment.                  */
    {
        SDL_GPUVertexAttribute attrs[4];
        SDL_zero(attrs);
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = offsetof(SceneVertex, position);
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset      = offsetof(SceneVertex, normal);
        attrs[2].location    = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset      = offsetof(SceneVertex, uv);
        attrs[3].location    = 3;
        attrs[3].buffer_slot = 1;
        attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
        attrs[3].offset      = 0;

        SDL_GPUVertexBufferDescription vb_descs[2];
        SDL_zero(vb_descs);
        vb_descs[0].slot               = 0;
        vb_descs[0].pitch              = sizeof(SceneVertex);
        vb_descs[0].input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_descs[0].instance_step_rate = 0;
        vb_descs[1].slot               = 1;
        vb_descs[1].pitch              = sizeof(Uint32);
        vb_descs[1].input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vb_descs[1].instance_step_rate = 0;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = ishadow_vert;
        pi.fragment_shader = ishadow_frag;

        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 4;
        pi.vertex_input_state.vertex_buffer_descriptions = vb_descs;
        pi.vertex_input_state.num_vertex_buffers         = 2;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* No color targets — depth-only shadow pass */
        pi.target_info.num_color_targets        = 0;
        pi.target_info.has_depth_stencil_target = true;
        pi.target_info.depth_stencil_format     = SHADOW_DEPTH_FMT;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;

        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        /* Depth bias to reduce shadow acne */
        pi.rasterizer_state.enable_depth_bias         = true;
        pi.rasterizer_state.depth_bias_constant_factor = 2.0f;
        pi.rasterizer_state.depth_bias_slope_factor    = 2.0f;

        state->indirect_shadow_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->indirect_shadow_pipeline) {
            SDL_Log("Failed to create indirect shadow pipeline: %s",
                    SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 11c. Debug box pipeline ──────────────────────────────────────
     * Same vertex layout as the indirect box pipeline (mesh + instance IDs).
     * Colors visible/culled objects differently for the debug split view. */
    {
        SDL_GPUVertexAttribute attrs[4];
        SDL_zero(attrs);
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = offsetof(SceneVertex, position);
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset      = offsetof(SceneVertex, normal);
        attrs[2].location    = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset      = offsetof(SceneVertex, uv);
        attrs[3].location    = 3;
        attrs[3].buffer_slot = 1;
        attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
        attrs[3].offset      = 0;

        SDL_GPUVertexBufferDescription vb_descs[2];
        SDL_zero(vb_descs);
        vb_descs[0].slot               = 0;
        vb_descs[0].pitch              = sizeof(SceneVertex);
        vb_descs[0].input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_descs[0].instance_step_rate = 0;
        vb_descs[1].slot               = 1;
        vb_descs[1].pitch              = sizeof(Uint32);
        vb_descs[1].input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vb_descs[1].instance_step_rate = 0;

        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = dbox_vert;
        pi.fragment_shader = dbox_frag;

        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 4;
        pi.vertex_input_state.vertex_buffer_descriptions = vb_descs;
        pi.vertex_input_state.num_vertex_buffers         = 2;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.has_depth_stencil_target  = true;
        pi.target_info.depth_stencil_format      = DEPTH_FMT;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;

        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->debug_box_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->debug_box_pipeline) {
            SDL_Log("Failed to create debug box pipeline: %s",
                    SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 11d. Frustum line pipeline ───────────────────────────────────
     * Line primitive type for drawing the camera frustum wireframe.
     * Depth test enabled but no depth write — lines render on top of
     * the grid without occluding scene objects.                         */
    {
        SDL_GPUVertexAttribute attrs[2];
        SDL_zero(attrs);
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = offsetof(LineVertex, position);
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[1].offset      = offsetof(LineVertex, color);

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot               = 0;
        vb_desc.pitch              = sizeof(LineVertex);
        vb_desc.input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_desc.instance_step_rate = 0;

        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = fline_vert;
        pi.fragment_shader = fline_frag;

        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 2;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.has_depth_stencil_target  = true;
        pi.target_info.depth_stencil_format      = DEPTH_FMT;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = false;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;

        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

        state->frustum_line_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->frustum_line_pipeline) {
            SDL_Log("Failed to create frustum line pipeline: %s",
                    SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 11e. Grid pipeline ───────────────────────────────────────────
     * Procedural anti-aliased grid floor with alpha blending for the
     * distance fade effect. Single float3 vertex attribute (position).  */
    {
        SDL_GPUVertexAttribute attrs[1];
        SDL_zero(attrs);
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = 0;

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot               = 0;
        vb_desc.pitch              = 12; /* 3 floats * 4 bytes */
        vb_desc.input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_desc.instance_step_rate = 0;

        /* Alpha blending for grid fade-out at distance */
        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = swapchain_format;
        ct.blend_state.enable_blend          = true;
        ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ct.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ct.blend_state.color_blend_op        = SDL_GPU_BLENDOP_ADD;
        ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ct.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ct.blend_state.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = grid_vs;
        pi.fragment_shader = grid_fs;

        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 1;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* No backface culling — grid visible from both sides */
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.has_depth_stencil_target  = true;
        pi.target_info.depth_stencil_format      = DEPTH_FMT;

        state->grid_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->grid_pipeline) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 11f. Truck pipeline ──────────────────────────────────────────
     * L13 instanced pattern: per-instance model matrix passed as 4 x
     * float4 vertex attributes. 7 total attributes (3 per-vertex from
     * the mesh + 4 per-instance mat4 columns).                         */
    {
        SDL_GPUVertexAttribute attrs[7];
        SDL_zero(attrs);

        /* Per-vertex: position */
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = offsetof(SceneVertex, position);

        /* Per-vertex: normal */
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset      = offsetof(SceneVertex, normal);

        /* Per-vertex: UV */
        attrs[2].location    = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset      = offsetof(SceneVertex, uv);

        /* Per-instance: model matrix column 0 */
        attrs[3].location    = 3;
        attrs[3].buffer_slot = 1;
        attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[3].offset      = 0;

        /* Per-instance: model matrix column 1 */
        attrs[4].location    = 4;
        attrs[4].buffer_slot = 1;
        attrs[4].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[4].offset      = 16;

        /* Per-instance: model matrix column 2 */
        attrs[5].location    = 5;
        attrs[5].buffer_slot = 1;
        attrs[5].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[5].offset      = 32;

        /* Per-instance: model matrix column 3 */
        attrs[6].location    = 6;
        attrs[6].buffer_slot = 1;
        attrs[6].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[6].offset      = 48;

        SDL_GPUVertexBufferDescription vb_descs[2];
        SDL_zero(vb_descs);
        vb_descs[0].slot               = 0;
        vb_descs[0].pitch              = sizeof(SceneVertex);
        vb_descs[0].input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_descs[0].instance_step_rate = 0;
        vb_descs[1].slot               = 1;
        vb_descs[1].pitch              = sizeof(TruckInstanceData);
        vb_descs[1].input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vb_descs[1].instance_step_rate = 0;

        SDL_GPUColorTargetDescription ct;
        SDL_zero(ct);
        ct.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = truck_vert;
        pi.fragment_shader = truck_frag;

        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 7;
        pi.vertex_input_state.vertex_buffer_descriptions = vb_descs;
        pi.vertex_input_state.num_vertex_buffers         = 2;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        pi.target_info.color_target_descriptions = &ct;
        pi.target_info.num_color_targets         = 1;
        pi.target_info.has_depth_stencil_target  = true;
        pi.target_info.depth_stencil_format      = DEPTH_FMT;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;

        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        state->truck_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->truck_pipeline) {
            SDL_Log("Failed to create truck pipeline: %s", SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 11g. Truck shadow pipeline ───────────────────────────────────
     * Depth-only shadow pass for the truck. Reuses the truck_scene
     * vertex shader (which handles per-instance mat4 columns) with
     * the indirect_shadow fragment shader (empty / depth-only).         */
    {
        SDL_GPUVertexAttribute attrs[7];
        SDL_zero(attrs);
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset      = offsetof(SceneVertex, position);
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset      = offsetof(SceneVertex, normal);
        attrs[2].location    = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset      = offsetof(SceneVertex, uv);
        attrs[3].location    = 3;
        attrs[3].buffer_slot = 1;
        attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[3].offset      = 0;
        attrs[4].location    = 4;
        attrs[4].buffer_slot = 1;
        attrs[4].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[4].offset      = 16;
        attrs[5].location    = 5;
        attrs[5].buffer_slot = 1;
        attrs[5].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[5].offset      = 32;
        attrs[6].location    = 6;
        attrs[6].buffer_slot = 1;
        attrs[6].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[6].offset      = 48;

        SDL_GPUVertexBufferDescription vb_descs[2];
        SDL_zero(vb_descs);
        vb_descs[0].slot               = 0;
        vb_descs[0].pitch              = sizeof(SceneVertex);
        vb_descs[0].input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_descs[0].instance_step_rate = 0;
        vb_descs[1].slot               = 1;
        vb_descs[1].pitch              = sizeof(TruckInstanceData);
        vb_descs[1].input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vb_descs[1].instance_step_rate = 0;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = truck_vert;
        pi.fragment_shader = ishadow_frag;

        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 7;
        pi.vertex_input_state.vertex_buffer_descriptions = vb_descs;
        pi.vertex_input_state.num_vertex_buffers         = 2;

        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* Depth-only — no color targets */
        pi.target_info.num_color_targets        = 0;
        pi.target_info.has_depth_stencil_target = true;
        pi.target_info.depth_stencil_format     = SHADOW_DEPTH_FMT;

        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;

        pi.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;

        /* Depth bias to reduce shadow acne */
        pi.rasterizer_state.enable_depth_bias         = true;
        pi.rasterizer_state.depth_bias_constant_factor = 2.0f;
        pi.rasterizer_state.depth_bias_slope_factor    = 2.0f;

        state->truck_shadow_pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pi);
        if (!state->truck_shadow_pipeline) {
            SDL_Log("Failed to create truck shadow pipeline: %s",
                    SDL_GetError());
            goto fail_release_shaders;
        }
    }

    /* ── 12. Release shaders (pipelines keep their own copies) ────────── */
    SDL_ReleaseGPUShader(device, truck_frag);
    SDL_ReleaseGPUShader(device, truck_vert);
    SDL_ReleaseGPUShader(device, grid_fs);
    SDL_ReleaseGPUShader(device, grid_vs);
    SDL_ReleaseGPUShader(device, fline_frag);
    SDL_ReleaseGPUShader(device, fline_vert);
    SDL_ReleaseGPUShader(device, dbox_frag);
    SDL_ReleaseGPUShader(device, dbox_vert);
    SDL_ReleaseGPUShader(device, ishadow_frag);
    SDL_ReleaseGPUShader(device, ishadow_vert);
    SDL_ReleaseGPUShader(device, ibox_frag);
    SDL_ReleaseGPUShader(device, ibox_vert);

    /* ── 13. Load glTF models ─────────────────────────────────────────── */
    {
        const char *base_path = SDL_GetBasePath();
        if (!base_path) {
            SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
            goto fail_cleanup;
        }

        /* Load BoxTextured */
        char box_path[PATH_BUFFER_SIZE];
        int len = SDL_snprintf(box_path, sizeof(box_path), "%s%s",
                               base_path, BOX_MODEL_PATH);
        if (len < 0 || (size_t)len >= sizeof(box_path)) {
            SDL_Log("Box model path too long");
            goto fail_cleanup;
        }

        state->box_model.gltf_arena = forge_arena_create(0);
        if (!forge_gltf_load(box_path, &state->box_model.scene, &state->box_model.gltf_arena)) {
            SDL_Log("Failed to load BoxTextured from '%s'", box_path);
            goto fail_cleanup;
        }
        if (!upload_model_to_gpu(device, &state->box_model,
                                  state->white_texture)) {
            SDL_Log("Failed to upload BoxTextured to GPU");
            goto fail_cleanup;
        }
        SDL_Log("Loaded BoxTextured: %d primitives",
                state->box_model.primitive_count);

        /* Load CesiumMilkTruck */
        char truck_path[PATH_BUFFER_SIZE];
        len = SDL_snprintf(truck_path, sizeof(truck_path), "%s%s",
                           base_path, TRUCK_MODEL_PATH);
        if (len < 0 || (size_t)len >= sizeof(truck_path)) {
            SDL_Log("Truck model path too long");
            goto fail_cleanup;
        }

        state->truck_model.gltf_arena = forge_arena_create(0);
        if (!forge_gltf_load(truck_path, &state->truck_model.scene, &state->truck_model.gltf_arena)) {
            SDL_Log("Failed to load CesiumMilkTruck from '%s'", truck_path);
            goto fail_cleanup;
        }
        if (!upload_model_to_gpu(device, &state->truck_model,
                                  state->white_texture)) {
            SDL_Log("Failed to upload CesiumMilkTruck to GPU");
            goto fail_cleanup;
        }
        SDL_Log("Loaded CesiumMilkTruck: %d primitives",
                state->truck_model.primitive_count);
    }

    /* ── 14. Set up truck instance buffer ─────────────────────────────── */
    /* The CesiumMilkTruck has multiple mesh-bearing nodes (wheels, body,
     * glass, trim), each with its own world_transform from the glTF
     * hierarchy.  We create one TruckInstanceData per mesh-bearing node,
     * storing that node's world_transform as the model matrix.  During
     * rendering, each node's draw uses first_instance to index into the
     * correct transform in the instance buffer. */
    {
        const ForgeGltfScene *ts = &state->truck_model.scene;
        TruckInstanceData truck_insts[64];
        int inst_count = 0;

        /* Initialize mapping to -1 (no mesh) */
        for (int i = 0; i < 64; i++)
            state->truck_node_to_inst[i] = -1;

        /* Build one instance per mesh-bearing node */
        for (int ni = 0; ni < ts->node_count && ni < 64; ni++) {
            if (ts->nodes[ni].mesh_index < 0) continue;
            state->truck_node_to_inst[ni] = inst_count;
            truck_insts[inst_count].model = ts->nodes[ni].world_transform;
            inst_count++;
        }

        Uint32 inst_size = (Uint32)(inst_count * (int)sizeof(TruckInstanceData));
        state->truck_instance_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX,
            truck_insts, inst_size);
        if (!state->truck_instance_buf) {
            SDL_Log("Failed to upload truck instance buffer");
            goto fail_cleanup;
        }
        state->truck_instance_count = inst_count;
        SDL_Log("Truck: %d mesh-bearing nodes → %d instance transforms",
                inst_count, inst_count);
    }

    /* ── 15. Upload grid geometry ─────────────────────────────────────── */
    {
        float grid_verts[GRID_NUM_VERTS * 3] = {
            -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
            -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
        };
        Uint16 grid_indices[GRID_NUM_INDICES] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vertex_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX,
            grid_verts, sizeof(grid_verts));
        if (!state->grid_vertex_buf) {
            SDL_Log("Failed to upload grid vertices");
            goto fail_cleanup;
        }

        state->grid_index_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX,
            grid_indices, sizeof(grid_indices));
        if (!state->grid_index_buf) {
            SDL_Log("Failed to upload grid indices");
            goto fail_cleanup;
        }
    }

    /* ── 16. Create instance ID buffer [0, 1, 2, ..., NUM_BOXES-1] ──── */
    /* Each indirect draw command uses one instance. The instance ID
     * vertex attribute tells the vertex shader which ObjectGPUData
     * entry to read from the storage buffer.                            */
    {
        Uint32 ids[NUM_BOXES];
        for (int i = 0; i < NUM_BOXES; i++) {
            ids[i] = (Uint32)i;
        }

        state->instance_id_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX,
            ids, sizeof(ids));
        if (!state->instance_id_buf) {
            SDL_Log("Failed to upload instance ID buffer");
            goto fail_cleanup;
        }
    }

    /* ── 17. Set up object placement + ObjectGPUData for all boxes ────── */
    {
        /* Extract mesh draw parameters from the BoxTextured model.
         * All 200 boxes share the same mesh — they differ only in their
         * model matrices (stored in the ObjectGPUData storage buffer). */
        const ForgeGltfScene *box_scene = &state->box_model.scene;
        Uint32 box_num_indices   = 0;
        Uint32 box_first_index   = 0;
        Sint32 box_vertex_offset = 0;
        mat4   box_mesh_base     = mat4_identity();

        for (int ni = 0; ni < box_scene->node_count; ni++) {
            if (box_scene->nodes[ni].mesh_index >= 0) {
                box_mesh_base = box_scene->nodes[ni].world_transform;
                int mi = box_scene->nodes[ni].mesh_index;
                const ForgeGltfMesh *mesh = &box_scene->meshes[mi];
                if (mesh->primitive_count > 0) {
                    int pi_idx = mesh->first_primitive;
                    const ForgeGltfPrimitive *prim =
                        &box_scene->primitives[pi_idx];
                    box_num_indices   = prim->index_count;
                    box_first_index   = 0;
                    box_vertex_offset = 0;
                }
                break;
            }
        }

        /* Generate deterministic box positions on a grid with stacking */
        for (int i = 0; i < NUM_BOXES; i++) {
            int grid_x = (i % BOX_GRID_COLS) - BOX_GRID_COLS / 2;
            int grid_z = (i / BOX_GRID_COLS) - BOX_GRID_COLS / 2;
            float x = (float)grid_x * BOX_GRID_SPACING;
            float z = (float)grid_z * BOX_GRID_SPACING;
            float y = BOX_GROUND_Y;

            /* Push boxes away from the truck at center */
            if (fabsf(x) < BOX_MIN_CLEARANCE &&
                fabsf(z) < BOX_MIN_CLEARANCE) {
                x += (x >= 0.0f ? BOX_PUSH_DIST : -BOX_PUSH_DIST);
            }

            /* Stack every BOX_STACK_MOD-th box on top of the previous */
            if (i % BOX_STACK_MOD == 0 && i > 0) {
                x = state->objects[i - 1].bounding_sphere[0];
                z = state->objects[i - 1].bounding_sphere[2];
                y = BOX_STACK_Y;
            }

            float scale = BOX_BASE_SCALE +
                          (float)(i % 3) * BOX_SCALE_VAR;

            /* Model matrix: translate * rotate * scale * mesh_base */
            mat4 t = mat4_translate(vec3_create(x, y, z));
            mat4 r = mat4_rotate_y((float)i * BOX_ROTATION_STEP);
            mat4 s = mat4_scale(vec3_create(scale, scale, scale));
            state->objects[i].model = mat4_multiply(t,
                mat4_multiply(r, mat4_multiply(s, box_mesh_base)));

            /* White color — texture provides the actual color */
            state->objects[i].color[0] = 1.0f;
            state->objects[i].color[1] = 1.0f;
            state->objects[i].color[2] = 1.0f;
            state->objects[i].color[3] = 1.0f;

            /* Bounding sphere: center = world position, radius from
             * the scaled unit-cube diagonal (sqrt(3)/2 ~= 0.866) */
            state->objects[i].bounding_sphere[0] = x;
            state->objects[i].bounding_sphere[1] = y;
            state->objects[i].bounding_sphere[2] = z;
            state->objects[i].bounding_sphere[3] =
                scale * CUBE_BSPHERE_SCALE;

            /* Draw arguments — all boxes share the same mesh */
            state->objects[i].num_indices    = box_num_indices;
            state->objects[i].first_index    = box_first_index;
            state->objects[i].vertex_offset  = box_vertex_offset;
            state->objects[i]._pad           = 0;
        }

        /* Upload object data to GPU storage buffer (read by both the
         * compute culling shader and the vertex shaders) */
        state->object_data_buf = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
            SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            state->objects, sizeof(state->objects));
        if (!state->object_data_buf) {
            SDL_Log("Failed to upload object data buffer");
            goto fail_cleanup;
        }
    }

    /* ── 18. Create indirect draw buffer ──────────────────────────────── */
    /* The compute shader writes one SDL_GPUIndirectDrawIndexedCommand
     * per object (20 bytes each). The CPU never touches this buffer
     * after creation — the GPU fills it every frame.                    */
    {
        SDL_GPUBufferCreateInfo bi;
        SDL_zero(bi);
        bi.usage = SDL_GPU_BUFFERUSAGE_INDIRECT |
                   SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        bi.size  = NUM_BOXES * 20; /* 5 x uint32 per command */

        state->indirect_buf = SDL_CreateGPUBuffer(device, &bi);
        if (!state->indirect_buf) {
            SDL_Log("Failed to create indirect buffer: %s",
                    SDL_GetError());
            goto fail_cleanup;
        }
    }

    /* ── 19. Create visibility buffer ─────────────────────────────────── */
    /* The compute shader writes per-object visibility flags (0 or 1).
     * The debug fragment shader reads these to color visible vs. culled
     * objects differently.                                              */
    {
        SDL_GPUBufferCreateInfo bi;
        SDL_zero(bi);
        bi.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                   SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        bi.size  = NUM_BOXES * sizeof(Uint32);

        state->visibility_buf = SDL_CreateGPUBuffer(device, &bi);
        if (!state->visibility_buf) {
            SDL_Log("Failed to create visibility buffer: %s",
                    SDL_GetError());
            goto fail_cleanup;
        }
    }

    /* ── 20. Create frustum wireframe vertex buffer ───────────────────── */
    /* Updated each frame with the camera frustum corners. Created as a
     * GPU buffer (no initial data — mapped and written per frame).      */
    {
        SDL_GPUBufferCreateInfo bi;
        SDL_zero(bi);
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bi.size  = FRUSTUM_LINE_VERTS * sizeof(LineVertex);

        state->frustum_line_buf = SDL_CreateGPUBuffer(device, &bi);
        if (!state->frustum_line_buf) {
            SDL_Log("Failed to create frustum line buffer: %s",
                    SDL_GetError());
            goto fail_cleanup;
        }
    }

    /* ── 21. Divider line buffer ──────────────────────────────────────── */
    /* The viewport split already creates a clear visual boundary between
     * the two halves of the debug view — no dedicated divider needed.   */
    state->divider_line_buf = NULL;

    /* ── 22. Camera initialisation ────────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = CAM_START_YAW_DEG * FORGE_DEG2RAD;
    state->cam_pitch    = CAM_START_PITCH_DEG * FORGE_DEG2RAD;
    state->last_ticks   = SDL_GetTicks();

    /* ── 23. Mouse capture ────────────────────────────────────────────── */
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s",
                SDL_GetError());
    }
    state->mouse_captured = true;

    /* ── 24. Capture init ─────────────────────────────────────────────── */
#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Capture init failed");
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    *appstate = state;
    SDL_Log("Lesson 38 initialised: %d boxes, culling=%s, debug=%s",
            NUM_BOXES,
            state->culling_enabled ? "ON" : "OFF",
            state->debug_view_enabled ? "ON" : "OFF");
    return SDL_APP_CONTINUE;

    /* ── Error cleanup ────────────────────────────────────────────────── */
fail_release_shaders:
    SDL_ReleaseGPUShader(device, truck_frag);
    SDL_ReleaseGPUShader(device, truck_vert);
    SDL_ReleaseGPUShader(device, grid_fs);
    SDL_ReleaseGPUShader(device, grid_vs);
    SDL_ReleaseGPUShader(device, fline_frag);
    SDL_ReleaseGPUShader(device, fline_vert);
    SDL_ReleaseGPUShader(device, dbox_frag);
    SDL_ReleaseGPUShader(device, dbox_vert);
    SDL_ReleaseGPUShader(device, ishadow_frag);
    SDL_ReleaseGPUShader(device, ishadow_vert);
    SDL_ReleaseGPUShader(device, ibox_frag);
    SDL_ReleaseGPUShader(device, ibox_vert);

fail_cleanup:
    /* Release any pipelines that were successfully created */
    if (state->truck_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->truck_shadow_pipeline);
    if (state->truck_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->truck_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->frustum_line_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->frustum_line_pipeline);
    if (state->debug_box_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->debug_box_pipeline);
    if (state->indirect_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->indirect_shadow_pipeline);
    if (state->indirect_box_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->indirect_box_pipeline);
    if (state->cull_pipeline)
        SDL_ReleaseGPUComputePipeline(device, state->cull_pipeline);

    /* Release buffers */
    if (state->frustum_line_buf)
        SDL_ReleaseGPUBuffer(device, state->frustum_line_buf);
    if (state->visibility_buf)
        SDL_ReleaseGPUBuffer(device, state->visibility_buf);
    if (state->indirect_buf)
        SDL_ReleaseGPUBuffer(device, state->indirect_buf);
    if (state->object_data_buf)
        SDL_ReleaseGPUBuffer(device, state->object_data_buf);
    if (state->instance_id_buf)
        SDL_ReleaseGPUBuffer(device, state->instance_id_buf);
    if (state->truck_instance_buf)
        SDL_ReleaseGPUBuffer(device, state->truck_instance_buf);
    if (state->grid_index_buf)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buf);
    if (state->grid_vertex_buf)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buf);

    /* Release models — free both GPU resources and parsed scene data */
    free_model_gpu(device, &state->truck_model);
    forge_arena_destroy(&state->truck_model.gltf_arena);
    free_model_gpu(device, &state->box_model);
    forge_arena_destroy(&state->box_model.gltf_arena);

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* Release textures and samplers */
    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->shadow_sampler)
        SDL_ReleaseGPUSampler(device, state->shadow_sampler);
    if (state->sampler)
        SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->shadow_depth_texture)
        SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);

    *appstate = NULL;   /* prevent SDL_AppQuit from double-freeing */
    SDL_free(state);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
}
/* ════════════════════════════════════════════════════════════════════════════
 * Part C — SDL_AppEvent + SDL_AppIterate
 *
 * Event handling (mouse look, toggle keys) and the per-frame render loop:
 *   Pass 0: Compute — GPU frustum culling writes indirect draw commands
 *   Pass 1: Shadow  — Depth-only for directional light
 *   Pass 2: Main    — Color scene (left viewport + optional debug right)
 * ════════════════════════════════════════════════════════════════════════════ */

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat) break;

        switch (event->key.scancode) {
        case SDL_SCANCODE_ESCAPE:
            if (state->mouse_captured) {
                SDL_SetWindowRelativeMouseMode(state->window, false);
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
            break;
        case SDL_SCANCODE_F:
            state->culling_enabled = !state->culling_enabled;
            SDL_Log("Frustum culling: %s",
                    state->culling_enabled ? "ON" : "OFF");
            break;
        case SDL_SCANCODE_V:
            state->debug_view_enabled = !state->debug_view_enabled;
            SDL_Log("Debug view: %s",
                    state->debug_view_enabled ? "ON" : "OFF");
            break;
        default:
            break;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!state->mouse_captured) {
            SDL_SetWindowRelativeMouseMode(state->window, true);
            state->mouse_captured = true;
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (state->mouse_captured) {
            /* MANDATORY: yaw DECREMENTS on positive xrel */
            state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
            state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
            if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
            if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
        }
        break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Delta time ──────────────────────────────────────────────────── */
    Uint64 now = SDL_GetTicks();
    float dt = (float)(now - state->last_ticks) / 1000.0f;
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;
    state->last_ticks = now;

    /* ── Main camera update (MANDATORY quaternion pattern) ───────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    vec3 forward = quat_forward(cam_orient);
    vec3 right   = quat_right(cam_orient);

    const bool *keys = SDL_GetKeyboardState(NULL);
    vec3 move = vec3_create(0.0f, 0.0f, 0.0f);
    if (keys[SDL_SCANCODE_W]) move = vec3_add(move, forward);
    if (keys[SDL_SCANCODE_S]) move = vec3_sub(move, forward);
    if (keys[SDL_SCANCODE_D]) move = vec3_add(move, right);
    if (keys[SDL_SCANCODE_A]) move = vec3_sub(move, right);
    if (keys[SDL_SCANCODE_SPACE])  move.y += 1.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) move.y -= 1.0f;

    if (vec3_length(move) > 0.001f) {
        move = vec3_scale(vec3_normalize(move), MOVE_SPEED * dt);
        state->cam_position = vec3_add(state->cam_position, move);
    }

    /* ── Window dimensions (handle resize / minimize) ────────────────── */
    int win_w, win_h;
    SDL_GetWindowSizeInPixels(state->window, &win_w, &win_h);
    if (win_w < 1 || win_h < 1) return SDL_APP_CONTINUE;

    /* Recreate depth texture on resize */
    if ((Uint32)win_w != state->depth_w || (Uint32)win_h != state->depth_h) {
        SDL_ReleaseGPUTexture(device, state->depth_texture);
        state->depth_texture = create_depth_texture(device,
                                                     (Uint32)win_w,
                                                     (Uint32)win_h,
                                                     DEPTH_FMT,
                                                     SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
        if (!state->depth_texture) return SDL_APP_FAILURE;
        state->depth_w = (Uint32)win_w;
        state->depth_h = (Uint32)win_h;
    }

    /* ── View-projection matrices ────────────────────────────────────── */
    /* When debug view is active the main camera renders into the left half,
     * so its aspect ratio is (w/2)/h instead of w/h. */
    float main_aspect = state->debug_view_enabled
        ? ((float)win_w * 0.5f) / (float)win_h
        : (float)win_w / (float)win_h;

    mat4 main_view = mat4_view_from_quat(state->cam_position, cam_orient);
    mat4 main_proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD, main_aspect,
                                       NEAR_PLANE, FAR_PLANE);
    mat4 main_vp   = mat4_multiply(main_proj, main_view);

    /* Debug camera — fixed overhead view looking at the scene center */
    vec3 debug_eye    = vec3_create(0.0f, DEBUG_CAM_HEIGHT, -DEBUG_CAM_BACK);
    vec3 debug_target = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 debug_up     = vec3_create(0.0f, 1.0f, 0.0f);
    mat4 debug_view   = mat4_look_at(debug_eye, debug_target, debug_up);
    float debug_aspect = ((float)win_w * 0.5f) / (float)win_h;
    mat4 debug_proj   = mat4_perspective(DEBUG_FOV_DEG * FORGE_DEG2RAD,
                                          debug_aspect, NEAR_PLANE, FAR_PLANE);
    mat4 debug_vp     = mat4_multiply(debug_proj, debug_view);

    /* ── Light setup (directional shadow map) ────────────────────────── */
    vec3 light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));

    float shadow_half = SHADOW_ORTHO_HALF;
    mat4 light_view = mat4_look_at(
        vec3_scale(light_dir, SHADOW_LIGHT_DIST),
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(-shadow_half, shadow_half,
                                  -shadow_half, shadow_half,
                                  0.1f, 100.0f);
    mat4 light_vp = mat4_multiply(light_proj, light_view);

    /* ── Frustum wireframe vertices (for debug view) ─────────────────── */
    vec3 frustum_corners[FRUSTUM_CORNERS];
    compute_frustum_corners(main_vp, frustum_corners);
    LineVertex frustum_lines[FRUSTUM_LINE_VERTS];
    build_frustum_line_vertices(frustum_corners, frustum_lines);

    /* Upload frustum line vertices — they change every frame as the main
     * camera moves. A small transfer buffer handles this. */
    {
        SDL_GPUTransferBufferCreateInfo xfer_info;
        SDL_zero(xfer_info);
        xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        xfer_info.size  = sizeof(frustum_lines);

        SDL_GPUTransferBuffer *xfer =
            SDL_CreateGPUTransferBuffer(device, &xfer_info);
        if (xfer) {
            void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
            if (mapped) {
                SDL_memcpy(mapped, frustum_lines, sizeof(frustum_lines));
                SDL_UnmapGPUTransferBuffer(device, xfer);
            }

            SDL_GPUCommandBuffer *upload_cmd =
                SDL_AcquireGPUCommandBuffer(device);
            if (upload_cmd) {
                SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(upload_cmd);
                if (copy) {
                    SDL_GPUTransferBufferLocation src;
                    SDL_zero(src);
                    src.transfer_buffer = xfer;

                    SDL_GPUBufferRegion dst;
                    SDL_zero(dst);
                    dst.buffer = state->frustum_line_buf;
                    dst.size   = sizeof(frustum_lines);

                    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
                    SDL_EndGPUCopyPass(copy);
                }
                if (!SDL_SubmitGPUCommandBuffer(upload_cmd)) {
                    SDL_Log("SDL_SubmitGPUCommandBuffer (frustum upload) "
                            "failed: %s", SDL_GetError());
                }
            }
            SDL_ReleaseGPUTransferBuffer(device, xfer);
        }
    }

    /* ── Frustum planes for the compute culling shader ────────────────── */
    CullUniforms cull_uniforms;
    SDL_zero(cull_uniforms);
    cull_uniforms.num_objects     = NUM_BOXES;
    cull_uniforms.enable_culling  = state->culling_enabled ? 1 : 0;
    extract_frustum_planes(main_vp, cull_uniforms.frustum_planes);

    /* ── Acquire command buffer + swapchain texture ──────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sc_w, sc_h;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain_tex, &sc_w, &sc_h)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }
    if (!swapchain_tex) {
        /* Swapchain not ready (e.g. window occluded) — must submit (not
         * cancel) because swapchain acquisition already occurred */
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ════════════════════════════════════════════════════════════════════
     * PASS 0 — COMPUTE: Frustum Culling
     *
     * Each workgroup thread processes one object. The compute shader
     * tests the object's bounding sphere against the six frustum planes
     * and writes an indexed indirect draw command. Culled objects get
     * num_instances = 0, which the GPU skips at zero cost.
     * ════════════════════════════════════════════════════════════════════ */
    {
        /* Push cull uniforms before beginning the compute pass */
        SDL_PushGPUComputeUniformData(cmd, 0,
                                       &cull_uniforms, sizeof(cull_uniforms));

        SDL_GPUStorageBufferReadWriteBinding rw_bindings[2];
        SDL_zero(rw_bindings);
        rw_bindings[0].buffer = state->indirect_buf;
        rw_bindings[0].cycle  = true;
        rw_bindings[1].buffer = state->visibility_buf;
        rw_bindings[1].cycle  = true;

        SDL_GPUComputePass *compute = SDL_BeginGPUComputePass(
            cmd, NULL, 0, rw_bindings, 2);
        if (compute) {
            SDL_BindGPUComputePipeline(compute, state->cull_pipeline);

            /* Object data (positions, bounding spheres) as read-only storage */
            SDL_GPUBuffer *ro_buf = state->object_data_buf;
            SDL_BindGPUComputeStorageBuffers(compute, 0, &ro_buf, 1);

            Uint32 groups = (NUM_BOXES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
            SDL_DispatchGPUCompute(compute, groups, 1, 1);

            SDL_EndGPUComputePass(compute);
        }
    }

    /* ════════════════════════════════════════════════════════════════════
     * PASS 1 — SHADOW MAP: Depth-only rendering
     *
     * Renders truck (regular draw) and boxes (indirect draw) into the
     * SHADOW_MAP_SIZE x SHADOW_MAP_SIZE depth texture for shadow mapping.
     * ════════════════════════════════════════════════════════════════════ */
    {
        SDL_GPUDepthStencilTargetInfo shadow_ds;
        SDL_zero(shadow_ds);
        shadow_ds.texture         = state->shadow_depth_texture;
        shadow_ds.load_op         = SDL_GPU_LOADOP_CLEAR;
        shadow_ds.store_op        = SDL_GPU_STOREOP_STORE;
        shadow_ds.clear_depth     = 1.0f;
        shadow_ds.cycle           = true;
        shadow_ds.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;

        SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &shadow_ds);
        if (shadow_pass) {
            SDL_GPUViewport shadow_viewport = {
                .x = 0, .y = 0,
                .w = (float)SHADOW_MAP_SIZE,
                .h = (float)SHADOW_MAP_SIZE,
                .min_depth = 0.0f, .max_depth = 1.0f
            };
            SDL_SetGPUViewport(shadow_pass, &shadow_viewport);

            /* ── Shadow: Truck (L13-style instanced draw) ────────────── */
            SDL_BindGPUGraphicsPipeline(shadow_pass,
                                         state->truck_shadow_pipeline);

            VertUniforms truck_svu = {
                .vp       = light_vp,
                .light_vp = light_vp
            };
            SDL_PushGPUVertexUniformData(cmd, 0,
                                          &truck_svu, sizeof(truck_svu));

            {
                const ForgeGltfScene *ts = &state->truck_model.scene;
                for (int ni = 0; ni < ts->node_count; ni++) {
                    const ForgeGltfNode *node = &ts->nodes[ni];
                    if (node->mesh_index < 0) continue;
                    int truck_inst = state->truck_node_to_inst[ni];
                    if (truck_inst < 0) continue;

                    const ForgeGltfMesh *mesh =
                        &ts->meshes[node->mesh_index];
                    for (int pi = 0; pi < mesh->primitive_count; pi++) {
                        int prim_idx = mesh->first_primitive + pi;
                        const GpuPrimitive *prim =
                            &state->truck_model.primitives[prim_idx];
                        if (!prim->vertex_buffer || !prim->index_buffer)
                            continue;

                        SDL_GPUBufferBinding vb[2];
                        SDL_zero(vb);
                        vb[0].buffer = prim->vertex_buffer;
                        vb[1].buffer = state->truck_instance_buf;
                        SDL_BindGPUVertexBuffers(shadow_pass, 0, vb, 2);

                        SDL_GPUBufferBinding ib;
                        SDL_zero(ib);
                        ib.buffer = prim->index_buffer;
                        SDL_BindGPUIndexBuffer(shadow_pass, &ib,
                                                prim->index_type);

                        /* first_instance selects this node's transform
                         * from the truck instance buffer */
                        SDL_DrawGPUIndexedPrimitives(shadow_pass,
                            prim->index_count, 1, 0, 0,
                            (Uint32)truck_inst);
                    }
                }
            }

            /* ── Shadow: Boxes (indirect draw) ───────────────────────── */
            SDL_BindGPUGraphicsPipeline(shadow_pass,
                                         state->indirect_shadow_pipeline);

            ShadowUniforms shadow_u = { .light_vp = light_vp };
            SDL_PushGPUVertexUniformData(cmd, 0,
                                          &shadow_u, sizeof(shadow_u));

            /* Object data storage buffer for the indirect shadow shader */
            SDL_GPUBuffer *obj_buf = state->object_data_buf;
            SDL_BindGPUVertexStorageBuffers(shadow_pass, 0, &obj_buf, 1);

            {
                const ForgeGltfScene *bs = &state->box_model.scene;
                for (int ni = 0; ni < bs->node_count; ni++) {
                    if (bs->nodes[ni].mesh_index < 0) continue;
                    const ForgeGltfMesh *mesh =
                        &bs->meshes[bs->nodes[ni].mesh_index];

                    for (int pi = 0; pi < mesh->primitive_count; pi++) {
                        int prim_idx = mesh->first_primitive + pi;
                        const GpuPrimitive *prim =
                            &state->box_model.primitives[prim_idx];
                        if (!prim->vertex_buffer || !prim->index_buffer)
                            continue;

                        SDL_GPUBufferBinding vb[2];
                        SDL_zero(vb);
                        vb[0].buffer = prim->vertex_buffer;
                        vb[1].buffer = state->instance_id_buf;
                        SDL_BindGPUVertexBuffers(shadow_pass, 0, vb, 2);

                        SDL_GPUBufferBinding ib;
                        SDL_zero(ib);
                        ib.buffer = prim->index_buffer;
                        SDL_BindGPUIndexBuffer(shadow_pass, &ib,
                                                prim->index_type);

                        /* Indirect draw — GPU reads commands written by
                         * the compute culling pass. Culled boxes have
                         * num_instances=0 and are skipped by hardware. */
                        SDL_DrawGPUIndexedPrimitivesIndirect(shadow_pass,
                            state->indirect_buf, 0, NUM_BOXES);
                    }
                    break; /* single mesh node for box model */
                }
            }

            SDL_EndGPURenderPass(shadow_pass);
        }
    }

    /* ════════════════════════════════════════════════════════════════════
     * PASS 2 — MAIN SCENE: Color + depth rendering
     *
     * Left viewport:  main camera (grid + truck + indirect boxes)
     * Right viewport: debug camera showing all boxes color-coded by
     *                 visibility, plus the main camera's frustum wireframe
     * ════════════════════════════════════════════════════════════════════ */
    {
        SDL_GPUColorTargetInfo ct;
        SDL_zero(ct);
        ct.texture    = swapchain_tex;
        ct.load_op    = SDL_GPU_LOADOP_CLEAR;
        ct.store_op   = SDL_GPU_STOREOP_STORE;
        ct.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

        SDL_GPUDepthStencilTargetInfo ds;
        SDL_zero(ds);
        ds.texture         = state->depth_texture;
        ds.load_op         = SDL_GPU_LOADOP_CLEAR;
        ds.store_op        = SDL_GPU_STOREOP_STORE;
        ds.clear_depth     = 1.0f;
        ds.cycle           = true;
        ds.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, &ds);
        if (!pass) {
            SDL_Log("SDL_BeginGPURenderPass failed");
            /* Must submit (not cancel) — swapchain texture already acquired */
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
            return SDL_APP_FAILURE;
        }

        int half_w     = win_w / 2;
        int main_width = state->debug_view_enabled ? half_w : win_w;

        /* ────────────────────────────────────────────────────────────────
         * LEFT VIEWPORT — Main camera
         * ──────────────────────────────────────────────────────────────── */
        {
            SDL_GPUViewport left_vp = {
                .x = 0, .y = 0,
                .w = (float)main_width, .h = (float)win_h,
                .min_depth = 0.0f, .max_depth = 1.0f
            };
            SDL_SetGPUViewport(pass, &left_vp);
            SDL_SetGPUScissor(pass,
                &(SDL_Rect){ 0, 0, main_width, win_h });

            /* ── Grid floor ──────────────────────────────────────────── */
            SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

            GridVertUniforms gvu = { .vp = main_vp };
            SDL_PushGPUVertexUniformData(cmd, 0, &gvu, sizeof(gvu));

            GridFragUniforms gfu;
            SDL_zero(gfu);
            gfu.line_color[0] = GRID_LINE_R;
            gfu.line_color[1] = GRID_LINE_G;
            gfu.line_color[2] = GRID_LINE_B;
            gfu.line_color[3] = 1.0f;
            gfu.bg_color[0]   = GRID_BG_R;
            gfu.bg_color[1]   = GRID_BG_G;
            gfu.bg_color[2]   = GRID_BG_B;
            gfu.bg_color[3]   = 1.0f;
            gfu.light_dir[0]  = light_dir.x;
            gfu.light_dir[1]  = light_dir.y;
            gfu.light_dir[2]  = light_dir.z;
            gfu.light_dir[3]  = 0.0f;
            gfu.eye_pos[0]    = state->cam_position.x;
            gfu.eye_pos[1]    = state->cam_position.y;
            gfu.eye_pos[2]    = state->cam_position.z;
            gfu.eye_pos[3]    = 0.0f;
            gfu.light_vp      = light_vp;
            gfu.grid_spacing   = GRID_SPACING;
            gfu.line_width     = GRID_LINE_WIDTH;
            gfu.fade_distance  = GRID_FADE_DIST;
            gfu.ambient        = GRID_AMBIENT;
            gfu.shininess      = GRID_SHININESS;
            gfu.specular_str   = GRID_SPECULAR_STR;
            gfu.shadow_texel   = 1.0f / (float)SHADOW_MAP_SIZE;
            SDL_PushGPUFragmentUniformData(cmd, 0, &gfu, sizeof(gfu));

            SDL_GPUTextureSamplerBinding shadow_bind;
            SDL_zero(shadow_bind);
            shadow_bind.texture = state->shadow_depth_texture;
            shadow_bind.sampler = state->shadow_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &shadow_bind, 1);

            SDL_GPUBufferBinding grid_vb = {
                .buffer = state->grid_vertex_buf
            };
            SDL_BindGPUVertexBuffers(pass, 0, &grid_vb, 1);

            SDL_GPUBufferBinding grid_ib = {
                .buffer = state->grid_index_buf
            };
            SDL_BindGPUIndexBuffer(pass, &grid_ib,
                                    SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES,
                                          1, 0, 0, 0);

            /* ── Truck (L13-style instanced draw) ────────────────────── */
            SDL_BindGPUGraphicsPipeline(pass, state->truck_pipeline);

            VertUniforms truck_vu = {
                .vp       = main_vp,
                .light_vp = light_vp
            };
            SDL_PushGPUVertexUniformData(cmd, 0,
                                          &truck_vu, sizeof(truck_vu));

            {
                const ForgeGltfScene *ts = &state->truck_model.scene;
                for (int ni = 0; ni < ts->node_count; ni++) {
                    const ForgeGltfNode *node = &ts->nodes[ni];
                    if (node->mesh_index < 0) continue;
                    int truck_inst = state->truck_node_to_inst[ni];
                    if (truck_inst < 0) continue;

                    const ForgeGltfMesh *mesh =
                        &ts->meshes[node->mesh_index];
                    for (int pi = 0; pi < mesh->primitive_count; pi++) {
                        int prim_idx = mesh->first_primitive + pi;
                        const GpuPrimitive *prim =
                            &state->truck_model.primitives[prim_idx];
                        if (!prim->vertex_buffer || !prim->index_buffer)
                            continue;

                        /* Per-material fragment uniforms */
                        TruckFragUniforms tfu;
                        SDL_zero(tfu);
                        tfu.light_dir[0]  = light_dir.x;
                        tfu.light_dir[1]  = light_dir.y;
                        tfu.light_dir[2]  = light_dir.z;
                        tfu.eye_pos[0]    = state->cam_position.x;
                        tfu.eye_pos[1]    = state->cam_position.y;
                        tfu.eye_pos[2]    = state->cam_position.z;
                        tfu.shadow_texel  = 1.0f / (float)SHADOW_MAP_SIZE;
                        tfu.shininess     = SCENE_SHININESS;
                        tfu.ambient       = SCENE_AMBIENT;
                        tfu.specular_str  = SCENE_SPECULAR_STR;

                        SDL_GPUTexture *tex = state->white_texture;
                        if (prim->material_index >= 0 &&
                            prim->material_index <
                                state->truck_model.material_count) {
                            const GpuMaterial *mat =
                                &state->truck_model
                                     .materials[prim->material_index];
                            tfu.base_color[0] = mat->base_color[0];
                            tfu.base_color[1] = mat->base_color[1];
                            tfu.base_color[2] = mat->base_color[2];
                            tfu.base_color[3] = mat->base_color[3];
                            tfu.has_texture =
                                mat->has_texture ? 1 : 0;
                            if (mat->texture) tex = mat->texture;
                        } else {
                            tfu.base_color[0] = 1.0f;
                            tfu.base_color[1] = 1.0f;
                            tfu.base_color[2] = 1.0f;
                            tfu.base_color[3] = 1.0f;
                            tfu.has_texture   = 0;
                        }
                        SDL_PushGPUFragmentUniformData(cmd, 0,
                            &tfu, sizeof(tfu));

                        /* Bind diffuse texture + shadow map */
                        SDL_GPUTextureSamplerBinding frag_binds[2];
                        SDL_zero(frag_binds);
                        frag_binds[0].texture = tex;
                        frag_binds[0].sampler = state->sampler;
                        frag_binds[1].texture =
                            state->shadow_depth_texture;
                        frag_binds[1].sampler = state->shadow_sampler;
                        SDL_BindGPUFragmentSamplers(pass, 0,
                                                     frag_binds, 2);

                        SDL_GPUBufferBinding vb[2];
                        SDL_zero(vb);
                        vb[0].buffer = prim->vertex_buffer;
                        vb[1].buffer = state->truck_instance_buf;
                        SDL_BindGPUVertexBuffers(pass, 0, vb, 2);

                        SDL_GPUBufferBinding ib;
                        SDL_zero(ib);
                        ib.buffer = prim->index_buffer;
                        SDL_BindGPUIndexBuffer(pass, &ib,
                                                prim->index_type);

                        /* first_instance selects this node's transform
                         * from the truck instance buffer */
                        SDL_DrawGPUIndexedPrimitives(pass,
                            prim->index_count, 1, 0, 0,
                            (Uint32)truck_inst);
                    }
                }
            }

            /* ── Boxes — THE INDIRECT DRAW (core of this lesson) ─────── */
            SDL_BindGPUGraphicsPipeline(pass,
                                         state->indirect_box_pipeline);

            VertUniforms box_vu = {
                .vp       = main_vp,
                .light_vp = light_vp
            };
            SDL_PushGPUVertexUniformData(cmd, 0,
                                          &box_vu, sizeof(box_vu));

            BoxFragUniforms bfu;
            SDL_zero(bfu);
            bfu.light_dir[0]  = light_dir.x;
            bfu.light_dir[1]  = light_dir.y;
            bfu.light_dir[2]  = light_dir.z;
            bfu.eye_pos[0]    = state->cam_position.x;
            bfu.eye_pos[1]    = state->cam_position.y;
            bfu.eye_pos[2]    = state->cam_position.z;
            bfu.shadow_texel  = 1.0f / (float)SHADOW_MAP_SIZE;
            bfu.shininess     = SCENE_SHININESS;
            bfu.ambient       = SCENE_AMBIENT;
            bfu.specular_str  = SCENE_SPECULAR_STR;
            SDL_PushGPUFragmentUniformData(cmd, 0, &bfu, sizeof(bfu));

            /* Object data as vertex storage buffer — the indirect box
             * vertex shader reads per-instance model matrices from here
             * using the instance ID */
            SDL_GPUBuffer *obj_buf = state->object_data_buf;
            SDL_BindGPUVertexStorageBuffers(pass, 0, &obj_buf, 1);

            /* Bind box diffuse texture + shadow map */
            {
                SDL_GPUTexture *box_tex = state->white_texture;
                if (state->box_model.material_count > 0 &&
                    state->box_model.materials[0].texture) {
                    box_tex = state->box_model.materials[0].texture;
                }

                SDL_GPUTextureSamplerBinding frag_binds[2];
                SDL_zero(frag_binds);
                frag_binds[0].texture = box_tex;
                frag_binds[0].sampler = state->sampler;
                frag_binds[1].texture = state->shadow_depth_texture;
                frag_binds[1].sampler = state->shadow_sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, frag_binds, 2);
            }

            /* Bind box mesh vertex buffer + instance ID buffer, then
             * issue the indirect draw — the GPU reads NUM_BOXES draw
             * commands from indirect_buf, each written by the compute
             * culling pass. Culled objects have num_instances=0. */
            {
                const ForgeGltfScene *bs = &state->box_model.scene;
                for (int ni = 0; ni < bs->node_count; ni++) {
                    if (bs->nodes[ni].mesh_index < 0) continue;
                    const ForgeGltfMesh *mesh =
                        &bs->meshes[bs->nodes[ni].mesh_index];

                    for (int pi = 0; pi < mesh->primitive_count; pi++) {
                        int prim_idx = mesh->first_primitive + pi;
                        const GpuPrimitive *prim =
                            &state->box_model.primitives[prim_idx];
                        if (!prim->vertex_buffer || !prim->index_buffer)
                            continue;

                        SDL_GPUBufferBinding vb[2];
                        SDL_zero(vb);
                        vb[0].buffer = prim->vertex_buffer;
                        vb[1].buffer = state->instance_id_buf;
                        SDL_BindGPUVertexBuffers(pass, 0, vb, 2);

                        SDL_GPUBufferBinding ib;
                        SDL_zero(ib);
                        ib.buffer = prim->index_buffer;
                        SDL_BindGPUIndexBuffer(pass, &ib,
                                                prim->index_type);

                        /* ═══════════════════════════════════════════════
                         * THE KEY CALL of this lesson:
                         * GPU reads 200 indexed draw commands from the
                         * indirect buffer. Each command was written by
                         * the compute culling shader. Culled objects
                         * have num_instances=0 — hardware skips them
                         * at zero cost, no CPU round-trip needed.
                         * ═══════════════════════════════════════════════ */
                        SDL_DrawGPUIndexedPrimitivesIndirect(pass,
                            state->indirect_buf, 0, NUM_BOXES);
                    }
                    break; /* single mesh node for box model */
                }
            }
        }

        /* ────────────────────────────────────────────────────────────────
         * RIGHT VIEWPORT — Debug camera (only when enabled)
         *
         * Shows the scene from above with all boxes color-coded:
         *   green = visible (inside frustum)
         *   red   = culled  (outside frustum)
         * Plus the main camera's frustum as a wireframe.
         * ──────────────────────────────────────────────────────────────── */
        if (state->debug_view_enabled) {
            SDL_GPUViewport right_vp = {
                .x = (float)half_w, .y = 0,
                .w = (float)(win_w - half_w), .h = (float)win_h,
                .min_depth = 0.0f, .max_depth = 1.0f
            };
            SDL_SetGPUViewport(pass, &right_vp);
            SDL_SetGPUScissor(pass,
                &(SDL_Rect){ half_w, 0, win_w - half_w, win_h });

            /* ── Grid floor (debug camera) ───────────────────────────── */
            SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);

            GridVertUniforms dgvu = { .vp = debug_vp };
            SDL_PushGPUVertexUniformData(cmd, 0, &dgvu, sizeof(dgvu));

            GridFragUniforms dgfu;
            SDL_zero(dgfu);
            dgfu.line_color[0] = GRID_LINE_R;
            dgfu.line_color[1] = GRID_LINE_G;
            dgfu.line_color[2] = GRID_LINE_B;
            dgfu.line_color[3] = 1.0f;
            dgfu.bg_color[0]   = GRID_BG_R;
            dgfu.bg_color[1]   = GRID_BG_G;
            dgfu.bg_color[2]   = GRID_BG_B;
            dgfu.bg_color[3]   = 1.0f;
            dgfu.light_dir[0]  = light_dir.x;
            dgfu.light_dir[1]  = light_dir.y;
            dgfu.light_dir[2]  = light_dir.z;
            dgfu.light_dir[3]  = 0.0f;
            dgfu.eye_pos[0]    = debug_eye.x;
            dgfu.eye_pos[1]    = debug_eye.y;
            dgfu.eye_pos[2]    = debug_eye.z;
            dgfu.eye_pos[3]    = 0.0f;
            dgfu.light_vp      = light_vp;
            dgfu.grid_spacing   = GRID_SPACING;
            dgfu.line_width     = GRID_LINE_WIDTH;
            dgfu.fade_distance  = GRID_FADE_DIST;
            dgfu.ambient        = GRID_AMBIENT;
            dgfu.shininess      = GRID_SHININESS;
            dgfu.specular_str   = GRID_SPECULAR_STR;
            dgfu.shadow_texel   = 1.0f / (float)SHADOW_MAP_SIZE;
            SDL_PushGPUFragmentUniformData(cmd, 0, &dgfu, sizeof(dgfu));

            SDL_GPUTextureSamplerBinding dsb;
            SDL_zero(dsb);
            dsb.texture = state->shadow_depth_texture;
            dsb.sampler = state->shadow_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &dsb, 1);

            SDL_GPUBufferBinding dgvb = {
                .buffer = state->grid_vertex_buf
            };
            SDL_BindGPUVertexBuffers(pass, 0, &dgvb, 1);

            SDL_GPUBufferBinding dgib = {
                .buffer = state->grid_index_buf
            };
            SDL_BindGPUIndexBuffer(pass, &dgib,
                                    SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_DrawGPUIndexedPrimitives(pass, GRID_NUM_INDICES,
                                          1, 0, 0, 0);

            /* ── Truck (debug camera) ────────────────────────────────── */
            SDL_BindGPUGraphicsPipeline(pass, state->truck_pipeline);

            VertUniforms dtvu = {
                .vp       = debug_vp,
                .light_vp = light_vp
            };
            SDL_PushGPUVertexUniformData(cmd, 0, &dtvu, sizeof(dtvu));

            {
                const ForgeGltfScene *ts = &state->truck_model.scene;
                for (int ni = 0; ni < ts->node_count; ni++) {
                    const ForgeGltfNode *node = &ts->nodes[ni];
                    if (node->mesh_index < 0) continue;
                    int truck_inst = state->truck_node_to_inst[ni];
                    if (truck_inst < 0) continue;

                    const ForgeGltfMesh *mesh =
                        &ts->meshes[node->mesh_index];
                    for (int pi = 0; pi < mesh->primitive_count; pi++) {
                        int prim_idx = mesh->first_primitive + pi;
                        const GpuPrimitive *prim =
                            &state->truck_model.primitives[prim_idx];
                        if (!prim->vertex_buffer || !prim->index_buffer)
                            continue;

                        TruckFragUniforms tfu;
                        SDL_zero(tfu);
                        tfu.light_dir[0]  = light_dir.x;
                        tfu.light_dir[1]  = light_dir.y;
                        tfu.light_dir[2]  = light_dir.z;
                        tfu.eye_pos[0]    = debug_eye.x;
                        tfu.eye_pos[1]    = debug_eye.y;
                        tfu.eye_pos[2]    = debug_eye.z;
                        tfu.shadow_texel  = 1.0f / (float)SHADOW_MAP_SIZE;
                        tfu.shininess     = SCENE_SHININESS;
                        tfu.ambient       = SCENE_AMBIENT;
                        tfu.specular_str  = SCENE_SPECULAR_STR;

                        SDL_GPUTexture *tex = state->white_texture;
                        if (prim->material_index >= 0 &&
                            prim->material_index <
                                state->truck_model.material_count) {
                            const GpuMaterial *mat =
                                &state->truck_model
                                     .materials[prim->material_index];
                            tfu.base_color[0] = mat->base_color[0];
                            tfu.base_color[1] = mat->base_color[1];
                            tfu.base_color[2] = mat->base_color[2];
                            tfu.base_color[3] = mat->base_color[3];
                            tfu.has_texture =
                                mat->has_texture ? 1 : 0;
                            if (mat->texture) tex = mat->texture;
                        } else {
                            tfu.base_color[0] = 1.0f;
                            tfu.base_color[1] = 1.0f;
                            tfu.base_color[2] = 1.0f;
                            tfu.base_color[3] = 1.0f;
                            tfu.has_texture   = 0;
                        }
                        SDL_PushGPUFragmentUniformData(cmd, 0,
                            &tfu, sizeof(tfu));

                        SDL_GPUTextureSamplerBinding fb[2];
                        SDL_zero(fb);
                        fb[0].texture = tex;
                        fb[0].sampler = state->sampler;
                        fb[1].texture = state->shadow_depth_texture;
                        fb[1].sampler = state->shadow_sampler;
                        SDL_BindGPUFragmentSamplers(pass, 0, fb, 2);

                        SDL_GPUBufferBinding vb[2];
                        SDL_zero(vb);
                        vb[0].buffer = prim->vertex_buffer;
                        vb[1].buffer = state->truck_instance_buf;
                        SDL_BindGPUVertexBuffers(pass, 0, vb, 2);

                        SDL_GPUBufferBinding ib;
                        SDL_zero(ib);
                        ib.buffer = prim->index_buffer;
                        SDL_BindGPUIndexBuffer(pass, &ib,
                                                prim->index_type);

                        /* first_instance selects this node's transform
                         * from the truck instance buffer */
                        SDL_DrawGPUIndexedPrimitives(pass,
                            prim->index_count, 1, 0, 0,
                            (Uint32)truck_inst);
                    }
                }
            }

            /* ── Debug boxes (ALL objects, visibility-colored) ────────── */
            /* The debug box shader reads per-instance transforms from
             * the object data storage buffer and visibility flags from
             * the visibility storage buffer. Green = visible, red = culled. */
            SDL_BindGPUGraphicsPipeline(pass, state->debug_box_pipeline);

            DebugVertUniforms dvu = { .vp = debug_vp };
            SDL_PushGPUVertexUniformData(cmd, 0, &dvu, sizeof(dvu));

            /* Vertex storage: object transforms */
            SDL_GPUBuffer *dobj = state->object_data_buf;
            SDL_BindGPUVertexStorageBuffers(pass, 0, &dobj, 1);

            /* Fragment storage: visibility flags */
            SDL_GPUBuffer *dvis = state->visibility_buf;
            SDL_BindGPUFragmentStorageBuffers(pass, 0, &dvis, 1);

            {
                const ForgeGltfScene *bs = &state->box_model.scene;
                for (int ni = 0; ni < bs->node_count; ni++) {
                    if (bs->nodes[ni].mesh_index < 0) continue;
                    const ForgeGltfMesh *mesh =
                        &bs->meshes[bs->nodes[ni].mesh_index];

                    for (int pi = 0; pi < mesh->primitive_count; pi++) {
                        int prim_idx = mesh->first_primitive + pi;
                        const GpuPrimitive *prim =
                            &state->box_model.primitives[prim_idx];
                        if (!prim->vertex_buffer || !prim->index_buffer)
                            continue;

                        SDL_GPUBufferBinding vb[2];
                        SDL_zero(vb);
                        vb[0].buffer = prim->vertex_buffer;
                        vb[1].buffer = state->instance_id_buf;
                        SDL_BindGPUVertexBuffers(pass, 0, vb, 2);

                        SDL_GPUBufferBinding ib;
                        SDL_zero(ib);
                        ib.buffer = prim->index_buffer;
                        SDL_BindGPUIndexBuffer(pass, &ib,
                                                prim->index_type);

                        /* Regular instanced draw — ALL boxes rendered so
                         * the debug view shows which are inside/outside
                         * the main camera's frustum */
                        SDL_DrawGPUIndexedPrimitives(pass,
                            prim->index_count, NUM_BOXES, 0, 0, 0);
                    }
                    break; /* single mesh node */
                }
            }

            /* ── Frustum wireframe (12 edges as line list) ───────────── */
            SDL_BindGPUGraphicsPipeline(pass,
                                         state->frustum_line_pipeline);

            LineVertUniforms lvu = { .vp = debug_vp };
            SDL_PushGPUVertexUniformData(cmd, 0, &lvu, sizeof(lvu));

            SDL_GPUBufferBinding fl_vb = {
                .buffer = state->frustum_line_buf
            };
            SDL_BindGPUVertexBuffers(pass, 0, &fl_vb, 1);

            SDL_DrawGPUPrimitives(pass, FRUSTUM_LINE_VERTS, 1, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    /* ── Submit command buffer + optional capture ────────────────────── */
#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain_tex)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            }
        }
        if (forge_capture_should_quit(&state->capture)) {
            return SDL_APP_SUCCESS;
        }
    } else
#endif
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Update window title with culling status */
    {
        char title[128];
        SDL_snprintf(title, sizeof(title),
            "Forge GPU - 38 Indirect Drawing | Culling: %s | Objects: %d",
            state->culling_enabled ? "ON" : "OFF", NUM_BOXES);
        SDL_SetWindowTitle(state->window, title);
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    if (!appstate) return;
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* Wait for GPU to finish before releasing resources */
    SDL_WaitForGPUIdle(device);

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* ── Release GPU buffers ─────────────────────────────────────────── */
    if (state->divider_line_buf)
        SDL_ReleaseGPUBuffer(device, state->divider_line_buf);
    if (state->frustum_line_buf)
        SDL_ReleaseGPUBuffer(device, state->frustum_line_buf);
    if (state->visibility_buf)
        SDL_ReleaseGPUBuffer(device, state->visibility_buf);
    if (state->indirect_buf)
        SDL_ReleaseGPUBuffer(device, state->indirect_buf);
    if (state->object_data_buf)
        SDL_ReleaseGPUBuffer(device, state->object_data_buf);
    if (state->instance_id_buf)
        SDL_ReleaseGPUBuffer(device, state->instance_id_buf);
    if (state->truck_instance_buf)
        SDL_ReleaseGPUBuffer(device, state->truck_instance_buf);
    if (state->grid_index_buf)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buf);
    if (state->grid_vertex_buf)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buf);

    /* ── Release model GPU resources ─────────────────────────────────── */
    free_model_gpu(device, &state->box_model);
    forge_arena_destroy(&state->box_model.gltf_arena);
    free_model_gpu(device, &state->truck_model);
    forge_arena_destroy(&state->truck_model.gltf_arena);

    /* ── Release textures ────────────────────────────────────────────── */
    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->shadow_depth_texture)
        SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);

    /* ── Release samplers ────────────────────────────────────────────── */
    if (state->shadow_sampler)
        SDL_ReleaseGPUSampler(device, state->shadow_sampler);
    if (state->sampler)
        SDL_ReleaseGPUSampler(device, state->sampler);

    /* ── Release pipelines ───────────────────────────────────────────── */
    if (state->truck_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->truck_shadow_pipeline);
    if (state->truck_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->truck_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->frustum_line_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->frustum_line_pipeline);
    if (state->debug_box_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->debug_box_pipeline);
    if (state->indirect_shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->indirect_shadow_pipeline);
    if (state->indirect_box_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->indirect_box_pipeline);
    if (state->cull_pipeline)
        SDL_ReleaseGPUComputePipeline(device, state->cull_pipeline);

    /* ── Release window and device ───────────────────────────────────── */
    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);

    SDL_free(state);
    SDL_Log("Lesson 38 cleanup complete");
}
