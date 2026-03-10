# Asset Lesson 08 — Animations

## Type

Hybrid — extends `common/gltf/forge_gltf.h` (parser) with animation data
structures and parsing, adds `common/gltf/forge_gltf_anim.h` for runtime
evaluation, creates a C animation extraction tool (`tools/anim/`), adds a
Python animation plugin (`pipeline/plugins/animation.py`), and extends the
pipeline registry to support multiple plugins per file extension.

## What it teaches

- How glTF stores animations: the `animations[]` → `channels[]` →
  `samplers[]` → `accessors[]` → binary buffer chain
- Channel targets: translation (vec3), rotation (quat), scale (vec3) on
  specific nodes via the `target.path` string
- Sampler interpolation modes: STEP and LINEAR (with SLERP for rotations)
- Binary search keyframe evaluation: finding the interval, computing the
  interpolation factor, handling boundary conditions
- How animation clips group channels with a shared timeline and duration
- glTF quaternion component order (`[x, y, z, w]`) vs the math library's
  `quat_create(w, x, y, z)`
- Applying evaluated channels back to node TRS, then recomputing world
  transforms through the hierarchy

## Motivation

GPU Lessons 31 and 32 both need animation data from glTF but the parser
(`forge_gltf.h`) does not parse the `"animations"` array. Each lesson works
around this differently:

- **L31 (Transform Animations)** hardcodes byte offsets, keyframe counts, and
  channel targets specific to CesiumMilkTruck. Constants like
  `ANIM_TIMESTAMP_OFFSET`, `ANIM_ROTATION0_OFFSET`, and
  `ANIM_KEYFRAME_COUNT 31` reach directly into the raw binary buffer.

- **L32 (Skinning Animations)** re-opens the glTF JSON file, re-parses it with
  cJSON, and walks the full `channels → samplers → accessors → bufferViews →
  buffers` chain to resolve data pointers. This is a fully generic animation
  parser (~120 lines) but it lives in the lesson's `main.c`, duplicating work
  the shared parser should do.

Both lessons also define their own `AnimChannel`, `AnimClip`, and evaluation
functions (`find_keyframe`, `evaluate_vec3_channel`, `evaluate_quat_channel`).

This lesson lifts all of that into the shared library so any lesson or project
can load and evaluate glTF animations with one call.

## Current state

### forge_gltf.h (`common/gltf/forge_gltf.h`)

Has:

- `ForgeGltfNode` with decomposed TRS fields (`translation`, `rotation`,
  `scale_xyz`, `has_trs`) — ready for animation to write into
- `ForgeGltfSkin` with joints and inverse bind matrices
- `ForgeGltfBuffer` with loaded binary data — animation keyframes live here
- Full accessor/bufferView resolution for vertices, indices, and skin data

Missing:

- No `ForgeGltfAnimation`, `ForgeGltfAnimChannel`, or `ForgeGltfAnimSampler`
  types
- No parsing of the `"animations"` JSON array
- No `animation_count` in `ForgeGltfScene`

### GPU Lesson 31 (`lessons/gpu/31-transform-animations/main.c`)

- Hardcoded constants: `ANIM_TIMESTAMP_OFFSET 144976`,
  `ANIM_ROTATION0_OFFSET 145100`, `ANIM_ROTATION1_OFFSET 145596`,
  `ANIM_KEYFRAME_COUNT 31`, `ANIM_DURATION 3.708f`
- Local types: `AnimChannel`, `AnimClip`, `AnimState`
- `parse_truck_animation()` — reads raw binary at known offsets
- `evaluate_rotation_channel()` — binary search + slerp

### GPU Lesson 32 (`lessons/gpu/32-skinning-animations/main.c`)

- Local types: `AnimChannel`, `AnimClip` (same layout as L31)
- `parse_animation()` — re-opens glTF JSON, walks the full accessor chain,
  resolves data pointers into `scene->buffers[]`. Handles translation,
  rotation, and scale paths. ~120 lines of generic parsing.
- `find_keyframe()` — binary search
- `evaluate_vec3_channel()` — lerp for translation/scale
- `evaluate_quat_channel()` — slerp for rotation, handles glTF `[x,y,z,w]`
  order

