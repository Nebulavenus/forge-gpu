# Asset Lesson 12 — Skin Data and Skinned Vertices

Adds a `.fskin` binary format for joint hierarchies and inverse bind matrices,
and extends `.fmesh` to version 3 with per-vertex joint indices and blend
weights. After this lesson, the asset pipeline can export and load everything
needed for skeletal animation: the skeleton topology, bind-pose transforms,
and skinned vertex data.

## What you'll learn

- The `.fskin` binary format: header, per-skin name/joint-count/skeleton,
  joint node indices, and inverse bind matrices (column-major 4x4)
- Runtime `.fskin` loader: sequential binary parsing with truncation checks,
  per-skin heap allocation for joints and inverse bind matrices
- Skinned vertex layout: extending the `.fmesh` format with `JOINTS_0`
  (4 x uint16) and `WEIGHTS_0` (4 x float) per vertex
- Backward compatibility: `.fmesh` v3 coexists with v2 — the loader accepts
  both, validating skin flags against stride and version
- Tool integration: `forge-scene-tool --skins` exports `.fskin` alongside
  `.fscene`, and `forge-mesh-tool` detects skin data on glTF primitives to
  produce v3 meshes

## Result

After completing this lesson:

- `forge_pipeline.h` can load `.fskin` files with joint hierarchies and
  inverse bind matrices
- `.fmesh` v3 files carry per-vertex joint indices and blend weights
- `forge-scene-tool --skins` extracts skin data from glTF models
- `forge-mesh-tool` produces v3 meshes when glTF primitives have skin data
- The scene pipeline plugin passes `--skins` by default
- Tests validate loaders across valid files, error cases, backward
  compatibility, and free safety

Load skin data at runtime:

```c
ForgePipelineSkinSet skins;
if (forge_pipeline_load_skins("model.fskin", &skins)) {
    for (uint32_t i = 0; i < skins.skin_count; i++) {
        ForgePipelineSkin *skin = &skins.skins[i];
        /* skin->joints[0..joint_count-1] are node indices */
        /* skin->inverse_bind_matrices[0..joint_count-1] are 16-float matrices */
    }
    forge_pipeline_free_skins(&skins);
}
```

Check if a loaded mesh has skin data:

```c
ForgePipelineMesh mesh;
if (forge_pipeline_load_mesh("character.fmesh", &mesh)) {
    if (forge_pipeline_has_skin_data(&mesh)) {
        /* Vertex stride is 56 (no tangents) or 72 (with tangents) */
        /* Each vertex contains joints[4] + weights[4] after base attributes */
    }
    forge_pipeline_free_mesh(&mesh);
}
```

## Key concepts

### Joint hierarchies

A skin defines a set of joints (bones) stored as node indices into the
scene's node tree. Each joint inherits its parent's transform, and
world-space joint transforms are computed by traversing from root to
leaves. The `skeleton` field identifies the root joint node.

### Inverse bind matrices

Each joint has an inverse bind matrix that transforms vertex positions
from model space into the joint's local coordinate system at the bind
pose (rest pose). At runtime, multiply the inverse bind matrix by the
joint's current world transform to produce the skinning matrix that
moves vertices from bind pose into the animated pose.

### Per-vertex skinning data

Each vertex stores up to 4 joint indices (uint16) and 4 blend weights
(float). The final skinned position is a weighted sum of the vertex
transformed by each influencing joint's skinning matrix. Weights should
sum to 1.0 for correct results.

### Binary format versioning

`.fmesh` v3 extends v2 with optional skin data. The `FLAG_SKINNED` bit
(bit 1) signals that vertices include joint indices and blend weights.
The loader validates that `FLAG_SKINNED` matches the vertex stride
(56 or 72 bytes), ensuring v2 files continue to load without skin
attributes and v3 files carry the extended layout.

## Building

From the repository root:

```bash
cmake -B build -S .
cmake --build build --target forge_scene_tool forge_mesh_tool test_pipeline
```

Test skin extraction with a glTF model that has skeletal data:

```bash
# Extract scene hierarchy + skin data
./build/tools/scene/forge_scene_tool assets/models/CesiumMan/CesiumMan.gltf \
    /tmp/cesiumman.fscene --skins --verbose
# Produces: /tmp/cesiumman.fscene, /tmp/cesiumman.fskin

# Process mesh with skinned vertices (auto-detected from glTF)
./build/tools/mesh/forge_mesh_tool assets/models/CesiumMan/CesiumMan.gltf \
    /tmp/cesiumman.fmesh --verbose
# Produces: /tmp/cesiumman.fmesh (v3 with FLAG_SKINNED if skin data present)
```

Run the pipeline tests:

```bash
ctest --test-dir build -R pipeline
```

