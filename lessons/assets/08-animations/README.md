# Asset Lesson 08 — Animations

Extends the glTF parser with animation support and adds the animation
processing pipeline. After this lesson, `forge_gltf_load()` parses the
`"animations"` array into structured data, `forge_gltf_anim.h` provides
runtime evaluation, `forge-anim-tool` extracts animations into `.fanim`
binaries, and the pipeline's animation plugin processes glTF files alongside
the existing mesh plugin.

## What you'll learn

- How glTF stores animations: the chain from `animations[]` to binary
  keyframe data
- Channel targets: translation (vec3), rotation (quat), and scale (vec3) on
  specific nodes
- Sampler interpolation modes: LINEAR and STEP
- Binary search keyframe evaluation: finding the interval, computing the
  blend factor, handling boundaries
- Quaternion component order: glTF `[x, y, z, w]` vs forge_math
  `quat(w, x, y, z)`
- Applying evaluated channels back to node TRS and recomputing world
  transforms
- The `.fanim` binary format: how animation clips, samplers, and channels
  are serialized
- Multi-plugin-per-extension: how the pipeline handles two plugins (mesh and
  animation) for the same `.gltf` file
- Per-plugin fingerprint caching: independent change tracking for each
  processing step

## Result

After completing this lesson:

- `common/gltf/forge_gltf.h` parses all animations, samplers, and channels
  from glTF files
- `common/gltf/forge_gltf_anim.h` evaluates keyframes and applies results to
  node transforms
- `tools/anim/` provides `forge-anim-tool`, a C tool that writes `.fanim`
  binaries and `.meta.json` sidecars
- `pipeline/plugins/animation.py` invokes the tool as a subprocess, alongside
  the existing mesh plugin
- 45 glTF parser tests and 166 pipeline tests validate the full stack

Any lesson or project can load and play back glTF animations with:

```c
forge_gltf_anim_apply(&scene.animations[0],
                       scene.nodes, scene.node_count,
                       elapsed_time, true /* loop */);
for (int i = 0; i < scene.root_node_count; i++) {
    forge_gltf_compute_world_transforms(&scene,
                                        scene.root_nodes[i],
                                        &identity);
}
```

The pipeline processes animations independently from meshes:

```bash
forge-pipeline --plugin animation    # only re-extract animations
forge-pipeline                       # process everything (mesh + animation)
```

## Key concepts

- **glTF animation structure** — animations contain samplers (keyframe data)
  and channels (what to animate), resolved through the accessor chain
- **Channel targets** — translation (vec3), rotation (quat), and scale (vec3)
  on specific nodes; "weights" channels are skipped
- **Interpolation modes** — LINEAR (component-wise lerp for vec3, slerp for
  quaternions) and STEP (hold previous value); CUBICSPLINE is detected and
  skipped with a warning
- **Binary search keyframe evaluation** — find the bracketing interval in
  O(log n), compute the blend factor, handle boundary cases (before first
  keyframe, after last keyframe)
- **Quaternion component order** — glTF stores `[x, y, z, w]`; forge_math
  uses `quat(w, x, y, z)` — the conversion is critical for correct rotations
- **`.fanim` binary format** — compact sequential format storing clips,
  samplers, and channels for offline use
- **Multi-plugin-per-extension** — the pipeline registry maps `.gltf`/`.glb`
  to both mesh and animation plugins, each tracked independently
- **Per-plugin fingerprint caching** — cache keys include the plugin name so
  changes to one plugin don't invalidate the other's results

## How glTF stores animations

A glTF file's `"animations"` array contains named clips. Each clip has
**samplers** (the keyframe data) and **channels** (what to animate).

```text
animation
├── samplers[]
│   ├── input  → accessor → timestamps (float[])
│   ├── output → accessor → values (vec3[] or vec4[])
│   └── interpolation: "LINEAR", "STEP", or "CUBICSPLINE"
└── channels[]
    ├── sampler → index into samplers[]
    └── target
        ├── node → index into nodes[]
        └── path → "translation", "rotation", or "scale"
```

The data lives in the binary `.bin` buffer, accessed through the standard
glTF accessor chain: `accessor → bufferView → buffer`. The parser resolves
these references during `forge_gltf_load()` and stores `const float *`
pointers directly into the loaded buffer data. No copying is needed — the
pointers remain valid for the lifetime of the `ForgeGltfScene`.

