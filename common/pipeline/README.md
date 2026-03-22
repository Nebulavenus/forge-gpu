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
- cJSON (texture .meta.json and .fanims manifest parsing)

## Mesh API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_mesh(path, mesh)` | Load a `.fmesh` binary file |
| `forge_pipeline_free_mesh(mesh)` | Free mesh memory |
| `forge_pipeline_has_tangents(mesh)` | Check for tangent data |
| `forge_pipeline_has_skin_data(mesh)` | Check for skinned vertex data (joints/weights) |
| `forge_pipeline_has_morph_data(mesh)` | Check for morph target deltas |
| `forge_pipeline_submesh_count(mesh)` | Number of submeshes in LOD 0 |
| `forge_pipeline_lod_index_count(mesh, lod)` | Get index count for a LOD |
| `forge_pipeline_lod_indices(mesh, lod)` | Get index pointer for a LOD |
| `forge_pipeline_lod_submesh(mesh, lod, idx)` | Get a submesh within a LOD |

## Texture API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_texture(path, tex)` | Load texture with mip chain |
| `forge_pipeline_free_texture(tex)` | Free texture memory |

## Compressed Texture API

| Function | Description |
|----------|-------------|
| `forge_pipeline_detect_sidecar(image_path, out)` | Detect `.meta.json` sidecar for pipeline textures |
| `forge_pipeline_load_ftex(ftex_path, tex)` | Load a `.ftex` block-compressed texture (BC7/BC5) |
| `forge_pipeline_free_compressed_texture(tex)` | Free compressed texture memory |

## Materials API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_materials(path, set)` | Load a `.fmat` material sidecar |
| `forge_pipeline_free_materials(set)` | Free material set memory |

## Scene API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_scene(path, scene)` | Load a `.fscene` node hierarchy |
| `forge_pipeline_free_scene(scene)` | Free scene memory |
| `forge_pipeline_scene_get_mesh(scene, idx)` | Get mesh-to-submesh mapping for a mesh index |
| `forge_pipeline_scene_compute_world_transforms(scene, ...)` | Recompute world transforms from local transforms |

## Animation API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_animation(path, file)` | Load a `.fanim` animation clip |
| `forge_pipeline_free_animation(file)` | Free animation memory |
| `forge_pipeline_load_anim_set(path, set)` | Load a `.fanims` animation manifest |
| `forge_pipeline_free_anim_set(set)` | Free animation set memory |
| `forge_pipeline_find_clip(set, name)` | Look up a clip by name in a manifest |
| `forge_pipeline_load_clip(set, base_dir, name, file)` | Load a named clip from a manifest |
| `forge_pipeline_anim_apply(...)` | Evaluate keyframes and apply to scene node transforms |

## Skin API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_skins(path, skins)` | Load a `.fskin` file (joint hierarchy + inverse bind matrices) |
| `forge_pipeline_free_skins(skins)` | Free skin data |
| `forge_pipeline_compute_joint_matrices(...)` | Compute final joint matrices for GPU upload |

## Atlas API

| Function | Description |
|----------|-------------|
| `forge_pipeline_load_atlas(path, atlas)` | Load an `atlas.json` metadata file (UV offset/scale per material) |
| `forge_pipeline_free_atlas(atlas)` | Free atlas data |

## See also

- [Asset Lesson 06](../../lessons/assets/06-loading-processed-assets/) — mesh and texture loading walkthrough
- [Asset Lesson 09](../../lessons/assets/09-scene-hierarchy/) — scene hierarchy loading
- [Asset Lesson 10](../../lessons/assets/10-animation-loader/) — animation loading
- [Asset Lesson 12](../../lessons/assets/12-skinned-animations/) — skin data loading
- [tools/mesh/](../../tools/mesh/) — the .fmesh writer
- [tools/scene/](../../tools/scene/) — the .fscene writer
- [tools/anim/](../../tools/anim/) — the .fanim writer
- [Asset Lesson 17](../../lessons/assets/17-texture-atlas/) — atlas packing and metadata
- [GPU Lesson 47](../../lessons/gpu/47-texture-atlas-rendering/) — atlas rendering on the GPU
- [pipeline/plugins/texture.py](../../pipeline/plugins/texture.py) — texture processor
