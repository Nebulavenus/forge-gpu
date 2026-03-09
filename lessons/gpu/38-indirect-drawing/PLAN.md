# Lesson 38 — Indirect Drawing — Implementation Plan

## Overview

GPU-driven rendering using `SDL_DrawGPUIndexedPrimitivesIndirect`. A compute
shader performs frustum culling and writes indirect draw commands — the CPU
never decides what to draw. Dual-camera split-screen: left half shows the main
camera with culled indirect draws, right half shows an overhead debug camera
with all objects color-coded green (visible) or red (culled), plus the main
camera's frustum drawn as wireframe lines.

## Scene

- CesiumMilkTruck at center — drawn with regular draw calls (always visible)
- 200 BoxTextured instances — drawn with indirect draw (GPU-culled)
- Deterministic grid layout (~40x40 unit area), some stacked
- Grid floor in both viewports

## Render Architecture

### Pass 0: Compute — Frustum Cull

- Reads: object_data_buf (RO storage), frustum planes (uniform)
- Writes: indirect_buf (RW storage), visibility_buf (RW storage)
- Dispatch: (200 + 63) / 64 = 4 groups of 64 threads
- Each thread writes one `SDL_GPUIndexedIndirectDrawCommand`
  - Visible: num_instances = 1
  - Culled: num_instances = 0

### Pass 1: Shadow Map (D32_FLOAT 2048x2048)

- Draw truck (regular draw calls)
- Draw boxes (indirect draw — same indirect buffer, shadow vertex shader)

### Pass 2: Main Scene Render (swapchain + D32_FLOAT depth)

**Left viewport (main camera):**

- Set viewport/scissor to left half
- Draw grid floor (regular)
- Draw truck (regular, Blinn-Phong + shadow)
- Draw boxes with `SDL_DrawGPUIndexedPrimitivesIndirect` (THE KEY CALL)
  - Vertex buf slot 0: box mesh vertices
  - Vertex buf slot 1: instance_id_buf [0,1,...,199]
  - Index buf: box mesh indices
  - Storage buf: object_data_buf (transforms + colors)
  - Shadow map sampler

**Right viewport (debug camera):**

- Set viewport/scissor to right half
- Draw grid floor (regular)
- Draw truck (regular)
- Draw ALL boxes with debug shader (regular instanced draw, 200 instances)
  - Fragment shader reads visibility_buf for green/red coloring
- Draw frustum wireframe (line list, 24 vertices, 12 edges)
- Draw vertical divider line between halves

### Per-Object Data Flow (first_instance pattern)

The instance vertex buffer `[0,1,...,199]` provides object_id as a per-instance
attribute (TEXCOORD3). Each indirect command sets `first_instance = idx` to
offset into this buffer. The vertex shader uses object_id to index into the
storage buffer of transforms. This is portable — SV_InstanceID is NOT used.

## Shaders (12 total)

| File | Stage | Purpose |
|------|-------|---------|
| frustum_cull.comp.hlsl | Compute | Frustum test, fill indirect buffer, write visibility |
| indirect_box.vert.hlsl | Vertex | MVP via storage buffer lookup using instance object_id |
| indirect_box.frag.hlsl | Fragment | Blinn-Phong + shadow + texture (storage-based) |
| indirect_shadow.vert.hlsl | Vertex | Light-space transform via storage buffer lookup |
| indirect_shadow.frag.hlsl | Fragment | Empty depth-only |
| debug_box.vert.hlsl | Vertex | MVP for debug view (all objects, passes object_id) |
| debug_box.frag.hlsl | Fragment | Reads visibility buffer → green/red |
| frustum_lines.vert.hlsl | Vertex | Simple MVP for wireframe lines |
| frustum_lines.frag.hlsl | Fragment | Solid yellow color |
| grid.vert.hlsl | Vertex | Grid floor (from L12+) |
| grid.frag.hlsl | Fragment | Grid + shadow (from L15+) |
| truck_scene.vert.hlsl | Vertex | Standard truck rendering (L13 pattern) |
| truck_scene.frag.hlsl | Fragment | Blinn-Phong + shadow + texture for truck |

## Pipelines (8 total)

