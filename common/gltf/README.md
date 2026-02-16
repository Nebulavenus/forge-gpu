# forge-gpu glTF Parser

A header-only glTF 2.0 parser for loading 3D scenes into forge-gpu.

## Quick Start

```c
#include "gltf/forge_gltf.h"

ForgeGltfScene scene;
if (forge_gltf_load("model.gltf", &scene)) {
    // Walk the scene hierarchy
    for (int i = 0; i < scene.root_node_count; i++) {
        int ni = scene.root_nodes[i];
        ForgeGltfNode *node = &scene.nodes[ni];
        // node->world_transform is already computed
        // node->mesh_index points to scene.meshes[]
    }

    // Access mesh primitives (each primitive = one draw call)
    for (int i = 0; i < scene.primitive_count; i++) {
        ForgeGltfPrimitive *prim = &scene.primitives[i];
        // prim->vertices, prim->indices ready for GPU upload
        // prim->material_index points to scene.materials[]
    }

    forge_gltf_free(&scene);
}
```

## What's Included

### Types

- **`ForgeGltfVertex`** -- Position (`vec3`) + normal (`vec3`) + UV (`vec2`),
  interleaved and ready for GPU upload
- **`ForgeGltfPrimitive`** -- Vertices + indices sharing one material (one draw call)
- **`ForgeGltfMesh`** -- A named collection of primitives
- **`ForgeGltfMaterial`** -- Base color (RGBA) + optional texture file path
- **`ForgeGltfNode`** -- Scene hierarchy node with TRS transform and mesh reference
- **`ForgeGltfBuffer`** -- A loaded binary buffer (`.bin` file)
- **`ForgeGltfScene`** -- Top-level container holding all parsed data

### Functions

- **`forge_gltf_load(path, scene)`** -- Load a `.gltf` file and all referenced
  `.bin` buffers. Returns `true` on success. Caller must call `forge_gltf_free()`
- **`forge_gltf_free(scene)`** -- Free all memory allocated by `forge_gltf_load`.
  Safe to call on a zeroed scene
- **`forge_gltf_compute_world_transforms(scene, node_idx, parent_world)`** --
  Recursively compute world transforms. Called automatically by `forge_gltf_load`,
  but exposed for recomputing after modifying local transforms

### Vertex Layout

Same as the OBJ parser -- the same GPU pipeline works for both:

| Attribute | Type | HLSL Semantic | Content |
|-----------|------|---------------|---------|
| location 0 | `float3` | `TEXCOORD0` | Position |
| location 1 | `float3` | `TEXCOORD1` | Normal |
| location 2 | `float2` | `TEXCOORD2` | UV |

## Scene Hierarchy

glTF scenes use a node tree where each node has a local transform (translation,
rotation, scale) and optionally references a mesh. World transforms are computed
by walking the tree from root to leaf, multiplying parent and child transforms.

```text
Root Node (identity)
├── Car Body (translate + rotate)
│   ├── Wheel FL (translate)
│   └── Wheel FR (translate)
└── Ground Plane (scale)
```

The parser handles this automatically:

1. **Local transforms** are parsed from TRS properties or raw matrices
2. **Parent-child relationships** are built from the `children` arrays
3. **World transforms** are computed recursively from the root nodes
4. **Root nodes** are identified from the default scene

Quaternion rotations are converted using the math library's `quat_to_mat4()`.
Note that glTF stores quaternions as `[x, y, z, w]` while the math library
uses `(w, x, y, z)` -- the parser handles this conversion.

## Indexed Drawing

Unlike the OBJ parser (which de-indexes into a flat vertex array), the glTF
parser preserves index buffers. Each primitive has:

- `vertices` / `vertex_count` -- vertex data for GPU upload
- `indices` / `index_count` / `index_stride` -- index data (16-bit or 32-bit)

Use `SDL_DrawGPUIndexedPrimitives` for rendering, which reduces memory usage
and improves GPU vertex cache performance.

## Materials

Each primitive can reference a material with:

- **`base_color[4]`** -- RGBA color factor (default: opaque white)
- **`texture_path`** -- File path to the base color texture (empty if none)
- **`has_texture`** -- Whether a texture path was resolved

The parser stores file paths, not GPU textures. The caller loads and creates
GPU textures however they prefer -- see Lesson 09 for a full example.

## Supported glTF Features

- **Meshes** with multiple primitives (one per material)
- **Materials** with PBR base color factor and base color texture
- **Scene hierarchy** with parent-child node relationships
- **TRS transforms** (translation, rotation, scale) and raw matrices
- **Indexed geometry** with 16-bit and 32-bit indices
- **Vertex attributes**: POSITION, NORMAL, TEXCOORD_0
- **Multiple binary buffers** referenced by URI
- **Accessor validation** (bounds checking, component type validation)

## Constants

| Constant | Default | Description |
|----------|---------|-------------|
| `FORGE_GLTF_MAX_NODES` | 512 | Maximum nodes in the scene |
| `FORGE_GLTF_MAX_MESHES` | 256 | Maximum meshes |
| `FORGE_GLTF_MAX_PRIMITIVES` | 1024 | Maximum draw calls |
| `FORGE_GLTF_MAX_MATERIALS` | 256 | Maximum materials |
| `FORGE_GLTF_MAX_IMAGES` | 128 | Maximum image references |
| `FORGE_GLTF_MAX_BUFFERS` | 16 | Maximum binary buffer files |
| `FORGE_GLTF_MAX_CHILDREN` | 256 | Maximum children per node |

These limits cover typical models (CesiumMilkTruck: 6 nodes; VirtualCity:
234 nodes, 167 materials).

## Dependencies

- **SDL3** -- for file I/O, logging, memory allocation
- **cJSON** -- for JSON parsing (`third_party/cJSON/`)
- **forge_math** -- for `vec2`, `vec3`, `vec4`, `mat4`, `quat` (`common/math/`)

## Where It's Used

- [`lessons/gpu/09-scene-loading/`](../../lessons/gpu/09-scene-loading/) -- Full
  example loading glTF scenes with multi-material rendering
- [`tests/gltf/`](../../tests/gltf/) -- Unit tests for the parser

## Design Philosophy

1. **CPU-only parsing** -- no GPU calls in the parser, making it testable and
   reusable. The caller handles GPU upload
2. **Header-only** -- just include `forge_gltf.h`, no build config needed
3. **Readability over performance** -- this code is meant to be learned from
4. **Defensive parsing** -- validates accessor bounds, component types, and
   buffer sizes before accessing data
5. **Static arrays** -- fixed-size arrays with generous limits instead of
   dynamic allocation for the scene structure (primitives and vertices are
   still dynamically allocated)

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
