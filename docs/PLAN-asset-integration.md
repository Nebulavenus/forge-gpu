# Asset Pipeline Integration Plan

## Goal

From GPU Lesson 39 onward, every asset — textures, meshes, procedural
geometry — enters through the asset pipeline. Textures are BC7/BC5 compressed,
tangents generated, meshes optimized. Skills enforce this for all future
lessons.

GPU Lessons 01–38 are not modified. They continue loading assets the way they
always have. Lesson 08 (Mesh Loading) gets a short forward reference hinting
that the asset pipeline track exists.

GPU Lesson 39 is the transition point: it teaches how to load pipeline-
processed assets (BC7/BC5 textures, `.fmesh` meshes) and from that point
forward every lesson uses the pipeline.

---

## Prerequisite: Asset Lesson 06 — Loading Processed Assets in C

Before GPU Lesson 39 can exist, readers need a lesson that teaches the C side:
how to read `.fmesh` files, how to load BC7/BC5 compressed textures, and how
the CMake build dependency works. This is the bridge between the Python
pipeline (Asset Lessons 01–05) and the GPU lessons that consume the results.

### Asset Lesson 06 — Loading Processed Assets

**What the reader learns:**

- The `.fmesh` binary format produced by `forge-mesh-tool` (header, vertex
  layout with tangents, index buffer, LOD table)
- Reading `.meta.json` sidecars to discover what the pipeline produced
- A header-only C loader: `common/pipeline/forge_pipeline.h`
- Loading BC7 compressed textures (albedo/diffuse — high-quality RGBA)
- Loading BC5 compressed textures (normal maps — two-channel RG, reconstruct
  Z in shader via `z = sqrt(1 - x² - y²)`)
- Mipmap loading from pipeline-generated mip chains
- CMake integration: the `forge-assets` target acquires processed assets
  (local → download release → run pipeline); GPU Lesson 39+ declares
  `add_dependencies(lesson_XX forge-assets)` so processed assets are always
  available at build time
- Integration test: load a processed CesiumMilkTruck, verify tangents exist,
  render it with normal mapping

**What it produces:**

- `common/pipeline/forge_pipeline.h` — header-only C library:
  - `forge_pipeline_load_mesh()` → reads `.fmesh` from `assets/processed/`
  - `forge_pipeline_load_texture()` → reads BC7/BC5 compressed textures
    from `assets/processed/`
  - `forge_pipeline_free_mesh()` / `forge_pipeline_free_texture()`
- Tests under `tests/pipeline/` verifying the C loader round-trips correctly
- A small SDL GPU demo that loads and renders a pipeline-processed model

**Depends on:** Asset Lessons 01–05, `forge-mesh-tool` in `tools/mesh/`

---

## Phase 0 — Pipeline config and .gitignore

### 0A: Root `pipeline.toml`

Create `pipeline.toml` pointing at the existing asset tree:

```toml
[pipeline]
source_dir = "assets"
output_dir = "assets/processed"
cache_dir  = ".forge-cache"

[texture]
max_size   = 2048
generate_mipmaps = true

[texture.compression]
albedo = "bc7"
normal = "bc5"

[mesh]
deduplicate       = true
optimize          = true
generate_tangents = true
lod_levels        = [1.0, 0.5, 0.25]

[bundle]
bundle_dir = "assets/bundles"
```

**Notes:**

- `source_dir = "assets"` (not `"assets/raw"`) because existing models already
  live under `assets/models/`, `assets/fonts/`, etc.
- Texture compression: BC7 for albedo/diffuse (high-quality RGBA), BC5 for
  normal maps (two-channel RG, reconstruct Z in shader). The texture plugin
  classifies maps by name convention (`*_Normal*` → BC5, everything else → BC7).
- LOD levels: full, half, quarter.

### 0B: `.gitignore` additions

```text
.forge-cache/
assets/processed/
assets/bundles/
```

### 0C: CMake `forge-assets` target

Add to root `CMakeLists.txt`. The target uses a three-tier strategy:

1. `assets/processed/` already exists locally → do nothing (already built)
2. Not found → download pre-built tarball from the latest GitHub release
3. Download fails (offline, no release yet) → run `uv run python -m pipeline` locally