### Samplers

A sampler pairs **input** timestamps with **output** values:

| Field | Type | Description |
|---|---|---|
| `input` | accessor index | Keyframe timestamps (SCALAR float) |
| `output` | accessor index | Keyframe values (VEC3 for T/S, VEC4 for R) |
| `interpolation` | string | `"LINEAR"` (default), `"STEP"`, or `"CUBICSPLINE"` |

Multiple channels can share the same sampler if they animate at the same
rate. CesiumMilkTruck's two wheel channels share one timestamp accessor.

### Channels

A channel binds a sampler to a node property:

| Field | Type | Description |
|---|---|---|
| `sampler` | index | Which sampler provides the keyframe data |
| `target.node` | index | Which node to animate |
| `target.path` | string | `"translation"`, `"rotation"`, or `"scale"` |

The `"weights"` path targets morph target blend shapes, which require mesh
morph target support. The parser skips these channels.

## Interpolation

### LINEAR

For translation and scale (vec3), LINEAR means component-wise lerp:

```text
result = a × (1 - α) + b × α
```

For rotation (quaternion), LINEAR means **spherical linear interpolation**
(slerp). Slerp follows the shortest arc on the unit quaternion hypersphere,
producing constant angular velocity — unlike lerp, which would distort
rotation speed.

### STEP

Hold the previous keyframe's value until the next keyframe is reached. No
blending. Useful for on/off switches, visibility toggles, or animation
events.

### CUBICSPLINE

Uses Hermite splines with in/out tangents per keyframe. The parser logs a
warning and skips CUBICSPLINE samplers — falling back to LINEAR would read
in-tangents as values due to the 3x data stride. This mode is rare in
practice.

## Extending the glTF parser

### New types

```c
typedef enum ForgeGltfAnimPath {
    FORGE_GLTF_ANIM_TRANSLATION = 0,
    FORGE_GLTF_ANIM_ROTATION    = 1,
    FORGE_GLTF_ANIM_SCALE       = 2
} ForgeGltfAnimPath;

typedef enum ForgeGltfInterpolation {
    FORGE_GLTF_INTERP_LINEAR = 0,
    FORGE_GLTF_INTERP_STEP   = 1
} ForgeGltfInterpolation;

typedef struct ForgeGltfAnimSampler {
    const float           *timestamps;      /* points into buffer data */
    const float           *values;          /* points into buffer data */
    int                    keyframe_count;
    int                    value_components; /* 3 for T/S, 4 for R */
    ForgeGltfInterpolation interpolation;
} ForgeGltfAnimSampler;

typedef struct ForgeGltfAnimChannel {
    int               target_node;
    ForgeGltfAnimPath target_path;
    int               sampler_index;
} ForgeGltfAnimChannel;

typedef struct ForgeGltfAnimation {
    char                  name[FORGE_GLTF_NAME_SIZE];
    float                 duration;
    ForgeGltfAnimSampler  samplers[FORGE_GLTF_MAX_ANIM_SAMPLERS];
    int                   sampler_count;
    ForgeGltfAnimChannel  channels[FORGE_GLTF_MAX_ANIM_CHANNELS];
    int                   channel_count;
} ForgeGltfAnimation;
```

`ForgeGltfScene` gains `animations[]` and `animation_count`.

### Parsing

The internal `forge_gltf__parse_animations()` runs during
`forge_gltf_load()` while the cJSON root is available. For each animation:

1. Parse each sampler's `input` and `output` accessors using the existing
   `forge_gltf__get_accessor()` helper — the same function that resolves
   vertex, index, and skin data
2. Map each channel's `target.path` string to the `ForgeGltfAnimPath` enum
3. Compute `duration` as the maximum timestamp across all samplers

The parser loads all animations in the file, not just the first. Character
models often contain multiple clips (idle, walk, run).

## Runtime evaluation

`forge_gltf_anim.h` provides three evaluation functions and one apply
function.

### Binary search

`forge_gltf_anim_find_keyframe()` finds the interval `[lo, lo+1]` containing
time `t` in O(log n). This is the same binary search pattern used in GPU
Lessons 31 and 32.

