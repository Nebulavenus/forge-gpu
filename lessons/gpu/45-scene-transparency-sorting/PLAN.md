# Lesson 45 — Scene Transparency Sorting

## Goal

Extend `forge_scene.h` with correct transparent rendering: two-pass draw
splitting opaque and blend submeshes, back-to-front depth sorting for
transparent draws, and alpha-tested shadow casting for MASK materials.

## Changes to forge_scene.h

### New types

- `ForgeSceneTransparentDraw` — sortable draw command (node, submesh, depth, world)
- `FORGE_SCENE_MODEL_MAX_SUBMESHES` constant (512)

### New fields

- `ForgeSceneModel.submesh_centroids[]` — object-space centroids per submesh
- `ForgeSceneModel.submesh_centroid_count`
- Same for `ForgeSceneSkinnedModel`
- `ForgeScene.model_shadow_mask_pipeline` — depth + alpha test for MASK shadows
- `ForgeScene.skinned_shadow_mask_pipeline` — same for skinned meshes

### Modified functions

- `forge_scene_init_model_pipelines()` — create shadow_mask pipelines
- `forge_scene_load_model()` — compute submesh centroids at load time
- `forge_scene_draw_model()` — two-pass: opaque+MASK first, sorted BLEND second
- `forge_scene_draw_model_shadows()` — MASK shadow support with texture sampling
- Same for skinned model variants
- `forge_scene_destroy()` — release new pipelines

### New shaders

- `shadow_mask.vert.hlsl` — pass through UVs for alpha testing
- `shadow_mask.frag.hlsl` — sample base color, discard below cutoff
- `shadow_mask_skinned.vert.hlsl` — skinned variant

## main.c Decomposition

### Chunk A (~250 lines): `/tmp/lesson_45_part_a.c`

- Header, includes, constants
- AppState struct
- SDL_AppInit: scene init, load models, override glass material to BLEND

### Chunk B (~250 lines): `/tmp/lesson_45_part_b.c`

- SDL_AppEvent: delegate to forge_scene
- SDL_AppIterate: shadow pass, main pass (draw models), UI panel
- SDL_AppQuit: cleanup

## Demo scene

- CesiumMilkTruck with windshield overridden to BLEND (alpha 0.3)
- UI toggle: sorting on/off to show artifacts vs correct rendering
- Stats: opaque/transparent draw counts
