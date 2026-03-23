# Asset Pipeline Lessons

A hybrid Python + C track for building asset processing tooling — from a CLI
pipeline to a full asset editor with a web frontend.

The pipeline is a **reusable Python library** at
[`pipeline/`](../../pipeline/) in the repo root. Install it with
`uv sync --extra dev` and use it in your own projects. These lessons teach
how each piece was designed and built.

## What you'll learn

- Scaffold a plugin-based CLI pipeline with TOML configuration (Python)
- Process textures: resize, compress, generate mipmaps, track changes (Python)
- Process meshes: deduplicate vertices, optimize indices, generate tangents and
  LODs using meshoptimizer and MikkTSpace (C tool invoked from Python)
- Generate procedural geometry: spheres, tori, capsules, and more from
  parametric equations (C header-only library)
- Pack processed assets into compressed bundles with random access
- Build a web frontend for browsing, previewing, and configuring assets
- Create a visual scene editor with undo/redo and live preview

## Result

The track produces a complete asset pipeline: a Python CLI that scans,
fingerprints, and processes raw art through plugins, backed by compiled C tools
for performance-critical mesh optimization and a header-only C library for
procedural geometry. Later lessons add a browser-based UI for asset browsing,
3D preview, and import settings.

## Key concepts

- **Python orchestrates, C processes** — The pipeline CLI is Python for rapid
  development and plugin flexibility. Performance-critical processing (mesh
  optimization, tangent generation) uses compiled C tools invoked as
  subprocesses. Procedural geometry lives in a header-only C library that GPU
  lessons include directly.
- **Reusable library** — The pipeline lives at `pipeline/` in the repo root
  as a reusable package (installed via `uv sync`). Each lesson adds functionality to the shared
  library, not to throwaway lesson-local code.
- **Incremental builds** — Only reprocess what changed. Fingerprint source
  files by content hash and skip unchanged assets.
- **Plugin architecture** — Each asset type (texture, mesh, scene) is a plugin
  that registers with the pipeline. New formats are added without modifying
  core code.
- **Metadata first** — Every processed asset has a sidecar file describing its
  source, settings, and output. The pipeline is fully reproducible.
- **Web-native tooling** — The frontend is a browser application served by
  an embedded Python server. No Electron, no native UI.

## Lessons

| # | Topic | Language | What it adds to the pipeline |
|---|-------|----------|------------------------------|
| 01 | [Pipeline Scaffold](01-pipeline-scaffold/) | Python | CLI, plugin discovery, scanning, fingerprinting, TOML config |
| 02 | [Texture Processing](02-texture-processing/) | Python | Image resize, format conversion, mipmap generation, GPU block compression (BC/ASTC), metadata sidecars |
| 03 | [Mesh Processing](03-mesh-processing/) | C + Python | Vertex deduplication, index optimization, tangent generation, LOD simplification |
| 04 | [Procedural Geometry](04-procedural-geometry/) | C | Header-only parametric geometry library: sphere, icosphere, cylinder, cone, torus, plane, cube, capsule |
| 05 | [Asset Bundles](05-asset-bundles/) | Python | Packing processed assets into `.forgepak` bundles with zstd compression, random-access TOC, and dependency tracking |
| 06 | [Loading Processed Assets](06-loading-processed-assets/) | C library | Header-only C loader for .fmesh meshes and pipeline-processed textures; `forge_pipeline.h` |
| 07 | [Materials](07-materials/) | C + Python | Full PBR material support: glTF parser extensions, multi-primitive mesh processing, .fmesh v2 submesh table, .fmat material sidecars |
| 08 | [Animations](08-animations/) | C | glTF animation parsing and runtime evaluation: channels, samplers, keyframe interpolation (lerp/slerp), looping |
| 09 | [Scene Hierarchy](09-scene-hierarchy/) | C + Python | glTF node tree extraction, `.fscene` binary format, runtime world transform computation, mesh instancing |
| 10 | [Animation Loader](10-animation-loader/) | C + Python | Runtime `.fanim` loader in `forge_pipeline.h`, per-clip export via `--split`, `.fanims` stub manifest |
| 11 | [Animation Manifest](11-animation-manifest/) | C + Python | `.fanims` JSON manifest loader, named clip lookup, convenience loading, loop flags and tags |
| 12 | [Skinned Animations](12-skinned-animations/) | C + Python | `.fskin` binary format, skinned vertices in `.fmesh` v3, scene/mesh tool extensions |
| 13 | [Morph Targets](13-morph-targets/) | C + Python | Morph target (blend shape) pipeline: glTF morph parsing, `.fmesh` morph deltas, weight animation channels, runtime loader |
| 14 | [Web UI Scaffold](14-web-ui-scaffold/) | Python + TypeScript | FastAPI backend, Vite + React + TypeScript frontend, asset browser with type filtering and search, WebSocket build status |
| 15 | [Asset Preview](15-asset-preview/) | Python + TypeScript | react-three-fiber 3D mesh preview, texture viewer with channel isolation, file serving endpoints, source vs. processed comparison |
| 16 | [Import Settings Editor](16-import-settings-editor/) | Python + TypeScript | Per-asset TOML sidecar files, three-layer settings merge (schema → global → per-asset), settings editor form, single-asset re-processing |
| 17 | [Texture Atlas](17-texture-atlas/) | Python | Guillotine bin packing, material-grouped atlas compositing, UV remap, padding and mipmap bleed, atlas metadata |
| 18 | [Scene Editor](18-scene-editor/) | Python + TypeScript | Visual scene composition with react-three-fiber viewport, transform gizmos, hierarchy panel, undo/redo, JSON scene files |
| 19 | [Pipeline Asset Viewer](19-pipeline-asset-viewer/) | TypeScript | Custom Three.js loaders for `.fmesh` and `.ftex` (binary) and `.fmat` (material JSON); `usePipelineModel` hook; LOD selector; scene editor integration |

## Building

### Prerequisites

- Python 3.10+ (for pipeline CLI and web frontend)
- CMake 3.24+ and a C compiler (for mesh processing tools and procedural
  geometry library)

### Installation

```bash
# From the forge-gpu repository root
uv sync --extra dev
```

### Running

```bash
# Run the pipeline CLI
forge-pipeline -v

# Try it with the lesson's sample assets
cd lessons/assets/01-pipeline-scaffold
forge-pipeline -v

# Run the tests
uv run pytest tests/pipeline/ -v
```

C tools (mesh processing, procedural geometry) are built via CMake alongside
GPU lessons:

```bash
cmake -B build
cmake --build build --config Debug
```

## Connection to other tracks

Asset pipeline lessons produce optimized runtime formats consumed by GPU
lessons. The mesh processing tool reads source glTF/OBJ files (using
`forge_gltf_load()` and `forge_obj_load()` during import) and writes optimized
binary output for fast GPU upload. The texture processing plugin generates
compressed, mipmapped textures ready for `SDL_CreateGPUTexture`. The procedural
geometry library (`common/shapes/`) provides parametric meshes used directly by
GPU and physics lessons.

| Track | Connection |
|---|---|
| GPU | Pipeline outputs feed directly into GPU lesson rendering; procedural geometry provides shapes for scenes |
| Engine | Build system concepts (incremental builds, dependency tracking) overlap; C tool compilation uses CMake |
| Math | Parametric surface equations build on vectors and trigonometry from math fundamentals |
| Physics | Procedural shapes (spheres, capsules, boxes) serve as collision proxies |

## Exercises

1. Add a new file type plugin to the pipeline scanner
2. Implement a texture atlas packing plugin that combines multiple textures
3. Write a mesh LOD export that produces multiple detail levels from one source
