# Lesson 44 — Pipeline Morph Target Animations

## Overview

Morph target (blend shape) animations using pipeline-processed assets. Two
glTF sample models demonstrate CPU-evaluated morph weights driving GPU vertex
displacement via storage buffers:

- **AnimatedMorphCube** — 2 morph targets animating cube geometry
- **SimpleMorph** — 2 morph targets deforming simple geometry

Both models have `.fanim` files with `MORPH_WEIGHTS` channels (target_path 3).
The lesson evaluates morph weights on the CPU each frame, computes blended
position/normal/tangent deltas, and uploads them to a GPU storage buffer that
the vertex shader reads for per-vertex displacement.

## Architecture

### Morph blending approach

Two possible approaches:

1. **Per-target storage buffers + GPU blending** — upload raw deltas per target,
   blend in the vertex shader with `Σ(weight_i × delta_i)`. More GPU memory
   but simpler CPU work. Shader reads N storage buffers.

2. **CPU-blended single buffer** — blend on CPU each frame, upload one buffer
   of final deltas. Single storage buffer read in shader. More CPU work but
   simpler shader and fewer GPU resources.

**Decision: CPU-blended single buffer (approach 2).**

Rationale: matches the skinning pattern (CPU computes joint matrices, uploads
one buffer). Keeps the vertex shader simple — one `StructuredBuffer<float4>`
for position deltas, one for normal deltas (float4 for consistent 16-byte
stride across SPIRV and DXIL). No variable-count buffer binding.
With max 8 targets and typical mesh sizes (< 10K vertices), CPU blending is
negligible.

### Vertex format

Reuse the standard 48-byte model vertex layout (position, normal, uv, tangent).
Morph targets do NOT change the vertex layout — deltas are applied additively
from storage buffers indexed by `SV_VertexID`.

This means the morph vertex shader uses `scene_model` vertex attributes (48B)
plus storage buffer reads, NOT the 72-byte skinned layout.

### GPU resources per morph model

- `vertex_buffer` — standard 48-byte model vertices (base mesh)
- `index_buffer` — uint32 indices
- `morph_delta_buffer` — storage buffer: vertex_count × float4 (blended position deltas, 16-byte stride)
- `morph_normal_buffer` — storage buffer: vertex_count × float4 (blended normal deltas, 16-byte stride)
- `morph_transfer_buffer` — persistent transfer buffer for CPU→GPU upload

### Animation evaluation

`forge_pipeline_anim_apply()` skips `MORPH_WEIGHTS` channels — the lesson
handles them directly:

1. Find channels with `target_path == FORGE_PIPELINE_ANIM_MORPH_WEIGHTS`
2. Sample the keyframe data at current time (binary search + lerp)
3. Extract per-target weights (sampler `value_components` = target count)
4. CPU-blend: `blended_delta[v] = Σ(weight_i × morph_targets[i].position_deltas[v])`
5. Upload blended deltas to GPU storage buffer

### Shader design

New shaders: `scene_morph.vert.hlsl` and `scene_morph_shadow.vert.hlsl`

The morph vertex shader:

- Reads base position/normal/tangent from vertex attributes (same as scene_model)
- Reads blended position deltas from `StructuredBuffer<float4>` via `SV_VertexID` (`.xyz` access)
- Reads blended normal deltas from second `StructuredBuffer<float4>` (`.xyz` access)
- Applies: `final_pos = base_pos + position_delta`
- Applies: `final_nrm = normalize(base_nrm + normal_delta)`
- Then follows standard world-space transform (same as scene_model.vert)

Shadow variant only needs position deltas (normals irrelevant for depth-only).

### forge_scene.h extensions

New struct and functions:

```c
typedef struct ForgeSceneMorphModel {
    /* Pipeline data */
    ForgePipelineScene       scene_data;
    ForgePipelineMesh        mesh;
    ForgePipelineMaterialSet materials;
    ForgePipelineAnimFile    animations;

    /* GPU resources */
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUBuffer *morph_delta_buffer;    /* vertex_count × float4 position (16-byte stride) */
    SDL_GPUBuffer *morph_normal_buffer;   /* vertex_count × float4 normal   (16-byte stride) */
    SDL_GPUTransferBuffer *morph_transfer_buffer; /* upload staging */

    /* Per-material textures */
    ForgeSceneModelTextures mat_textures[FORGE_SCENE_MODEL_MAX_MATERIALS];
    uint32_t                mat_texture_count;

    /* CPU-side blended deltas (uploaded each frame) */
    float *blended_position_deltas;  /* vertex_count × 4 (16-byte stride) */
    float *blended_normal_deltas;    /* vertex_count × 4 (16-byte stride) */

    /* Morph weight state */
    float    morph_weights[FORGE_PIPELINE_MAX_MORPH_TARGETS];
    uint32_t morph_target_count;

    /* Animation state */
    float    anim_time;
    float    anim_speed;
    int      current_clip;
    bool     looping;

    /* Stats */
    uint32_t draw_calls;
    ForgeSceneVramStats vram;
} ForgeSceneMorphModel;
```

