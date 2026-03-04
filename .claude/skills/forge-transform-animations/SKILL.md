---
name: forge-transform-animations
description: >
  Add keyframe animation with glTF loading, quaternion slerp, and path-following
  to an SDL GPU project.
---

Add transform animations to an SDL3 GPU scene using keyframe data loaded from
glTF files and procedural path-following. The technique evaluates keyframe
channels (binary search + interpolation), applies animated transforms to a node
hierarchy, and composes multiple animation layers (data-driven + procedural).
Use this skill when you need animated objects — spinning wheels, walking
characters, vehicles following paths, or any keyframe-driven motion.

See [GPU Lesson 31 — Transform Animations](../../../lessons/gpu/31-transform-animations/)
for the full walkthrough.

## Key API calls

| Function | Purpose |
|----------|---------|
| `quat_slerp(a, b, t)` | Spherical linear interpolation between two quaternions |
| `quat_to_mat4(q)` | Convert a quaternion to a 4x4 rotation matrix |
| `mat4_translate(v)` | Build a translation matrix from a vec3 |
| `mat4_scale(v)` | Build a scale matrix from a vec3 |
| `mat4_multiply(a, b)` | Compose two transforms: apply B first, then A |
| `forge_gltf_load()` | Load a glTF model with node hierarchy and buffer data |

## Animation data structures

```c
/* A channel targets one property of one node */
typedef struct ForgeAnimChannel {
    Uint32 target_node;    /* index into the node array */
    Uint32 target_path;    /* 0=translation, 1=rotation, 2=scale */
    Uint32 interpolation;  /* 0=STEP, 1=LINEAR, 2=CUBICSPLINE */
    Uint32 keyframe_count;
    float *timestamps;     /* sorted keyframe times */
    float *values;         /* keyframe values (vec3 or quat) */
} ForgeAnimChannel;

/* A clip is a collection of channels */
typedef struct ForgeAnimClip {
    char  name[64];
    float duration;
    Uint32 channel_count;
    ForgeAnimChannel channels[8];
} ForgeAnimClip;

/* Playback state for a clip instance */
typedef struct ForgeAnimState {
    ForgeAnimClip *clip;
    float current_time;
    float speed;
    bool  looping;
    bool  playing;
} ForgeAnimState;
```

## Keyframe evaluation — binary search + slerp

```c
static quat evaluate_rotation_channel(const ForgeAnimChannel *ch, float t)
{
    if (ch->keyframe_count == 0)
        return quat_identity();
    if (t <= ch->timestamps[0] || ch->keyframe_count == 1) {
        float *v = ch->values;
        return quat_create(v[3], v[0], v[1], v[2]);
    }
    Uint32 last = ch->keyframe_count - 1;
    if (t >= ch->timestamps[last]) {
        float *v = ch->values + last * 4;
        return quat_create(v[3], v[0], v[1], v[2]);
    }

    /* Binary search for bracketing keyframes */
    Uint32 lo = 0, hi = last;
    while (lo + 1 < hi) {
        Uint32 mid = (lo + hi) / 2;
        if (ch->timestamps[mid] <= t) lo = mid;
        else hi = mid;
    }

    float t0 = ch->timestamps[lo];
    float t1 = ch->timestamps[hi];
    float alpha = (t - t0) / (t1 - t0);

    /* Read quaternions (stored as x, y, z, w in glTF) */
    float *v0 = ch->values + lo * 4;
    float *v1 = ch->values + hi * 4;
    quat q0 = quat_create(v0[3], v0[0], v0[1], v0[2]);
    quat q1 = quat_create(v1[3], v1[0], v1[1], v1[2]);

    return quat_slerp(q0, q1, alpha);
}
```

## Transform hierarchy rebuild

```c
static void rebuild_node_transforms(ForgeGltfScene *scene,
                                     int node_idx, mat4 parent_world)
{
    ForgeGltfNode *node = &scene->nodes[node_idx];

    /* Recompute local transform from current T/R/S */
    if (node->has_trs) {
        mat4 T = mat4_translate(node->translation);
        mat4 R = quat_to_mat4(node->rotation);
        mat4 S = mat4_scale(node->scale_xyz);
        node->local_transform = mat4_multiply(T, mat4_multiply(R, S));
    }

    node->world_transform = mat4_multiply(parent_world, node->local_transform);

    for (int i = 0; i < node->child_count; i++)
        rebuild_node_transforms(scene, node->children[i], node->world_transform);
}
```

## Common mistakes

- **Not converting glTF quaternion order.** glTF stores quaternions as
  `[x, y, z, w]` but `quat_create()` takes `(w, x, y, z)`. Swap during
  loading or you get completely wrong rotations.

- **Forgetting to rebuild the node hierarchy after animation.** After setting
  animated T/R/S values, you must walk the tree to recompute world transforms.
  Without this, child nodes use stale parent transforms.

- **Using lerp for rotation interpolation.** Linear interpolation of
  quaternions produces non-uniform angular velocity and denormalized results.
  Always use `quat_slerp()` for rotation channels.

- **Not wrapping time for looping animations.** Use `fmodf(t, duration)` for
  looping clips. Without wrapping, the animation plays once and freezes.

- **Hardcoding buffer offsets.** Parse accessor and bufferView metadata from
  the glTF JSON. Different models have different buffer layouts.

- **Ignoring the Yup2Zup node.** Some models include coordinate system
  conversion nodes. Include them in the hierarchy walk or the model will be
  incorrectly oriented.

## Composing animation layers

The key insight: local transforms compose through the hierarchy. Apply
different animation sources at different levels:

```text
truck_placement (from path animation)
  └── Node 5 "Yup2Zup" (static rotation from glTF)
        └── Node 4 "Truck Body" (mesh)
              ├── Node 1 "Front Axle" (static translation)
              │     └── Node 0 "Wheels" (rotation from keyframes)
              └── Node 3 "Rear Axle" (static translation)
                    └── Node 2 "Wheels" (rotation from keyframes)
```

Each frame: evaluate wheel keyframes → set wheel node rotations →
rebuild hierarchy with path placement as root → render with final world
transforms.
