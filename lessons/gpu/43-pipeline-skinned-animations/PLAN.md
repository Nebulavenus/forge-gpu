# Lesson 43 — Pipeline Skinned Animations

## Overview

Skeletal animation and transform animation using pre-processed pipeline
assets. Three models demonstrate different animation patterns:

- **CesiumMan** — walk cycle with 19-joint skeleton (skinned)
- **BrainStem** — articulated robot with 18-joint skeleton (skinned)
- **AnimatedCube** — rotation animation without skinning (transform only)

## main.c Decomposition

The lesson is a single file (~380 lines), under the chunk threshold.

### Structure

- File header, includes: SDL, forge_math, forge_pipeline, forge_scene
  (with `FORGE_SCENE_MODEL_SUPPORT`)
- Constants: camera defaults, model placements, UI layout, speed range
- `AppState` struct: ForgeScene, two ForgeSceneSkinnedModel, one
  ForgeSceneModel, animation state for AnimatedCube
- `SDL_AppInit()`: scene init, load 3 models (CesiumMan + BrainStem as
  skinned, AnimatedCube as regular model with separate `.fanim`)
- `SDL_AppEvent()`: delegate to forge_scene_handle_event
- `SDL_AppIterate()`: animation update, shadow pass, main pass, UI panel
- `SDL_AppQuit()`: free all models + animation data, destroy scene

## Key Concepts

- **Skinned model pipeline:** `.fmesh` v3 (72-byte vertices with joints +
  weights) + `.fskin` (joint hierarchy + inverse bind matrices) + `.fanim`
  (keyframe clips)
- **Joint matrix equation:** `joint_matrix[i] = inv(mesh_world) × joint_world × IBM[i]`
- **GPU storage buffer:** Joint matrices uploaded each frame as a
  `StructuredBuffer<float4x4>` read by the vertex shader
- **Transform animation:** Non-skinned model animated by evaluating
  `.fanim` keyframes and using the resulting world transform as placement

## Dependencies

- `forge_pipeline.h` — animation evaluation functions
  (`forge_pipeline_anim_apply`, `forge_pipeline_scene_compute_world_transforms`,
  `forge_pipeline_compute_joint_matrices`)
- `forge_scene.h` — `ForgeSceneSkinnedModel`, skinned pipelines, draw/update API
- Skinned shaders: `scene_skinned.vert.hlsl`, `scene_skinned_shadow.vert.hlsl`
