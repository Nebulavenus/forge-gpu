# Asset Lesson 06 — Loading Processed Assets in C

A header-only C library for loading `.fmesh` binary meshes and pipeline-processed
textures at runtime. This bridges the Python asset pipeline (Lessons 01-05) and
the GPU lessons that render processed assets starting with
[GPU Lesson 39](../../gpu/39-pipeline-processed-assets/). GPU lessons 01-38
continue using `forge_gltf_load()` for model loading.

## What you'll learn

- The `.fmesh` binary format: header, vertex layout, LOD table, index buffer
- Loading `.fmesh` files with `forge_pipeline_load_mesh()`
- Accessing LOD levels and checking for tangent data
- The `.meta.json` sidecar format for textures
- Loading textures with mip chains via `forge_pipeline_load_texture()`
- How this bridges the Python pipeline (Asset Lessons 01-05) and GPU lessons
  that consume processed assets (starting at Lesson 39)

## Result

After completing this lesson, you will have:

- A header-only C library (`common/pipeline/forge_pipeline.h`) that loads
  `.fmesh` meshes with LOD levels and optional tangent data
- A texture loader that reads `.meta.json` sidecars and loads mip chains
- A test suite verifying 35 cases across mesh loading, texture loading,
  error handling, and resource cleanup

## Key concepts

- **`.fmesh` binary format** — a 32-byte header followed by LOD table, vertex
  data, and index data, designed for direct GPU buffer upload
- **Vertex stride** — 32 bytes without tangents, 48 bytes with MikkTSpace
  tangent vectors (the `w` component stores the bitangent sign)
- **LOD levels** — progressive mesh simplification stored as separate index
  ranges sharing the same vertex buffer
- **`.meta.json` sidecar** — JSON metadata alongside each texture describing
  dimensions, mip levels, and processing settings
- **Raw mip loading** — each mip level is loaded as raw file bytes, allowing
  the same API for both PNG (needs decoding) and future BC7/BC5 (GPU-ready)

## The `.fmesh` binary format

The `.fmesh` format is designed for direct GPU upload. Vertex and index data are
stored in exactly the layout the GPU expects — loading is a `memcpy` into a GPU
buffer with no parsing, no conversion, and no per-vertex processing at runtime.

The file has four contiguous sections:

```text
Offset   Size                            Section
──────   ─────────────────────────────   ──────────────────────────────
0        32 bytes                        Header
32       12 × lod_count bytes            LOD table
32+LODs  vertex_count × vertex_stride    Interleaved vertex data
...      total_indices × 4               Concatenated index data (uint32)
```

### Header (32 bytes)

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 4 | `magic` | `"FMSH"` — identifies the file format |
| 4 | 4 | `version` | Format version (currently 1) |
| 8 | 4 | `vertex_count` | Number of unique vertices |
| 12 | 4 | `vertex_stride` | Bytes per vertex (32 or 48) |
| 16 | 4 | `lod_count` | Number of LOD levels (1-8) |
| 20 | 4 | `flags` | Bit field (bit 0: has tangents) |
| 24 | 8 | `reserved` | Padding for future use |

All multi-byte values are stored in little-endian order, matching x86 and ARM
conventions. No byte swapping is needed on modern hardware.

### LOD table (12 bytes per entry)

Each LOD entry describes a range within the concatenated index data:

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 4 | `index_count` | Number of indices in this LOD |
| 4 | 4 | `index_offset` | Byte offset into the index data section |
| 8 | 4 | `target_error` | Simplification error metric (float) |

LOD 0 is always the full-detail mesh. Higher LOD levels have progressively fewer
triangles, produced by meshoptimizer's quadric error simplification in
[Asset Lesson 03](../03-mesh-processing/).

### Vertex layouts

The `vertex_stride` field determines the vertex layout:

**Stride 32 — no tangent data:**

| Component | Type | Bytes | Cumulative |
|---|---|---|---|
| position | float×3 | 12 | 12 |
| normal | float×3 | 12 | 24 |
| uv | float×2 | 8 | 32 |

**Stride 48 — with MikkTSpace tangents:**

