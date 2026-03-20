# Lesson 46 — Particle Animations: main.c Decomposition

## Chunk A (~350 lines): Headers, constants, structs, helpers

- File header comment (lesson description, controls, license)
- `#define SDL_MAIN_USE_CALLBACKS 1`
- Includes: SDL3, SDL_main, stddef.h, forge_math.h, forge_scene.h
- Compiled shader includes (3 shaders x 3 formats = 9 includes)
- `#define` constants (MAX_PARTICLES, WORKGROUP_SIZE, atlas params, UI layout,
  emitter defaults, compute/shader resource counts)
- `SimUniforms` struct (dt, gravity, drag, frame_counter, emitter params)
- `BillboardUniforms` struct (view_proj, cam_right, cam_up)
- `EmitterType` enum
- `app_state` struct
- `create_sim_pipeline()` — compute pipeline from bytecode
- `create_particle_pipeline()` — graphics pipeline with blend params
- `generate_atlas()` — procedural 4x4 Gaussian atlas as SDL_Surface

## Chunk B (~300 lines): SDL_AppInit + SDL_AppEvent

- `SDL_AppInit`:
  - Allocate app_state
  - Build font path, configure ForgeSceneConfig
  - `forge_scene_init()`
  - Create compute pipeline via `create_sim_pipeline()`
  - Create additive + alpha graphics pipelines via `create_particle_pipeline()`
  - Generate atlas surface, upload via `forge_scene_upload_texture()`
  - Create atlas sampler (linear, clamp)
  - Create particle buffer (COMPUTE_STORAGE_WRITE | GRAPHICS_STORAGE_READ)
  - Create spawn counter buffer (COMPUTE_STORAGE_WRITE)
  - Create persistent transfer buffer for counter reset (4 bytes)
  - Initialize UI state and defaults
- `SDL_AppEvent`:
  - Delegate to `forge_scene_handle_event()`
  - Handle 'B' key for burst spawn
  - Handle '1'/'2'/'3' for emitter type switching

## Chunk C (~300 lines): SDL_AppIterate + SDL_AppQuit

- `SDL_AppIterate`:
  - `forge_scene_begin_frame()`
  - Counter reset: map transfer, write spawn_count, unmap, copy pass
  - Compute pass: push SimUniforms, bind RW buffers, dispatch
  - Shadow pass (begin/end, no draws)
  - Main pass: draw grid, bind particle pipeline, bind storage buffer,
    bind atlas sampler, push BillboardUniforms, draw primitives
  - UI pass: window with spawn rate / gravity / drag sliders, emitter
    type checkboxes, burst button, info labels
  - `forge_scene_end_frame()`
- `SDL_AppQuit`:
  - Wait for GPU idle
  - Release: compute pipeline, 2 graphics pipelines, particle buffer,
    counter buffer, transfer buffer, atlas texture, atlas sampler
  - `forge_scene_destroy()`
  - `SDL_free(state)`
