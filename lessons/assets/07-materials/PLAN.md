# Asset Lesson 07 — Materials

## Type

Type B + C — extends `common/gltf/forge_gltf.h` (parser),
`tools/mesh/main.c` (mesh tool), `common/pipeline/forge_pipeline.h` (runtime
loader), and `pipeline/plugins/mesh.py` (Python plugin). Produces a lesson
README and tests.

## What it teaches

- The full glTF PBR metallic-roughness material model: what each parameter
  controls and how the data flows from source file to GPU
- How materials map to submeshes (one material per primitive, multiple
  primitives per mesh)
- Binary format versioning (.fmesh v2 replaces v1)
- The .fmesh v2 format: submesh index ranges embedded in the LOD table
- The .fmat JSON sidecar format for material properties and texture references
- Multi-primitive mesh processing: concatenating vertex/index buffers across
  primitives, preserving submesh boundaries, and per-submesh LOD simplification
- Runtime loading of materials and submeshes for per-material draw calls

## Design decisions

1. **Sidecar .fmat JSON** for material data, not embedded in .fmesh binary.
   Materials are descriptive metadata (colors, texture paths, blend modes) that
   benefit from human-readable format and independent versioning.
2. **Submesh table in .fmesh v2** header. Submesh index ranges are geometry
   data tightly coupled to the vertex/index buffers, so they belong in the
   binary.
3. **LODs apply to the whole mesh.** Each LOD stores a complete set of submesh
   index ranges. At runtime, pick one LOD and get all submeshes at that detail
   level.
4. **Full glTF PBR material granularity**: baseColorFactor/Texture,
   metallicFactor, roughnessFactor, metallicRoughnessTexture,
   normalTexture+scale, occlusionTexture+strength, emissiveTexture+factor,
   alphaMode, alphaCutoff, doubleSided.
5. **v2-only** — the pipeline reprocesses from source, so v1 backward
   compatibility is unnecessary. The loader rejects v1 files.
6. **Per-submesh LOD simplification** — each submesh is simplified
   independently at each LOD ratio, since submeshes have independent topology
   and may use different materials (e.g., opaque vs. transparent).

## Current state

### ForgeGltfMaterial (`common/gltf/forge_gltf.h`)

Has: `base_color[4]`, `texture_path` + `has_texture`, `name`, `alpha_mode`,
`alpha_cutoff`, `double_sided`, `normal_map_path` + `has_normal_map`.

Missing: `metallic_factor`, `roughness_factor`,
`metallic_roughness_texture_path`, `normal_scale`,
`occlusion_texture_path` + `occlusion_strength`,
`emissive_texture_path` + `emissive_factor[3]`.

### Mesh tool (`tools/mesh/main.c`, ~1100 lines)

- Processes only the first primitive; warns about extras
- Writes .fmesh v1: 32-byte header, LOD table (12 bytes/entry), vertices,
  indices
- Writes .meta.json sidecar with processing stats
- No material awareness
- 8 bytes reserved in header at offset 24

### forge_pipeline.h (`common/pipeline/forge_pipeline.h`, ~810 lines)

- `ForgePipelineMesh`: vertices, indices, vertex_count, vertex_stride, lods,
  lod_count, flags
- `ForgePipelineLod`: index_count, index_offset, target_error
- Validates `version == 1` strictly — must accept 1 or 2
- No submesh or material types

### Pipeline mesh plugin (`pipeline/plugins/mesh.py`, ~226 lines)

- Invokes `forge-mesh-tool` as subprocess, reads `.meta.json` output
- No awareness of `.fmat` sidecar

## Binary format: .fmesh v2

```text
Header (32 bytes):
Offset  Size   Field
------  ----   -----
  0       4    Magic "FMSH"
  4       4    Version (uint32 LE, value = 2)
  8       4    vertex_count (uint32 LE)
 12       4    vertex_stride (uint32 LE, 32 or 48)
 16       4    lod_count (uint32 LE)
 20       4    flags (uint32 LE, bit 0 = tangents)
 24       4    submesh_count (uint32 LE)   — NEW (was first 4 of reserved)
 28       4    reserved (zero-filled)      — shrunk from 8 to 4 bytes

LOD-Submesh table:
  For each LOD (lod_count entries):
    target_error (float as uint32 LE)                    — 4 bytes
    For each submesh (submesh_count entries):             — 12 bytes each
      index_count  (uint32 LE)
      index_offset (uint32 LE, byte offset into index section)
      material_index (int32 LE, -1 = none)

  Total LOD table size: lod_count × (4 + submesh_count × 12) bytes

Vertex data: vertex_count × vertex_stride bytes
Index data: all indices concatenated (uint32 LE)
```

