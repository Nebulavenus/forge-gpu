# Lesson 08 — Loading a Mesh (OBJ)

In this lesson you'll load a real 3D model from a file and render it with a
texture and a fly-around camera. This is the first time geometry comes from a
file rather than being hard-coded in the source.

## What you'll learn

- The **Wavefront OBJ** file format — how 3D models are stored as text
- **De-indexing** — why OBJ's triple-index scheme can't map directly to the GPU
- **Quad triangulation** — splitting 4-sided faces into triangles
- **File-based texture loading** — loading a PNG diffuse map with `SDL_LoadSurface`
- **Non-indexed rendering** — `SDL_DrawGPUPrimitives` with a flat vertex array
- Using a **reusable OBJ parser library** (`common/obj/forge_obj.h`)

## Result

![Textured space shuttle rendered with a fly-around camera](assets/screenshot.png)

A textured space shuttle model gently rotating, with a fly-around camera
(WASD + mouse look from Lesson 07).

## Prerequisites

Before starting this lesson, you should be comfortable with:

- **Lesson 04** — Textures & Samplers (loading images, UV coordinates)
- **Lesson 05** — Mipmaps (mipmap generation, trilinear filtering)
- **Lesson 06** — Depth Buffer & 3D Transforms (MVP pipeline, depth testing)
- **Lesson 07** — Camera & Input (first-person camera, delta time)

## The Wavefront OBJ format

OBJ is one of the oldest and simplest 3D model formats — a plain text file that
any text editor can open. It was created by Wavefront Technologies in the 1980s
and is still widely used today as an exchange format.

### Lines you'll encounter

An OBJ file is a sequence of lines, each starting with a keyword:

```text
# This is a comment

v  4.1117 0.9833 -16.0626     # vertex position (x, y, z)
vt 0.6523 0.8234               # texture coordinate (u, v)
vn 0.0000 1.0000  0.0000       # vertex normal (x, y, z)
f  1/1/1 2/2/1 3/3/2 4/4/2    # face (indices into v/vt/vn arrays)
```

**Positions (`v`)** define points in 3D space. Our shuttle model has 1,123 of
them.

**Texture coordinates (`vt`)** are 2D coordinates that map the 3D surface onto
a 2D texture image. Values typically range from 0 to 1, where (0,0) is one
corner of the texture and (1,1) is the opposite corner.

**Normals (`vn`)** are unit vectors perpendicular to the surface at each vertex.
We load them now but won't use them until Lesson 10 (Basic Lighting).

**Faces (`f`)** define polygons by referencing positions, texture coordinates,
and normals using the format `v/vt/vn`. Each number is an index into the
corresponding array.

### 1-based indexing

OBJ indices start at 1, not 0. The first vertex position in the file is `v` #1.
When parsing, we subtract 1 to convert to the 0-based arrays that C and GPUs
expect.

### Quads vs triangles

OBJ faces can have 3 vertices (triangle) or 4 vertices (quad). GPUs only render
triangles, so we split each quad into two triangles:

```text
Quad face: f 1/1/1 2/2/1 3/3/2 4/4/2

  4 ─── 3        4 ─── 3       4 ─── 3
  │     │   →    │  /  │  →    │ ╲   │
  │     │        │ /   │       │   ╲ │
  1 ─── 2        1 ─── 2       1 ─── 2

Triangle 1: (1, 2, 3)    Triangle 2: (1, 3, 4)
```

Our shuttle has 1,032 quads and 172 triangles = 2,236 total triangles.

## The de-indexing problem

Here's a subtle but important problem. In OBJ, each face vertex has **three
separate indices** — one for position, one for UV, and one for normal:

```text
f 1/5/3 2/6/3 3/7/4
     ↑   ↑  ↑
     │   │  └─ normal index
     │   └──── texcoord index
     └──────── position index
```

GPU index buffers only support **one index per vertex**. A single index looks up
the same position, UV, and normal from the same vertex buffer entry. But in OBJ,
position #1 might pair with UV #5 in one face and UV #8 in another face.

The solution is **de-indexing**: we expand every face corner into a full vertex
with its own position, UV, and normal. This means some position data gets
duplicated, but every vertex has a unique combination of attributes that the GPU
can handle.

```text
OBJ triple-index:                GPU flat vertices:
  pos[1], uv[5], norm[3]    →    vertex 0: {pos1, uv5, norm3}
  pos[2], uv[6], norm[3]    →    vertex 1: {pos2, uv6, norm3}
  pos[3], uv[7], norm[4]    →    vertex 2: {pos3, uv7, norm4}
```

After de-indexing, we have 2,236 triangles × 3 = 6,708 vertices. We draw them
with `SDL_DrawGPUPrimitives` (no index buffer needed).

## The OBJ parser library

The parser lives in `common/obj/forge_obj.h` — a header-only library following
the same pattern as `common/capture/forge_capture.h`. See
[Engine Lesson 05](../../engine/05-header-only-libraries/) for how header-only
libraries work (`static inline` (functions with internal linkage defined directly
in headers to avoid duplicate symbols), include guards, and the one-definition
rule (ODR: each symbol must have exactly one definition across the whole
program)).

### Two-pass parsing

