# Asset Lesson 07 — Materials

Extends the asset pipeline with full PBR material support. The glTF parser
captures all metallic-roughness material fields, the mesh tool processes
multi-primitive meshes and writes material sidecars, and the runtime loader
provides per-submesh draw data with material references.

## What you'll learn

- The glTF PBR metallic-roughness material model and what each parameter
  controls
- How materials map to submeshes — one material per glTF primitive, multiple
  primitives per mesh
- The `.fmesh` v2 binary format with submesh index ranges in the LOD table
- The `.fmat` JSON sidecar format for material properties and texture
  references
- Multi-primitive mesh processing: concatenating vertex/index buffers,
  preserving submesh boundaries, per-submesh LOD simplification
- Runtime loading of materials and submeshes for per-material draw calls

## Result

After completing this lesson:

- `common/gltf/forge_gltf.h` captures all PBR material fields from glTF files
- `tools/mesh/main.c` processes every primitive in a glTF mesh and writes
  `.fmesh` v2 + `.fmat` sidecars
- `common/pipeline/forge_pipeline.h` loads submeshes and materials at runtime
- `pipeline/plugins/mesh.py` reports material sidecars in pipeline metadata
- 29 glTF parser tests and 45 pipeline loader tests validate the implementation

## The glTF PBR material model

glTF 2.0 defines materials using the metallic-roughness PBR model. Each
material has scalar factors, texture maps, and rendering options:

| Field | Type | Default | Purpose |
|---|---|---|---|
| `baseColorFactor` | `float[4]` | `[1,1,1,1]` | Base color multiplier (RGBA) |
| `baseColorTexture` | texture | — | Albedo map (sRGB) |
| `metallicFactor` | `float` | `1.0` | How metallic the surface is (0–1) |
| `roughnessFactor` | `float` | `1.0` | Surface roughness (0=mirror, 1=diffuse) |
| `metallicRoughnessTexture` | texture | — | Blue=metallic, green=roughness |
| `normalTexture` | texture | — | Tangent-space normal map |
| `normalTexture.scale` | `float` | `1.0` | Normal map intensity multiplier |
| `occlusionTexture` | texture | — | Ambient occlusion (red channel) |
| `occlusionTexture.strength` | `float` | `1.0` | AO intensity multiplier |
| `emissiveTexture` | texture | — | Emission color map (sRGB) |
| `emissiveFactor` | `float[3]` | `[0,0,0]` | Emission color multiplier |
| `alphaMode` | enum | `OPAQUE` | `OPAQUE`, `MASK`, or `BLEND` |
| `alphaCutoff` | `float` | `0.5` | Discard threshold for `MASK` mode |
| `doubleSided` | `bool` | `false` | Disable back-face culling |

### Metallic-roughness packed texture

The metallic-roughness texture packs two channels into one image. The blue
channel stores metallic and the green channel stores roughness. The red channel
is often used for ambient occlusion (the "ORM" packing convention:
Occlusion-Roughness-Metallic), though the glTF spec defines occlusion as a
separate texture reference that may share the same image file.

### Materials and primitives

A glTF mesh contains one or more **primitives**, each with its own vertex
data, index buffer, and material reference. A mesh with three materials (body,
wheels, glass) has three primitives. The mesh tool concatenates all primitives
into one vertex buffer and one index buffer, tracking the boundaries as
**submeshes**. At draw time, each submesh is rendered with a separate draw call
using its assigned material.

## Extending the glTF parser

`ForgeGltfMaterial` in `common/gltf/forge_gltf.h` gains these fields:

```c
/* Normal texture scale */
float normal_scale;                                    /* default 1.0 */

/* PBR metallic-roughness */
float metallic_factor;                                 /* default 1.0 */
float roughness_factor;                                /* default 1.0 */
char  metallic_roughness_path[FORGE_GLTF_PATH_SIZE];
bool  has_metallic_roughness;

/* Occlusion */
char  occlusion_path[FORGE_GLTF_PATH_SIZE];
bool  has_occlusion;
float occlusion_strength;                              /* default 1.0 */

/* Emissive */
float emissive_factor[3];                              /* default (0,0,0) */
char  emissive_path[FORGE_GLTF_PATH_SIZE];
bool  has_emissive;
```

The parser resolves each texture through the glTF indirection chain:
material → textureInfo → texture → image → URI. A shared helper
`forge_gltf__resolve_texture()` eliminates the 20-line nested lookup that
previously repeated for each texture type.

An important fix: the previous parser wrapped all material parsing inside
`if (pbr) { ... }`, which skipped `normalTexture`, `occlusionTexture`, and
`emissiveTexture` when `pbrMetallicRoughness` was absent. These are
material-level properties defined outside the PBR block and must always be
parsed.

## Multi-primitive mesh processing

