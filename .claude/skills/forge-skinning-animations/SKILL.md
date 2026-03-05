---
name: forge-skinning-animations
description: >
  Add skeletal skinning animations with glTF joint hierarchies, inverse bind
  matrices, and per-vertex blend weights to an SDL GPU project.
---

Add vertex skinning to an SDL3 GPU scene using skeletal animation data from
glTF files. The technique evaluates keyframe channels for translation, rotation
(slerp), and scale, rebuilds the joint hierarchy, computes joint matrices
(`inverse(meshWorld) × jointWorld × inverseBindMatrix`), and blends up to 4 joint transforms per
vertex in the shader. Use this skill when you need animated characters,
creatures, or any mesh that deforms with a skeleton.

See [GPU Lesson 32 — Skinning Animations](../../../lessons/gpu/32-skinning-animations/)
for the full walkthrough.

## Key API calls

| Function | Purpose |
|----------|---------|
| `quat_slerp(a, b, t)` | Interpolate between keyframe rotations |
| `quat_to_mat4(q)` | Convert quaternion rotation to 4x4 matrix |
| `mat4_multiply(a, b)` | Compose joint matrix = world * inverseBindMatrix |
| `vec3_lerp(a, b, t)` | Interpolate translation and scale keyframes |
| `forge_gltf_load()` | Load glTF with skins, joints, and animation data |

## Skinned vertex layout

```c
typedef struct SkinVertex {
    vec3   position;    /* 12 bytes — FLOAT3   */
    vec3   normal;      /* 12 bytes — FLOAT3   */
    vec2   uv;          /*  8 bytes — FLOAT2   */
    Uint16 joints[4];   /*  8 bytes — USHORT4  */
    float  weights[4];  /* 16 bytes — FLOAT4   */
} SkinVertex;           /* 56 bytes total      */
```

Pipeline vertex attributes:

- Location 0: `FLOAT3` at `offsetof(SkinVertex, position)`
- Location 1: `FLOAT3` at `offsetof(SkinVertex, normal)`
- Location 2: `FLOAT2` at `offsetof(SkinVertex, uv)`
- Location 3: `USHORT4` at `offsetof(SkinVertex, joints)`
- Location 4: `FLOAT4` at `offsetof(SkinVertex, weights)`

## Joint matrix computation (per frame)

```c
/* Full glTF formula: inverse(meshWorld) * jointWorld * inverseBindMatrix.
 * The inverse mesh world accounts for parent nodes above the mesh
 * (e.g. Z_UP, Armature rotations in CesiumMan). */
mat4 inv_mesh_world = mat4_inverse(mesh_node->world_transform);
for (int i = 0; i < skin->joint_count; i++) {
    int node = skin->joints[i];
    joint_matrices[i] = mat4_multiply(
        inv_mesh_world,
        mat4_multiply(scene.nodes[node].world_transform,
                      skin->inverse_bind_matrices[i]));
}
```

## Shader pattern (vertex)

The vertex shader receives two uniform buffer slots:

- Slot 0: scene uniforms (MVP, model, light_vp)
- Slot 1: joint matrices array

```hlsl
cbuffer JointUniforms : register(b1, space1)
{
    column_major float4x4 joint_mats[MAX_JOINTS];
};

/* Compute skin matrix from 4 weighted joints */
float4x4 skin_mat = input.weights.x * joint_mats[input.joints.x]
                   + input.weights.y * joint_mats[input.joints.y]
                   + input.weights.z * joint_mats[input.joints.z]
                   + input.weights.w * joint_mats[input.joints.w];

float4 world = mul(skin_mat, float4(input.pos, 1.0));
```

## Animation evaluation

Parse animation channels from glTF JSON and evaluate each frame:

1. Binary search for the keyframe interval bracketing current time
2. Compute interpolation factor alpha in [0, 1]
3. Translation/scale: `vec3_lerp(a, b, alpha)`
4. Rotation: `quat_slerp(qa, qb, alpha)` (glTF stores [x,y,z,w])
5. Rebuild hierarchy: `parent_world * T * R * S` for each node
6. Compute joint matrices and push to GPU uniform slot 1

## Pipeline setup

```c
/* Skinned vertex shader needs 2 uniform buffers. */
SDL_GPUShaderCreateInfo info;
info.num_uniform_buffers = 2;  /* slot 0: scene, slot 1: joints */

/* Push both uniform slots per draw call. */
SDL_PushGPUVertexUniformData(cmd, 0, &scene_uniforms, sizeof(scene_uniforms));
SDL_PushGPUVertexUniformData(cmd, 1, &joint_uniforms, sizeof(joint_uniforms));
```

## Shadow pass

Use a separate `shadow_skin.vert.hlsl` that applies the same skin matrix
before projecting into light clip space. Both passes share the same joint
uniform data — no extra CPU work for skinned shadows.

## Common mistakes

- **glTF quaternion order** — glTF stores quaternions as `[x, y, z, w]`, but
  the forge math library uses `quat_create(w, x, y, z)` with `w` first.
  Swapping the order silently produces incorrect rotations.
- **Missing inverse mesh world** — The simplified formula
  `world * inverseBindMatrix` only works when the mesh node sits at the scene
  root. The full glTF formula is
  `inverse(meshWorld) * jointWorld * inverseBindMatrix`. CesiumMan has `Z_UP`
  and `Armature` parent nodes, so skipping the inverse mesh world produces
  distorted results.
- **Forgetting shadow skinning** — The shadow pass must apply the same skin
  matrix in its vertex shader. Without this, the shadow stays in the bind pose
  while the mesh animates.
- **Joint count mismatch** — The `MAX_JOINTS` constant in the shader must match
  the C-side `JointUniforms` struct. A mismatch silently corrupts uniform data.
- **Weight normalization** — glTF requires weights to sum to 1.0, but some
  exporters produce slightly denormalized weights. The linear blend still works
  acceptably, but be aware of this if you see subtle artifacts.

## glTF skin data

The `forge_gltf.h` parser provides:

- `ForgeGltfSkin`: joints array, inverse bind matrices, skeleton root
- `ForgeGltfPrimitive`: joint_indices (USHORT4), weights (FLOAT4)
- `ForgeGltfNode`: skin_index for nodes referencing a skin

Load with `forge_gltf_load()` — skins are parsed automatically.