| Component | Type | Bytes | Cumulative |
|---|---|---|---|
| position | float×3 | 12 | 12 |
| normal | float×3 | 12 | 24 |
| uv | float×2 | 8 | 32 |
| tangent | float×4 | 16 | 48 |

The tangent `w` component stores the bitangent sign (+1 or -1). The shader
reconstructs the bitangent as `B = cross(N, T.xyz) * T.w`, which handles
mirrored UV seams correctly.

### Flags

| Bit | Constant | Meaning |
|---|---|---|
| 0 | `FORGE_PIPELINE_FLAG_TANGENTS` | Mesh has tangent vectors for normal mapping |

When this flag is set, `vertex_stride` is 48 and each vertex includes a 4-component
tangent vector. The flag and stride are validated for consistency during loading.

## Loading a mesh

The library is header-only. Define `FORGE_PIPELINE_IMPLEMENTATION` in exactly
one `.c` file to include the implementation:

```c
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

ForgePipelineMesh mesh;
if (forge_pipeline_load_mesh("assets/processed/models/truck.fmesh", &mesh)) {
    printf("Loaded %u vertices (stride %u), %u LOD levels\n",
           mesh.vertex_count, mesh.vertex_stride, mesh.lod_count);

    /* Access LOD 0 (full detail): */
    uint32_t count = forge_pipeline_lod_index_count(&mesh, 0);
    const uint32_t *idx = forge_pipeline_lod_indices(&mesh, 0);

    /* Upload vertex and index data to GPU buffers... */

    /* Check for tangents: */
    if (forge_pipeline_has_tangents(&mesh)) {
        ForgePipelineVertexTan *v = (ForgePipelineVertexTan *)mesh.vertices;
        /* v[i].tangent[3] is the bitangent sign */
    } else {
        ForgePipelineVertex *v = (ForgePipelineVertex *)mesh.vertices;
    }

    forge_pipeline_free_mesh(&mesh);
}
```

The loader performs these steps internally:

1. Reads the entire file into memory with `SDL_LoadFile`
2. Validates the magic bytes, version, stride, and LOD count
3. Checks that the tangent flag and stride are consistent
4. Verifies the file is large enough for the declared data
5. Copies vertex, index, and LOD data into separate allocations
6. Frees the file buffer and returns the populated struct

The caller owns the allocated memory and must call `forge_pipeline_free_mesh()`
when done. Calling free on a zeroed or already-freed mesh is safe (no-op).

### Selecting LOD levels at runtime

Each LOD level shares the same vertex buffer but uses a different range of the
index buffer. The runtime selects a LOD based on the object's distance from the
camera or its screen-space size:

```c
/* Pick LOD based on distance (simple threshold example) */
uint32_t lod = 0;
if (distance > 50.0f && mesh.lod_count > 1) lod = 1;
if (distance > 100.0f && mesh.lod_count > 2) lod = 2;

uint32_t idx_count = forge_pipeline_lod_index_count(&mesh, lod);
const uint32_t *idx_data = forge_pipeline_lod_indices(&mesh, lod);

/* Draw with the selected LOD's index range */
SDL_DrawGPUIndexedPrimitives(render_pass, idx_count, 1,
    (uint32_t)(idx_data - mesh.indices), 0, 0);
```

## Texture metadata and mip chains

The Python texture plugin ([Asset Lesson 02](../02-texture-processing/))
produces two outputs for each texture: the processed image file and a
`.meta.json` sidecar containing metadata about the processing results.

### The `.meta.json` sidecar format

```json
{
  "source": "brick_diffuse.png",
  "output": "brick_diffuse.png",
  "output_width": 1024,
  "output_height": 1024,
  "mip_levels": [
    { "level": 0, "width": 1024, "height": 1024 },
    { "level": 1, "width": 512,  "height": 512  },
    { "level": 2, "width": 256,  "height": 256  }
  ],
  "settings": {
    "max_size": 2048,
    "generate_mips": true
  }
}
```

The loader reads this sidecar to discover:

- **Base dimensions** — `output_width` and `output_height` of the processed image
- **Mip chain** — how many mip levels exist and the dimensions of each
- **File paths** — mip 0 uses the base image path; mip N uses
  `<stem>_mipN.<ext>` (e.g. `brick_diffuse_mip1.png`)