## Design decisions

1. **Parsing happens inside `forge_gltf_load()`.** The JSON root is already
   available during load. L32's approach of re-opening the file is unnecessary
   overhead. The binary buffers are already loaded in `scene->buffers[]`, so
   data pointers can be resolved directly.

2. **Data pointers, not copies.** `ForgeGltfAnimSampler` stores `const float *`
   pointers into the loaded binary buffer data, matching L31 and L32's approach.
   The data is valid for the lifetime of the `ForgeGltfScene`. No allocation or
   copying needed.

3. **Separate evaluation header.** `forge_gltf_anim.h` contains runtime
   evaluation functions (binary search, lerp, slerp, apply). This keeps the
   parser header focused on data structures and I/O, and lets callers who only
   need the raw data skip the evaluation code.

4. **Named enums for paths and interpolation.** `ForgeGltfAnimPath`
   (TRANSLATION=0, ROTATION=1, SCALE=2) and `ForgeGltfInterpolation`
   (LINEAR=0, STEP=1) replace the bare integers L31/L32 use. The values match
   L31/L32's conventions so existing code can adopt them incrementally.

5. **No CUBICSPLINE.** The glTF spec defines CUBICSPLINE interpolation with
   in/out tangents per keyframe. Neither L31 nor L32 uses it, and it is rare in
   practice. The parser logs a warning and skips the sampler entirely.
   Can be added later without changing the struct layout.

6. **No morph targets (weights).** The `"weights"` animation path targets blend
   shape weights, which require mesh morph target support. The parser skips
   channels with `target.path == "weights"`. Can be added when morph targets
   are implemented.

7. **All animations parsed, not just the first.** L32 only parses
   `animations[0]`. The parser loads all animations in the file. Models like
   character rigs often have multiple clips (idle, walk, run).

## API design

### New constants

```c
#define FORGE_GLTF_MAX_ANIMATIONS     16
#define FORGE_GLTF_MAX_ANIM_CHANNELS  128
#define FORGE_GLTF_MAX_ANIM_SAMPLERS  128
```

### New types in `forge_gltf.h`

```c
/* Animation channel target path — which TRS component to animate. */
typedef enum ForgeGltfAnimPath {
    FORGE_GLTF_ANIM_TRANSLATION = 0,
    FORGE_GLTF_ANIM_ROTATION    = 1,
    FORGE_GLTF_ANIM_SCALE       = 2
} ForgeGltfAnimPath;

/* Sampler interpolation mode. */
typedef enum ForgeGltfInterpolation {
    FORGE_GLTF_INTERP_LINEAR = 0,  /* lerp for vec3, slerp for quat */
    FORGE_GLTF_INTERP_STEP   = 1   /* hold previous value until next keyframe */
} ForgeGltfInterpolation;

/* An animation sampler: keyframe timestamps paired with output values.
 * Pointers reference data inside scene->buffers[] (not owned). */
typedef struct ForgeGltfAnimSampler {
    const float           *timestamps;     /* keyframe_count floats */
    const float           *values;         /* keyframe_count × N floats */
    int                    keyframe_count;
    int                    value_components; /* 3 for T/S, 4 for R */
    ForgeGltfInterpolation interpolation;
} ForgeGltfAnimSampler;

/* An animation channel: binds a sampler to a node property. */
typedef struct ForgeGltfAnimChannel {
    int               target_node;   /* index into scene->nodes[] */
    ForgeGltfAnimPath target_path;   /* which TRS component */
    int               sampler_index; /* index into parent animation's samplers[] */
} ForgeGltfAnimChannel;

/* A named animation clip containing samplers and channels. */
typedef struct ForgeGltfAnimation {
    char                  name[FORGE_GLTF_NAME_SIZE];
    float                 duration;  /* max timestamp across all samplers */
    ForgeGltfAnimSampler  samplers[FORGE_GLTF_MAX_ANIM_SAMPLERS];
    int                   sampler_count;
    ForgeGltfAnimChannel  channels[FORGE_GLTF_MAX_ANIM_CHANNELS];
    int                   channel_count;
} ForgeGltfAnimation;
```

### Extended `ForgeGltfScene`