### v1 files

The loader rejects v1 files. The pipeline reprocesses from source glTF/OBJ
files, so backward compatibility is unnecessary — rerunning the mesh tool
produces v2 output.

## .fmat sidecar format

```json
{
  "version": 1,
  "materials": [
    {
      "name": "MaterialName",
      "base_color_factor": [1.0, 1.0, 1.0, 1.0],
      "base_color_texture": "model_baseColor.png",
      "metallic_factor": 1.0,
      "roughness_factor": 1.0,
      "metallic_roughness_texture": "model_metallicRoughness.png",
      "normal_texture": "model_normal.png",
      "normal_scale": 1.0,
      "occlusion_texture": "model_occlusion.png",
      "occlusion_strength": 1.0,
      "emissive_factor": [0.0, 0.0, 0.0],
      "emissive_texture": null,
      "alpha_mode": "OPAQUE",
      "alpha_cutoff": 0.5,
      "double_sided": false
    }
  ]
}
```

Texture paths are relative to the .fmat file location. `null` or absent means
no texture for that slot.

## API design

### Extended ForgeGltfMaterial

```c
typedef struct ForgeGltfMaterial {
    /* Existing fields */
    float base_color[4];
    char  texture_path[FORGE_GLTF_PATH_SIZE];
    bool  has_texture;
    char  name[FORGE_GLTF_NAME_SIZE];
    ForgeGltfAlphaMode alpha_mode;
    float              alpha_cutoff;
    bool               double_sided;
    char  normal_map_path[FORGE_GLTF_PATH_SIZE];
    bool  has_normal_map;

    /* NEW: Normal texture scale */
    float normal_scale;                                    /* default 1.0 */

    /* NEW: PBR metallic-roughness */
    float metallic_factor;                                 /* default 1.0 */
    float roughness_factor;                                /* default 1.0 */
    char  metallic_roughness_path[FORGE_GLTF_PATH_SIZE];
    bool  has_metallic_roughness;

    /* NEW: Occlusion */
    char  occlusion_path[FORGE_GLTF_PATH_SIZE];
    bool  has_occlusion;
    float occlusion_strength;                              /* default 1.0 */

    /* NEW: Emissive */
    float emissive_factor[3];                              /* default (0,0,0) */
    char  emissive_path[FORGE_GLTF_PATH_SIZE];
    bool  has_emissive;
} ForgeGltfMaterial;
```

### New pipeline types

```c
typedef struct ForgePipelineSubmesh {
    uint32_t index_count;     /* indices for this submesh within its LOD */
    uint32_t index_offset;    /* byte offset into the mesh's index section */
    int32_t  material_index;  /* index into materials array, -1 = none */
} ForgePipelineSubmesh;

typedef struct ForgePipelineMaterial {
    char  name[64];
    float base_color_factor[4];
    char  base_color_texture[512];         /* relative path, empty = none */
    float metallic_factor;
    float roughness_factor;
    char  metallic_roughness_texture[512];
    char  normal_texture[512];
    float normal_scale;
    char  occlusion_texture[512];
    float occlusion_strength;
    float emissive_factor[3];
    char  emissive_texture[512];
    int   alpha_mode;                      /* 0=OPAQUE, 1=MASK, 2=BLEND */
    float alpha_cutoff;
    bool  double_sided;
} ForgePipelineMaterial;

typedef struct ForgePipelineMaterialSet {
    ForgePipelineMaterial *materials;
    uint32_t               material_count;
} ForgePipelineMaterialSet;
```

### Extended ForgePipelineMesh