### Vec3 evaluation

`forge_gltf_anim_eval_vec3()` evaluates a translation or scale sampler:

1. Clamp `t` to `[timestamps[0], timestamps[count-1]]`
2. Binary search for the interval
3. Compute blend factor `α = (t - t0) / (t1 - t0)`
4. Lerp between the two bracketing values

### Quaternion evaluation

`forge_gltf_anim_eval_quat()` evaluates a rotation sampler:

1. Same clamping and binary search as vec3
2. Convert from glTF `[x, y, z, w]` to forge_math `quat(w, x, y, z)`
3. Slerp between the two bracketing quaternions

The component order conversion is critical. glTF stores quaternions as
`[x, y, z, w]` in the binary buffer, but `quat_create()` takes `(w, x, y, z)`.
Getting this wrong produces garbage rotations.

### Applying to nodes

`forge_gltf_anim_apply()` evaluates all channels and writes results to
node TRS fields:

```c
void forge_gltf_anim_apply(
    const ForgeGltfAnimation *anim,
    ForgeGltfNode *nodes, int node_count,
    float t, bool loop);
```

1. If `loop` is true, wrap `t` to `[0, duration)` via `fmodf`
2. For each channel, evaluate the sampler at time `t`
3. Write the result to the target node's `translation`, `rotation`, or
   `scale_xyz`
4. Rebuild `local_transform` from the updated TRS: `T × R × S`

After calling `forge_gltf_anim_apply()`, you must recompute world transforms
for every root node to propagate changes through the node hierarchy. The apply
function updates local transforms only:

```c
for (int i = 0; i < scene.root_node_count; i++) {
    forge_gltf_compute_world_transforms(&scene,
                                        scene.root_nodes[i],
                                        &identity);
}
```

## How L31 and L32 handled this before

### GPU Lesson 31 — hardcoded offsets

L31 hardcodes byte offsets into CesiumMilkTruck's binary buffer:

```c
#define ANIM_TIMESTAMP_OFFSET  144976
#define ANIM_ROTATION0_OFFSET  145100
#define ANIM_KEYFRAME_COUNT    31
```

This works for one specific model but cannot handle any other glTF file.

### GPU Lesson 32 — re-parses the JSON

L32 re-opens the `.gltf` file, re-parses it with cJSON, and walks the full
accessor chain. This is generic but duplicates work the shared parser should
do — and it reads the file twice (once for the parser, once for animations).

### After this lesson

Both approaches are replaced by:

```c
ForgeGltfScene scene;
forge_gltf_load("model.gltf", &scene);
/* scene.animations[] is populated — no extra parsing needed */
```

## The `.fanim` binary format

The C tool `forge-anim-tool` writes a compact binary that stores all clips
from a glTF file. The format is sequential and little-endian:

```text
Header (12 bytes):
  magic          4 bytes   "FANM"
  version        u32       1
  clip_count     u32       number of animation clips

Per clip:
  name           64 bytes  null-terminated, zero-padded
  duration       float     max timestamp across all samplers
  sampler_count  u32
  channel_count  u32

Per sampler (after the clip header):
  keyframe_count    u32
  value_components  u32    3 for translation/scale, 4 for rotation
  interpolation     u32    0 = LINEAR, 1 = STEP
  timestamps[]      float  keyframe_count floats
  values[]          float  keyframe_count × value_components floats

Per channel (after all samplers for this clip):
  target_node    i32       index into scene nodes
  target_path    u32       0 = translation, 1 = rotation, 2 = scale
  sampler_index  u32       index into this clip's sampler array
```

The tool also writes a `.meta.json` sidecar with clip names, durations,
channel counts, and total keyframes — used by the pipeline for logging and
metadata tracking.

### Usage

```bash
forge-anim-tool model.gltf output.fanim --verbose
```

The tool loads the glTF via `forge_gltf_load()`, reads `scene.animations[]`,
and writes the binary. If the file contains no animations, it writes a valid
header with `clip_count = 0`.

## Pipeline integration

### The animation plugin