Each mip level is loaded as raw file bytes. For current pipeline output (PNG
files), these bytes need decoding with SDL_image or stb_image before GPU upload.
For future compressed formats (BC7/BC5), the raw bytes are GPU-ready and can be
uploaded directly.

## Loading a texture

```c
ForgePipelineTexture tex;
if (forge_pipeline_load_texture("assets/processed/textures/diffuse.png", &tex)) {
    printf("Loaded texture %ux%u, %u mip levels\n",
           tex.width, tex.height, tex.mip_count);

    /* tex.mips[0].data — raw file bytes for mip 0 */
    /* tex.mips[0].width, tex.mips[0].height — base dimensions */
    /* tex.mips[0].size — byte count of the raw file data */

    /* Decode with your preferred image library, then upload to GPU.
     * For example with stb_image:
     *   int w, h, channels;
     *   uint8_t *pixels = stbi_load_from_memory(
     *       tex.mips[0].data, tex.mips[0].size,
     *       &w, &h, &channels, 4);
     */

    forge_pipeline_free_texture(&tex);
}
```

The loader derives the `.meta.json` path by replacing the file extension with
`.meta.json` (e.g. `diffuse.png` becomes `diffuse.meta.json`). If the meta file
lists a `mip_levels` or `mips` array, each level is loaded from its corresponding
file. If no mip array is present, the base image is loaded as a single mip level.

