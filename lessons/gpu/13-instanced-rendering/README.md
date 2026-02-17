# Lesson 13 — Instanced Rendering

## What you'll learn

- How **instance buffers** let the GPU draw many objects in a single draw call
- Using `SDL_GPU_VERTEXINPUTRATE_INSTANCE` to pass per-instance data
- Encoding a model matrix as 4 vertex attributes (4 × float4 columns)
- Binding two vertex buffer slots on one pipeline (per-vertex + per-instance)
- Loading and rendering two different glTF models in one scene
- Why instanced rendering matters for performance

## Result

![Instanced rendering — boxes and ducks on a grid](assets/screenshot.png)

A grid of textured boxes surrounded by an army of 256 rubber ducks, all on a
procedural floor grid. ~47 boxes and 256 ducks are rendered with just **3 draw
calls** (grid + boxes + ducks) instead of 300+ individual calls. The boxes form
a 6×6 ground layer with some stacked on a second layer, and the ducks fill a
16×16 grid across the scene — each facing a different direction.

## Key concepts

### The problem: one draw call per object

In previous lessons, rendering each object required its own draw call: push the
model matrix uniform, bind the vertex buffer, and call
`SDL_DrawGPUIndexedPrimitives`. For a scene with N objects, that means N draw
calls — each one carrying CPU-side overhead (uniform push, buffer binding, driver
validation). At 44 objects this is fine, but at 1,000 or 10,000 objects, the CPU
becomes the bottleneck long before the GPU runs out of capacity.

### The solution: per-instance vertex buffers

Instanced rendering eliminates the per-object CPU overhead. Instead of pushing a
new model matrix for each draw, ALL instance transforms are packed into a single
GPU buffer. The pipeline declares this buffer as `VERTEXINPUTRATE_INSTANCE`,
which tells the GPU to advance it once per instance rather than once per vertex.
One draw call renders all instances.

The key API concept:

```c
/* Slot 1 advances per-INSTANCE, not per-vertex.
 * Note: leave instance_step_rate at 0 (the default) — SDL3 GPU uses
 * the input_rate flag alone to control per-instance advancement. */
inst_vb_descs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
```

And the draw call specifies the instance count:

```c
/* Draw all instances at once — 2nd arg is instance count */
SDL_DrawGPUIndexedPrimitives(pass, index_count, instance_count, 0, 0, 0);
```

### Pipeline layout: two vertex buffer slots

The instanced pipeline declares **two vertex buffer slots**:

| Slot | Input rate | Pitch | Contents |
|------|-----------|-------|----------|
| 0 | `VERTEX` | 32 bytes | position, normal, UV (mesh geometry) |
| 1 | `INSTANCE` | 64 bytes | model matrix (4 × float4 columns) |

Every vertex in an instance sees the same per-instance data (the model matrix).
The GPU handles this automatically — there is no shader branching or manual
indexing.

### Passing a mat4 as vertex attributes

Vertex attributes are limited to `float4` (16 bytes) each. A 4×4 matrix is
64 bytes — four times too large for a single attribute. The solution is to split
the matrix into 4 columns:

| Location | Format | Content |
|----------|--------|---------|
| 3 | FLOAT4 | model matrix column 0 |
| 4 | FLOAT4 | model matrix column 1 |
| 5 | FLOAT4 | model matrix column 2 |
| 6 | FLOAT4 | model matrix column 3 |

The vertex shader reconstructs the full matrix:

```hlsl
float4x4 model = transpose(float4x4(
    input.model_c0,  /* column 0 */
    input.model_c1,  /* column 1 */
    input.model_c2,  /* column 2 */
    input.model_c3   /* column 3 */
));
```

The `transpose` is needed because HLSL's `float4x4(a, b, c, d)` treats the
arguments as rows, but our math library stores matrices column-major.

### Instance buffer upload

Instance transforms are computed on the CPU and uploaded once at initialization,
the same way vertex data is uploaded:

```c
/* Generate transforms */
InstanceData instances[INSTANCE_COUNT];
for (int i = 0; i < count; i++) {
    mat4 t = mat4_translate(positions[i]);
    mat4 r = mat4_rotate_y(angles[i]);
    instances[i].model = mat4_multiply(t, r);
}

/* Upload to GPU as a vertex buffer */
model->instance_buffer = upload_gpu_buffer(
    device, SDL_GPU_BUFFERUSAGE_VERTEX,
    instances, count * sizeof(InstanceData));
```

At render time, both buffers are bound simultaneously:

```c
SDL_GPUBufferBinding vb_bindings[2];
vb_bindings[0].buffer = prim->vertex_buffer;    /* per-vertex  */
vb_bindings[1].buffer = model->instance_buffer;  /* per-instance */
SDL_BindGPUVertexBuffers(pass, 0, vb_bindings, 2);
```

### Multi-model instanced scene

