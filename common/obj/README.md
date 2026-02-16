# forge-gpu OBJ Parser

A header-only Wavefront OBJ parser for loading 3D models into forge-gpu.

## Quick Start

```c
#include "obj/forge_obj.h"

ForgeObjMesh mesh;
if (forge_obj_load("model.obj", &mesh)) {
    // mesh.vertices is ready for GPU upload
    // Every 3 consecutive vertices form one triangle
    // Upload to a vertex buffer, then draw:
    //   SDL_DrawGPUPrimitives(pass, mesh.vertex_count, 1, 0, 0);
    forge_obj_free(&mesh);
}
```

## What's Included

### Types

- **`ForgeObjVertex`** -- Position (`vec3`) + normal (`vec3`) + UV (`vec2`),
  interleaved and ready for GPU upload
- **`ForgeObjMesh`** -- A flat array of de-indexed vertices (no index buffer needed)

### Functions

- **`forge_obj_load(path, out_mesh)`** -- Load an OBJ file into a flat vertex
  array. Returns `true` on success, `false` on error (logged via `SDL_Log`)
- **`forge_obj_free(mesh)`** -- Free memory allocated by `forge_obj_load`

### Vertex Layout

The vertex matches this GPU pipeline layout:

| Attribute | Type | HLSL Semantic | Content |
|-----------|------|---------------|---------|
| location 0 | `float3` | `TEXCOORD0` | Position |
| location 1 | `float3` | `TEXCOORD1` | Normal |
| location 2 | `float2` | `TEXCOORD2` | UV |

This is the same layout as `ForgeGltfVertex`, so the same pipeline works for
both OBJ and glTF models.

## Supported Features

- Positions (`v`), texture coordinates (`vt`), normals (`vn`)
- Triangular and quad faces (`f`) with `v/vt/vn` indices
- Quads are automatically triangulated into two triangles
- 1-based OBJ indices (converted internally to 0-based)
- Windows (`\r\n`) and Unix (`\n`) line endings
- Scientific notation in float values (e.g. `1.5e-3`)

## Limitations

These are intentional simplifications for a learning library:

- **Single-object files only** -- ignores `g`/`o` grouping
- **No material library** -- `mtllib`/`usemtl` are ignored
- **No negative indices** -- relative indexing not supported
- **Triangles and quads only** -- no n-gons with more than 4 vertices

## How De-indexing Works

OBJ files allow separate index streams for position, UV, and normal. A vertex
might use position 5 with UV 12 and normal 3 -- a combination that can't map
1:1 to a single GPU index buffer without duplication.

The parser solves this by "de-indexing": each triangle gets its own copy of each
vertex with all attributes baked in. This means no index buffer is needed -- just
draw with `SDL_DrawGPUPrimitives`.

For indexed drawing (shared vertices, smaller buffers), see the glTF parser
(`common/gltf/`) which supports `SDL_DrawGPUIndexedPrimitives`.

## Dependencies

- **SDL3** -- for file I/O, logging, memory allocation
- **forge_math** -- for `vec2`, `vec3` types (`common/math/`)

## Where It's Used

- [`lessons/gpu/08-mesh-loading/`](../../lessons/gpu/08-mesh-loading/) -- Full
  example loading an OBJ model with textures and mipmaps
- [`tests/obj/`](../../tests/obj/) -- Unit tests for the parser

## Design Philosophy

1. **Readability over performance** -- this code is meant to be learned from
2. **Header-only** -- just include `forge_obj.h`, no build config needed
3. **No dependencies beyond SDL** -- no external parsing libraries
4. **Two-pass parsing** -- first pass counts elements, second pass reads data,
   so all memory is allocated up front with no dynamic resizing

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
