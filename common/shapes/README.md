# forge_shapes.h — Procedural Geometry Library

A header-only C library that generates 3D meshes from mathematical descriptions.
Every shape is unit-scale and centred at the origin — use `mat4_translate` /
`mat4_scale` to place and size it in your scene.

## Quick start

```c
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

ForgeShape sphere = forge_shapes_sphere(32, 16);

/* Upload positions, normals, uvs, indices to GPU buffers... */
/* ... render ... */

forge_shapes_free(&sphere);
```

Only **one** `.c` file in your project should define
`FORGE_SHAPES_IMPLEMENTATION` before including the header. All other files
include it without the define to get the declarations only. This is the same
pattern used by [stb libraries](https://github.com/nothings/stb).

## Conventions

| Property | Value |
|---|---|
| Coordinate system | Right-handed, Y-up (+X right, +Y up, +Z toward camera) |
| Winding order | Counter-clockwise (CCW) front faces |
| UV origin | Bottom-left (U right, V up) |
| Index type | `uint32_t` |
| Memory | `SDL_malloc` / `SDL_free` — caller owns the `ForgeShape` |
| Dependencies | `math/forge_math.h`, `SDL3/SDL.h` (no GPU calls) |

## ForgeShape struct

```c
typedef struct {
    vec3     *positions;    /* [vertex_count]  XYZ positions             */
    vec3     *normals;      /* [vertex_count]  unit normals (may be NULL) */
    vec2     *uvs;          /* [vertex_count]  texture coords (may be NULL) */
    uint32_t *indices;      /* [index_count]   CCW triangle list          */
    int       vertex_count;
    int       index_count;
} ForgeShape;
```

**Struct-of-arrays layout** — each field maps directly to one `SDL_GPUBuffer`
with no interleaving. Upload positions to the position buffer, normals to the
normal buffer, and so on. This avoids stride arithmetic at upload time and lets
shaders that do not need UVs skip that buffer entirely.

## API reference

### Generators

| Function | Parameters | Vertex count | Index count |
|---|---|---|---|
| `forge_shapes_sphere` | `slices`, `stacks` | (slices+1)(stacks+1) | slices × stacks × 6 |
| `forge_shapes_icosphere` | `subdivisions` | ~10 × 4^n + 2 | 60 × 4^n |
| `forge_shapes_cylinder` | `slices`, `stacks` | (slices+1)(stacks+1) | slices × stacks × 6 |
| `forge_shapes_cone` | `slices`, `stacks` | (slices+1)(stacks+1) | slices × stacks × 6 |
| `forge_shapes_torus` | `slices`, `stacks`, `major_radius`, `tube_radius` | (slices+1)(stacks+1) | slices × stacks × 6 |
| `forge_shapes_plane` | `slices`, `stacks` | (slices+1)(stacks+1) | slices × stacks × 6 |
| `forge_shapes_cube` | `slices`, `stacks` | 6(slices+1)(stacks+1) | 6 × slices × stacks × 6 |
| `forge_shapes_capsule` | `slices`, `stacks`, `cap_stacks`, `half_height` | (slices+1)(stacks + 2×cap_stacks + 1) | slices(stacks + 2×cap_stacks) × 6 |

### Utilities

| Function | Description |
|---|---|
| `forge_shapes_free` | Release all memory; sets pointers to NULL and counts to 0. Safe on zeroed shapes. |
| `forge_shapes_compute_flat_normals` | Unweld mesh, replace normals with face normals. Gives hard edges. |
| `forge_shapes_merge` | Combine multiple shapes into a single `ForgeShape`. |

## Shape gallery

Common tessellation levels and their mesh sizes:

| Shape | Parameters | Vertices | Indices | Triangles |
|---|---|---|---|---|
| Sphere | 32 slices, 16 stacks | 561 | 3,072 | 1,024 |
| Sphere | 64 slices, 32 stacks | 2,145 | 12,288 | 4,096 |
| Icosphere | 0 subdivisions | 12 | 60 | 20 |
| Icosphere | 2 subdivisions | 162 | 960 | 320 |
| Cylinder | 32 slices, 1 stack | 66 | 192 | 64 |
| Torus | 32 slices, 16 stacks | 561 | 3,072 | 1,024 |
| Plane | 4 slices, 4 stacks | 25 | 96 | 32 |
| Cube | 1 slice, 1 stack | 24 | 36 | 12 |

## Smooth vs flat normals

By default, generators produce **smooth normals** — each vertex normal is
computed from the surface equation, giving a continuous appearance across
triangles. Call `forge_shapes_compute_flat_normals()` to replace them with
**flat normals** — every triangle gets three unique vertices sharing the face's
geometric normal, creating hard edges.

## Adding a new shape

1. Add the generator function declaration before the `#ifdef` guard
2. Implement it inside the `FORGE_SHAPES_IMPLEMENTATION` block
3. Document: shape description, origin, scale, UV convention, normal method,
   vertex/index count formula
4. Add tests in `tests/shapes/test_shapes.c`

See the existing generators for the pattern — allocate arrays with
`SDL_malloc`, fill positions/normals/uvs/indices, return the `ForgeShape`.
