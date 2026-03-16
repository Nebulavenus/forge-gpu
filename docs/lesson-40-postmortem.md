# Lesson 40 Postmortem — What Went Wrong

This document records the failures encountered during the implementation of
GPU Lesson 40 (Scene Renderer) so they can inform future work.

## 1. Pipeline assets were omitted despite being in the plan

The planning agent gave the user three options: glTF-only, procedural-only,
or a mix of both. The user chose the mix. The coding agent ignored this and
wrote a procedural-shapes-only implementation. The user had to catch this and
demand the change.

**Root cause:** The coding agent treated the lesson as a "demo" rather than
reading the plan's intent — exercise `forge_scene.h` with both
`forge_pipeline.h` assets AND `forge_shapes.h` procedural geometry.

**Lesson:** Read the plan. Respect user choices made during planning. The
plan is the contract.

## 2. SDL stencil load op zero-value assertion

`SDL_GPU_LOADOP_LOAD` is enum value 0. Using `SDL_zero()` on a
depth-stencil target info struct leaves `stencil_load_op = LOAD`. When
`cycle = true`, SDL asserts: "Cannot cycle depth target when load op or
stencil load op is LOAD."

**Fix:** Always explicitly set `stencil_load_op = SDL_GPU_LOADOP_DONT_CARE`
and `stencil_store_op = SDL_GPU_STOREOP_DONT_CARE` on depth-stencil targets.
Never rely on `SDL_zero()` for these fields.

## 3. Copy pass inside a render pass

The original `forge_scene_draw_ui()` finalized UI vertex/index data and
uploaded it via a copy pass. But `draw_ui` was called inside the main render
pass, and SDL does not allow a copy pass while a render pass is active.

**Fix:** Restructured the frame lifecycle. UI finalization and upload happen
inside `forge_scene_begin_main_pass()` (before the render pass starts).
`forge_scene_draw_ui()` is now a pure draw call — no uploads.

## 4. Light direction convention mismatch

The light direction was stored as `(-0.3, -1.0, -0.5)` (pointing downward)
but the shaders expected a direction "toward the light" (positive Y = up).
Result: completely dark scene.

**Fix:** Changed default to `vec3_normalize(vec3_create(0.3, 1.0, 0.5))`.
The convention is: `light_dir` points toward the light source, not away
from it. Shaders use `dot(N, light_dir)` directly.

## 5. Camera mouse look fighting UI widgets

When the mouse cursor was visible (not captured) and the user dragged a UI
slider, the camera also rotated because mouse motion events were processed
by both the UI and the camera.

**Fix:** Check `ui_ctx->hot != FORGE_UI_ID_NONE ||
ui_ctx->active != FORGE_UI_ID_NONE` before applying camera mouse look.

## 6. Background and grid floor indistinguishable

Clear color `(0.06, 0.06, 0.08)` was too close to the grid floor background
color `(0.014, 0.014, 0.045)`. The floor blended into the background.

**Fix:** Changed clear color to `(0.15, 0.15, 0.20)` for visible contrast.

## 7. Texture path mismatch between .fmat and processed filenames

The `.fmat` material sidecar stores the original glTF texture path (e.g.
`assets/models/CesiumMilkTruck/CesiumMilkTruck.png`). The pipeline renames
processed textures to `<model>_baseColor.png`. The `filename_from_path()`
approach extracted `CesiumMilkTruck.png` which doesn't exist in the
processed directory.

**Fix:** Try the processed naming convention `<name>_baseColor.png` first,
then fall back to the fmat-derived filename.

**Deeper issue:** The `.fmat` format should store the processed filename,
not the original glTF path. This is a pipeline tool issue.

## 8. .fmesh does not preserve node hierarchy

The `forge_mesh_tool` flattens all primitives into a single vertex/index
buffer without recording the glTF node hierarchy. Models with multiple
nodes (e.g. CesiumMilkTruck with separate wheel nodes) render with all
parts at the origin — wheels aren't positioned at the axles.

The initial "fix" attempt was to bake world transforms into vertex
positions during export. This is wrong — glTF nodes are a runtime scene
graph. The same mesh can be instanced by multiple nodes with different
transforms, and nodes nest hierarchically. Baking destroys this.

**Correct fix:** The `.fmesh` format needs to describe the node tree:
which nodes exist, their parent-child relationships, their local
transforms, and which mesh/submeshes each node references. The runtime
then computes world transforms and draws each instance with its node's
transform. This is a significant format extension that belongs in a
dedicated asset pipeline lesson.

**Impact on lesson 40:** CesiumMilkTruck cannot be used with the current
pipeline. Need to use single-node models (WaterBottle, BoxTextured) or
load the truck via raw `forge_gltf_load()`.

## 9. `forge_ui_wctx_slider` does not exist

The lesson code called `forge_ui_wctx_slider()` — a function that doesn't
exist. The window context (`forge_ui_window.h`) provides `window_begin` /
`window_end` for grouping, but individual widgets (sliders, checkboxes)
are called on the underlying `ForgeUiContext` via `forge_ui_ctx_slider_layout()`.

**Fix:** Replace `forge_ui_wctx_slider(s->ui_wctx, ...)` with
`forge_ui_ctx_slider_layout(s->ui_ctx, ..., size)`.

## 10. `mat4_ortho` vs `mat4_orthographic`

Code called `mat4_ortho()` but the math library function is named
`mat4_orthographic()`. A naming assumption that wasn't verified against
the actual API.

**Lesson:** Check the header before using a function name from memory.

## Former blockers (resolved)

### Asset Lesson 09 — Scene Hierarchy (complete)

Addressed issue #8 above. A new C tool (`tools/scene/`) extracts the glTF
node hierarchy into a `.fscene` binary format. The runtime loader in
`forge_pipeline.h` reads it back and computes world transforms by walking the
tree. This gives the renderer the node tree, per-node transforms, and
mesh-to-submesh mappings needed to draw multi-node models correctly.

See [docs/asset-lesson-09-scene-hierarchy.md](asset-lesson-09-scene-hierarchy.md)
for full design details.

**Status:** Complete. C tool, Python plugin, runtime loader, and tests are
implemented and merged. Verified against CesiumMilkTruck — hierarchy,
instancing, and transforms are correct.

### ForgeGltfScene stack overflow (resolved)

The `ForgeGltfScene` struct previously used fixed-size arrays (512 nodes ×
256 children, 1024 primitives, etc.) that were too large for the stack. This
was resolved by migrating to arena-allocated dynamic arrays via `ForgeArena`.
All consumers (`forge_gltf_load`, mesh tool, anim tool, scene tool) now use
the arena allocator. See `common/gltf/forge_gltf.h` for the current
implementation.

### Lesson 40 resolution

With both blockers resolved, GPU Lesson 40 (Scene Renderer) was completed
along with Lessons 41–43 which build on it.