```c
typedef struct ForgeGltfScene {
    /* ... existing fields ... */

    ForgeGltfAnimation animations[FORGE_GLTF_MAX_ANIMATIONS];
    int                animation_count;
} ForgeGltfScene;
```

### New functions in `forge_gltf_anim.h`

```c
/* Find the keyframe interval containing time t.
 * Returns index lo such that timestamps[lo] <= t < timestamps[lo+1]. */
static int forge_gltf_anim_find_keyframe(
    const float *timestamps, int count, float t);

/* Evaluate a vec3 sampler (translation or scale) at time t.
 * Returns interpolated value. Clamps at boundaries. */
static vec3 forge_gltf_anim_eval_vec3(
    const ForgeGltfAnimSampler *sampler, float t);

/* Evaluate a quaternion sampler (rotation) at time t.
 * Returns normalized interpolated quaternion. Handles glTF [x,y,z,w]
 * to forge_math quat(w,x,y,z) conversion. Clamps at boundaries. */
static quat forge_gltf_anim_eval_quat(
    const ForgeGltfAnimSampler *sampler, float t);

/* Apply all channels of an animation to scene nodes at time t.
 * If loop is true, wraps t to [0, duration). Otherwise clamps.
 * Writes to node->translation, rotation, scale_xyz.
 * Recomputes node->local_transform from the updated TRS.
 * Caller must call forge_gltf_compute_world_transforms() afterward
 * to propagate changes through the hierarchy. */
static void forge_gltf_anim_apply(
    const ForgeGltfAnimation *anim,
    ForgeGltfNode *nodes, int node_count,
    float t, bool loop);
```

## Implementation phases

### Phase 1: Add animation types to `forge_gltf.h`

**File:** `common/gltf/forge_gltf.h`

1. Add constants: `FORGE_GLTF_MAX_ANIMATIONS`, `MAX_ANIM_CHANNELS`,
   `MAX_ANIM_SAMPLERS`
2. Add enums: `ForgeGltfAnimPath`, `ForgeGltfInterpolation`
3. Add structs: `ForgeGltfAnimSampler`, `ForgeGltfAnimChannel`,
   `ForgeGltfAnimation`
4. Add to `ForgeGltfScene`: `animations[]` + `animation_count`

Estimated: ~50 lines added (types only, no logic).

### Phase 2: Parse animations in `forge_gltf_load()`

**File:** `common/gltf/forge_gltf.h`

Add a `forge_gltf__parse_animations()` internal function, called from
`forge_gltf_load()` while the cJSON root is still available. Logic adapted
from L32's `parse_animation()`:

1. Get `"animations"` array from JSON root
2. For each animation (up to `FORGE_GLTF_MAX_ANIMATIONS`):
   a. Parse name
   b. Parse `"samplers"` array — for each sampler:
      - Resolve `input` accessor → bufferView → buffer to get timestamp pointer
      - Resolve `output` accessor → bufferView → buffer to get value pointer
      - Read `count` from input accessor for `keyframe_count`
      - Read `interpolation` string (default "LINEAR")
      - Determine `value_components` from output accessor `type`
        (VEC3=3, VEC4=4, SCALAR=1)
      - Validate buffer bounds
   c. Parse `"channels"` array — for each channel:
      - Read `target.node` and `target.path`
      - Map path string to `ForgeGltfAnimPath` enum
      - Store `sampler` index
      - Skip `"weights"` path with no error
   d. Compute `duration` as max timestamp across all samplers

Estimated: ~120 lines (mirrors L32's parse_animation, adapted to use the
already-parsed cJSON root and existing accessor resolution helpers).

### Phase 3: Create `forge_gltf_anim.h`

**File:** `common/gltf/forge_gltf_anim.h` (new, header-only)

Functions extracted from L32's evaluation code:

1. `forge_gltf_anim_find_keyframe()` — binary search, from L32 line 874
2. `forge_gltf_anim_eval_vec3()` — lerp with clamping, from L32 line 892
3. `forge_gltf_anim_eval_quat()` — slerp with glTF→forge_math quaternion
   conversion, from L32 line 918
4. `forge_gltf_anim_apply()` — new function that iterates channels, evaluates
   each via the appropriate eval function, writes results to node TRS, and
   recomputes local_transform from TRS

