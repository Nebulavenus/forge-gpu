---
name: forge-pipeline-assets
description: Load and render .fmesh binary meshes and pipeline-processed textures in SDL GPU applications
---

# Skill: Pipeline-Processed Assets

Load and render `.fmesh` binary meshes and pipeline-processed textures in SDL
GPU applications. This pattern replaces raw glTF loading with optimized binary
assets produced by the forge-gpu asset pipeline.

## When to use

- Loading mesh assets processed by `forge-mesh-tool` (`.fmesh` format)
- Loading textures with `.meta.json` sidecar metadata
- Setting up dual vertex formats (48-byte with tangents, 32-byte without)
- Implementing LOD selection based on camera distance
- Normal mapping with Gram-Schmidt TBN from pipeline tangent data

## Key files

| File | Purpose |
|------|---------|
| `common/pipeline/forge_pipeline.h` | Runtime loader for `.fmesh` + textures |
| `tools/mesh/main.c` | CLI tool that generates `.fmesh` from OBJ/glTF |
| `lessons/gpu/39-pipeline-processed-assets/main.c` | Reference implementation |

## Key API calls

- `forge_pipeline_load_mesh(path, &mesh)` — load `.fmesh` binary into CPU struct
- `forge_pipeline_free_mesh(&mesh)` — release CPU-side mesh data
- `forge_pipeline_has_tangents(&mesh)` — check if mesh has 48-byte tangent vertices
- `forge_pipeline_lod_index_count(&mesh, lod)` — get index count for a LOD level
- `forge_pipeline_lod_indices(&mesh, lod)` — get index pointer for a LOD level
- `forge_pipeline_load_texture(path, &tex)` — load texture via `.meta.json` sidecar
- `forge_pipeline_free_texture(&tex)` — release CPU-side texture data

## Pattern

### 1. Include the pipeline library

```c
/* In exactly ONE .c file: */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"
```

### 2. Load a .fmesh mesh

```c
ForgePipelineMesh mesh;
if (!forge_pipeline_load_mesh("model.fmesh", &mesh)) {
    /* handle error */
}

/* Check vertex format */
if (forge_pipeline_has_tangents(&mesh)) {
    /* 48-byte stride: position + normal + uv + tangent */
    ForgePipelineVertexTan *verts = (ForgePipelineVertexTan *)mesh.vertices;
} else {
    /* 32-byte stride: position + normal + uv */
    ForgePipelineVertex *verts = (ForgePipelineVertex *)mesh.vertices;
}

/* Upload to GPU */
SDL_GPUBuffer *vb = upload_gpu_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
    mesh.vertices, mesh.vertex_count * mesh.vertex_stride);

/* Access LOD levels */
for (uint32_t lod = 0; lod < mesh.lod_count; lod++) {
    uint32_t count = forge_pipeline_lod_index_count(&mesh, lod);
    const uint32_t *indices = forge_pipeline_lod_indices(&mesh, lod);
}

/* Cleanup */
forge_pipeline_free_mesh(&mesh);
```

### 3. Load a pipeline texture

```c
ForgePipelineTexture tex;
if (!forge_pipeline_load_texture("diffuse.png", &tex)) {
    /* handle error — needs diffuse.meta.json sidecar */
}

/* Decode mip 0 (currently PNG bytes) */
SDL_IOStream *io = SDL_IOFromMem(tex.mips[0].data, tex.mips[0].size);
SDL_Surface *surface = SDL_LoadSurface_IO(io, false);
if (!surface) {
    /* decode failed — fall back to loading from disk */
    SDL_CloseIO(io);
    forge_pipeline_free_texture(&tex);
    surface = SDL_LoadSurface("diffuse.png");
} else {
    SDL_CloseIO(io);
    forge_pipeline_free_texture(&tex);
}
```

### 4. Generate .fmesh files

```bash
# From glTF with LOD levels and tangent generation
./build/tools/mesh/forge_mesh_tool \
    input.gltf output.fmesh \
    --lod-levels 1.0,0.5,0.25 --verbose

# Creates output.fmesh + output.meta.json
```

### 5. Create .meta.json sidecars for textures

```json
{
  "width": 1024,
  "height": 1024,
  "format": "rgba8",
  "mips": [
    { "level": 0, "width": 1024, "height": 1024, "file": "texture.png" }
  ]
}
```

## Vertex formats

| Format | Stride | Fields | Use case |
|--------|--------|--------|----------|
| No tangent | 32 bytes | pos(12) + normal(12) + uv(8) | Basic Blinn-Phong |
| With tangent | 48 bytes | pos(12) + normal(12) + uv(8) + tangent(16) | Normal mapping |

## LOD selection

```c
uint32_t select_lod(float distance, uint32_t max_lods) {
    if (max_lods <= 1) return 0;
    if (distance < 3.0f) return 0;
    if (distance < 8.0f && max_lods > 1) return 1;
    if (max_lods > 2) return 2;
    return max_lods - 1;
}
```

## Common mistakes

- **Forgetting `FORGE_PIPELINE_IMPLEMENTATION`** — the header is header-only;
  exactly one `.c` file must define this macro before including it, or you get
  linker errors (undefined symbols for all `forge_pipeline_*` functions).
- **Using the wrong vertex stride for the pipeline** — `.fmesh` files with
  tangents have 48-byte vertices; without tangents, 32 bytes. Check
  `forge_pipeline_has_tangents()` and create separate GPU pipelines for each
  stride. SDL GPU validates stride at pipeline creation time.
- **Not creating separate shadow pipelines per stride** — shadow shaders only
  read position (attribute 0), but the vertex stride still must match. You need
  one shadow pipeline per vertex stride (48-byte and 32-byte).
- **Loading textures without the `.meta.json` sidecar** —
  `forge_pipeline_load_texture()` reads `<path>.meta.json` alongside the
  texture file. If the sidecar is missing, the call fails.
- **Using `SDL_LoadBMP_IO` for PNG data** — `SDL_LoadBMP_IO` only decodes
  BMP format. Use `SDL_LoadSurface_IO` (which auto-detects PNG and BMP) or
  fall back to loading the file directly from disk with `SDL_LoadSurface()`.
- **Forgetting to free pipeline data** — `forge_pipeline_load_mesh` and
  `forge_pipeline_load_texture` allocate memory internally. Always call the
  matching `_free` function, even if you've already uploaded the data to
  the GPU.

## Reference

- [Lesson 39 — Pipeline-Processed Assets](lessons/gpu/39-pipeline-processed-assets/)
- [Asset Lesson 06 — Loading Processed Assets](lessons/assets/06-loading-processed-assets/)
