# Lesson 37 — 3D Picking — Implementation Plan

## Overview

Interactive object picking using two GPU-based techniques: color-ID picking
(render each object with a unique flat color to an offscreen target, read back
the pixel under the mouse) and stencil-ID picking (per-object stencil reference
values). Highlight selected object with stencil outline from Lesson 34.

## Scene

8–10 procedural objects (cubes and spheres) on a grid floor. Click to select,
Tab to toggle picking method. Selected object gets a stencil outline.

## Render Passes

1. **Shadow pass** — D32_FLOAT 2048×2048 depth-only
2. **Main scene pass** — Swapchain color + D24S8 depth-stencil
   - Draw all objects with Blinn-Phong + shadow
   - If stencil-ID mode: each object sets stencil ref = index+1, ALWAYS/REPLACE
   - Draw grid floor
   - Draw crosshair (screen-space, no depth)
3. **ID pass** (color-ID only, only on click) — Offscreen R8G8B8A8 + D24S8
   - Draw all objects with flat ID color shader
   - Clear to (0,0,0,0) — background = no object
4. **Copy pass** (only on click) — Download 1 pixel from ID texture or D24S8
5. **Outline pass** (when object selected) — Swapchain color + D24S8 (LOAD)
   - Phase A: Draw selected object with stencil REPLACE (ref=200)
   - Phase B: Draw scaled-up with NOT_EQUAL (ref=200), solid outline color

## Shaders (10 total)

| File | Stage | Purpose |
|------|-------|---------|
| scene.vert.hlsl | Vertex | MVP + shadow coords (from L34) |
| scene.frag.hlsl | Fragment | Blinn-Phong + shadow (from L34) |
| shadow.vert.hlsl | Vertex | Light-space transform (from L34) |
| shadow.frag.hlsl | Fragment | Empty depth-only (from L34) |
| grid.vert.hlsl | Vertex | Grid floor (from L34) |
| grid.frag.hlsl | Fragment | Grid + shadow (from L34) |
| id_pass.vert.hlsl | Vertex | MVP only (position only) |
| id_pass.frag.hlsl | Fragment | Flat object ID color |
| outline.frag.hlsl | Fragment | Solid outline color (from L34) |
| crosshair.vert.hlsl | Vertex | Screen-space quad |
| crosshair.frag.hlsl | Fragment | Flat crosshair color |

## Pipelines (9 total)

1. `shadow_pipeline` — depth-only for shadow map
2. `scene_pipeline` — main Blinn-Phong, no stencil
3. `scene_stencil_pipeline` — Blinn-Phong + stencil ALWAYS/REPLACE
4. `grid_pipeline` — grid floor, no stencil
5. `id_pipeline` — color-ID pass, flat color, depth test
6. `crosshair_pipeline` — screen-space, no depth
7. `outline_write_pipeline` — stencil REPLACE (ref=200)
8. `outline_draw_pipeline` — stencil NOT_EQUAL, depth disabled
9. (grid uses same pipeline structure as L34 minus portal variants)

## main.c Decomposition (MANDATORY chunked writes)

### Chunk A: Header + Constants + Types + Helpers (~500 lines)

- License, includes, compiled shader includes
- Constants (WINDOW_WIDTH, SHADOW_MAP_SIZE, OBJECT_COUNT, etc.)
- Vertex layout, uniform structures
- SceneObject struct with name, shape type, position, scale, color
- PickingMethod enum (COLOR_ID, STENCIL_ID)
- app_state struct
- create_shader helper
- upload_gpu_buffer helper
- add_box, generate_cube, generate_sphere geometry helpers
- index_to_color / color_to_index helpers
- generate_grid helper

### Chunk B: SDL_AppInit (~600 lines)

- Device + window creation
- Depth-stencil format negotiation
- Texture creation (shadow_depth, main_depth, id_texture, id_depth)
- Transfer buffer for readback
- Sampler creation
- Shader creation (all 11 shaders)
- Vertex input state setup
- Pipeline creation (all 9 pipelines)
- Geometry generation + upload (cube, sphere, grid, crosshair)
- Scene object initialization
- Camera setup
- Capture init

### Chunk C: SDL_AppEvent + SDL_AppIterate (~600 lines)

- Event handling (quit, mouse click, Tab toggle, WASD, Esc)
- Timing + camera update
- Shadow pass
- Main scene pass (with conditional stencil-ID writes)
- Grid rendering
- Crosshair rendering
- ID pass (only on click, color-ID mode)
- Copy pass + readback + decode
- Outline pass (selected object)
- Submit + capture

### Chunk D: SDL_AppQuit (~80 lines)

- Resource cleanup in reverse order

## Key Implementation Details

- ID pass only runs on click, not every frame
- Background = (0,0,0) in ID pass, object IDs start at 1
- Outline stencil ref = 200 (no conflict with picking values 1-10)
- Transfer buffer created once at init, reused per pick
- SDL_WaitForGPUIdle after submit for synchronous readback
- Mouse coordinates from SDL_GetMouseState map directly to texture coords
- Crosshair: two thin screen-space quads at center
- Window title updated with method + selected object name