```cmake
set(FORGE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets/processed")
set(FORGE_ASSETS_STAMP "${FORGE_ASSETS_DIR}/.stamp")
set(FORGE_RELEASE_TAG "assets-latest")

add_custom_command(
    OUTPUT ${FORGE_ASSETS_STAMP}
    COMMENT "Acquiring processed assets"
    COMMAND ${CMAKE_COMMAND} -E echo "Checking for processed assets..."
    COMMAND ${CMAKE_COMMAND}
        -DASSETS_DIR=${FORGE_ASSETS_DIR}
        -DRELEASE_TAG=${FORGE_RELEASE_TAG}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/AcquireAssets.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(forge-assets DEPENDS ${FORGE_ASSETS_STAMP})
```

The `cmake/AcquireAssets.cmake` script implements the three-tier logic:

- Checks if `assets/processed/` exists and is non-empty
- If missing, tries `gh release download ${RELEASE_TAG}` for the tarball
- If download fails, runs `uv run python -m pipeline --verbose`
- Writes a `.stamp` file so CMake knows assets are up to date

GPU Lesson 39+ declares a build dependency on this target:

```cmake
add_dependencies(lesson_39 forge-assets)
add_dependencies(lesson_40 forge-assets)
# ... all lessons 39+ that load assets
```

Building any lesson 39+ automatically acquires processed assets. Lessons 01–38
have no dependency on it.

For local development (modifying assets or the pipeline itself), run
`uv run python -m pipeline` directly — the existing `assets/processed/` directory
satisfies the CMake target and skips the download.

### 0D: Verify pipeline runs on existing assets

```bash
uv sync --extra dev
uv run python -m pipeline --verbose
cmake --build build --target forge-assets
```

Run the pipeline first to exercise real mesh conversion and texture
compression. Then build the CMake target to verify it detects the local
`assets/processed/` directory correctly. Fix any plugin issues.

---

## Phase 1 — Process existing assets

### 1A: Models through mesh plugin

| Model | Format |
|-------|--------|
| space-shuttle | OBJ + PNG |
| CesiumMilkTruck | glTF |
| Suzanne | glTF |
| BoxTextured | glTF |
| Duck | glTF |
| CesiumMan | glTF |
| Searchlight | glTF |

Each model gets: vertex dedup, index optimization (meshoptimizer), MikkTSpace
tangent generation, LOD levels. Output: `.fmesh` in `assets/processed/models/`.

### 1B: Textures through texture plugin

All PNG/JPG/TGA textures: BaseColor, normal maps, metallic-roughness maps,
skybox faces, ShuttleDiffuseMap.png.

Each texture gets: resize to max 2048, mipmap generation, GPU block
compression. Albedo/diffuse maps → BC7 (high-quality RGBA). Normal maps → BC5
(two-channel RG; reconstruct Z in shader via `z = sqrt(1 - x² - y²)`).
Classification by filename convention (`*_Normal*`, `*_normal*` → BC5,
everything else → BC7). Output: compressed textures in
`assets/processed/textures/` with `.meta.json` sidecars.

### 1C: Bundle creation

```bash
uv run python -m pipeline bundle --output assets/bundles/forge.forgepak
```

---

## Phase 2 — Asset Lesson 06 (Loading Processed Assets in C)

See the prerequisite section above. This lesson must land before GPU Lesson 39
can exist. It produces `common/pipeline/forge_pipeline.h` and teaches readers
how the C side works.

---

## Phase 3 — GPU Lesson 08 README hint

Lesson 08 (Mesh Loading) teaches OBJ format mechanics — that's its job and it
stays unchanged. But it's the first time a reader loads an external model, so
it's the right place to plant a seed.

Add a short section at the end of the README, after the exercises:

> **What's next: the asset pipeline**
>
> In this lesson you loaded a single OBJ model by hand — parsing vertices,
> normals, and UVs from a text file. That works for learning, but real projects
> have hundreds of models and textures that need deduplication, tangent
> generation, compression, and LOD creation before they're ready for the GPU.
>
> The [Asset Pipeline track](../../assets/) walks through building a
> production pipeline that processes raw assets into optimized formats.
> [GPU Lesson 39](../39-pipeline-processed-assets/) is where this course
> switches to pipeline-processed assets for everything going forward.