Both models (BoxTextured and Duck) use the **same instanced pipeline** — they
have identical vertex formats (ForgeGltfVertex) and the same instance data
layout (mat4). The only difference is:

- Different vertex/index buffers (different geometry)
- Different instance buffers (different placement transforms)
- Different material uniforms and textures

This is a common pattern: one instanced pipeline handles many different meshes.
The pipeline defines the *format*; the buffers provide the *data*.

### When to use instanced rendering

Instanced rendering is most beneficial when:

- Drawing **many copies** of the same mesh (trees, rocks, buildings, particles)
- Each copy needs a different **transform** (position, rotation, scale)
- The CPU draw-call overhead becomes a bottleneck

For scenes with few unique objects or where each object has fundamentally
different rendering requirements, the overhead of setting up instance buffers may
not be worth it. The break-even point depends on the hardware, but as a general
guideline, instancing starts paying off around 10-20+ instances of the same mesh.

### Baking the glTF node hierarchy into instance transforms

In non-instanced rendering (Lessons 9-12), each draw call uses the node's
`world_transform` as the model matrix. This matrix encodes the full glTF node
hierarchy — translation, rotation, and scale accumulated from root to leaf. For
example, the Duck model has a root node with 0.01 scale; its `world_transform`
shrinks the duck from ~200 raw vertex units down to ~2 world units.

With instanced rendering, the vertex shader uses **only** the instance matrix.
There is no per-draw model uniform. If the instance matrix contains only a
placement transform (position + rotation), the glTF hierarchy is lost — the duck
would render at its raw vertex size (enormous).

The solution: **pre-multiply** the node's `world_transform` into every instance
matrix at upload time:

```c
/* Find the node hierarchy transform for this model's mesh */
mat4 mesh_base_transform = mat4_identity();
for (int ni = 0; ni < scene->node_count; ni++) {
    if (scene->nodes[ni].mesh_index >= 0) {
        mesh_base_transform = scene->nodes[ni].world_transform;
        break;
    }
}

/* Bake it into every instance: final = placement × node_hierarchy */
for (int i = 0; i < instance_count; i++) {
    instances[i].model = mat4_multiply(instances[i].model,
                                       mesh_base_transform);
}
```

This way, the instance matrix accounts for both the placement in the scene AND
the glTF model's intended scale/orientation. The vertex shader doesn't need to
know about node hierarchies at all.

### Normal transformation with instanced matrices

The vertex shader uses the same adjugate transpose method from Lesson 10 to
transform normals correctly. The only difference is that the model matrix comes
from the instance buffer instead of a uniform:

```hlsl
float3x3 m = (float3x3)model;
float3x3 adj_t;
adj_t[0] = cross(m[1], m[2]);
adj_t[1] = cross(m[2], m[0]);
adj_t[2] = cross(m[0], m[1]);
output.world_norm = mul(adj_t, input.normal);
```

This preserves correct lighting under non-uniform scale — important since the
baked instance matrices may contain non-trivial scaling from the glTF hierarchy.

## Math connections

- **Vectors** ([Math Lesson 01](../../math/01-vectors/)) — dot product for
  Blinn-Phong lighting
- **Matrices** ([Math Lesson 05](../../math/05-matrices/)) — model matrix
  composition (translate × rotate × scale) for instance transforms
- **Orientation** ([Math Lesson 08](../../math/08-orientation/)) — quaternion
  camera for first-person navigation

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrow keys | Move forward/back/left/right |
| Space | Fly up |
| Left Shift | Fly down |
| Mouse | Look around |
| Click | Recapture mouse |
| Escape | Release mouse (press again to quit) |

## Building

```bash
cmake -B build
cmake --build build --config Debug --target 13-instanced-rendering
```

Run:

```bash
python scripts/run.py 13
```

## AI skill

This lesson distills into the
[`/instanced-rendering`](../../../.claude/skills/instanced-rendering/SKILL.md)
skill. Use it when you need to draw many copies of a mesh efficiently — forests,
particle systems, city blocks, or any scene with repeated geometry.

## Exercises

1. **Dynamic instance count.** Add keyboard controls (e.g. +/- keys) to
   increase or decrease the number of box instances at runtime. You will need
   to regenerate and re-upload the instance buffer each time the count changes.

2. **Per-instance color.** Add a `float4 color` to the instance data (expanding
   it to 80 bytes per instance). Pass it through to the fragment shader and
   multiply it with the surface color. Give each box a unique tint.

3. **Animated instances.** Instead of static transforms, recompute instance
   matrices each frame. Make boxes slowly rotate, or have ducks waddle back and
   forth using `sin(time + instance_index)`. This requires re-uploading the
   instance buffer every frame.

4. **Instance count stress test.** Increase the box count to 1,000 or 10,000
   instances. Observe how frame rate scales — instanced rendering should handle
   thousands of simple objects without CPU bottlenecks. Compare with the
   non-instanced approach (one draw call per object) to see the difference.
