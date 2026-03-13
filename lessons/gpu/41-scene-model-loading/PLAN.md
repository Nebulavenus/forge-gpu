# Lesson 41 — Scene Model Loading

## Overview

Load and render pipeline-processed 3D models through `forge_scene.h`.
Three models (CesiumMilkTruck, Suzanne, Duck) demonstrate scene hierarchy
traversal, per-primitive material binding, and instanced meshes.

## main.c Decomposition

### Chunk A (~350 lines): `/tmp/lesson_41_part_a.c`

- File header comment, license
- `#define SDL_MAIN_USE_CALLBACKS 1`
- Includes: SDL, forge_math, forge_pipeline, forge_scene (with MODEL_SUPPORT)
- Constants: window, camera defaults, model placements, UI layout
- AppState struct: ForgeScene, three ForgeSceneModel, camera state
- `SDL_AppInit()`:
  - Create window + GPU device via forge_scene_init
  - Load CesiumMilkTruck model
  - Load Suzanne model
  - Load Duck model
  - Set initial camera position

### Chunk B (~400 lines): `/tmp/lesson_41_part_b.c`

- `SDL_AppEvent()`: delegate to forge_scene_handle_event
- `SDL_AppIterate()`:
  - `forge_scene_begin_frame()`, early-out on false
  - Camera update (quaternion FPS pattern)
  - Compute placement matrices for 3 models
  - Shadow pass: `forge_scene_draw_model_shadows()` for each
  - Main pass: `forge_scene_draw_model()` for each, `forge_scene_draw_grid()`
  - UI pass: stats panel (node/mesh/material counts, draw calls, FPS)
  - `forge_scene_end_frame()`

### Chunk C (~100 lines): `/tmp/lesson_41_part_c.c`

- `SDL_AppQuit()`:
  - Free all three models
  - Destroy scene
  - SDL_free state
