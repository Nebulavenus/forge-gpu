# Lesson 03 — Uniforms & Motion

## What you'll learn

- How **uniform buffers** send data from the CPU to GPU shaders every frame
- How **push uniforms** work in SDL GPU — a lightweight way to set small data
  without creating a GPU buffer object
- How to **track elapsed time** and pass it to a shader
- How to **animate geometry** on the GPU using math in the vertex shader
- How to **correct for aspect ratio** so shapes don't stretch on non-square windows

## Result

The same colored triangle from Lesson 02, but now it spins continuously
around its center. The rotation happens entirely in the vertex shader —
the vertex buffer data never changes.

![Lesson 03 result](assets/screenshot.png)

## Key concepts

### Uniform buffers

In Lesson 02, the vertex shader had no external input besides the vertex
data itself. But real rendering needs per-frame data — camera position,
time, light direction, transformation matrices.

A **uniform buffer** is the standard way to pass this kind of data.
"Uniform" means the value is the same for every vertex (or fragment) in a
single draw call — unlike vertex attributes, which vary per vertex.

### Push uniforms vs. GPU buffers

SDL GPU offers two ways to set uniform data:

| Method | When to use |
|--------|-------------|
| **Push uniforms** (`SDL_PushGPUVertexUniformData`) | Small data that changes every frame — time, matrices, colors |
| **GPU uniform buffer** (create + upload like vertex buffers) | Large or rarely-changing data |

Push uniforms are simpler: you push a pointer to a C struct, and SDL copies
it internally. No buffer creation, no transfer buffers, no copy passes. For
our small `Uniforms` struct (time + aspect ratio), push uniforms are the right
choice.

### The uniform data flow

```text
CPU (each frame)                    GPU (vertex shader)
─────────────────                   ───────────────────
Uniforms uniforms;                  cbuffer Uniforms : register(b0, space1)
uniforms.time   = elapsed;          {
uniforms.aspect = w / h;                float time;
                                        float aspect;
SDL_PushGPUVertexUniformData(       };
    cmd, 0, &uniforms,
    sizeof(uniforms));              // both fields are now available
```

The `0` in the push call is the **slot index**, matching `b0` in the HLSL
`register(b0, space1)`. Vertex shader uniforms always use `space1`;
fragment shader uniforms use `space3`.

### Animation in the vertex shader

Instead of modifying vertex data on the CPU every frame (slow — requires
re-uploading to the GPU), we do the math in the vertex shader:

```hlsl
float c = cos(time);
float s = sin(time);

float2 rotated;
rotated.x = position.x * c - position.y * s;
rotated.y = position.x * s + position.y * c;
```

This is a standard 2D rotation matrix applied per-vertex. The GPU runs
this for all three vertices in parallel — much faster than doing it on
the CPU and re-uploading.

### Aspect ratio correction

NDC (normalized device coordinates) range from -1 to +1 on both axes, but
our window is 1280×720 — wider than it is tall. Without correction, a shape
that should look circular gets stretched horizontally.

The fix is simple: divide x by the aspect ratio (`width / height`) after
all your other transforms:

```hlsl
rotated.x /= aspect;
```

This is another reason uniforms are so useful — we pass the aspect ratio
alongside the time, and the shader handles the rest. In a later lesson
we'll replace this with a proper projection matrix, but for 2D work this
one-line fix keeps shapes looking correct on any window size.

**Note:** We're still working in clip space (outputting directly to NDC coordinates).
To understand the full transformation pipeline from model space to screen space,
see [lessons/math/02-coordinate-spaces](../../math/02-coordinate-spaces/).

## Shader compilation

The HLSL source files in `shaders/` are pre-compiled to SPIRV (Vulkan) and
DXIL (D3D12). If you modify a shader, recompile with:

```bash
# From lessons/03-uniforms-and-motion/

# SPIRV (requires Vulkan SDK)
dxc -spirv -T vs_6_0 -E main shaders/triangle.vert.hlsl -Fo shaders/compiled/triangle.vert.spv
dxc -spirv -T ps_6_0 -E main shaders/triangle.frag.hlsl -Fo shaders/compiled/triangle.frag.spv

# DXIL (requires Windows SDK or Vulkan SDK)
dxc -T vs_6_0 -E main shaders/triangle.vert.hlsl -Fo shaders/compiled/triangle.vert.dxil
dxc -T ps_6_0 -E main shaders/triangle.frag.hlsl -Fo shaders/compiled/triangle.frag.dxil
```

Then regenerate the C headers:

```bash
python ../../scripts/bin_to_header.py shaders/compiled/triangle.vert.spv triangle_vert_spirv shaders/compiled/triangle_vert_spirv.h
python ../../scripts/bin_to_header.py shaders/compiled/triangle.frag.spv triangle_frag_spirv shaders/compiled/triangle_frag_spirv.h
python ../../scripts/bin_to_header.py shaders/compiled/triangle.vert.dxil triangle_vert_dxil shaders/compiled/triangle_vert_dxil.h
python ../../scripts/bin_to_header.py shaders/compiled/triangle.frag.dxil triangle_frag_dxil shaders/compiled/triangle_frag_dxil.h
```

## Building

```bash
# From the repository root
cmake -B build
cmake --build build --config Debug
```

Run:

```bash
python scripts/run.py 03

# Or directly:
# Windows
build\lessons\gpu\03-uniforms-and-motion\Debug\03-uniforms-and-motion.exe
# Linux / macOS
./build/lessons/gpu/03-uniforms-and-motion/03-uniforms-and-motion
```

## AI skill

This lesson has a matching Claude Code skill:
[`uniforms-and-motion`](../../../.claude/skills/uniforms-and-motion/SKILL.md) —
invoke it with `/uniforms-and-motion` or copy it into your own project's
`.claude/skills/` directory. It distils the uniform buffer pattern from this
lesson into a reusable reference that AI assistants can follow.

## Exercises

1. **Change the speed** — Modify `ROTATION_SPEED` to make the triangle spin
   faster or slower. What happens with a negative value?

2. **Pulse the size** — Add a `scale` field to the uniform struct and pass
   `sin(time) * 0.3 + 0.7` from the CPU. Multiply the vertex position by
   `scale` in the shader before rotating. (Remember to update
   `register(b0, space1)` if you change the cbuffer layout.)

3. **Orbit instead of spin** — Instead of rotating the triangle's vertices,
   add an offset: `position.x += cos(time) * 0.5`. This makes the triangle
   move in a circle while keeping its orientation.

4. **Color cycling** — Pass `time` to the fragment shader via a second
   uniform buffer (`register(b0, space3)`, `num_uniform_buffers = 1` on the
   fragment shader). Use it to shift the colors over time.
