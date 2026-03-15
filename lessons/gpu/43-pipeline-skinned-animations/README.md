# Lesson 43 — Pipeline Skinned Animations

## What you'll learn

- Load and render skinned models from pipeline-processed assets (`.fmesh` v3, `.fskin`, `.fanim`)
- Understand the skinned vertex format — 72 bytes with joint indices and blend weights
- Evaluate keyframe animations on the CPU: binary search, lerp, slerp
- Compute joint matrices and upload them to a GPU storage buffer each frame
- Apply transform animation to non-skinned models using the same keyframe system
- Use `ForgeSceneSkinnedModel` to integrate skinned rendering into the scene renderer

## Result

![Pipeline Skinned Animations](assets/animation.gif)

Three animated models on a grid floor. CesiumMan walks with a 19-joint
skeleton, BrainStem articulates with 18 joints, and AnimatedCube rotates
using transform animation without any skinning. A UI panel shows per-model
animation time and speed controls.

## Prerequisites

- [Lesson 32 — Skinning Animations](../32-skinning-animations/) — glTF-based
  skeletal animation, inverse bind matrices, joint hierarchies
- [Lesson 41 — Scene Model Loading](../41-scene-model-loading/) — pipeline
  model loading (`.fscene` + `.fmesh` + `.fmat`)
- [Lesson 42 — Pipeline Texture Compression](../42-pipeline-texture-compression/) —
  pipeline asset format conventions

## Key concepts

### Skinned vertex format

The pipeline supports a 56-byte stride (without tangents) or 72-byte stride
(with tangents). This lesson's loader requires the 72-byte layout. Compared
to the 48-byte format from lessons 39-42, the additional bytes carry joint
indices and blend weights:

| Attribute | Type | Size | Offset | Purpose |
|-----------|------|------|--------|---------|
| position | float3 | 12 | 0 | Vertex position in bind pose |
| normal | float3 | 12 | 12 | Surface normal in bind pose |
| uv | float2 | 8 | 24 | Texture coordinates |
| tangent | float4 | 16 | 32 | Tangent direction + handedness sign |
| joints | uint16x4 | 8 | 48 | Indices into the joint matrix array |
| weights | float4 | 16 | 56 | Blend weights (sum to 1.0) |

The `.fmesh` v3 format sets the `FORGE_PIPELINE_FLAG_SKINNED` flag in its
header so the loader can detect the stride and configure the vertex layout
accordingly.

### Pipeline asset files for animation

Three file types work together:

- **`.fskin`** — Joint hierarchy (parent indices), inverse bind matrices, and
  the mapping from skin joints to scene nodes. This is the skeleton's rest
  pose and topology.
- **`.fanim`** — Keyframe clips with per-channel TRS (translation, rotation,
  scale) data. Rotation channels store quaternions. Each channel targets a
  specific node by index.
- **`.fmesh` v3** — Same binary mesh format as v2, but with the skinned flag
  and a skinned vertex stride (56 or 72 bytes, depending on tangents).

All three are generated at build time by the asset pipeline tools from glTF
source models. At runtime, loading is a single read per file — no parsing of
glTF JSON, no runtime processing.

### Animation evaluation pipeline

Animation runs entirely on the CPU each frame, in three stages:

**1. Apply keyframes** — `forge_pipeline_anim_apply()` iterates each channel
in the clip, binary-searches for the two keyframes bracketing the current
time, and interpolates: `lerp` for translation and scale, `slerp` for
rotation quaternions. The result overwrites each node's `local_transform`
as a composed `T × R × S` matrix.

**2. Propagate hierarchy** — `forge_pipeline_scene_compute_world_transforms()`
walks the node tree from roots to leaves, multiplying
`parent_world * child_local` at each level to produce world-space transforms.

**3. Compute joint matrices** — `forge_pipeline_compute_joint_matrices()`
produces the final matrix for each joint:

```text
joint_matrix[i] = inverse(mesh_world) × joint_world[i] × inverse_bind_matrix[i]
```

The inverse bind matrix transforms from model space to the joint's local
space in the bind pose. Multiplying by the joint's current world transform
moves the vertex to the joint's animated position. The `inverse(mesh_world)`
factor accounts for the mesh node's own transform in the scene hierarchy.

### GPU joint storage buffer

Joint matrices are uploaded to the GPU each frame as a storage buffer. The
buffer holds up to `FORGE_PIPELINE_MAX_SKIN_JOINTS` (256) joints — 16,384 bytes
(256 x 64 bytes per 4x4 matrix).
`forge_scene_load_skinned_model()` allocates a persistent transfer buffer once,
and `forge_scene_update_skinned_animation()` reuses it each frame:

1. Map the persistent transfer buffer to get a CPU-writable pointer
2. Copy the joint matrices with `memcpy`
3. Unmap the transfer buffer
4. Issue a copy pass from transfer buffer to storage buffer

Reusing the transfer buffer avoids per-frame allocation overhead.

The vertex shader reads the storage buffer as a `StructuredBuffer<float4x4>`:

```hlsl
StructuredBuffer<float4x4> joint_mats : register(t0, space0);
```

### Skinned vertex shader

The vertex shader blends up to four joint transforms per vertex using the
joint indices and weights:

