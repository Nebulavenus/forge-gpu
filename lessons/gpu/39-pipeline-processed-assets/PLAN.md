# Lesson 39 — Pipeline-Processed Assets: Implementation Plan

## main.c Decomposition (4 Chunks)

### Chunk A (~500 lines): `/tmp/lesson_39_part_a.c`

- `#define SDL_MAIN_USE_CALLBACKS 1`, all includes
- `#include "pipeline/forge_pipeline.h"` with `FORGE_PIPELINE_IMPLEMENTATION`
- 20 compiled shader includes (10 shaders x 2 formats)
- All `#define` constants
- Struct typedefs: SceneVertUniforms, SceneFragUniforms, GridVertUniforms,
  GridFragUniforms, SkyVertUniforms, RenderMode enum, LoadedModel, AppState
- Helper functions: create_shader(), upload_gpu_buffer(),
  load_texture_from_surface(), load_texture_linear(), select_lod()

### Chunk B (~550 lines): `/tmp/lesson_39_part_b.c`

- SDL_AppInit: device, window, swapchain, depth textures, samplers
- 10 shader loads, 6 pipeline creations
- Pipeline model loading (forge_pipeline_load_mesh -> upload)
- Raw model loading (forge_gltf_load -> extract -> upload)
- Texture loading for both paths
- Grid geometry, camera init, capture init

### Chunk C (~450 lines): `/tmp/lesson_39_part_c.c`

- SDL_AppEvent: quit, mouse, keyboard (1/2/3 modes, L LOD, I info)
- SDL_AppIterate: camera update, light VP, LOD selection
- Shadow pass, scene pass (3 modes), grid, sky
- Submit + capture

### Chunk D (~80 lines): `/tmp/lesson_39_part_d.c`

- SDL_AppQuit: release all GPU resources, free pipeline/gltf data

## Assembly

```bash
cat /tmp/lesson_39_part_a.c /tmp/lesson_39_part_b.c \
    /tmp/lesson_39_part_c.c /tmp/lesson_39_part_d.c \
    > lessons/gpu/39-pipeline-processed-assets/main.c
```
