# Asset Pipeline Integration Plan

## Goal

From GPU Lesson 39 onward, every asset — textures, meshes, procedural
geometry — enters through the asset pipeline. Textures are BC7/BC5 compressed,
tangents generated, meshes optimized. Skills enforce this for all future
lessons.

GPU Lessons 01–38 are not modified. They continue loading assets the way they
always have.

---

## Status

### Done

- **Phase 0 — Pipeline config and .gitignore** (PR #415, #416)
  - Root `pipeline.toml` with `source_dir = "assets"`, basisu compression,
    mesh optimization, LOD levels
  - `.gitignore` entries for `assets/processed/` and `assets/bundles/`
  - CMake `forge-assets` target with three-tier acquisition
    (local → GitHub release → run pipeline)
  - `cmake/AcquireAssets.cmake` script

- **Phase 1 — Process existing assets** (PR #419)
  - All 19 source assets (6 models + 13 textures) process successfully
  - Shared tool finder (`pipeline/tool_finder.py`) discovers C tools in
    CMake build directories
  - Scanner excludes output directory to prevent reprocessing loop
  - SDL3.dll post-build copy for tool executables on Windows
  - Locale-safe LOD parsing (`SDL_strtod` instead of `strtof`)
  - Normal map auto-detection by filename convention
  - Bundle creation verified (178 entries, 28.5 MB)

- **Asset Lesson 06 — Loading Processed Assets in C** (already existed)
  - `common/pipeline/forge_pipeline.h` — header-only C loader for `.fmesh`
    and compressed textures
  - `lessons/assets/06-loading-processed-assets/`

- **GPU Lesson 39 — Pipeline-Processed Assets** (already existed)
  - `lessons/gpu/39-pipeline-processed-assets/` — teaches loading pipeline
    output in SDL GPU
  - GPU Lessons 40–47 already use `forge_pipeline_load_mesh()` /
    `forge_pipeline_load_texture()` / `forge_scene_*()` APIs

- **GPU Lesson 08 README hint** (already existed)
  - "What's next" section references the asset pipeline track

- **Phase 3 — Update skills** (PR #426)
  - `dev-gpu-lesson`: Asset Pipeline Mandate section (GPU Lessons 39+)
  - `dev-physics-lesson`: Asset Pipeline Mandate subsection
  - `dev-final-pass`: Sections 19 (Asset Pipeline Compliance, GPU 39+) and
    20 (Asset Pipeline Compliance, physics/audio) with reporting template
  - `dev-audio-lesson`: Asset Pipeline Mandate subsection
  - `dev-asset-lesson`: Cross-Track Asset Pipeline Mandate section
  - `dev-ui-lesson` and `dev-engine-lesson`: Asset Pipeline Mandate section

### Remaining

| Phase | Scope | Description |
|-------|-------|-------------|
| 2 | Small | Wire `forge-assets` CMake dependency into lessons 39+ |
| 4 | Medium | Scene editor export path (JSON → binary `.fscene`) |
| 5 | Medium | CI integration — run pipeline in GitHub Actions |

---

## Phase 2 — Wire CMake `forge-assets` dependency

GPU Lessons 39–47 load processed assets but do not declare a CMake dependency
on `forge-assets`. Add `add_dependencies` so building any of these lessons
automatically acquires processed assets via the three-tier fallback.

For each lesson directory in `lessons/gpu/39-*` through `lessons/gpu/47-*`:

```cmake
add_dependencies(lesson_XX forge-assets)
```

The target name varies per lesson — check each `CMakeLists.txt` for the
`add_executable` target name.

This is a small change (one line per lesson) with no code impact.

---

## Phase 3 — Update skills

### 3A: `dev-gpu-lesson`

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

### 3B: `dev-physics-lesson`

> **Asset Pipeline Mandate**
>
> - All physics shapes (spheres, cubes, capsules, planes) MUST be created via
>   `forge_shapes_*()` from `common/shapes/forge_shapes.h`.
> - NEVER define inline vertex arrays for physics bodies.
> - If a lesson needs textured objects, load via
>   `forge_pipeline_load_texture()`.
> - The procedural grid floor may remain shader-generated.

### 3C: `dev-final-pass`

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

Additional PR validation:

> **Asset Pipeline Check** (GPU Lessons 39+)
>
> - [ ] `uv run python -m pipeline --verbose` succeeds
> - [ ] No raw asset loads in lessons 39+
> - [ ] No inline vertex arrays for standard 3D shapes
> - [ ] CMake declares `forge-assets` dependency
> - [ ] New assets (if any) added to `assets/` and processable by pipeline

### 3D: `dev-asset-lesson`

> **Cross-track mandate:** All GPU Lessons 39+ MUST use pipeline-processed
> assets via `forge_pipeline_load_mesh()` / `forge_pipeline_load_texture()`.
> Physics lessons use `forge_shapes_*()` for procedural bodies and
> `forge_pipeline_load_texture()` for any textures. When adding pipeline
> features, verify they don't break GPU lesson asset loading.

### 3E: `dev-ui-lesson` and `dev-engine-lesson`

> If this lesson requires textures or 3D assets for any reason, they MUST go
> through the asset pipeline. No ad-hoc asset loading.

---

## Phase 4 — Scene editor export path

The scene editor (`pipeline/web/`, Asset Lesson 18) introduces a second asset
path that the integration plan must account for. The editor saves authored
scenes as JSON files in `{output_dir}/scenes/`. These are composition data —
object placements, transforms, and references to pipeline assets — not binary
files that GPU lessons can load directly.

For GPU lessons to render authored scenes, the JSON must be exported to the
binary `.fscene` format that `forge_pipeline_load_scene()` reads. This is
item 3i in `pipeline/web/PLAN.md`.

### 4A: Scene export endpoint

Add `POST /api/scenes/{id}/export` to the FastAPI backend. The endpoint:

1. Loads the authored scene JSON
2. Resolves each object's `asset_id` to a processed `.fmesh` path
3. Writes a binary `.fscene` file to `assets/processed/scenes/`
4. The `.fscene` uses the same format as `forge-scene-tool` output (header,
   node array with transforms, mesh references, material indices)

The export produces files identical in format to what `forge-scene-tool`
generates from glTF — the C loader (`forge_pipeline_load_scene()`) does not
need changes.

### 4B: Pipeline integration

Exported `.fscene` files live in `assets/processed/scenes/` alongside other
pipeline output. They are:

- **Gitignored** (already covered by `assets/processed/`)
- **Included in the `forge-assets` three-tier acquisition** (bundled in the
  release tarball if present)
- **Loadable by GPU lessons** via `forge_pipeline_load_scene()`

The pipeline's `scan` command does not process scene JSON — the export is
triggered explicitly from the web UI or a future CLI command
(`uv run python -m pipeline export-scenes`).

### 4C: Skill updates

Add to the `dev-gpu-lesson` mandate:

> - Authored scenes from the web editor are loaded via
>   `forge_pipeline_load_scene()` after export. Never parse scene JSON
>   directly in C code.

### 4D: Future — scene plugin

If scene authoring becomes a regular workflow, add a `scene` export plugin
that automatically converts all scene JSON files in `{output_dir}/scenes/`
to binary `.fscene` files during a pipeline run. This would make scene export
part of the standard `uv run python -m pipeline` command rather than a
separate manual step.

---

## Phase 5 — CI integration

### 5A: Run the real pipeline in CI

Add a GitHub Actions workflow that runs the full pipeline:

```yaml
name: Asset Pipeline
on:
  push:
    paths:
      - 'assets/**'
      - 'pipeline/**'
      - 'pipeline.toml'
      - 'tools/**'
  pull_request:
    paths:
      - 'assets/**'
      - 'pipeline/**'
      - 'pipeline.toml'
      - 'tools/**'

jobs:
  process-assets:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: astral-sh/setup-uv@v5
      - name: Install dependencies
        run: uv sync --extra dev
      - name: Build tools
        run: |
          cmake -B build -DFORGE_USE_SHIM=OFF
          cmake --build build --target forge_mesh_tool forge_anim_tool forge_scene_tool forge_texture_tool basisu
      - name: Run pipeline
        run: uv run python -m pipeline --verbose
      - name: Upload processed assets
        uses: actions/upload-artifact@v4
        with:
          name: processed-assets
          path: assets/processed/
```

### 5B: Publish pre-built assets as a release artifact

On pushes to `main` that change source assets, publish a tarball to a rolling
`assets-latest` GitHub release:

```yaml
      - name: Package processed assets
        if: github.ref == 'refs/heads/main'
        run: tar czf processed-assets.tar.gz -C assets/processed .
      - name: Update release
        if: github.ref == 'refs/heads/main'
        run: |
          gh release delete assets-latest --yes || true
          gh release create assets-latest processed-assets.tar.gz \
            --title "Pre-built processed assets" \
            --notes "Auto-generated by CI. Download and extract to assets/processed/."
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

The tarball must contain flat contents (no root directory) — see the comment
in `cmake/AcquireAssets.cmake` for why.

### 5C: Add to merge gate

Add the asset pipeline job to the merge gate workflow so PRs that break asset
processing cannot be merged.

---

## Future asset types

The pipeline is designed to be extended with new asset types via the plugin
system. Each new format follows the same pattern: a Python plugin scans source
files, invokes a C tool (if needed), and writes processed output to
`assets/processed/`. A C loader in `common/pipeline/forge_pipeline.h` reads
the binary format at runtime.

The following asset types are planned across lesson tracks (see `PLAN.md`).
Each one adds a pipeline plugin, a binary format, and a loader function.

| Asset type | Format | Plugin | C tool | C loader | Lesson |
|------------|--------|--------|--------|----------|--------|
| Textures | `.ktx2` (BC7/BC5) | `texture` | basisu | `forge_pipeline_load_texture()` | Done |
| Meshes | `.fmesh` | `mesh` | `forge_mesh_tool` | `forge_pipeline_load_mesh()` | Done |
| Scenes | `.fscene` | `scene` | `forge_scene_tool` | `forge_pipeline_load_scene()` | Done |
| Animations | `.fanim` | `animation` | `forge_anim_tool` | `forge_pipeline_load_anim()` | Done |
| Materials | `.fmat` | `mesh` (sidecar) | `forge_mesh_tool` | `forge_pipeline_load_materials()` | Done |
| Skin data | `.fskin` | `animation` (sidecar) | `forge_anim_tool` | `forge_pipeline_load_skin()` | Done |
| Texture atlases | atlas `.json` | `atlas` | — | `forge_pipeline_load_atlas()` | Done |
| Authored scenes | scene `.json` → `.fscene` | export endpoint | — | `forge_pipeline_load_scene()` | Phase 4 |
| Navmeshes | `.fnav` | `navmesh` | `tools/nav/` | `forge_pipeline_load_navmesh()` | Asset 25–26 |
| Particle effects | `.fpart` | `particle` | — | `forge_pipeline_load_effect()` | Asset 22 |
| Animation events | `.faevt` | `anim_events` | — | `forge_pipeline_load_anim_events()` | Asset 23 |
| Material defs | `.fmat2` | `material` | — | `forge_pipeline_load_material_def()` | Asset 24 |
| Noise textures | `.ktx2` (BC4/BC5) | `noise` | — | `forge_pipeline_load_texture()` | Asset 20 |
| Vegetation textures | `.ktx2` (BC7) | `vegetation` | — | `forge_pipeline_load_texture()` | Asset 21 |
| Lua scripts | `.lua` | `script` | — | (runtime `luaL_loadfile`) | Future |

### What the integration provides for new types

Each new asset type gets the following for free from the existing
infrastructure:

- **Incremental builds** — the scanner fingerprints source files and the
  per-plugin cache skips unchanged assets
- **Tool discovery** — `pipeline/tool_finder.py` finds new C tools in
  `build/tools/*/` automatically
- **Three-tier acquisition** — `forge-assets` CMake target includes all
  processed output in the release tarball
- **CI validation** — the pipeline workflow catches processing regressions
- **Import settings** — per-asset `.import.toml` sidecars override plugin
  defaults
- **Web UI** — the asset browser shows all processed assets with thumbnails
  and metadata

### What each new type must provide

1. A plugin in `pipeline/plugins/` that implements `AssetPlugin.process()`
2. A binary format specification (header magic, version, layout)
3. A loader function in `common/pipeline/forge_pipeline.h`
4. Tests in `tests/pipeline/`
5. A C tool in `tools/` if processing requires native code

### Capstone demo game

The `demos/arena/` project (see `PLAN.md`) is the integration test for all
asset types working together. Every asset — meshes, textures, navmeshes,
particle effects, animation events, materials, authored scenes — enters
through the pipeline. The capstone validates that:

- `uv run python -m pipeline` processes everything from source to
  `assets/processed/`
- `cmake --build build --target arena-demo` acquires all processed assets
  via the `forge-assets` target
- The game runs using only `forge_pipeline_load_*()` functions — no raw
  asset loading

---

## Resolved decisions

1. **Texture compression format:** BC7 for albedo/diffuse (high-quality RGBA),
   BC5 for normal maps (two-channel RG, reconstruct Z in shader). The texture
   plugin auto-detects normal maps by filename convention (`*_normal*`,
   `*_nrm*`). Override per-asset with `.import.toml` sidecars.

2. **Bundle loading in C:** Lessons load from `assets/processed/` directory
   tree for now. `.forgepak` bundle loading is a future optimization for
   shipping/packaging.

3. **Three-tier asset acquisition.** The `forge-assets` CMake target is a
   build dependency for GPU Lessons 39+. It checks: (1) local
   `assets/processed/` exists → use it, (2) download pre-built tarball from
   the `assets-latest` GitHub release, (3) run `uv run python -m pipeline`
   locally. Python is only needed if you're modifying assets or the pipeline
   itself. Lessons 01–38 have no pipeline dependency.

4. **Existing lessons stay unchanged.** Lessons 01–38 are not retroactively
   updated. They continue loading raw assets via `forge_gltf_load()`,
   `forge_obj_load()`, and inline vertex arrays.