```c
typedef struct ForgePipelineMesh {
    void             *vertices;
    uint32_t         *indices;
    uint32_t          vertex_count;
    uint32_t          vertex_stride;
    ForgePipelineLod *lods;            /* target_error per LOD */
    uint32_t          lod_count;
    uint32_t          flags;
    /* v2 additions: */
    uint32_t              submesh_count; /* 1 for v1, N for v2 */
    ForgePipelineSubmesh *submeshes;     /* flat: lod_count × submesh_count */
} ForgePipelineMesh;
```

Flat indexing: submesh `s` of LOD `l` is at
`submeshes[l * submesh_count + s]`.

### New functions

```c
bool forge_pipeline_load_materials(const char *path,
                                    ForgePipelineMaterialSet *set);
void forge_pipeline_free_materials(ForgePipelineMaterialSet *set);

uint32_t forge_pipeline_submesh_count(const ForgePipelineMesh *mesh);
const ForgePipelineSubmesh *forge_pipeline_lod_submesh(
    const ForgePipelineMesh *mesh, uint32_t lod, uint32_t submesh);
```

## Implementation phases

### Phase 1: Extend ForgeGltfMaterial (glTF parser)

**File:** `common/gltf/forge_gltf.h`

1. Add new fields to `ForgeGltfMaterial`
2. Set defaults in `forge_gltf__parse_materials()`:
   `normal_scale=1`, `metallic_factor=1`, `roughness_factor=1`,
   `occlusion_strength=1`, `emissive_factor={0,0,0}`
3. Parse `metallicFactor`, `roughnessFactor` from `pbrMetallicRoughness`
4. Parse `metallicRoughnessTexture` using existing texture→image→URI chain
5. Parse `normalTexture.scale`
6. Parse `occlusionTexture` + `.strength`
7. Parse `emissiveTexture` + `emissiveFactor`

Estimated: ~80 lines added.

### Phase 2: Extend mesh tool

**File:** `tools/mesh/main.c`

#### 2a. Multi-primitive loading

Rewrite `load_gltf()` to iterate all primitives:

1. For each primitive: extract vertices into shared `MeshVertex` array
2. Offset indices by cumulative vertex count
3. Record submesh boundaries: `{index_count, index_offset, material_index}`

#### 2b. Per-submesh optimization and LOD

1. Dedup the merged vertex buffer
2. Per-submesh: vertex cache + overdraw optimization
3. Full buffer: vertex fetch optimization (reorders vertices, remaps indices)
4. Generate tangents on the full buffer
5. Per-submesh LOD simplification at each ratio

#### 2c. Write .fmesh v2

New header with `submesh_count`, LOD-submesh table layout.

#### 2d. Write .fmat sidecar

JSON with material array from the parsed glTF scene.

#### main.c Decomposition (chunked-write)

**Chunk A (~400 lines):** Includes, types, MikkTSpace callbacks, arg parsing,
helpers.

**Chunk B (~500 lines):** `load_obj()`, `load_gltf()` (multi-primitive).

**Chunk C (~400 lines):** `process_mesh()` (per-submesh opt + LOD),
`write_fmesh()` (v2).

**Chunk D (~200 lines):** `write_fmat()`, `write_meta_json()`, `main()`.

### Phase 3: Extend forge_pipeline.h

**File:** `common/pipeline/forge_pipeline.h`

1. Add `ForgePipelineSubmesh`, `ForgePipelineMaterial`,
   `ForgePipelineMaterialSet` types
2. Extend `ForgePipelineMesh` with `submesh_count` and `submeshes`
3. Update `forge_pipeline_load_mesh()` to accept version 1 or 2
4. Add `forge_pipeline_load_materials()` (cJSON-based .fmat loader)
5. Add submesh accessor functions
6. Update `forge_pipeline_free_mesh()` to free submeshes

Estimated: ~200 lines added.

### Phase 4: Update Python mesh plugin

**File:** `pipeline/plugins/mesh.py`

- Check for `.fmat` sidecar after mesh tool runs
- Add `material_file` to result metadata

### Phase 5: Tests

**5a. glTF parser tests** (`tests/gltf/test_gltf.c`):