The texture loader depends on [cJSON](https://github.com/DaveGamble/cJSON) for
JSON parsing — the same library used throughout forge-gpu's asset loading code.

## BC7 and BC5 texture compression (looking ahead)

From [GPU Lesson 39](../../gpu/39-pipeline-processed-assets/) onward, the
pipeline will support block-compressed texture formats that bypass CPU-side
decoding entirely:

- **BC7** — High-quality RGBA block compression for albedo and diffuse textures.
  Each 4x4 pixel block is encoded as 16 bytes, giving a fixed 4:1 compression
  ratio over RGBA8. BC7 supports multiple partition modes that adapt to block
  content, preserving quality across gradients, edges, and solid regions.

- **BC5** — Two-channel (RG) block compression for normal maps. Stores the X
  and Y components of the normal vector; the Z component is reconstructed in
  the shader:

  ```hlsl
  float3 normal;
  normal.xy = bc5_sample.rg * 2.0 - 1.0;
  normal.z  = sqrt(1.0 - saturate(dot(normal.xy, normal.xy)));
  ```

  This reconstruction works because unit normal vectors satisfy
  `x² + y² + z² = 1`, so `z = sqrt(1 - x² - y²)`.

When the pipeline produces BC7/BC5 output, the texture loader's raw file bytes
are already in the GPU's native format. No CPU-side decoding step is needed —
the bytes go directly from `tex.mips[i].data` into an `SDL_GPUTexture` via
`SDL_UploadToGPUTexture`.

## API reference

### Types

| Type | Description |
|---|---|
| `ForgePipelineVertex` | Vertex without tangents (32 bytes): position, normal, uv |
| `ForgePipelineVertexTan` | Vertex with tangents (48 bytes): position, normal, uv, tangent |
| `ForgePipelineLod` | LOD entry: index_count, index_offset, target_error |
| `ForgePipelineMesh` | Loaded mesh: vertices, indices, LODs, flags |
| `ForgePipelineMipLevel` | One mip level: data pointer, width, height, byte size |
| `ForgePipelineTexture` | Loaded texture: mip array, base dimensions, format |
| `ForgePipelineTextureFormat` | Pixel format enum: `FORGE_PIPELINE_TEX_RGBA8`, `FORGE_PIPELINE_TEX_RGB8` |

### Functions

| Function | Returns | Description |
|---|---|---|
| `forge_pipeline_load_mesh(path, mesh)` | `bool` | Load a `.fmesh` file into `mesh`. Caller must free with `forge_pipeline_free_mesh()`. |
| `forge_pipeline_free_mesh(mesh)` | `void` | Release all memory owned by `mesh`. Safe on NULL or zeroed structs. |
| `forge_pipeline_has_tangents(mesh)` | `bool` | Check if the mesh has tangent vectors (stride 48). |
| `forge_pipeline_lod_index_count(mesh, lod)` | `uint32_t` | Get the index count for LOD level `lod`. Returns 0 if out of range. |
| `forge_pipeline_lod_indices(mesh, lod)` | `const uint32_t *` | Get a pointer into `mesh->indices` at the correct offset for `lod`. Returns NULL if out of range. |
| `forge_pipeline_load_texture(path, tex)` | `bool` | Load texture metadata and mip chain. Caller must free with `forge_pipeline_free_texture()`. |
| `forge_pipeline_free_texture(tex)` | `void` | Release all memory owned by `tex`. Safe on NULL or zeroed structs. |

### Constants

| Constant | Value | Description |
|---|---|---|
| `FORGE_PIPELINE_FMESH_MAGIC` | `"FMSH"` | Magic bytes identifying `.fmesh` files |
| `FORGE_PIPELINE_FMESH_VERSION` | `1` | Current format version |
| `FORGE_PIPELINE_HEADER_SIZE` | `32` | Header size in bytes |
| `FORGE_PIPELINE_LOD_ENTRY_SIZE` | `12` | LOD table entry size in bytes |
| `FORGE_PIPELINE_VERTEX_STRIDE_NO_TAN` | `32` | Vertex stride without tangents |
| `FORGE_PIPELINE_VERTEX_STRIDE_TAN` | `48` | Vertex stride with tangents |
| `FORGE_PIPELINE_FLAG_TANGENTS` | `1 << 0` | Header flag indicating tangent data |
| `FORGE_PIPELINE_MAX_LODS` | `8` | Maximum supported LOD levels |
| `FORGE_PIPELINE_MAX_MIP_LEVELS` | `16` | Maximum supported texture mip levels |

## Where it connects

| Track | Connection |
|---|---|
| [Asset Lessons 01-05](../01-pipeline-scaffold/) | The Python pipeline that produces the `.fmesh` files and processed textures this loader reads |
| [Asset Lesson 03 — Mesh Processing](../03-mesh-processing/) | The `forge-mesh-tool` that writes the `.fmesh` binary format |
| [Asset Lesson 02 — Texture Processing](../02-texture-processing/) | The texture plugin that produces `.meta.json` sidecars |
| [GPU Lesson 39 — Pipeline-Processed Assets](../../gpu/39-pipeline-processed-assets/) | The first GPU lesson that uses this loader to render pipeline-processed meshes and textures |
| [GPU Lesson 17 — Normal Maps](../../gpu/17-normal-maps/) | Uses the tangent vectors that the pipeline pre-generates via MikkTSpace |

## Building

### Prerequisites

- SDL3 (with GPU API)
- CMake 3.24+
- cJSON (fetched automatically via CMake FetchContent)

### Compile

The library is header-only — there is no separate build target. Include it in
any C project that links SDL3 and cJSON:

```c
/* In exactly one .c file: */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

/* In all other files that need the types: */
#include "pipeline/forge_pipeline.h"
```

## Exercises

1. **Total index count.** Add a `forge_pipeline_mesh_total_indices()` function
   that sums `index_count` across all LOD levels. This is useful for allocating
   a single GPU index buffer that holds every LOD.

2. **BC7/BC5 format detection.** Add format detection to the texture loader
   based on file extension (`.bc7`, `.bc5`) or a `compression` field in the
   `.meta.json` sidecar. Set a new `ForgePipelineTextureFormat` enum value so
   callers know whether the mip data needs decoding or is GPU-ready.

3. **Mesh statistics printer.** Write a small program that loads a `.fmesh` file
   and prints vertex count, vertex stride, LOD count, per-LOD index counts, and
   total file size to stdout. Use this to verify pipeline output without
   launching a GPU window.

4. **LOD offset validation.** Add a validation pass in `forge_pipeline_load_mesh()`
   that checks each LOD's `index_offset` against the cumulative sum of preceding
   LOD index counts multiplied by `sizeof(uint32_t)`. Log a warning if the
   offsets are inconsistent — this catches corrupted or hand-edited `.fmesh`
   files early.
