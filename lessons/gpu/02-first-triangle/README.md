# Lesson 02 — First Triangle

## What you'll learn

- How to define **vertex data** (position + color) and upload it to the GPU
- How **shaders** work — vertex shaders transform positions, fragment shaders output colors
- How to build a **graphics pipeline** that ties shaders, vertex layout, and rasterizer settings together
- The GPU upload pattern: **transfer buffer → GPU buffer** (staging)
- How to **bind** a pipeline and vertex buffer and issue a **draw call**

## Result

A colored triangle on a dark background. Each vertex has a different color
(red, green, blue) and the GPU smoothly interpolates between them — this is
called *smooth shading* or *Gouraud shading*.

<!-- TODO: screenshot -->

## Key concepts

### Vertex buffers

A vertex buffer is a block of GPU memory holding per-vertex data. Each vertex
in our triangle has a 2D position (`float2`) and an RGB color (`float3`),
packed into a 20-byte struct:

```c
typedef struct Vertex {
    float x, y;      /* position in NDC */
    float r, g, b;   /* color           */
} Vertex;
```

The GPU can't read CPU memory directly, so we use a **transfer buffer** as a
staging area: map it, copy data in, then record a GPU copy command to move the
data into the final vertex buffer.

### Shaders

Shaders are small programs compiled ahead of time and executed on the GPU:

| Stage    | Runs per… | Job |
|----------|-----------|-----|
| Vertex   | vertex    | Transform position, pass data to next stage |
| Fragment | pixel     | Compute the final color for each covered pixel |

Our vertex shader passes the position through (no transform — we're already in
NDC) and forwards the color. The fragment shader outputs the interpolated color
directly.

Shader source is in `shaders/*.hlsl`. Pre-compiled SPIRV and DXIL bytecodes
are in `shaders/*_spirv.h` and `shaders/*_dxil.h`.

### Graphics pipeline

The pipeline is an immutable object that bundles:

- **Vertex & fragment shaders** — which GPU programs to run
- **Vertex input layout** — how to read fields from the vertex buffer
- **Primitive type** — how vertices are assembled (triangle list, strip, etc.)
- **Rasterizer state** — fill mode, culling, winding order
- **Color target format** — must match the swapchain

Once created, you bind it during a render pass and issue draw calls.

### The draw call

Inside the render pass, the sequence is:

1. `SDL_BindGPUGraphicsPipeline` — select which pipeline to use
2. `SDL_BindGPUVertexBuffers` — point the GPU at vertex data
3. `SDL_DrawGPUPrimitives` — tell the GPU how many vertices to process

The GPU then runs the vertex shader on each vertex, assembles triangles,
rasterizes them into pixels, and runs the fragment shader on each pixel.

## Shader compilation

The HLSL source files in `shaders/` are pre-compiled to SPIRV (Vulkan) and
DXIL (D3D12). If you modify a shader, recompile with:

```bash
# SPIRV (requires Vulkan SDK)
dxc -spirv -T vs_6_0 -E main shaders/triangle.vert.hlsl -Fo shaders/triangle.vert.spv
dxc -spirv -T ps_6_0 -E main shaders/triangle.frag.hlsl -Fo shaders/triangle.frag.spv

# DXIL (requires Windows SDK or Vulkan SDK)
dxc -T vs_6_0 -E main shaders/triangle.vert.hlsl -Fo shaders/triangle.vert.dxil
dxc -T ps_6_0 -E main shaders/triangle.frag.hlsl -Fo shaders/triangle.frag.dxil
```

Then regenerate the C headers (Python):

```bash
python scripts/spirv_to_header.py shaders/triangle.vert.spv triangle_vert_spirv shaders/triangle_vert_spirv.h
```

## Building

```bash
# From the repository root
cmake -B build
cmake --build build --config Debug
```

Run:

```bash
# Windows
build\lessons\02-first-triangle\Debug\02-first-triangle.exe

# Linux / macOS
./build/lessons/02-first-triangle/02-first-triangle
```

## AI skill

This lesson has a matching Claude Code skill:
[`first-triangle`](../../../.claude/skills/first-triangle/SKILL.md) — invoke it
with `/first-triangle` or copy it into your own project's `.claude/skills/`
directory.  It distils the vertex buffer, shader, and pipeline patterns from
this lesson into a reusable reference that AI assistants can follow.

## Exercises

1. **Change the colors** — Make the triangle yellow-cyan-magenta instead of
   red-green-blue. What happens with white-white-white?

2. **Move the triangle** — Shift all vertices so the triangle sits in the
   top-right corner. What range of x/y values keeps it on screen?

3. **Add a second triangle** — Add three more vertices to `triangle_vertices`
   and increase `VERTEX_COUNT` to 6. Draw two triangles side by side.

4. **Wireframe mode** — Change `SDL_GPU_FILLMODE_FILL` to
   `SDL_GPU_FILLMODE_LINE` in the pipeline to see the triangle edges.
