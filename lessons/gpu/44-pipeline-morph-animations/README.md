# Lesson 44 — Pipeline Morph Target Animations

## What you'll learn

- Load morph target (blend shape) data from pipeline-processed `.fmesh` files
- Evaluate morph weight animation from `.fanim` keyframe channels on the CPU
- Blend per-vertex position and normal deltas weighted by morph targets
- Upload blended deltas to GPU storage buffers and apply them in the vertex shader via `SV_VertexID`
- Use `ForgeSceneMorphModel` to integrate morph rendering into the scene renderer
- Toggle between animated and manual morph weight control

## Result

![Pipeline Morph Animations](assets/animation.gif)

Two morphing models on a grid floor. AnimatedMorphCube deforms between its
base shape and two morph targets with animated weights. SimpleMorph demonstrates
the same blending on a triangle. A UI panel shows per-model animation time,
speed sliders, and a toggle for manual per-target weight control.

## Prerequisites

- [Lesson 41 — Scene Model Loading](../41-scene-model-loading/) — pipeline
  model loading (`.fscene` + `.fmesh` + `.fmat`)
- [Lesson 43 — Pipeline Skinned Animations](../43-pipeline-skinned-animations/) —
  animation evaluation pattern, storage buffer uploads
- [Asset Lesson 13 — Morph Targets](../../assets/13-morph-targets/) — morph
  data in the pipeline format

## Key concepts

### Morph targets (blend shapes)

Morph targets store per-vertex displacement deltas — differences from the base
mesh position, normal, and optionally tangent. The final vertex position is:

$$
\mathbf{p}_{\text{final}} = \mathbf{p}_{\text{base}} + \sum_{i=0}^{N-1} w_i \cdot \Delta\mathbf{p}_i
$$

where $w_i$ is the weight for morph target $i$ and $\Delta\mathbf{p}_i$ is
that target's position delta. Normals blend the same way (then re-normalize).

Unlike skeletal animation, morph targets deform individual vertices directly.
This makes them suited for facial expressions, muscle bulges, corrective
shapes, and any deformation that bone rotations cannot express.

### Pipeline morph data

The `.fmesh` binary format stores morph targets when the `FLAG_MORPHS`
(0x4) bit is set. After the index data, the file contains:

| Field | Type | Description |
|-------|------|-------------|
| morph_target_count | uint32 | Number of targets (max 8) |
| morph_attr_flags | uint32 | Bitmask: position (0x1), normal (0x2), tangent (0x4) |
| Per-target metadata | 68 bytes each | Name (64 chars) + default weight (float) |
| Position deltas | vertex_count × 3 floats | Per-target, if position flag set (uploaded as float4 with 16-byte stride) |
| Normal deltas | vertex_count × 3 floats | Per-target, if normal flag set (uploaded as float4 with 16-byte stride) |
| Tangent deltas | vertex_count × 3 floats | Per-target, if tangent flag set |

At runtime, `forge_pipeline_load_mesh()` parses this data into
`ForgePipelineMorphTarget` structs with `position_deltas`, `normal_deltas`,
and `tangent_deltas` float arrays.

### Morph weight animation

Morph weights are animated through `.fanim` files using channels with
`target_path = MORPH_WEIGHTS` (value 3). Unlike TRS channels that have a
fixed component count (3 for translation/scale, 4 for rotation), morph weight
channels have a variable component count equal to the number of morph targets.

`forge_pipeline_anim_apply()` intentionally skips morph weight channels — the
caller evaluates them directly. This lesson's update function binary-searches
the keyframe timestamps and linearly interpolates the weight values, the same
approach the pipeline uses for TRS channels.

### CPU blending, GPU displacement

This lesson uses CPU-side blending: each frame, the CPU iterates all morph
targets and accumulates weighted deltas into two flat arrays (position and
normal). These arrays are uploaded to GPU storage buffers via a persistent
transfer buffer.

The morph vertex shader reads the blended deltas using `SV_VertexID`:

```hlsl
StructuredBuffer<float4> morph_pos_deltas : register(t0, space0);
StructuredBuffer<float4> morph_nrm_deltas : register(t1, space0);

float3 morphed_pos = input.position + morph_pos_deltas[input.vertex_id].xyz;
float3 morphed_nrm = normalize(input.normal + morph_nrm_deltas[input.vertex_id].xyz);
```

This approach matches the skinning pattern from Lesson 43 (CPU computes data,
GPU applies it). With a maximum of 8 targets and typical mesh sizes under
10K vertices, CPU blending is negligible.

### Vertex format

Morph models use the standard 48-byte vertex layout (position, normal, UV,
tangent) — the same as `ForgeSceneModel`. Morph deltas are additive from
storage buffers, so no extra per-vertex attributes are needed. This keeps
the vertex stride efficient and the pipeline simple.

## Building

```bash
cmake -B build
cmake --build build --target 44-pipeline-morph-animations
```

Run from the build directory:

```bash
./build/lessons/gpu/44-pipeline-morph-animations/44-pipeline-morph-animations
```

## Math

This lesson uses:

- **Vectors** — [Math Lesson 01](../../math/01-vectors/) for positions and deltas
- **Matrices** — [Math Lesson 05](../../math/05-matrices/) for model transforms

## AI skill

The [`forge-pipeline-morph-animations`](../../../.claude/skills/forge-pipeline-morph-animations/SKILL.md)
skill distills this lesson's pattern into a reusable recipe. Invoke with
`/forge-pipeline-morph-animations` or copy into your own project.

## Exercises

1. **Add a third model** — Download `MorphStressTest` from the glTF sample
   assets, process it through the pipeline, and load it alongside the existing
   two. This model has textures and more morph targets.

2. **GPU-side blending** — Instead of blending on the CPU, upload raw
   per-target deltas to separate storage buffers and blend in the vertex
   shader with a weight uniform array. Compare GPU memory usage and frame
   times with the CPU approach.

3. **Combine skinning and morphs** — Create a model with both skinning and
   morph targets (e.g., a character with facial blend shapes). Write a vertex
   shader that applies morph deltas first, then skins the result with joint
   matrices.

4. **Morph target interpolation** — Add a mode that smoothly transitions
   between two morph target presets (e.g., "smile" and "frown") over time,
   independent of the `.fanim` animation.