`pipeline/plugins/animation.py` follows the same subprocess pattern as the
mesh plugin: it locates `forge-anim-tool` via `shutil.which()` or a
configured `tool_path`, invokes it on each `.gltf`/`.glb` file, and reads
the `.meta.json` sidecar. If the tool is not installed, it logs a warning
and returns the source file unchanged — the pipeline continues without
animation extraction.

### Multi-plugin-per-extension

Before this lesson, the plugin registry mapped each file extension to a
single plugin. Both the mesh and animation plugins handle `.gltf` and `.glb`,
so the registry now maps extensions to a **list** of plugins. When a glTF
file is processed, both plugins run:

```text
model.gltf → mesh plugin    → model.fmesh + model.fmat
           → animation plugin → model.fanim + model.meta.json
```

This means animators can iterate on animations without reprocessing meshes
and textures — a significant workflow improvement for projects with large
models.

### Per-plugin fingerprint caching

The fingerprint cache tracks each (file, plugin) pair independently. Cache
keys changed from `"models/bottle.gltf"` to `"models/bottle.gltf:mesh"` and
`"models/bottle.gltf:animation"`. Changing animation tool settings
reprocesses animations only; mesh results stay cached.

### The `--plugin` filter

The CLI gained a `--plugin` flag:

```bash
forge-pipeline --plugin animation    # only run animation extraction
forge-pipeline --plugin mesh         # only run mesh processing
forge-pipeline                       # run all plugins
```

This is useful during development when you only need to reprocess one asset
type.

## Building

### Compile the animation tool

```bash
cmake -B build
cmake --build build --config Debug --target forge_anim_tool
```

### Run tests

```bash
ctest --test-dir build -R gltf -C Debug        # C parser + evaluation tests
python -m pytest tests/pipeline/ -v             # pipeline plugin tests
```

### Process a glTF file

```bash
./build/tools/anim/forge_anim_tool model.gltf output.fanim --verbose
```

## Connection to other lessons

| Lesson | Relationship |
|---|---|
| [GPU 31 — Transform Animations](../../gpu/31-transform-animations/) | Uses the animation techniques this lesson generalizes |
| [GPU 32 — Skinning Animations](../../gpu/32-skinning-animations/) | Contains the generic parser this lesson lifts into the shared library |
| [Asset 01 — Pipeline Scaffold](../01-pipeline-scaffold/) | Plugin registry, CLI, and fingerprint cache extended here |
| [Asset 03 — Mesh Processing](../03-mesh-processing/) | Same C tool + Python plugin subprocess pattern |
| [Asset 07 — Materials](../07-materials/) | Same pattern: extending the glTF parser with new data types |
| [Math 08 — Orientation](../../math/08-orientation/) | Quaternion slerp and the forge_math quat type |

## Exercises

1. **Add CUBICSPLINE support.** The glTF spec defines cubic Hermite spline
   interpolation with in/out tangents per keyframe. Each output value is
   stored as three consecutive values: in-tangent, value, out-tangent.
   Implement the Hermite evaluation formula and handle the 3× data stride.

2. **Animation blending.** Evaluate two animations at the same time and blend
   their results. For vec3 properties, lerp between the two values. For
   quaternions, slerp. This is the foundation for crossfading between
   animation clips (idle → walk).

3. **Update GPU Lesson 31** to use `scene.animations[0]` and
   `forge_gltf_anim_apply()` instead of hardcoded offsets. Remove the local
   `AnimChannel`, `AnimClip`, and `AnimState` types.

4. **Update GPU Lesson 32** to use `scene.animations[0]` instead of the
   local `parse_animation()` function. The ~120 lines of JSON re-parsing
   become a single struct access.

## AI skill

The [forge-pipeline-library](../../../.claude/skills/forge-pipeline-library/SKILL.md)
skill documents the full pipeline package API, including the `AnimationPlugin`
that extracts glTF animation clips into `.fanim` binaries. It covers the
plugin's subprocess integration with `forge-anim-tool`, the shared extension
handling (`.gltf`/`.glb` files produce both mesh and animation outputs), and
the CLI commands for processing animated assets.

## Further reading

- [glTF 2.0 Specification — Animations](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations)
- [Quaternion slerp](https://en.wikipedia.org/wiki/Slerp)
- [Cubic Hermite spline](https://en.wikipedia.org/wiki/Cubic_Hermite_spline)
