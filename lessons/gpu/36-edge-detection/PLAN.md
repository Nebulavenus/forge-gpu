# Lesson 36 — Edge Detection: Build Plan

## main.c Decomposition

The main.c file must be written in 3 chunks to stay within the 32K token
limit per Write call.

### Part A: Headers, types, helpers, geometry (~400-600 lines)

- `#include` directives (SDL3, forge_math, forge_gltf, forge_capture, cJSON, shader headers)
- Constants (`SHADOW_MAP_SIZE`, `WINDOW_WIDTH`, `WINDOW_HEIGHT`, edge detection params)
- Type definitions:
  - `SceneVertex` (pos, normal, uv, tangent)
  - `FullscreenVertex` (pos, uv)
  - `VertUniforms`, `FragUniforms`, `EdgeUniforms`, `GhostUniforms`
  - `AppState` (device, window, pipelines, textures, buffers, camera, model)
- Helper functions:
  - `create_depth_stencil_texture()` — D24S8 with format fallback
  - `create_render_target()` — offscreen color target creation
  - `create_shadow_pipeline()` — depth-only shadow pass
  - `create_scene_pipeline()` — MRT pipeline (color + normals)
  - `create_grid_pipeline()` — MRT grid pipeline
  - `create_edge_pipeline()` — full-screen Sobel composite
  - `create_xray_mark_pipeline()` — depth_fail_op stencil mark
  - `create_ghost_pipeline()` — additive Fresnel ghost
  - `upload_fullscreen_quad()` — screen-filling quad for edge composite
- Geometry loading (Suzanne via forge_gltf_load)

### Part B: SDL_AppInit (~400-600 lines)

- Window and device creation
- Swapchain configuration (SDR_LINEAR)
- Depth-stencil texture creation (D24_UNORM_S8_UINT with fallback)
- G-buffer texture creation (scene color RT0, normals RT1)
- Shadow map texture creation
- Sampler creation (scene, shadow, edge)
- Shader loading (all vertex/fragment shader pairs)
- Pipeline creation (shadow, scene MRT, grid MRT, edge composite, xray mark, ghost)
- Model loading and GPU buffer upload
- Fullscreen quad upload
- Camera initialization

### Part C: AppEvent, AppIterate, AppQuit (~400-600 lines)

- `SDL_AppEvent` — keyboard/mouse input, camera controls
- `SDL_AppIterate`:
  1. Shadow pass — render scene to shadow map
  2. Scene pass (MRT) — render lit scene to color RT0 + normals RT1 + depth-stencil
  3. Grid pass (MRT) — render grid to same MRT targets (load, not clear)
  4. Edge composite pass — full-screen Sobel on depth + normals, composite onto scene
  5. X-ray mark pass — draw target object with depth_fail_op, color_write_mask=0
  6. Ghost pass — draw target object with stencil NOT_EQUAL ref=0, additive Fresnel
  7. Present — blit final result to swapchain
- `SDL_AppQuit` — release all GPU resources