```hlsl
float4x4 skin_mat =
    input.weights.x * joint_mats[input.joints.x] +
    input.weights.y * joint_mats[input.joints.y] +
    input.weights.z * joint_mats[input.joints.z] +
    input.weights.w * joint_mats[input.joints.w];

float4 skinned_pos = mul(skin_mat, float4(input.position, 1.0));
float3 skinned_nrm = normalize(mul((float3x3)skin_mat, input.normal));
float3 skinned_tan = normalize(mul((float3x3)skin_mat, input.tangent.xyz));
```

After skinning, the vertex follows the same world-space transform, TBN
construction, and shadow projection as the non-skinned `scene_model` shader.
The fragment shader is shared — skinning is purely a vertex-stage operation.

### Loading a skinned model

`forge_scene_load_skinned_model()` takes paths to all five pipeline files
and a base directory for texture lookup:

```c
if (!forge_scene_load_skinned_model(&state->scene, &state->cesium_man,
                                     fscene, fmesh, fmat, fskin, fanim,
                                     base_dir)) {
    SDL_Log("Failed to load CesiumMan");
    return SDL_APP_FAILURE;
}
```

The function validates that the `.fmesh` has the `FORGE_PIPELINE_FLAG_SKINNED`
flag set, uploads vertex and index data to GPU buffers, creates the joint
storage buffer, and loads all referenced textures.

### Updating animation each frame

For skinned models, a single call advances time, evaluates keyframes,
propagates the hierarchy, computes joint matrices, and uploads them to
the GPU:

```c
state->cesium_man.anim_speed = state->cesium_speed;
forge_scene_update_skinned_animation(s, &state->cesium_man, dt);
```

### Transform animation without skinning

AnimatedCube has no skeleton — it uses a rotation animation applied directly
to the scene node's transform. The same keyframe evaluation functions work,
but instead of driving joint matrices, the animation updates the node's
`world_transform` which `forge_scene_draw_model()` reads internally:

```c
/* Advance time and evaluate keyframes */
state->cube_anim_time += dt * state->cube_anim_speed;
if (state->cube_anim.clip_count > 0) {
    forge_pipeline_anim_apply(
        &state->cube_anim.clips[0],
        state->animated_cube.scene_data.nodes,
        state->animated_cube.scene_data.node_count,
        state->cube_anim_time, true);
}

/* Propagate parent-child hierarchy */
forge_pipeline_scene_compute_world_transforms(
    state->animated_cube.scene_data.nodes,
    state->animated_cube.scene_data.node_count,
    state->animated_cube.scene_data.roots,
    state->animated_cube.scene_data.root_count,
    state->animated_cube.scene_data.children,
    state->animated_cube.scene_data.child_count);
```

The model then draws with `forge_scene_draw_model()` as usual — the updated
node transforms are already baked into the scene hierarchy.

### Drawing skinned models

Skinned models need their joint storage buffer bound at the vertex stage.
`forge_scene_draw_skinned_model()` handles this, selecting the correct
pipeline variant based on material properties (alpha mode, double-sided):

```c
forge_scene_draw_skinned_model(s, &state->cesium_man, cesium_place);
```

The shadow pass uses a separate function that binds the same joint buffer
but uses the shadow vertex shader:

```c
forge_scene_draw_skinned_model_shadows(s, &state->cesium_man, cesium_place);
```

## Building

```bash
cmake --build build --target 43-pipeline-skinned-animations
python scripts/run.py 43
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Move camera |
| Mouse | Look around |
| Space / Shift | Fly up / down |
| Escape | Release mouse |

## AI skill

The [`/forge-pipeline-skinned-animations`](../../../.claude/skills/forge-pipeline-skinned-animations/)
skill provides the API reference, vertex layout, shader patterns, and common
mistakes for adding pipeline-based skeletal animation to a project.

## Cross-references

- [Lesson 32 — Skinning Animations](../32-skinning-animations/) — the glTF-based
  version of skeletal animation, covering the same math with raw glTF data
- [Lesson 41 — Scene Model Loading](../41-scene-model-loading/) — pipeline
  model loading that this lesson extends with skinned support
- [Lesson 42 — Pipeline Texture Compression](../42-pipeline-texture-compression/) —
  compressed texture loading used by the skinned models' materials
- [Asset Lesson 03 — Mesh Processing](../../assets/03-mesh-processing/) —
  the mesh tool that generates `.fmesh` v3 files with skinned vertices
- [Math Lesson 08 — Orientation](../../math/08-orientation/) — quaternion
  slerp used in animation interpolation
- [Math Library — `forge_math.h`](../../../common/math/) — full math API
  reference

## Exercises

1. **Add a fourth model.** Pick a glTF model with a different animation
   (e.g., Fox from glTF-Sample-Assets). Process it through the asset pipeline
   to generate `.fmesh`, `.fskin`, `.fanim`, and `.fmat` files, then load and
   render it alongside the existing three.

2. **Implement animation blending.** Evaluate two animation clips
   independently, then blend per-joint TRS components with a configurable
   weight factor: lerp translation and scale, slerp rotation quaternions.
   Rebuild each joint matrix from the blended TRS. Add a UI slider to
   control the blend weight. This is the foundation of animation state
   machines used in game engines.

3. **Visualize the skeleton.** Use `forge_scene` debug line drawing to render
   the joint hierarchy as colored lines. For each joint with a parent, draw a
   line from the joint's world position to its parent's world position. This
   is a standard debugging technique for verifying that joint transforms are
   correct.
