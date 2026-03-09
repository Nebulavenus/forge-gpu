# forge_pipeline.h

Header-only C library for loading assets processed by the forge-gpu asset
pipeline.

## Usage

In exactly one `.c` file:

```c
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"
```

All other files include the header without the define.

## Dependencies

- SDL3 (SDL_LoadFile, SDL_malloc, SDL_free, SDL_Log)
- cJSON (texture .meta.json parsing)

## Mesh API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_mesh(path, mesh)` | Load a `.fmesh` binary file |
| `forge_pipeline_free_mesh(mesh)` | Free mesh memory |
| `forge_pipeline_has_tangents(mesh)` | Check for tangent data |
| `forge_pipeline_lod_index_count(mesh, lod)` | Get index count for a LOD |
| `forge_pipeline_lod_indices(mesh, lod)` | Get index pointer for a LOD |

## Texture API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_texture(path, tex)` | Load texture with mip chain |
| `forge_pipeline_free_texture(tex)` | Free texture memory |

## See also

- [Asset Lesson 06](../../lessons/assets/06-loading-processed-assets/) — walkthrough
- [tools/mesh/](../../tools/mesh/) — the .fmesh writer
- [pipeline/plugins/texture.py](../../pipeline/plugins/texture.py) — texture processor