## `.fskin` binary format

All values are little-endian.

```text
Header (12 bytes):
  magic         4B   "FSKN"
  version       u32  1
  skin_count    u32

Per skin:
  name                    64B  (null-terminated, zero-padded)
  joint_count             u32
  skeleton                i32  (-1 if unset)
  joints[]                joint_count × i32 (node indices)
  inverse_bind_matrices[] joint_count × 16 floats (column-major 4x4)
```

## `.fmesh` v3 skinned vertex layout

Version 3 adds the `FLAG_SKINNED` bit (bit 1) and two new strides:

| Stride | Layout | Bytes |
|--------|--------|-------|
| 56 | pos(3f) + norm(3f) + uv(2f) + joints(4×u16) + weights(4f) | 56 |
| 72 | pos(3f) + norm(3f) + uv(2f) + tan(4f) + joints(4×u16) + weights(4f) | 72 |

Version 2 files continue to load unchanged — the loader accepts both v2
and v3, and validates that skinned flags are consistent with stride and
version.

## API reference

### Skin loader

```c
/* Load a .fskin file — allocates per-skin joints and inverse bind matrices */
bool forge_pipeline_load_skins(const char *path, ForgePipelineSkinSet *skins);

/* Free all skin data — NULL-safe, zeroes the struct */
void forge_pipeline_free_skins(ForgePipelineSkinSet *skins);

/* Check if a loaded mesh contains skin data */
bool forge_pipeline_has_skin_data(const ForgePipelineMesh *mesh);
```

### Types

```c
typedef struct ForgePipelineSkin {
    char     name[64];
    int32_t *joints;                /* joint_count node indices */
    float   *inverse_bind_matrices; /* joint_count × 16 floats */
    uint32_t joint_count;
    int32_t  skeleton;              /* root joint node, -1 if unset */
} ForgePipelineSkin;

typedef struct ForgePipelineSkinSet {
    ForgePipelineSkin *skins;
    uint32_t           skin_count;
} ForgePipelineSkinSet;
```

## Tool changes

### `forge-scene-tool --skins`

When `--skins` is passed, the scene tool writes a `.fskin` file alongside
the `.fscene` output. Skin data is serialized from the glTF parser's
`ForgeGltfSkin` structs, including joint node indices and inverse bind
matrices. The output path replaces `.fscene` with `.fskin`.

### `forge-mesh-tool` skinned vertex support

The mesh tool detects `has_skin_data` on glTF primitives. When all
primitives carry joint/weight data:

- Vertex deduplication and vertex-fetch optimization are disabled (skin
  attributes are stored in parallel arrays, not in the dedup comparison key)
- Vertex cache and overdraw optimization proceed as normal (index-only)
- Output version is bumped to 3 with `FLAG_SKINNED` set
- Joint indices (4 x uint16) and weights (4 x float) are interleaved after
  the base vertex attributes in the binary output

### Scene pipeline plugin

The scene plugin passes `--skins` by default. Disable with
`skins = false` in the `[scene]` section of `pipeline.toml`.

## Tests

28 tests covering the skin loader and skinned mesh loader:

| Group | Count | Coverage |
|-------|-------|----------|
| Skin valid loading | 5 | Single/multi skin, IBM roundtrip, joints, skeleton |
| Skin error cases | 10 | NULL args, bad magic/version, truncation, count limits |
| Skin free safety | 3 | NULL, zeroed, double-free |
| Skinned mesh loading | 8 | Stride 56/72, FLAG_SKINNED, v2 backward compat, accessor |
| Backward compat | 2 | v2 mesh still loads, v2 has no skin flag |

## Exercises

1. **Skin inspector**: Write a small program that loads a `.fskin` file and
   prints the joint hierarchy as an indented tree, using the node names from
   the corresponding `.fscene` file.

2. **Weight normalization**: Add a validation pass that checks whether blend
   weights sum to 1.0 for each vertex, logging any vertices with significant
   deviation.

3. **Joint limit enforcement**: Extend the mesh tool to detect when any
   vertex references more than 4 joints and reduce to the 4 highest-weight
   joints with re-normalization.

## Cross-references

- [Asset Lesson 08](../08-animations/) — glTF animation parsing and the
  `.fanim` binary format
- [Asset Lesson 09](../09-scene-hierarchy/) — scene hierarchy extraction
  and the `.fscene` format
- [Asset Lesson 10](../10-animation-loader/) — runtime `.fanim` loader
- [Asset Lesson 11](../11-animation-manifest/) — animation manifest and
  named clip lookup
- [GPU Lesson 32](../../gpu/32-skinning-animations/) — skeletal skinning
  on the GPU with joint hierarchies and blend weights
