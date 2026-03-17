# Lesson 34 — Portals & Outlines: Implementation Plan

## Overview

Teach stencil buffer fundamentals through three visual techniques: a portal
showing an alternate world, object outlines via stencil silhouette expansion,
and a debug visualization of stencil buffer contents. All geometry is
procedural (cubes, spheres, portal frame). Uses traditional vertex input (not
vertex pulling) to keep focus on the stencil concepts.

## dev-gpu-lesson Compliance Checklist

This plan follows every requirement from the `/dev-gpu-lesson` skill:

- [x] Start from clean main branch (`git checkout main && git pull`)
- [x] Determine math needs — uses `mat4_scale_uniform`, `mat4_translate`,
      `mat4_multiply`, `vec3_create`, `vec3_negate`, `mat4_perspective`,
      `mat4_view_from_quat`, `quat_from_euler` — all exist in forge_math.h
- [x] Create lesson directory: `lessons/gpu/34-stencil-testing/`
- [x] main.c uses SDL callback architecture with `#define SDL_MAIN_USE_CALLBACKS 1`
- [x] Uses `#include "math/forge_math.h"` — all math via library types
- [x] Uses `SDL_calloc` / `SDL_free` for app_state
- [x] Every SDL bool return checked with descriptive error messages
- [x] Window: `#define WINDOW_WIDTH 1280`, `#define WINDOW_HEIGHT 720`
- [x] No magic numbers — all constants defined
- [x] Extensive comments explaining *why* and *purpose*
- [x] C99 style matching SDL conventions
- [x] Vertex data uses math library types (vec3, vec2)
- [x] CMakeLists.txt follows project pattern
- [x] README.md follows structure: learn, result, concepts, math, building, skill, exercises
- [x] Root CMakeLists.txt updated with `add_subdirectory`
- [x] Root README.md updated with lesson row
- [x] PLAN.md updated to check off lesson
- [x] Diagrams via `/dev-create-diagram` — 6+ matplotlib diagrams
- [x] Matching skill created in `.claude/skills/`
- [x] Markdown linting with markdownlint-cli2
- [x] **Chunked write for main.c** — mandatory 3-part decomposition below
- [x] Build verification via Task agent with `model: "haiku"`
- [x] Screenshot capture via `/dev-add-screenshot`
- [x] No glTF assets (all procedural) — CMakeLists has no cJSON, no asset copies
- [x] Unix line endings (LF)
- [x] All shaders compiled via `scripts/compile_shaders.py`

## Architecture

### Render Passes

```text
Pass 1: Shadow map (depth-only, D32_FLOAT, 2048x2048)
  - Draw main world cubes only

Pass 2: Main scene (swapchain color + D24_UNORM_S8_UINT depth-stencil)
  Phase A: Main world + grid    (depth-writing occluders first)
  Phase B: Portal mask write    (stencil=1, no color, no depth write)
  Phase C: Portal world         (stencil==1, alternate objects + tint)
  Phase D: Portal frame         (stencil ALWAYS, visible frame mesh)
  Phase E: Outline pass         (stencil REPLACE then NOT_EQUAL for outlines)

Pass 3: Debug overlay           (if toggled, fullscreen stencil visualization)
```

### Pipelines (9 scene + 1 debug = 10 total)

| # | Name                | Stencil Test | Pass Op  | Color Write | Depth Write |
|---|---------------------|--------------|----------|-------------|-------------|
| 1 | shadow_pipeline     | disabled     | —        | no target   | yes         |
| 2 | mask_pipeline       | ALWAYS       | REPLACE  | disabled    | no          |
| 3 | portal_pipeline     | EQUAL (1)    | KEEP     | yes         | yes         |
| 4 | main_pipeline       | NOT_EQUAL(1) | KEEP     | yes         | yes         |
| 5 | frame_pipeline      | ALWAYS       | KEEP     | yes         | yes         |
| 6 | outline_write_pip   | ALWAYS       | REPLACE  | yes         | yes         |
| 7 | outline_draw_pip    | NOT_EQUAL(2) | KEEP     | yes         | no          |
| 8 | grid_pipeline       | NOT_EQUAL(1) | KEEP     | yes         | yes         |
| 9 | grid_portal_pip     | EQUAL (1)    | KEEP     | yes         | yes         |

### Shaders

| File                      | Stage    | Purpose                                      |
|---------------------------|----------|----------------------------------------------|
| scene.vert.hlsl           | Vertex   | MVP + world pos + normal + light clip coords  |
| scene.frag.hlsl           | Fragment | Blinn-Phong + shadow + base_color + tint      |
| shadow.vert.hlsl          | Vertex   | Light-space MVP transform                     |
| shadow.frag.hlsl          | Fragment | Empty (depth-only, required by SDL GPU)       |
| grid.vert.hlsl            | Vertex   | Grid floor vertex transform                   |
| grid.frag.hlsl            | Fragment | Procedural grid + shadow + tint_color uniform |
| outline.frag.hlsl         | Fragment | Solid outline color output (reuses scene.vert) |
| debug_overlay.vert.hlsl   | Vertex   | Fullscreen quad from SV_VertexID              |
| debug_overlay.frag.hlsl   | Fragment | Sample debug texture with transparency        |