The parser reads the file twice:

1. **First pass** — count `v`, `vt`, `vn`, and `f` lines to determine array sizes
2. **Second pass** — parse the actual data and build the de-indexed vertex array

This avoids dynamic resizing (no realloc) and keeps the code simple.

### Vertex layout

```c
typedef struct ForgeObjVertex {
    vec3 position;  /* 12 bytes */
    vec3 normal;    /* 12 bytes */
    vec2 uv;        /*  8 bytes */
} ForgeObjVertex;   /* 32 bytes total */
```

This matches our pipeline's three vertex attributes:

| Location | Attribute | Format | Offset |
|----------|-----------|--------|--------|
| 0 | position | FLOAT3 | 0 |
| 1 | normal | FLOAT3 | 12 |
| 2 | uv | FLOAT2 | 24 |

### UV flipping

OBJ files typically use OpenGL-style texture coordinates where V=0 is at the
**bottom** of the image. GPUs (and most image formats) expect V=0 at the
**top**. The parser flips V coordinates automatically: `v_gpu = 1.0 - v_obj`.

### Usage

```c
#include "obj/forge_obj.h"

ForgeObjMesh mesh;
if (forge_obj_load("model.obj", &mesh)) {
    /* mesh.vertices is ready for GPU upload */
    /* mesh.vertex_count vertices, every 3 form a triangle */

    /* Upload to vertex buffer... */

    forge_obj_free(&mesh);
}
```

## Loading a texture from a file

We combine patterns from Lesson 04 (texture loading) and Lesson 05 (mipmaps):

1. **`SDL_LoadSurface`** loads the PNG into CPU memory
2. **`SDL_ConvertSurface`** converts to ABGR8888 (GPU's R8G8B8A8 in memory)
3. **Create GPU texture** with `SAMPLER | COLOR_TARGET` usage and mip levels
4. **Upload** the base level via a transfer buffer and copy pass
5. **`SDL_GenerateMipmapsForGPUTexture`** auto-generates all mip levels

The texture is 1024×1024, giving us 11 mip levels (1024, 512, 256, ... 1).

> **Note:** `SDL_LoadSurface` only supports BMP and PNG — not JPG. The original
> texture was a JPG, so we converted it to PNG during asset preparation.

## The sampler

We use trilinear filtering with REPEAT address mode — the same "gold standard"
sampler from Lesson 05:

```c
SDL_GPUSamplerCreateInfo smp_info;
SDL_zero(smp_info);
smp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
smp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
smp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
smp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
smp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
smp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
smp_info.max_lod        = 1000.0f;
```

## The pipeline

Same setup as Lesson 07 (depth test + back-face culling), but with three vertex
attributes instead of two, and a fragment-stage sampler for the diffuse texture.

## Rendering

Because the mesh is de-indexed (flat vertex array), we use
`SDL_DrawGPUPrimitives` instead of `SDL_DrawGPUIndexedPrimitives`:

```c
SDL_DrawGPUPrimitives(pass, state->mesh_vertex_count, 1, 0, 0);
```

One draw call renders the entire shuttle — all 6,708 vertices (2,236 triangles).

## Shaders

| File | Purpose |
|------|---------|
| `mesh.vert.hlsl` | Transforms mesh vertices via the MVP matrix and passes UV coordinates for texture sampling |
| `mesh.frag.hlsl` | Samples the diffuse texture at interpolated UVs for flat (unlit) texture mapping |

## Building and running

```bash
# Build
cmake -B build
cmake --build build --config Debug

# Run
python scripts/run.py 08
```

**Controls:**

- WASD / Arrow keys — move forward/back/left/right
- Space / Left Shift — fly up / fly down
- Mouse — look around
- Escape — release mouse / quit

## Model attribution

**Space Shuttle** by [Microsoft](https://sketchfab.com/Microsoft) on
[Sketchfab](https://sketchfab.com/3d-models/space-shuttle-0b4ef1a8fdd54b7286a2a374ac5e90d7),
licensed under [CC Attribution](https://creativecommons.org/licenses/by/4.0/).

## Exercises

1. **Load a different model** — Download another OBJ model from Sketchfab or
   Turbosquid and render it. You may need to adjust the camera position and
   move speed for models of different sizes.

2. **Wireframe mode** — Change `SDL_GPU_FILLMODE_FILL` to
   `SDL_GPU_FILLMODE_LINE` in the pipeline creation to see the mesh triangles.

3. **Color by normal** — Modify the vertex and fragment shaders to output the
   normal vector as a color (`output.color = float4(abs(input.normal), 1.0)`).
   This is a common debugging technique to verify normals are correct.

4. **Print model stats** — After loading, print the bounding box (min/max x, y,
   z) of the model. Use this to automatically center the model at the origin
   and scale it to a reasonable size.

5. **Remove the rotation** — Comment out the model rotation and start the camera
   facing the shuttle. What initial yaw angle do you need to look in the -Z
   direction?

## What's next

In **Lesson 09 — Loading a Scene (glTF)**, we'll load a more complex format
that supports multiple meshes, materials, and a scene hierarchy. In
**Lesson 10 — Basic Lighting**, we'll use the normal vectors we're already
loading to add diffuse and specular shading.