- `test_material_metallic_roughness` — factors parse correctly
- `test_material_metallic_roughness_texture` — texture path resolved
- `test_material_normal_scale` — normalTexture.scale
- `test_material_occlusion` — occlusion texture + strength
- `test_material_emissive` — emissive texture + factor
- `test_material_defaults` — verify spec defaults for all new fields

**5b. Pipeline loader tests** (`tests/pipeline/test_pipeline.c`):

- `test_load_fmesh_v2` — 2 submeshes, verify submesh data
- `test_load_fmesh_v1_compat` — v1 file → submesh_count=1, material=-1
- `test_load_fmesh_v2_multi_lod` — 3 LODs × 2 submeshes
- `test_submesh_accessors` — submesh_count, lod_submesh
- `test_load_materials` — full .fmat, verify all fields
- `test_load_materials_defaults` — missing optional fields
- `test_free_materials_null` — no crash

**5c. Python tests** (`tests/pipeline/test_mesh.py`):

- `test_fmat_sidecar_in_metadata`

### Phase 6: Lesson README

**File:** `lessons/assets/07-materials/README.md`

1. What you will learn
2. The glTF PBR material model — each parameter explained
3. Extending the glTF parser — code walkthrough
4. Multi-primitive mesh processing — concatenation and submesh boundaries
5. The .fmesh v2 format — binary layout, backward compatibility
6. The .fmat sidecar — JSON schema, texture path resolution
7. Per-submesh LOD simplification
8. Runtime loading — forge_pipeline.h extensions
9. Exercises

### Phase 7: Diagrams

**File:** `scripts/forge_diagrams/assets/lesson_07.py`

1. Material data flow: glTF → parser → mesh tool → .fmesh v2 + .fmat → loader
   → GPU
2. .fmesh v2 binary layout: header, LOD-submesh table, vertex data, index data
3. Multi-primitive concatenation: N primitives → one vertex buffer with submesh
   index ranges

### Phase 8: Update lesson index

**File:** `lessons/assets/README.md` — add lesson 07 row.

## Implementation order

1. Phase 1 (glTF parser) — no dependencies
2. Phase 5a (glTF tests) — validates Phase 1
3. Phase 2 (mesh tool) — depends on Phase 1
4. Phase 3 (pipeline loader) — depends on Phase 2 for v2 format
5. Phase 4 (Python plugin) — depends on Phase 2
6. Phase 5b, 5c (remaining tests) — validates Phases 3-4
7. Phase 6 (README)
8. Phase 7 (diagrams)
9. Phase 8 (lesson index)

## Risk areas

1. **Mesh tool size.** At ~1500 lines, `tools/mesh/main.c` requires the
   chunked-write pattern. The 4-chunk decomposition keeps each part within
   limits.

2. **Per-submesh optimization.** Vertex cache and overdraw optimization must
   run per-submesh (not merged) because each submesh is a separate draw call.
   Vertex fetch optimization runs on the full merged buffer afterward since all
   submeshes share one vertex buffer.

3. **Index offset calculation.** The index_offset for each submesh in each LOD
   is a running total across all preceding entries. Off-by-one errors here
   corrupt rendering. Tests must verify offsets with multi-LOD multi-submesh
   fixtures.

4. **Backward compatibility.** The loader must handle v1 files without submesh
   data. Synthesized single-submesh behavior must be identical to the current
   v1 loader.

## Files to create

- `lessons/assets/07-materials/PLAN.md` (this file)
- `lessons/assets/07-materials/README.md`
- `scripts/forge_diagrams/assets/lesson_07.py`

## Files to modify

- `common/gltf/forge_gltf.h` — extend ForgeGltfMaterial + parser
- `tools/mesh/main.c` — multi-primitive, .fmesh v2, .fmat writer
- `common/pipeline/forge_pipeline.h` — submesh/material types + loaders
- `pipeline/plugins/mesh.py` — .fmat awareness
- `tests/gltf/test_gltf.c` — new material field tests
- `tests/pipeline/test_pipeline.c` — v2 + .fmat loading tests
- `tests/pipeline/test_mesh.py` — .fmat sidecar test
- `lessons/assets/README.md` — add lesson 07 row

## Not in this PR

- `lessons/gpu/39-pipeline-processed-assets/` — reworked separately to use
  .fmat and per-submesh drawing