Estimated: ~120 lines.

### Phase 4: Tests

**File:** `tests/gltf/test_gltf.c`

#### 4a. Animation parsing tests (using synthetic glTF)

Write a minimal glTF JSON + binary with one animation:

- 1 sampler: 3 keyframes, LINEAR, translation
- 1 channel: targets node 0, path "translation"

Tests:

- `test_anim_parse_count` — `scene.animation_count == 1`
- `test_anim_parse_name` — animation name matches
- `test_anim_parse_sampler_count` — 1 sampler
- `test_anim_parse_channel_count` — 1 channel
- `test_anim_parse_keyframe_count` — 3 keyframes
- `test_anim_parse_channel_target` — node 0, TRANSLATION
- `test_anim_parse_timestamps` — values match written data
- `test_anim_parse_values` — values match written data
- `test_anim_parse_duration` — equals last timestamp
- `test_anim_parse_interpolation` — LINEAR

#### 4b. Animation parsing tests (using CesiumMilkTruck)

If the model is available at the L31 assets path:

- `test_truck_anim_count` — 1 animation
- `test_truck_anim_name` — "Wheels"
- `test_truck_anim_channels` — 2 channels
- `test_truck_anim_targets` — both target rotation
- `test_truck_anim_keyframes` — 31 keyframes each
- `test_truck_anim_duration` — approximately 3.708s

#### 4c. Evaluation tests (synthetic data)

Create known keyframe data in memory and test evaluation:

- `test_eval_vec3_at_start` — returns first value
- `test_eval_vec3_at_end` — returns last value
- `test_eval_vec3_midpoint` — correct lerp at t=0.5 between two keyframes
- `test_eval_quat_at_start` — returns first quaternion
- `test_eval_quat_at_end` — returns last quaternion
- `test_eval_quat_midpoint` — correct slerp, result is normalized
- `test_eval_quat_component_order` — glTF [x,y,z,w] → forge_math [w,x,y,z]
- `test_eval_step_interpolation` — holds previous value until next keyframe

#### 4d. Apply tests

- `test_apply_translation` — node translation updated correctly
- `test_apply_rotation` — node rotation updated correctly
- `test_apply_scale` — node scale updated correctly
- `test_apply_loop_wraps` — time > duration wraps to beginning
- `test_apply_clamp` — time > duration clamps to last keyframe

Estimated: ~250 lines added to test_gltf.c.

### Phase 5: Lesson README

**File:** `lessons/assets/08-animations/README.md`

1. What you will learn
2. How glTF stores animations — the data chain from JSON to binary
3. Sampler interpolation — STEP vs LINEAR, slerp for rotations
4. Extending the glTF parser — code walkthrough of new types and parsing
5. Runtime evaluation — binary search, interpolation, boundary handling
6. Applying animations — writing TRS, recomputing transforms
7. Quaternion component order — glTF vs forge_math conventions
8. How L31 and L32 used to do it (before vs after comparison)
9. Exercises

### Phase 6: Diagrams

**File:** `scripts/forge_diagrams/assets/lesson_08.py`

1. glTF animation data chain: `animation → channel → sampler → accessor →
   bufferView → buffer → float[]`
2. Keyframe interpolation: timeline with keyframes, binary search interval,
   lerp/slerp between A and B
3. Channel-to-node mapping: animation channels fanning out to different nodes
   in the scene hierarchy

### Phase 7: Update lesson index

**File:** `lessons/assets/README.md` — add lesson 08 row.

## Implementation order

1. Phase 1 (types) — no dependencies
2. Phase 2 (parsing) — depends on Phase 1
3. Phase 3 (evaluation header) — depends on Phase 1 types
4. Phase 4 (tests) — validates Phases 1-3
5. Phase 5 (README)
6. Phase 6 (diagrams)
7. Phase 7 (lesson index)

Phases 1-3 can be done in a single pass since they are all modifications to
headers in `common/gltf/`.

## Risk areas

1. **Accessor resolution.** The parser already resolves accessors for vertices,
   indices, and skin data. Animation uses the same accessor → bufferView →
   buffer chain. The implementation should reuse or mirror the existing
   resolution pattern in `forge_gltf.h` rather than introducing a new one.

