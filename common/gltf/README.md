# forge-gpu glTF Parser

A header-only glTF 2.0 parser for loading 3D scenes into forge-gpu.

## Quick Start

```c
#include "gltf/forge_gltf.h"

ForgeArena arena = forge_arena_create(0);
if (!arena.first) {
    SDL_Log("Failed to create arena");
    return;
}
ForgeGltfScene scene;
if (forge_gltf_load("model.gltf", &scene, &arena)) {
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
}
forge_arena_destroy(&arena);  // frees all scene memory
```

## What's Included

### Types

- **`ForgeGltfVertex`** -- Position (`vec3`) + normal (`vec3`) + UV (`vec2`),
  interleaved and ready for GPU upload
- **`ForgeGltfPrimitive`** -- Vertices + indices sharing one material (one draw call)
- **`ForgeGltfMesh`** -- A named collection of primitives
- **`ForgeGltfMaterial`** -- Material properties: base color (RGBA), texture
  path, alpha mode (opaque/mask/blend), alpha cutoff, double-sided flag,
  optional normal map path
- **`ForgeGltfAlphaMode`** -- Enum: `FORGE_GLTF_ALPHA_OPAQUE`, `FORGE_GLTF_ALPHA_MASK`,
  `FORGE_GLTF_ALPHA_BLEND`
- **`ForgeGltfNode`** -- Scene hierarchy node with name, TRS transform, skin
  reference, and mesh reference
- **`ForgeGltfSkin`** -- Skeletal skin binding: joint node indices and inverse
  bind matrices
- **`ForgeGltfAnimation`** -- Named animation clip with samplers and channels
- **`ForgeGltfAnimSampler`** -- Keyframe timestamps paired with output values,
  with LINEAR or STEP interpolation
- **`ForgeGltfAnimChannel`** -- Binds a sampler to a node's TRS property
  (translation, rotation, or scale)
- **`ForgeGltfMorphTarget`** -- Per-mesh morph target: position, normal,
  and tangent deltas for blend shape animation
- **`ForgeGltfAnimPath`** -- Enum: `FORGE_GLTF_ANIM_TRANSLATION`,
  `FORGE_GLTF_ANIM_ROTATION`, `FORGE_GLTF_ANIM_SCALE`,
  `FORGE_GLTF_ANIM_MORPH_WEIGHTS`
- **`ForgeGltfInterpolation`** -- Enum: `FORGE_GLTF_INTERP_LINEAR`,
  `FORGE_GLTF_INTERP_STEP`
- **`ForgeGltfBuffer`** -- A loaded binary buffer (`.bin` file)
- **`ForgeGltfScene`** -- Top-level container holding all parsed data

### Functions

- **`forge_gltf_load(path, scene, arena)`** -- Load a `.gltf` file and all
  referenced `.bin` buffers. All scene memory is allocated from the provided
  arena. Returns `true` on success. On failure, the arena may contain partial
  allocations — destroy it to clean up
- **`forge_gltf_free(scene)`** -- Legacy no-op. All memory is owned by the
  arena passed to `forge_gltf_load()`. Destroy the arena to release everything
- **`forge_gltf_compute_world_transforms(scene, node_idx, parent_world)`** --
  Recursively compute world transforms. Returns `false` if the depth limit
  (`FORGE_GLTF_MAX_DEPTH`) is reached, indicating a possible cycle. Called
  automatically by `forge_gltf_load` (which propagates the failure), but
  exposed for recomputing after modifying local transforms

### Vertex Layout

Same as the OBJ parser -- the same GPU pipeline works for both:

| Attribute | Type | HLSL Semantic | Content |
|-----------|------|---------------|---------|
| location 0 | `float3` | `TEXCOORD0` | Position |
| location 1 | `float3` | `TEXCOORD1` | Normal |
| location 2 | `float2` | `TEXCOORD2` | UV |

Tangent vectors (`vec4`) are stored separately in `prim->tangents` when present
(`prim->has_tangents`). Per-vertex skin data (joint indices and weights) is
stored in `prim->joint_indices` and `prim->weights`.

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
- `has_uvs` -- whether TEXCOORD_0 was present in the source data
- `tangents` / `has_tangents` -- optional vec4 tangent vectors for normal mapping

Use `SDL_DrawGPUIndexedPrimitives` for rendering, which reduces memory usage
and improves GPU vertex cache performance.

## Materials

Each primitive can reference a material with:

- **`name`** -- Material name from glTF
- **`base_color[4]`** -- RGBA color factor (default: opaque white)
- **`texture_path`** / **`has_texture`** -- Base color texture path
- **`normal_map_path`** / **`has_normal_map`** -- Normal map texture path
- **`metallic_roughness_path`** / **`has_metallic_roughness`** -- Metallic-roughness texture path
- **`occlusion_path`** / **`has_occlusion`** -- Ambient occlusion texture path
- **`emissive_factor[3]`** / **`emissive_path`** / **`has_emissive`** -- Emissive color and texture
- **`alpha_mode`** -- `OPAQUE`, `MASK`, or `BLEND` (default: opaque)
- **`alpha_cutoff`** -- Threshold for alpha mask mode (default: 0.5)
- **`double_sided`** -- Whether to render both faces

The parser stores file paths, not GPU textures. The caller loads and creates
GPU textures however they prefer -- see Lesson 09 for a full example.

## Supported glTF Features

- **Meshes** with multiple primitives (one per material)
- **Materials** with PBR base color factor and base color texture
- **Alpha modes** (OPAQUE, MASK, BLEND) with configurable cutoff
- **Normal map textures** (`normalTexture`)
- **Double-sided materials**
- **`KHR_materials_transmission`** extension (approximated as blend)
- **Scene hierarchy** with parent-child node relationships and named nodes
- **TRS transforms** (translation, rotation, scale) and raw matrices
- **Skins** with inverse bind matrices and skeletal joint hierarchies
- **Animations** with LINEAR and STEP interpolation for translation, rotation,
  and scale channels
- **Vertex skin attributes** (`JOINTS_0`, `WEIGHTS_0`) for per-vertex bone weights
- **Indexed geometry** with 16-bit and 32-bit indices
- **Vertex attributes**: POSITION, NORMAL, TEXCOORD_0, TANGENT
- **Multiple binary buffers** referenced by URI
- **Morph targets** with position, normal, and tangent deltas
- **Accessor validation** (bounds checking, component type validation)

## Constants

| Constant | Default | Description |
|----------|---------|-------------|
| `FORGE_GLTF_MAX_NODES` | 512 | Default node array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_MESHES` | 256 | Default mesh array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_PRIMITIVES` | 1024 | Default primitive array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_MATERIALS` | 256 | Default material array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_IMAGES` | 128 | Default image array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_BUFFERS` | 16 | Maximum binary buffer files (enforced) |
| `FORGE_GLTF_MAX_SKINS` | 8 | Default skin array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_JOINTS` | 128 | Default joint array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_ANIMATIONS` | 16 | Default animation array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_ANIM_CHANNELS` | 128 | Default channel array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_ANIM_SAMPLERS` | 128 | Default sampler array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_MORPH_TARGETS` | 8 | Maximum morph targets per mesh (enforced) |
| `FORGE_GLTF_JOINTS_PER_VERT` | 4 | Joint influences per vertex |
| `FORGE_GLTF_MAX_CHILDREN` | 256 | Default children array sizing hint (not enforced) |
| `FORGE_GLTF_MAX_DEPTH` | 256 | Maximum hierarchy depth (recursion limit — enforced) |
| `FORGE_GLTF_PATH_SIZE` | 512 | Maximum file path length |
| `FORGE_GLTF_NAME_SIZE` | 64 | Maximum node/material name length |
| `FORGE_GLTF_DEFAULT_ALPHA_CUTOFF` | 0.5 | Default alpha mask threshold |

Most constants are sizing hints from before the arena migration and are no
longer enforced as hard caps — the loader dynamically allocates arrays via the
arena. `FORGE_GLTF_MAX_BUFFERS`, `FORGE_GLTF_MAX_MORPH_TARGETS`, and
`FORGE_GLTF_MAX_DEPTH` remain enforced.
See `forge_gltf.h` for the current allocation behavior.

## Dependencies

- **SDL3** -- for file I/O, logging, memory allocation
- **cJSON** -- for JSON parsing (`third_party/cJSON/`)
- **forge_math** -- for `vec2`, `vec3`, `vec4`, `mat4`, `quat` (`common/math/`)
- **forge_arena** -- for arena allocation (`common/arena/`)

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
5. **Arena allocation** -- all scene memory is allocated from a caller-provided
   arena, so cleanup is a single `forge_arena_destroy` call. Only the binary
   buffer array (`FORGE_GLTF_MAX_BUFFERS`) uses a fixed-size inline array

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