No code changes to lesson 08.

---

## Phase 4 — GPU Lesson 39: Pipeline-Processed Assets

This is the transition lesson. It teaches:

- Loading BC7 textures for albedo (high-quality RGBA block compression)
- Loading BC5 textures for normal maps (two-channel RG, reconstruct Z:
  `z = sqrt(saturate(1 - dot(n.xy, n.xy)))`)
- Loading optimized `.fmesh` files with pre-generated tangents and LOD levels
- Using `forge_pipeline_load_mesh()` and `forge_pipeline_load_texture()` from
  `common/pipeline/forge_pipeline.h` (produced by Asset Lesson 06)
- The CMake `forge-assets` build dependency — building the lesson automatically
  acquires processed assets (local, download, or pipeline run)
- Comparing raw vs. processed: visual quality, load time, vertex count with
  and without deduplication
- References the Asset Pipeline lesson track for readers who want to understand
  how the processing works

**From this lesson onward, every GPU lesson uses the pipeline.** Lessons 40+
use `forge_pipeline_load_mesh()` / `forge_pipeline_load_texture()` and declare
`add_dependencies(lesson_XX forge-assets)` in CMake.

---

## Phase 5 — Update skills

### 5A: `dev-gpu-lesson`

Add a mandatory section:

> **Asset Pipeline Mandate (GPU Lessons 39+)**
>
> - All geometry MUST come from `forge_shapes_*()` (procedural) or
>   `forge_pipeline_load_mesh()` on pipeline-processed assets.
> - NEVER define inline vertex arrays for 3D objects.
> - NEVER load raw unprocessed assets — use `forge_pipeline_load_mesh()` /
>   `forge_pipeline_load_texture()` which load from `assets/processed/`.
> - All textures are BC7 (albedo) or BC5 (normal maps) — shaders must
>   reconstruct normal Z from BC5 two-channel data.
> - If a lesson needs a new model, add it to `assets/models/`, run
>   `uv run python -m pipeline`, and load the processed output.
> - Reference `lessons/assets/` when first introducing an asset.
> - Purely procedural shader lessons (fullscreen effects) are exempt.
> - CMake: `add_dependencies(lesson_XX forge-assets)` is mandatory.

### 5B: `dev-physics-lesson`

> **Asset Pipeline Mandate**
>
> - All physics shapes (spheres, cubes, capsules, planes) MUST be created via
>   `forge_shapes_*()` from `common/shapes/forge_shapes.h`.
> - NEVER define inline vertex arrays for physics bodies.
> - If a lesson needs textured objects, load via
>   `forge_pipeline_load_texture()`.
> - The procedural grid floor may remain shader-generated.

### 5C: `dev-final-pass`

New checklist category:

> **Asset Pipeline Compliance** (GPU Lessons 39+)
>
> - [ ] No inline vertex array definitions for 3D objects
> - [ ] All meshes loaded via `forge_shapes_*()` or
>       `forge_pipeline_load_mesh()`
> - [ ] All textures loaded via `forge_pipeline_load_texture()`
> - [ ] Normal map shaders reconstruct Z from BC5 two-channel data
> - [ ] No ad-hoc geometry construction (manual vertex buffer fills for
>       standard shapes)
> - [ ] CMake has `add_dependencies(lesson_XX forge-assets)`
> - [ ] If new assets added, `uv run python -m pipeline` runs clean

### 5D: `dev-final-pass` (additional PR validation)

> **Asset Pipeline Check** (GPU Lessons 39+)
>
> - [ ] `uv run python -m pipeline --verbose` succeeds (validates current branch processes assets correctly)
> - [ ] No raw asset loads in lessons 39+
> - [ ] No inline vertex arrays for standard 3D shapes
> - [ ] CMake declares `forge-assets` dependency
> - [ ] New assets (if any) added to `assets/` and processable by pipeline

### 5E: `dev-asset-lesson`

> **Cross-track mandate:** All GPU Lessons 39+ MUST use pipeline-processed
> assets via `forge_pipeline_load_mesh()` / `forge_pipeline_load_texture()`.
> Physics lessons use `forge_shapes_*()` for procedural bodies and
> `forge_pipeline_load_texture()` for any textures. When adding pipeline
> features, verify they don't break GPU lesson asset loading.