1. `cull_pipeline` — compute, frustum culling
2. `indirect_box_pipeline` — graphics, indirect-drawn boxes (main camera)
3. `indirect_shadow_pipeline` — graphics, shadow pass for boxes (indirect draw)
4. `debug_box_pipeline` — graphics, debug view boxes (regular instanced)
5. `frustum_line_pipeline` — graphics, line list for frustum wireframe
6. `grid_pipeline` — graphics, grid floor
7. `truck_pipeline` — graphics, truck scene rendering
8. `truck_shadow_pipeline` — graphics, truck shadow pass

## GPU Buffers

1. `object_data_buf` — COMPUTE_STORAGE_READ | GRAPHICS_STORAGE_READ — per-object transforms, bounds, draw args
2. `indirect_buf` — INDIRECT | COMPUTE_STORAGE_WRITE — 200 indirect draw commands (20 bytes each)
3. `visibility_buf` — COMPUTE_STORAGE_WRITE | GRAPHICS_STORAGE_READ — 200 uint32 flags
4. `instance_id_buf` — VERTEX — [0,1,...,199] static, uploaded once
5. `box_vertex_buf` — VERTEX — BoxTextured mesh vertices
6. `box_index_buf` — INDEX — BoxTextured mesh indices
7. `truck_vertex_buf` — VERTEX — CesiumMilkTruck mesh vertices
8. `truck_index_buf` — INDEX — CesiumMilkTruck mesh indices
9. `grid_vertex_buf` / `grid_index_buf` — grid floor
10. `frustum_line_buf` — VERTEX — 24 line vertices, updated each frame

## Controls

| Key | Action |
|-----|--------|
| WASD/Mouse | Move/look (main camera) |
| Space/LShift | Fly up/down |
| F | Toggle frustum culling on/off |
| D | Toggle debug view visibility |
| Escape | Release mouse cursor |

## main.c Decomposition (MANDATORY chunked writes)

Estimated total: ~1800-2200 lines. Split into 4 chunks.

### Chunk A: Header + Constants + Types + Helpers (~550 lines)

Write to `/tmp/lesson_38_part_a.c`.

Contents:

- License header, file description comment
- `#define SDL_MAIN_USE_CALLBACKS 1`
- All `#include` directives (SDL, stddef, string, stdio, math, forge_math, forge_gltf, forge_capture)
- All compiled shader `#include` directives (24 headers: 12 shaders × 2 formats)
- All `#define` constants:
  - Window/scene (WINDOW_WIDTH, WINDOW_HEIGHT, NUM_BOXES 200, etc.)
  - Camera (FOV, NEAR/FAR, MOVE_SPEED, MOUSE_SENSITIVITY, PITCH_CLAMP)
  - Lighting (LIGHT_DIR, AMBIENT, SHININESS, SPECULAR)
  - Grid (HALF_SIZE, LINE_WIDTH, FADE_DIST, colors)
  - Shadow map (SHADOW_MAP_SIZE 2048, SHADOW_DEPTH_FMT)
  - Compute (WORKGROUP_SIZE 64)
  - Buffer sizes, vertex pitches
  - Shader resource counts for all 8 pipelines
- All `typedef struct` definitions:
  - `SceneVertex` (position, normal, uv)
  - `InstanceData` (mat4 model columns as 4 vec4s)
  - `LineVertex` (position, color)
  - `ObjectGPUData` (model, color, bounding_sphere, num_indices, first_index, vertex_offset, _pad)
  - `VertUniforms` (vp, light_vp)
  - `FragUniforms` (light_dir, eye_pos, shadow settings, ambient, shininess, specular)
  - `GridVertUniforms`, `GridFragUniforms`
  - `CullUniforms` (frustum\_planes[6], num\_objects, enable\_culling, \_pad)
  - `DebugVertUniforms` (vp)
  - `LineVertUniforms` (vp)
  - `app_state` struct (all GPU resources, camera state, scene data, flags)
- Helper functions:
  - `create_shader()` — loads SPIRV/DXIL based on device format
  - `upload_gpu_buffer()` — transfer buffer upload pattern
  - `extract_frustum_planes()` — Gribb-Hartmann from VP matrix
  - `compute_frustum_corners()` — 8 world-space corners from inverse VP
  - `build_frustum_line_vertices()` — 24 LineVertex from 8 corners