API:

```c
bool forge_scene_load_morph_model(ForgeScene *scene, ForgeSceneMorphModel *model,
    const char *fscene_path, const char *fmesh_path, const char *fmat_path,
    const char *fanim_path, const char *base_dir);

void forge_scene_update_morph_animation(ForgeScene *scene,
    ForgeSceneMorphModel *model, float dt);

void forge_scene_draw_morph_model(ForgeScene *scene,
    ForgeSceneMorphModel *model, mat4 placement);

void forge_scene_draw_morph_model_shadows(ForgeScene *scene,
    ForgeSceneMorphModel *model, mat4 placement);

void forge_scene_free_morph_model(ForgeScene *scene,
    ForgeSceneMorphModel *model);
```

### UI panel

- Per-model section: target names, current weights, animation time
- Per-target weight sliders (0.0–1.0) for manual override
- Animation speed slider
- Toggle: animated vs manual weights

## Dependencies

- **Asset Lesson 13** — morph data in `.fmesh` + `.fanim`
- **GPU Lesson 43** — skinned animation pattern (template)
- **forge_pipeline.h** — `ForgePipelineMorphTarget`, `forge_pipeline_has_morph_data()`
- **forge_scene.h** — base scene renderer, model loading pattern

## Assets needed

Download from glTF-Sample-Assets and process through the pipeline:

- `AnimatedMorphCube` — cube with 2 morph targets, animated weights
- `SimpleMorph` — triangle with 2 morph targets, animated weights

Process with:

```bash
uv run python -m pipeline process assets/AnimatedMorphCube/AnimatedMorphCube.gltf
uv run python -m pipeline process assets/SimpleMorph/SimpleMorph.gltf
```

Or use `forge-mesh-tool` and `forge-anim-tool` directly.

## main.c Decomposition

Estimated size: ~450-550 lines. Under the 800-line threshold but close enough
to merit a 2-chunk split if it grows.

### Chunk A — Header + init (~250 lines)

- File header comment, includes, constants
- `AppState` struct
- `asset_path()` helper
- `SDL_AppInit()`: scene init, load both morph models
- `SDL_AppEvent()`: delegate to forge_scene_handle_event

### Chunk B — Iterate + quit (~250 lines)

- Morph weight evaluation helper (samples `.fanim` morph channels)
- `SDL_AppIterate()`: update morph weights, blend deltas, upload,
  shadow pass, main pass, UI panel
- `SDL_AppQuit()`: free models, destroy scene

### Contract between chunks

- Chunk A defines `AppState` with fields both chunks reference
- Chunk B's morph evaluation reads `ForgePipelineAnimFile` from `AppState`
- No circular dependencies — concatenation order is A then B

## Key concepts taught

1. **Morph targets** — what they are, why they complement skeletal animation
2. **Delta encoding** — storing displacements vs absolute positions
3. **Weight blending** — `final = base + Σ(w_i × delta_i)` for position and normal
4. **Storage buffer displacement** — vertex shader reads per-vertex deltas via SV_VertexID
5. **Animation channel routing** — `target_path == 3` channels carry morph weights
6. **CPU vs GPU evaluation** — tradeoffs of blending on CPU vs GPU

## Files to create/modify

### New files

- `lessons/gpu/44-pipeline-morph-animations/main.c`
- `lessons/gpu/44-pipeline-morph-animations/CMakeLists.txt`
- `lessons/gpu/44-pipeline-morph-animations/README.md`
- `lessons/gpu/44-pipeline-morph-animations/assets/` (processed models)
- `common/scene/shaders/scene_morph.vert.hlsl`
- `common/scene/shaders/scene_morph_shadow.vert.hlsl`
- `.claude/skills/forge-pipeline-morph-animations/SKILL.md`

### Modified files

- `common/scene/forge_scene.h` — add `ForgeSceneMorphModel`, load/draw/free functions, morph pipelines
- `CMakeLists.txt` (root) — add `add_subdirectory(lessons/gpu/44-pipeline-morph-animations)`
- `README.md` (root) — add row to GPU lessons table
- `PLAN.md` (root) — check off Lesson 44
- `scripts/compile_shaders.py` or `scripts/compile_scene_shaders.py` — add morph shaders