### Geometry (all procedural)

- **Cubes**: 36 vertices each (6 faces × 2 triangles × 3 verts), with normals
- **Spheres**: UV sphere (~20 lat × 20 lon), with normals
- **Portal frame**: 4 box sections (top, left, right, threshold)
- **Portal mask quad**: Single quad filling the portal opening
- **Grid floor**: 4-vertex quad on XZ plane (reuse from L33)
- **Fullscreen quad**: Generated from SV_VertexID in debug overlay shader

### Uniform Structures

```c
typedef struct SceneVertUniforms {
    mat4 mvp;       /* model-view-projection */
    mat4 model;     /* model (world) matrix */
    mat4 light_vp;  /* light VP × model for shadow coords */
} SceneVertUniforms;                                /* 192 bytes */

typedef struct SceneFragUniforms {
    float base_color[4];    /* RGBA material color */
    float eye_pos[3];       /* camera world position */
    float ambient;
    float light_dir[4];     /* xyz directional light direction */
    float light_color[3];   /* RGB light color */
    float light_intensity;
    float shininess;
    float specular_str;
    float tint[3];          /* additive tint for portal world */
    float _pad0;
} SceneFragUniforms;                                /* 88 bytes */

typedef struct GridVertUniforms {
    mat4 vp;        /* view-projection */
    mat4 light_vp;  /* light VP for shadow coords */
} GridVertUniforms;                                 /* 128 bytes */

typedef struct GridFragUniforms {
    float line_color[4];
    float bg_color[4];
    float light_dir[3];
    float light_intensity;
    float eye_pos[3];
    float grid_spacing;
    float line_width;
    float fade_distance;
    float ambient;
    float _pad;
    float tint_color[4];    /* multiplicative tint for portal grid */
} GridFragUniforms;                                 /* 96 bytes */

typedef struct OutlineFragUniforms {
    float outline_color[4]; /* RGBA outline color */
} OutlineFragUniforms;                              /* 16 bytes */
```

### app_state Structure

```c
typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Pipelines (9 scene + 1 debug = 10) */
    SDL_GPUGraphicsPipeline *shadow_pipeline;
    SDL_GPUGraphicsPipeline *mask_pipeline;
    SDL_GPUGraphicsPipeline *portal_pipeline;
    SDL_GPUGraphicsPipeline *main_pipeline;
    SDL_GPUGraphicsPipeline *frame_pipeline;
    SDL_GPUGraphicsPipeline *outline_write_pipeline;
    SDL_GPUGraphicsPipeline *outline_draw_pipeline;
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUGraphicsPipeline *grid_portal_pipeline;
    SDL_GPUGraphicsPipeline *debug_pipeline;

    /* Render targets */
    SDL_GPUTexture *shadow_depth;    /* D32_FLOAT 2048x2048 */
    SDL_GPUTexture *main_depth;      /* D24_UNORM_S8_UINT window-sized */

    /* Samplers */
    SDL_GPUSampler *nearest_clamp;   /* shadow map sampling */

    /* Geometry buffers */
    SDL_GPUBuffer *cube_vb;          /* cube vertices */
    SDL_GPUBuffer *cube_ib;          /* cube indices */
    SDL_GPUBuffer *sphere_vb;        /* sphere vertices */
    SDL_GPUBuffer *sphere_ib;        /* sphere indices */
    SDL_GPUBuffer *portal_frame_vb;  /* portal frame vertices */
    SDL_GPUBuffer *portal_frame_ib;  /* portal frame indices */
    SDL_GPUBuffer *portal_mask_vb;   /* portal opening quad vertices */
    SDL_GPUBuffer *portal_mask_ib;   /* portal opening quad indices */
    SDL_GPUBuffer *grid_vb;          /* grid floor quad */
    SDL_GPUBuffer *grid_ib;          /* grid floor indices */

    /* Geometry counts */
    Uint32 cube_index_count;
    Uint32 sphere_index_count;
    Uint32 portal_frame_index_count;
    Uint32 portal_mask_index_count;

    /* Debug overlay */
    SDL_GPUTexture *debug_texture;   /* stencil visualization RGBA */
    SDL_GPUSampler *debug_sampler;   /* nearest for debug texture */
    bool show_stencil_debug;         /* toggle with V key */

    /* Light */
    mat4 light_vp;

    /* Swapchain format */
    SDL_GPUTextureFormat swapchain_format;

    /* Camera */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Timing & input */
    Uint64 last_ticks;
    bool   mouse_captured;

#ifdef FORGE_CAPTURE
    ForgeCapture capture;
#endif
} app_state;
```

## main.c Decomposition

This file is too large for a single Write call. Use the chunked-write pattern:

- **Part A** (lines ~1-500): File header, `#define SDL_MAIN_USE_CALLBACKS 1`,
  all `#include` directives (SDL, math, compiled shaders), all `#define`
  constants, all `typedef struct` definitions (Vertex, SceneVertUniforms,
  SceneFragUniforms, GridVertUniforms, GridFragUniforms, OutlineFragUniforms,
  app_state), `create_shader()` helper, `upload_gpu_buffer()` helper,
  procedural geometry generators (`generate_cube_geometry()`,
  `generate_sphere_geometry()`, `generate_portal_frame()`,
  `generate_portal_mask_quad()`)

- **Part B** (lines ~501-1100): `SDL_AppInit()` — device/window creation,
  format negotiation (D24_UNORM_S8_UINT with D32_FLOAT_S8_UINT fallback),
  depth-stencil texture creation, shadow depth texture creation, sampler
  creation, geometry generation and upload, all 10 pipeline creations
  (shadow, mask, portal, main, frame, outline_write, outline_draw, grid,
  grid_portal, debug), camera/light initialization, capture init

- **Part C** (lines ~1101-1700): `SDL_AppEvent()` — input handling (WASD,
  mouse, V for debug toggle, Escape for mouse capture), `SDL_AppIterate()` —
  camera update, shadow pass, main scene pass (phases A-E: mask write, portal
  world, main world, grid floor, portal frame, outline pass), debug overlay
  pass (stencil readback + visualization), submit; `SDL_AppQuit()` — cleanup
  in reverse order

Agents B and C depend on Agent A's type definitions. Run A first, then B+C
in parallel.

## Team Assignments

### Team 1: Shaders (parallel, independent)

- **Agent: shader-writer** — Write all 9 HLSL shader files to
  `lessons/gpu/34-stencil-testing/shaders/`. Then compile with
  `python scripts/compile_shaders.py 34`.

### Team 2: Diagrams (parallel, independent)

- **Agent: diagram-creator** — Add 6+ diagram functions to
  `scripts/forge_diagrams/gpu/lesson_34.py`, register in `__main__.py`,
  generate PNGs to `lessons/gpu/34-stencil-testing/assets/`.

### Team 3: main.c (sequential A, then parallel B+C)

- **Agent A: code-header** — Write Part A to `/tmp/lesson_34_part_a.c`
- **Agent B: code-init** — Write Part B to `/tmp/lesson_34_part_b.c`
  (after A completes)
- **Agent C: code-render** — Write Part C to `/tmp/lesson_34_part_c.c`
  (after A completes, parallel with B)
- **Assembly**: `cat /tmp/lesson_34_part_a.c /tmp/lesson_34_part_b.c
  /tmp/lesson_34_part_c.c > lessons/gpu/34-stencil-testing/main.c`

### Team 4: Support files (after Team 1 + Team 3)

- **Agent: support-files** — CMakeLists.txt, root CMakeLists.txt update,
  root README.md update, root PLAN.md update, skill file

### Team 5: README (after diagrams + code)

- **Agent: readme-writer** — Write README.md with diagrams embedded

### Team 6: Quality (after everything)

- **Agent: build-test** — Build, run, screenshot, markdown lint

## Constant Definitions

```c
#define WINDOW_WIDTH       1280
#define WINDOW_HEIGHT      720
#define SHADOW_MAP_SIZE    2048
#define FOV_DEG            60.0f
#define NEAR_PLANE         0.1f
#define FAR_PLANE          200.0f
#define MOVE_SPEED         5.0f
#define MOUSE_SENSITIVITY  0.003f
#define GRID_HALF_SIZE     50.0f
#define GRID_INDEX_COUNT   6
#define CLEAR_R            0.05f
#define CLEAR_G            0.05f
#define CLEAR_B            0.08f
#define PORTAL_WIDTH       2.0f
#define PORTAL_HEIGHT      3.0f
#define PORTAL_THICKNESS   0.2f
#define OUTLINE_SCALE      1.04f
#define STENCIL_PORTAL     1
#define STENCIL_OUTLINE    2
#define SPHERE_LAT_SEGS    20
#define SPHERE_LON_SEGS    20
#define SHADOW_DEPTH_FMT   SDL_GPU_TEXTUREFORMAT_D32_FLOAT
```

## Scene Layout

```text
Main world (stencil != 1):
  Cube A: pos (2, 0.5, -3), size 1.0, color red
  Cube B: pos (-1, 0.5, -2), size 1.0, color blue (outlined)
  Cube C: pos (-1, 1.5, -2), size 0.8, color cyan (stacked on B)
  Cube D: pos (3, 0.5, 1), size 1.2, color green (outlined)

Portal: pos (0, 0, -5), facing +Z
  Frame: stone grey (0.5, 0.5, 0.5)
  Opening: 2.0 wide × 3.0 tall

Portal world (stencil == 1):
  Sphere A: pos (0, 1.0, -7), radius 0.8, color gold
  Sphere B: pos (-1.5, 0.6, -6), radius 0.6, color magenta
  Sphere C: pos (1.2, 0.5, -8), radius 0.5, color cyan

Camera initial: pos (3, 2, 2), yaw -2.5, pitch -0.3
Light dir: normalized (0.4, -0.8, -0.6)
```