### Chunk B: SDL_AppInit (~600 lines)

Write to `/tmp/lesson_38_part_b.c`.

Contents:

- `SDL_AppInit()` function
- Device + window creation with sRGB swapchain
- Depth texture creation (main pass + shadow map)
- Sampler creation (scene + shadow comparison)
- All shader creation (12 shaders × create_shader calls)
- All pipeline creation (8 pipelines with full vertex input state)
- glTF model loading (BoxTextured + CesiumMilkTruck)
- Mesh vertex/index buffer uploads
- Grid geometry generation and upload
- Instance ID buffer [0..199] creation and upload
- Object data buffer creation (200 ObjectGPUData) with deterministic layout
- Indirect draw buffer creation
- Visibility buffer creation
- Frustum line vertex buffer creation
- Texture uploads (box texture, truck texture)
- Camera initialization
- Capture initialization

### Chunk C: SDL_AppEvent + SDL_AppIterate (~600 lines)

Write to `/tmp/lesson_38_part_c.c`.

Contents:

- `SDL_AppEvent()` — quit, mouse capture/release, WASD keys, F toggle culling, D toggle debug
- `SDL_AppIterate()`:
  - Delta time computation
  - Main camera update (quaternion camera pattern)
  - Debug camera setup (fixed overhead position)
  - Extract frustum planes from main camera VP
  - Compute frustum corners for wireframe
  - Build frustum line vertices
  - Upload frustum lines to GPU (transfer buffer)
  - Update object data buffer if needed
  - Acquire command buffer + swapchain texture
  - **Compute pass:** bind cull pipeline, push uniforms, bind storage buffers, dispatch
  - **Shadow pass:** render truck + indirect-drawn boxes to shadow map
  - **Main render pass:**
    - Left viewport: grid + truck + indirect box draw
    - Right viewport (if debug on): grid + truck + debug boxes + frustum lines + divider
  - Submit command buffer
  - Capture handling

### Chunk D: SDL_AppQuit (~80 lines)

Write to `/tmp/lesson_38_part_d.c`.

Contents:

- `SDL_AppQuit()` — release all GPU resources in reverse order
- Free app_state

## Assembly Command

```bash
cat /tmp/lesson_38_part_a.c /tmp/lesson_38_part_b.c \
    /tmp/lesson_38_part_c.c /tmp/lesson_38_part_d.c \
    > lessons/gpu/38-indirect-drawing/main.c
```

## Key Implementation Details

### Frustum Plane Extraction (Vulkan depth [0,1])

Near plane = row2 (NOT row3+row2 like OpenGL). Far plane = row3-row2.
Normalize each plane by length of its xyz normal.

### Instance ID Buffer Pattern

`instance_id_buf` = [0, 1, 2, ..., 199]. Bound as vertex buffer slot 1 with
INSTANCE rate. Each indirect command sets `first_instance = idx` to offset
into this buffer. Vertex shader receives object_id as TEXCOORD3.

### Buffer Usage Flags (CRITICAL)

- `object_data_buf`: COMPUTE_STORAGE_READ | GRAPHICS_STORAGE_READ
- `indirect_buf`: INDIRECT | COMPUTE_STORAGE_WRITE
- `visibility_buf`: COMPUTE_STORAGE_WRITE | GRAPHICS_STORAGE_READ
- `instance_id_buf`: VERTEX
- `frustum_line_buf`: VERTEX

### Bounding Sphere

For BoxTextured (unit cube scaled by s): radius = s \* sqrt(3)/2 ≈ s \* 0.866.
Center = world-space position of the box.

### Debug Camera

Fixed position: eye = (0, 35, -30), target = (0, 0, 0), FOV = 70 degrees.
Uses mat4_look_at (not quaternion — this is a fixed camera, not player-controlled).

### Cross-References

- L11: Compute shader patterns
- L13: Instanced rendering with per-instance vertex buffer
- L33: Vertex pulling / storage buffers in vertex shaders
- L37: GPU readback pattern (for exercises)