The mesh tool (`tools/mesh/main.c`) processes all primitives in a glTF mesh:

1. **Concatenation** — vertices from each primitive are appended to a shared
   buffer. Indices are offset by the cumulative vertex count so they reference
   the correct vertices in the merged buffer.

2. **Submesh tracking** — each primitive becomes a submesh entry recording its
   index count, byte offset into the index section, and material index.

3. **Per-submesh optimization** — vertex cache and overdraw optimization run
   per-submesh because each submesh is a separate draw call with independent
   triangle ordering. Vertex fetch optimization runs on the full merged buffer
   afterward since all submeshes share one vertex buffer.

4. **Per-submesh LOD simplification** — each submesh is simplified
   independently at each LOD ratio. Submeshes have independent topology and
   may use different materials (opaque geometry can simplify aggressively while
   transparent geometry needs more care).

5. **Tangent generation** — MikkTSpace runs on the full merged buffer after
   optimization. Tangent frames are consistent across submesh boundaries.

## The `.fmesh` v2 format

The v2 format adds a `submesh_count` field to the header and replaces the flat
LOD table with a LOD-submesh table:

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
 24       4    submesh_count (uint32 LE)
 28       4    reserved (zero)

LOD-submesh table:
  For each LOD (lod_count entries):
    target_error (float as uint32 LE)              — 4 bytes
    For each submesh (submesh_count entries):       — 12 bytes each
      index_count  (uint32 LE)
      index_offset (uint32 LE, byte offset into index section)
      material_index (int32 LE, -1 = no material)

  Total table size: lod_count × (4 + submesh_count × 12) bytes

Vertex data: vertex_count × vertex_stride bytes
Index data: all indices concatenated (uint32 LE)
```

The `submesh_count` field occupies the first 4 bytes of what was reserved
padding in v1, shrinking the reserved area from 8 to 4 bytes. The version
field changes from 1 to 2.

## The `.fmat` sidecar format

Material properties are stored in a JSON sidecar file alongside the `.fmesh`
binary. The sidecar approach keeps material metadata human-readable and
independently versioned from the binary geometry format.

```json
{
  "version": 1,
  "materials": [
    {
      "name": "BottleMat",
      "base_color_factor": [1.0, 1.0, 1.0, 1.0],
      "base_color_texture": "WaterBottle_baseColor.png",
      "metallic_factor": 1.0,
      "roughness_factor": 1.0,
      "metallic_roughness_texture": "WaterBottle_occlusionRoughnessMetallic.png",
      "normal_texture": "WaterBottle_normal.png",
      "normal_scale": 1.0,
      "occlusion_texture": "WaterBottle_occlusionRoughnessMetallic.png",
      "occlusion_strength": 1.0,
      "emissive_factor": [1.0, 1.0, 1.0],
      "emissive_texture": "WaterBottle_emissive.png",
      "alpha_mode": "OPAQUE",
      "alpha_cutoff": 0.5,
      "double_sided": false
    }
  ]
}
```

Texture paths are filenames relative to the `.fmat` file location. A `null`
value means no texture for that slot — the renderer uses the scalar factor
alone. The material array is indexed by `material_index` from the `.fmesh`
submesh table.

## Runtime loading

`forge_pipeline.h` adds submesh and material types:

```c
typedef struct ForgePipelineSubmesh {
    uint32_t index_count;     /* indices for this submesh */
    uint32_t index_offset;    /* byte offset into the index section */
    int32_t  material_index;  /* index into ForgePipelineMaterialSet, -1 = none */
} ForgePipelineSubmesh;

typedef struct ForgePipelineMaterial {
    char  name[64];
    float base_color_factor[4];
    char  base_color_texture[512];
    float metallic_factor;
    float roughness_factor;
    char  metallic_roughness_texture[512];
    char  normal_texture[512];
    float normal_scale;
    char  occlusion_texture[512];
    float occlusion_strength;
    float emissive_factor[3];
    char  emissive_texture[512];
    int   alpha_mode;     /* 0=OPAQUE, 1=MASK, 2=BLEND */
    float alpha_cutoff;
    bool  double_sided;
} ForgePipelineMaterial;
```

`ForgePipelineMesh` gains a flat submesh array indexed as
`submeshes[lod * submesh_count + submesh_idx]`:

```c
/* New fields in ForgePipelineMesh */
ForgePipelineSubmesh *submeshes;     /* lod_count × submesh_count entries */
uint32_t              submesh_count; /* number of submeshes (primitives) */
```

New functions:

```c
/* Load materials from a .fmat JSON sidecar */
bool forge_pipeline_load_materials(const char *path,
                                    ForgePipelineMaterialSet *set);
void forge_pipeline_free_materials(ForgePipelineMaterialSet *set);