### 5F: `dev-ui-lesson` and `dev-engine-lesson`

> If this lesson requires textures or 3D assets for any reason, they MUST go
> through the asset pipeline. No ad-hoc asset loading.

---

## Phase 6 — CI integration

### 6A: Run the real pipeline in CI

Add a GitHub Actions workflow that runs the full pipeline (not `--dry-run`):

```yaml
name: Asset Pipeline
on:
  push:
    paths:
      - 'assets/**'
      - 'pipeline/**'
      - 'pipeline.toml'
      - 'tools/mesh/**'
  pull_request:
    paths:
      - 'assets/**'
      - 'pipeline/**'
      - 'pipeline.toml'
      - 'tools/mesh/**'

jobs:
  process-assets:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - name: Install pipeline
        run: uv sync --extra dev
      - name: Build mesh tool
        run: |
          cmake -B build
          cmake --build build --target forge-mesh-tool
      - name: Run pipeline
        run: uv run python -m pipeline --verbose
      - name: Upload processed assets
        uses: actions/upload-artifact@v4
        with:
          name: processed-assets
          path: assets/processed/
```

This exercises real mesh conversion, texture compression, and sidecar writing.
Fails the build on any processing regression.

### 6B: Publish pre-built assets as a release artifact

On pushes to `main` that change source assets, publish a tarball to a rolling
`assets-latest` GitHub release:

```yaml
      - name: Package processed assets
        if: github.ref == 'refs/heads/main'
        run: tar czf processed-assets.tar.gz -C assets processed/
      - name: Update release
        if: github.ref == 'refs/heads/main'
        run: |
          gh release delete assets-latest --yes || true
          gh release create assets-latest processed-assets.tar.gz \
            --title "Pre-built processed assets" \
            --notes "Auto-generated by CI. Download and extract to assets/."
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

This is what the CMake `forge-assets` target downloads when
`assets/processed/` doesn't exist locally. Developers who aren't modifying
assets never need Python or the pipeline tools — they get pre-built assets
from the release.

### 6C: Add to merge gate

Add the asset pipeline job to the merge gate workflow so PRs that break asset
processing cannot be merged.

---

## Execution order

| Phase | Depends on | Scope |
|-------|------------|-------|
| 0 (Config + gitignore + CMake) | Nothing | Small — 3 files |
| 1 (Process assets) | Phase 0 | Medium — run pipeline, fix issues |
| 2 (Asset Lesson 06) | Phase 1 | Medium — new lesson + C header |
| 3 (Lesson 08 hint) | Nothing | Small — README paragraph |
| 4 (GPU Lesson 39) | Phase 2 | Medium — new lesson |
| 5 (Update skills) | Nothing | Medium — 6 skill files |
| 6 (CI + release artifact) | Phase 1 | Medium — workflow + release |

Phases 0, 3, and 5 can run in parallel.
Phase 6 can run as soon as Phase 1 is done (needs processed assets to validate).
Phase 4 requires Phase 2 (the C loader must exist first).

---

## Resolved decisions

1. **Texture compression format:** BC7 for albedo/diffuse (high-quality RGBA),
   BC5 for normal maps (two-channel RG, reconstruct Z in shader). The texture
   plugin classifies by filename convention. Asset Lesson 06 teaches the C
   reader for these formats.

2. **Bundle loading in C:** Lessons load from `assets/processed/` directory
   tree for now. `.forgepak` bundle loading is a future optimization for
   shipping/packaging.

3. **Three-tier asset acquisition.** The `forge-assets` CMake target is a
   build dependency for GPU Lessons 39+. It checks: (1) local
   `assets/processed/` exists → use it, (2) download pre-built tarball from
   the `assets-latest` GitHub release, (3) run `uv run python -m pipeline` locally.
   Python is only needed if you're modifying assets or the pipeline itself.
   Lessons 01–38 have no pipeline dependency.

4. **Existing lessons stay unchanged.** Lessons 01–38 are not retroactively
   updated. They continue loading raw assets via `forge_gltf_load()`,
   `forge_obj_load()`, and inline vertex arrays. This preserves the learning
   flow — each lesson teaches one concept at a time. The pipeline is
   introduced as its own concept in Lesson 39.