2. **Buffer bounds validation.** L32 validates that resolved offsets plus data
   size do not exceed the buffer. The parser must do the same. Corrupt or
   truncated files must not cause out-of-bounds reads.

3. **Quaternion component order.** glTF stores quaternions as `[x, y, z, w]`.
   The math library's `quat_create()` takes `(w, x, y, z)`. The evaluation
   function must handle this conversion. L32 does it correctly (line 938) — the
   same pattern applies.

4. **Multiple animations.** L32 only parses the first animation. The parser
   loads all animations, which means the sampler/channel arrays must be per-
   animation (not global). The fixed-size arrays (`MAX_ANIM_SAMPLERS`,
   `MAX_ANIM_CHANNELS`) are per-animation, which keeps the design simple but
   means models with many channels per animation may hit limits. 128 channels
   covers CesiumMan (57 channels) with headroom.

## Files created

- `lessons/assets/08-animations/PLAN.md` (this file)
- `lessons/assets/08-animations/README.md`
- `common/gltf/forge_gltf_anim.h` — runtime evaluation (binary search, lerp, slerp, apply)
- `tools/anim/main.c` — C animation extraction tool
- `tools/anim/CMakeLists.txt` — build configuration
- `pipeline/plugins/animation.py` — animation pipeline plugin
- `tests/pipeline/test_animation.py` — 16 animation plugin tests
- `scripts/forge_diagrams/assets/lesson_08.py`

## Files modified

- `common/gltf/forge_gltf.h` — animation types + parsing
- `tests/gltf/test_gltf.c` — 16 animation parsing and evaluation tests (45 total)
- `pipeline/plugin.py` — multi-plugin-per-extension registry
- `pipeline/__main__.py` — `--plugin` filter, per-plugin cache keys
- `tests/pipeline/test_plugin.py` — updated for list-based registry
- `lessons/assets/README.md` — add lesson 08 row
- `CMakeLists.txt` (root) — add `tools/anim` subdirectory

## Pipeline implementation

### Phase 8: Multi-plugin-per-extension registry

**File:** `pipeline/plugin.py`

Changed `_ext_map` from `dict[str, AssetPlugin]` to
`dict[str, list[AssetPlugin]]`. `get_by_extension()` returns a list of
plugins. This allows both mesh and animation plugins to handle `.gltf`/`.glb`.

### Phase 9: Per-plugin fingerprint caching

**File:** `pipeline/__main__.py`

Cache keys changed from `"relative_path"` to `"relative_path:plugin_name"`.
Added `--plugin` CLI flag to run only a specific plugin. Updated processing
loop to iterate all plugins per extension.

### Phase 10: Animation plugin

**File:** `pipeline/plugins/animation.py`

Subprocess plugin that invokes `forge-anim-tool`. Same pattern as the mesh
plugin: find tool via `shutil.which()` or `tool_path` setting, build args,
capture output, read `.meta.json` sidecar. Graceful fallback when tool not
installed.

### Phase 11: C animation tool

**Files:** `tools/anim/main.c`, `tools/anim/CMakeLists.txt`

CLI tool: `forge-anim-tool <input.gltf> <output.fanim> [--verbose]`

1. Load glTF via `forge_gltf_load()`
2. Read `scene.animations[]`
3. Write `.fanim` binary (FANM header, per-clip samplers and channels)
4. Write `.meta.json` sidecar with clip metadata

### Phase 12: Pipeline tests

**File:** `tests/pipeline/test_animation.py` (16 tests)

Covers plugin registration, tool invocation, custom path, tool-not-installed
warning, tool failure, timeout, OSError, missing output, metadata recording,
output extension, output dir creation, shared extension with mesh plugin.

**File:** `tests/pipeline/test_plugin.py` (updated)

Updated for list-based `get_by_extension()`, added multi-plugin-per-extension
and defensive copy tests.

## Not in this PR

- No morph target / blend shape animation (`"weights"` path)
- No CUBICSPLINE interpolation (parser warns and skips the sampler)
- No changes to L31 or L32 source — those are separate follow-up PRs that
  replace local animation code with the shared library
