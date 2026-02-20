# Lesson 19 — Debug Lines

## What you'll learn

- How to build an **immediate-mode debug drawing system** for diagnostic
  visualization
- How to render **colored line primitives** with `SDL_GPU_PRIMITIVETYPE_LINELIST`
- How to use a **dynamic vertex buffer** updated every frame via transfer buffers
- How to create **two pipelines from the same shaders** — one depth-tested
  (world-space) and one always-on-top (overlay)
- How to draw common debug shapes: **lines, grids, axes, circles, wireframe
  boxes**

## Result

![Lesson 19 screenshot](assets/screenshot.png)

A gray ground grid with colored wireframe boxes, circles on various planes,
coordinate-axis gizmos (RGB = XYZ), and an animated spinning circle.  The origin
gizmo and a small yellow box are drawn as overlay — always visible, even when
behind other geometry.

## Key concepts

### Immediate-mode debug drawing

In an immediate-mode pattern, debug geometry is rebuilt from scratch every frame:

1. **Reset** vertex counts to zero
2. **Accumulate** vertices by calling helper functions (`debug_line`,
   `debug_box_wireframe`, `debug_circle`, etc.)
3. **Upload** all vertices to the GPU via a transfer buffer
4. **Draw** with one or two draw calls

This is the opposite of retained-mode rendering where you create buffers once and
reuse them.  Immediate mode is ideal for debug drawing because the data changes
every frame — positions, colors, and which shapes to draw can all change based on
game state.

```c
/* Each frame: */
state->world_count = 0;
state->overlay_count = 0;

/* Accumulate whatever debug shapes you need this frame. */
debug_grid(state, 20, 1.0f, gray);
debug_axes(state, origin, 2.0f, true);
debug_box_wireframe(state, min, max, orange, false);

/* Upload and draw. */
```

### Dynamic vertex buffers

Unlike previous lessons where vertex data was uploaded once during init, debug
lines require uploading new vertex data every frame.  The pattern:

1. **Pre-allocate** a GPU vertex buffer and a transfer buffer at init (sized for
   `MAX_DEBUG_VERTICES`)
2. Each frame, **map** the transfer buffer, **memcpy** the CPU vertices,
   **unmap**
3. Issue a **copy command** to transfer data to the GPU vertex buffer
4. **Draw** using the now-populated GPU buffer

```c
/* Map → copy → unmap → upload */
void *mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
SDL_memcpy(mapped, vertices, total_vertices * sizeof(DebugVertex));
SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

/* Copy pass to transfer data to the GPU buffer. */
SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
SDL_UploadToGPUBuffer(copy, &src, &dst, false);
SDL_EndGPUCopyPass(copy);
```

### World-space vs overlay lines

The lesson creates two pipelines from the same vertex and fragment shaders — the
only difference is the depth-stencil state:

- **Line pipeline** (world-space): depth test ON, depth write ON.  Lines are
  occluded by geometry in front of them, just like regular 3D objects.
- **Overlay pipeline**: depth test OFF, depth write OFF.  Lines are always
  visible regardless of what's in front of them.

This is essential for diagnostic tools.  A coordinate-axis gizmo at the origin
should always be visible so you can orient yourself.  But wireframe bounds around
a physics object should be occluded normally.

Both pipelines share the same GPU vertex buffer.  World vertices occupy the first
`world_count` entries; overlay vertices follow starting at index `world_count`.
Two `SDL_DrawGPUPrimitives` calls with different `first_vertex` offsets select
the right batch.

### Line primitives

`SDL_GPU_PRIMITIVETYPE_LINELIST` interprets every pair of vertices as a separate
line segment.  Unlike a triangle list, there is no face orientation, so culling
is disabled (`SDL_GPU_CULLMODE_NONE`).

The vertex format is minimal — position and color:

```c
typedef struct DebugVertex {
    vec3 position;   /* 12 bytes — world-space position */
    vec4 color;      /* 16 bytes — RGBA color           */
} DebugVertex;       /* 28 bytes total                  */
```

### Debug shape helpers

The lesson provides five building-block functions:

| Function | What it draws | Vertices |
|---|---|---|
| `debug_line` | Single line segment | 2 |
| `debug_grid` | XZ ground grid | 4 per grid line |
| `debug_axes` | RGB coordinate gizmo (X=red, Y=green, Z=blue) | 6 |
| `debug_circle` | Circle from line segments | 2 × segments |
| `debug_box_wireframe` | 12 edges of an axis-aligned bounding box | 24 |

The `debug_circle` function constructs an orthonormal basis for the circle's
plane using cross products, then generates points at equal angle intervals around
the circumference.

## Math used

All math comes from `common/math/forge_math.h`:

- `vec3_create`, `vec3_add`, `vec3_scale`, `vec3_cross`, `vec3_normalize` —
  position arithmetic and circle plane construction
  (see [Lesson 01 — Vectors](../../math/01-vectors/))
- `mat4_perspective`, `mat4_multiply`, `mat4_view_from_quat` — camera matrices
  (see [Lesson 06 — Projections](../../math/06-projections/))
- `quat_from_euler`, `quat_forward`, `quat_right` — camera orientation
  (see [Lesson 08 — Orientation](../../math/08-orientation/))

## Building

```bash
# Compile shaders (both SPIRV and DXIL)
python scripts/compile_shaders.py 19

# Build
cmake --build build --config Debug --target 19-debug-lines

# Run
python scripts/run.py 19
```

## AI skill

The debug drawing pattern is captured as a reusable Claude Code skill in
`.claude/skills/debug-lines/SKILL.md`.  Use `/debug-lines` to add immediate-mode
debug drawing to any SDL GPU project.

## Exercises

1. **Add a debug sphere** — Write `debug_sphere()` using three circles (one on
   each axis plane) to approximate a wireframe sphere.

2. **Add a debug arrow** — Write `debug_arrow(start, end, color)` that draws a
   line with a small arrowhead at the end.  Use cross products to orient the
   arrowhead perpendicular to the arrow direction.

3. **Add debug text** — SDL3's `SDL_RenderDebugText` can draw text.  Research
   how to combine GPU rendering with SDL's 2D text rendering to add labels next
   to debug shapes.

4. **Fade distant lines** — Modify the fragment shader to fade lines based on
   distance from the camera, reducing visual clutter for far-away debug shapes.

5. **Per-system toggle** — Add keyboard shortcuts (1-5) that toggle different
   debug categories (grid, axes, boxes, circles).  This mimics how real engines
   let developers toggle debug visualizations per system.