/* Submesh accessors */
uint32_t forge_pipeline_submesh_count(const ForgePipelineMesh *mesh);
const ForgePipelineSubmesh *forge_pipeline_lod_submesh(
    const ForgePipelineMesh *mesh, uint32_t lod, uint32_t submesh_idx);
```

### Per-submesh draw loop

With submeshes and materials loaded, the rendering loop draws each submesh
with its own material:

```c
ForgePipelineMesh mesh;
ForgePipelineMaterialSet materials;
forge_pipeline_load_mesh("model.fmesh", &mesh);
forge_pipeline_load_materials("model.fmat", &materials);

uint32_t lod = 0;  /* pick a LOD level */
for (uint32_t s = 0; s < mesh.submesh_count; s++) {
    const ForgePipelineSubmesh *sub =
        forge_pipeline_lod_submesh(&mesh, lod, s);

    /* Bind the material's textures (bounds-check against material count
     * in case .fmesh and .fmat are mismatched) */
    if (sub->material_index >= 0
        && (uint32_t)sub->material_index < materials.material_count) {
        ForgePipelineMaterial *mat =
            &materials.materials[sub->material_index];
        /* bind mat->base_color_texture, normal_texture, etc. */
    }

    /* Draw this submesh's index range */
    uint32_t first_index = sub->index_offset / sizeof(uint32_t);
    SDL_DrawGPUIndexedPrimitives(pass, sub->index_count, 1,
                                  first_index, 0, 0);
}
```

## Pipeline integration

The Python mesh plugin (`pipeline/plugins/mesh.py`) detects the `.fmat`
sidecar written by the mesh tool and includes it in the pipeline result
metadata:

```python
metadata["material_file"] = "model.fmat"
metadata["material_count"] = 3
```

This allows downstream pipeline steps (bundling, dependency tracking) to know
which materials a mesh references and which texture files those materials
depend on.

## Where it connects

- [Asset Lesson 03](../03-mesh-processing/) — the mesh tool this lesson
  extends
- [Asset Lesson 06](../06-loading-processed-assets/) — the runtime loader
  this lesson extends
- [GPU Lesson 36](../../gpu/36-scene-loading/) — glTF scene loading with
  materials
- [GPU Lesson 39](../../gpu/39-pipeline-processed-assets/) — renders
  pipeline-processed assets (will use .fmat in a future update)

## Key concepts

- **PBR metallic-roughness material model** — the standard glTF 2.0 material
  representation with base color, metallic, roughness, normal, occlusion, and
  emissive properties
- **Submesh-to-material mapping** — one material per glTF primitive, with
  multiple primitives concatenated into a single vertex/index buffer
- **`.fmesh` v2 binary format** — extends v1 with a LOD-submesh table that
  stores per-submesh index ranges and material indices at each LOD level
- **`.fmat` JSON sidecar** — human-readable material properties and texture
  references stored alongside the binary geometry
- **Multi-primitive mesh processing** — concatenating vertex/index buffers
  across primitives while preserving submesh boundaries for per-submesh LOD
  simplification
- **Runtime material and submesh loading API** — `forge_pipeline_load_materials()`
  and per-submesh accessors for issuing per-material draw calls

## Building

### Compile the mesh tool

```bash
cmake -B build
cmake --build build --config Debug --target forge_mesh_tool
```

### Run tests

```bash
ctest --test-dir build -R gltf -C Debug
ctest --test-dir build -R pipeline -C Debug
```

## Exercises

1. **Alpha mask rendering.** Load a model with `alphaMode: "MASK"` and
   implement the alpha cutoff test in the fragment shader:
   `if (color.a < alpha_cutoff) discard;`

2. **Emissive glow.** Use the emissive factor and texture to add a bloom
   contribution. Models like WaterBottle have emissive maps that should glow
   without external light.

3. **Double-sided materials.** When `double_sided` is true, skip back-face
   culling for that submesh's draw call. This requires a separate pipeline
   state object with `cull_mode = SDL_GPU_CULLMODE_NONE`.

4. **Material sorting.** Sort submeshes by alpha mode before drawing: opaque
   first, then alpha-mask, then alpha-blend (back-to-front). This is the
   standard approach for correct transparency rendering.

## AI skill

The [forge-pipeline-library](../../../.claude/skills/forge-pipeline-library/SKILL.md)
skill documents the full pipeline package API, including the `MeshPlugin` that
produces `.fmat` material sidecars alongside `.fmesh` files. It covers how
per-plugin settings in `pipeline.toml` control material extraction and how the
bundler tracks material dependencies.

## Further reading

- [glTF 2.0 Specification — Materials](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials)
- [glTF 2.0 Specification — Metallic-Roughness](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material)
- [Real-Time Rendering, Chapter 9 — Physically Based Shading](https://www.realtimerendering.com/)
