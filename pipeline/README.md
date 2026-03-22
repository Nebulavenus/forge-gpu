# forge-pipeline

A plugin-based asset processing pipeline for game and graphics projects.
Scan source files, fingerprint them by content hash, and process only what
changed — with plugins for each asset type.

Part of [forge-gpu](https://github.com/RosyGameStudio/forge-gpu). Learn how
it works in the [asset pipeline lessons](../lessons/assets/).

## Installation

```bash
# From the forge-gpu repository root (recommended)
uv sync --extra dev

# Or with pip (outside the forge-gpu dev workflow)
pip install -e ".[dev]"
```

## Quick start

```bash
# Scan a directory for assets and report what needs processing
forge-pipeline -c pipeline.toml

# Verbose output with debug logging
forge-pipeline -v

# Override the source directory
forge-pipeline --source-dir path/to/assets
```

## Configuration

Create a `pipeline.toml` in your project:

```toml
[pipeline]
source_dir = "assets/raw"       # where raw source assets live
output_dir = "assets/processed" # where processed assets go
cache_dir  = ".forge-cache"     # fingerprint cache for incremental builds

# Per-plugin settings — each plugin reads its own section
[texture]
max_size = 2048
generate_mipmaps = true

[mesh]
deduplicate = true
generate_tangents = true
```

## Architecture

The pipeline has eight components:

| Component | Module | Purpose |
|---|---|---|
| Configuration | `pipeline.config` | Parse TOML, produce typed `PipelineConfig` dataclass |
| Plugin system | `pipeline.plugin` | `AssetPlugin` base class, `PluginRegistry`, file-based discovery |
| Scanner | `pipeline.scanner` | Walk directories, SHA-256 fingerprint, classify NEW/CHANGED/UNCHANGED |
| Bundler | `pipeline.bundler` | Pack processed assets into compressed bundles with random-access TOC |
| CLI | `pipeline.__main__` | `argparse` entry point tying everything together |
| Import settings | `pipeline.import_settings` | Per-asset `.import.toml` sidecar overrides |
| Atlas packer | `pipeline.atlas` | Guillotine bin-packing for texture atlases |
| Web server | `pipeline.server` | FastAPI app for browsing assets, REST API, WebSocket status |

```text
pipeline.toml --> CLI (__main__.py)
                   |
                   +---> Plugin Discovery (plugins/*.py)
                   |        register by name + extension
                   |
                   +---> Scanner (scanner.py)
                   |        |
                   |        +-- walk source directory
                   |        +-- fingerprint (SHA-256)
                   |        +-- compare against cache
                   |        +-- classify: NEW / CHANGED / UNCHANGED
                   |
                   +---> Bundler (bundler.py)
                            +-- pack processed assets
                            +-- compress with zstd
                            +-- random-access TOC
```

## Writing plugins

Create a Python file that defines an `AssetPlugin` subclass:

```python
# my_plugins/audio.py
from pathlib import Path
from pipeline.plugin import AssetPlugin, AssetResult

class AudioPlugin(AssetPlugin):
    name = "audio"
    extensions = [".wav", ".ogg", ".mp3"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        output = output_dir / source.with_suffix(".opus").name
        # ... processing logic ...
        return AssetResult(source=source, output=output)
```

Point the pipeline at your plugins directory:

```bash
forge-pipeline --plugins-dir my_plugins/
```

The plugin is discovered automatically — no core code changes needed.

## Built-in plugins

| Plugin | Extensions | Processing |
|---|---|---|
| `texture` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` | Resize, format convert, mipmaps, GPU compression |
| `mesh` | `.obj`, `.gltf`, `.glb` | Vertex dedup, index optimization, tangents, LOD |
| `scene` | `.gltf`, `.glb` | Scene hierarchy extraction |
| `animation` | `.gltf`, `.glb` | Animation clip extraction |
| `atlas` | _(post-processing)_ | Texture atlas packing |

## Output file formats

The pipeline produces several binary and JSON file types. All binary
formats use little-endian byte order.

### Binary formats

| Format | Magic | Version | Producer | C Loader | Purpose |
|--------|-------|---------|----------|----------|---------|
| `.fmesh` | `FMSH` | 2 / 3 | `forge-mesh-tool` | `forge_pipeline_load_mesh()` | Optimized mesh with LODs and submeshes |
| `.fscene` | `FSCN` | 1 | `forge-scene-tool` | `forge_pipeline_load_scene()` | Scene node hierarchy with transforms |
| `.fanim` | `FANM` | 1 | `forge-anim-tool` | `forge_pipeline_load_animation()` | Animation clips with keyframe data |
| `.fskin` | `FSKN` | 1 | `forge-scene-tool` | `forge_pipeline_load_skins()` | Joint indices and inverse bind matrices |
| `.ftex` | `FTEX` | 1 | `forge-texture-tool` | `forge_pipeline_load_ftex()` | GPU block-compressed textures (BC7/BC5) |
| `.forgepak` | `FPAK` | 1 | Python `BundleWriter` | Python `BundleReader` | Asset bundle with per-entry zstd compression |

All C loaders are in `common/pipeline/forge_pipeline.h` (header-only).

### JSON and TOML sidecar formats

| Format | Producer | Purpose |
|--------|----------|---------|
| `.fmat` | `forge-mesh-tool` | PBR material properties and texture paths |
| `.fanims` | `forge-anim-tool` | Animation manifest listing per-clip `.fanim` files |
| `.meta.json` | All plugins | Processing metadata (dimensions, settings, compression info) |
| `.import.toml` | User / web UI | Per-asset setting overrides |
| `atlas.json` | Atlas plugin | Atlas rect positions and UV coordinates |

### Processed image formats

| Format | Producer | Purpose |
|--------|----------|---------|
| `.png` / `.jpg` / `.bmp` | Texture plugin | Resized base image (mip 0) |
| `*_mip{N}.png` | Texture plugin | Mip levels 1..N (halved dimensions) |
| `.ktx2` | `basisu` (external) | Basis Universal compressed texture (intermediate) |
| `.astc` | `astcenc` (external) | ASTC compressed texture |
| `atlas.png` | Atlas plugin | Composited texture atlas image |

---

### `.fmesh` — Mesh binary

Stores optimized vertex/index data with multiple LOD levels and
submeshes (primitives). Written by `forge-mesh-tool` using meshoptimizer
for vertex/index cache optimization and MikkTSpace for tangent
generation.

```text
Header (32 bytes)
├─ magic            4B     "FMSH"
├─ version          u32    2 (basic) or 3 (skinned/morphed)
├─ vertex_count     u32
├─ vertex_stride    u32    32, 48, 56, or 72
├─ lod_count        u32    1–8
├─ flags            u32    bit 0 = tangents, bit 1 = skinned, bit 2 = morphs
├─ submesh_count    u32    1–64
└─ reserved         4B

LOD-Submesh Table
├─ per LOD: target_error (f32)
│  └─ per submesh: index_count (u32), index_offset (u32), material_idx (i32)

Vertex Data (vertex_count × vertex_stride bytes)
Index Data (all LODs, u32 per index)

Morph Targets (optional, version 3 with FLAG_MORPHS)
├─ target_count     u32
├─ attr_flags       u32    bit 0 = positions, bit 1 = normals, bit 2 = tangents
├─ per target: name (64B), default_weight (f32)
└─ per target: delta arrays (vertex_count × 3 × f32 per enabled attribute)
```

**Vertex layouts:**

| Stride | Attributes |
|--------|------------|
| 32 | position (vec3), normal (vec3), uv (vec2) |
| 48 | position, normal, uv, tangent (vec4) |
| 56 | position, normal, uv, joints (4 × u16), weights (4 × f32) |
| 72 | position, normal, uv, tangent, joints, weights |

---

### `.fscene` — Scene hierarchy

Stores the glTF node tree with parent-child relationships, per-node
transforms (TRS and matrix), and mesh/skin references. World transforms
are computed at load time by walking the tree.

```text
Header (24 bytes)
├─ magic            4B     "FSCN"
├─ version          u32    1
├─ node_count       u32
├─ mesh_count       u32
├─ root_count       u32
└─ reserved         u32

Root Indices (root_count × u32)
Mesh Table (mesh_count × 8B: first_submesh u32, submesh_count u32)
Node Table (node_count × 192B per node)
├─ name             64B    null-terminated
├─ parent           i32    -1 = root
├─ mesh_index       i32    -1 = no mesh
├─ skin_index       i32    -1 = no skin
├─ first_child      u32    index into children array
├─ child_count      u32
├─ has_trs          u32    1 = TRS valid
├─ translation      f32[3]
├─ rotation         f32[4] quaternion (x, y, z, w)
├─ scale            f32[3]
└─ local_transform  f32[16] column-major 4×4

Children Array (sum of all child_count × u32)
```

---

### `.fanim` — Animation clips

Stores animation clips with samplers (keyframe data) and channels
(which node property each sampler drives). Supports linear and step
interpolation. Quaternion rotations use slerp at runtime.

```text
Header (12 bytes)
├─ magic            4B     "FANM"
├─ version          u32    1
└─ clip_count       u32

Per Clip (76 bytes)
├─ name             64B    null-terminated
├─ duration         f32    seconds
├─ sampler_count    u32
└─ channel_count    u32

Per Sampler
├─ keyframe_count   u32
├─ value_components u32    3 (translation/scale), 4 (rotation), N (morph weights)
├─ interpolation    u32    0 = LINEAR, 1 = STEP
├─ timestamps       f32[keyframe_count]
└─ values           f32[keyframe_count × value_components]

Per Channel (12 bytes)
├─ target_node      i32    scene node index
├─ target_path      u32    0 = translation, 1 = rotation, 2 = scale, 3 = morph_weights
└─ sampler_index    u32
```

---

### `.fskin` — Skinning data

Stores joint hierarchies and inverse bind matrices for skeletal
animation. Written by `forge-scene-tool` with the `--skins` flag.

```text
Header (12 bytes)
├─ magic            4B     "FSKN"
├─ version          u32    1
└─ skin_count       u32

Per Skin
├─ name             64B    null-terminated
├─ skeleton         i32    root joint node index (-1 = unset)
├─ joint_count      u32
├─ joints           i32[joint_count]     node indices
└─ inverse_bind_matrices  f32[joint_count × 16]  column-major 4×4
```

---

### `.ftex` — GPU compressed texture

Stores pre-transcoded GPU block-compressed texture data (BC7 or BC5)
with a mip chain. No runtime transcoding needed — blocks are uploaded
directly to the GPU.

```text
Header (32 bytes = 8 × u32)
├─ magic            u32    0x58455446 ("FTEX" little-endian)
├─ version          u32    1
├─ format           u32    1 = BC7_SRGB, 2 = BC7_UNORM, 3 = BC5_UNORM
├─ width            u32    base mip width
├─ height           u32    base mip height
├─ mip_count        u32
├─ reserved1        u32
└─ reserved2        u32

Mip Entries (mip_count × 16B)
├─ offset           u32    byte offset from file start
├─ size             u32    compressed block size
├─ width            u32    mip level width
└─ height           u32    mip level height

Block Data (concatenated mip levels)
```

**Format codes:**

| Code | SDL GPU Format | Color space | Use |
|------|---------------|-------------|-----|
| 1 | BC7_SRGB | sRGB | Base color, emissive |
| 2 | BC7_UNORM | Linear | Metallic-roughness, AO |
| 3 | BC5_UNORM | Linear | Normal maps (RG only, Z reconstructed in shader) |

---

### `.forgepak` — Asset bundle

Packs multiple processed assets into a single file with per-entry zstd
compression. Each entry is compressed independently for O(1) random
access — the reader seeks to an offset and decompresses one block
without reading the rest.

```text
Header (24 bytes)
├─ magic            4B     "FPAK"
├─ version          u32    1
├─ entry_count      u32
├─ toc_offset       u64    byte offset to TOC from file start
└─ toc_size         u32    compressed TOC size

Entry Data (concatenated zstd or raw blocks)

Table of Contents (zstd-compressed JSON array at end of file)
├─ per entry:
│  ├─ path              string   POSIX relative path
│  ├─ offset            int      byte offset to compressed data
│  ├─ size              int      compressed size
│  ├─ original_size     int      uncompressed size
│  ├─ compression       string   "zstd" or "none"
│  ├─ fingerprint       string   SHA-256 hex digest
│  └─ dependencies      string[] referenced asset paths (optional)
```

The TOC is written last so entries can be streamed sequentially. The
header is patched after the TOC is written with the final offset and
size.

---

### `.fmat` — Material sidecar

JSON file alongside `.fmesh` describing PBR materials extracted from
glTF. Material indices in the `.fmesh` submesh table reference entries
in this array.

```json
{
  "version": 1,
  "materials": [
    {
      "name": "BrickWall",
      "base_color_factor": [1.0, 1.0, 1.0, 1.0],
      "base_color_texture": "textures/brick_albedo.png",
      "metallic_factor": 0.0,
      "roughness_factor": 0.8,
      "metallic_roughness_texture": "textures/brick_mr.png",
      "normal_texture": "textures/brick_normal.png",
      "normal_scale": 1.0,
      "occlusion_texture": "textures/brick_ao.png",
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

---

### `.fanims` — Animation manifest

JSON manifest listing per-clip `.fanim` files produced by split-mode
animation extraction.

```json
{
  "version": 1,
  "model": "character",
  "clips": {
    "idle": {
      "file": "character_idle.fanim",
      "duration": 1.5,
      "loop": true,
      "tags": ["locomotion"]
    },
    "run": {
      "file": "character_run.fanim",
      "duration": 0.8,
      "loop": true,
      "tags": ["locomotion"]
    }
  }
}
```

---

### `.meta.json` — Processing metadata

JSON sidecar written by each plugin alongside its output. Content
varies by asset type.

**Texture metadata:**

```json
{
  "source": "brick_albedo.png",
  "output": "brick_albedo.png",
  "original_width": 4096,
  "original_height": 4096,
  "output_width": 2048,
  "output_height": 2048,
  "mip_levels": [
    { "level": 0, "width": 2048, "height": 2048 },
    { "level": 1, "width": 1024, "height": 1024 }
  ],
  "settings": { "max_size": 2048, "generate_mipmaps": true, "output_format": "png" },
  "compression": {
    "codec": "uastc",
    "container": "ktx2",
    "compressed_file": "brick_albedo.ktx2",
    "uncompressed_bytes": 16777216,
    "compressed_bytes": 4194304,
    "ratio": 4.0,
    "normal_map": false
  }
}
```

**Mesh metadata:**

```json
{
  "source": "hero.gltf",
  "format_version": 3,
  "vertex_count": 1024,
  "vertex_stride": 48,
  "has_tangents": true,
  "has_morphs": false,
  "submesh_count": 3,
  "lod_count": 3,
  "lod_ratios": [1.0, 0.5, 0.25],
  "original_vertex_count": 2904,
  "dedup_ratio": 0.35
}
```

---

### `.import.toml` — Per-asset settings

TOML sidecar alongside source files for per-asset pipeline overrides.
Keys match the plugin's settings schema. Missing keys fall back to the
global `[plugin_name]` section in `pipeline.toml`.

```toml
normal_map = true
compression = "basisu"
basisu_quality = 200
```

---

### `atlas.json` — Atlas metadata

JSON file describing the packed texture atlas layout. UV offset/scale
values remap material UVs from individual-texture space to atlas space.

```json
{
  "version": 1,
  "width": 2048,
  "height": 2048,
  "padding": 4,
  "utilization": 0.87,
  "entries": {
    "brick": {
      "x": 4, "y": 4,
      "width": 512, "height": 512,
      "u_offset": 0.001953, "v_offset": 0.001953,
      "u_scale": 0.25, "v_scale": 0.25
    }
  }
}
```

---

### Validation limits

All binary loaders enforce maximum sizes defined in
`common/pipeline/forge_pipeline.h`:

| Constant | Value | Applies to |
|----------|-------|------------|
| `FORGE_PIPELINE_MAX_LODS` | 8 | LOD levels per mesh |
| `FORGE_PIPELINE_MAX_SUBMESHES` | 64 | Submeshes per mesh |
| `FORGE_PIPELINE_MAX_NODES` | 4096 | Scene nodes |
| `FORGE_PIPELINE_MAX_ROOTS` | 256 | Scene root nodes |
| `FORGE_PIPELINE_MAX_SCENE_MESHES` | 1024 | Mesh entries in scene |
| `FORGE_PIPELINE_MAX_ANIM_CLIPS` | 256 | Animation clips |
| `FORGE_PIPELINE_MAX_ANIM_SAMPLERS` | 512 | Samplers per clip |
| `FORGE_PIPELINE_MAX_ANIM_CHANNELS` | 512 | Channels per clip |
| `FORGE_PIPELINE_MAX_KEYFRAMES` | 65536 | Keyframes per sampler |
| `FORGE_PIPELINE_MAX_SKINS` | 64 | Skins per file |
| `FORGE_PIPELINE_MAX_SKIN_JOINTS` | 256 | Joints per skin |
| `FORGE_PIPELINE_MAX_ANIM_SET_CLIPS` | 256 | Clips in a manifest |
| `FORGE_PIPELINE_MAX_CLIP_TAGS` | 16 | Tags per clip |
| `FORGE_PIPELINE_MAX_MATERIALS` | 256 | Materials per `.fmat` file |
| `FORGE_PIPELINE_MAX_MORPH_TARGETS` | 8 | Morph targets per mesh |
| `FORGE_PIPELINE_MAX_MIP_LEVELS` | 16 | Mip levels per `.meta.json` texture |
| `FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS` | 32 | Mip levels per `.ftex` texture |

## Web UI

Browse assets in the browser:

```bash
uv run python -m pipeline serve
```

Opens at `http://localhost:8000`. API docs at `http://localhost:8000/api/docs`.

Options:

```bash
uv run python -m pipeline serve --port 3000          # custom port
uv run python -m pipeline serve --host 0.0.0.0       # listen on all interfaces
```

For frontend development with hot reload:

```bash
# Terminal 1 — backend
uv run python -m pipeline serve

# Terminal 2 — frontend (proxies API calls to backend)
cd pipeline/web
npm install   # first time only
npm run dev   # opens at http://localhost:5173
```

The frontend is built with Vite + React + TypeScript. Run `npm run build`
in `pipeline/web/` to produce static files that FastAPI serves in production.

## Why content hashes?

The pipeline fingerprints files with SHA-256 content hashes instead of
timestamps. Content hashes are:

- **Deterministic** — same bytes always produce the same hash
- **Portable** — survive `git clone`, file copies, and CI
- **Correct** — touching a file without changing it does not trigger a rebuild

## API reference

### `pipeline.config`

```python
from pipeline.config import load_config, default_config, PipelineConfig

config = load_config(Path("pipeline.toml"))
config.source_dir   # Path — where raw assets live
config.output_dir   # Path — where processed assets go
config.cache_dir    # Path — fingerprint cache location
config.plugin_settings  # dict[str, dict] — per-plugin TOML sections
```

### `pipeline.plugin`

```python
from pipeline.plugin import AssetPlugin, PluginRegistry, AssetResult

registry = PluginRegistry()
registry.discover(Path("plugins/"))       # import and register all plugins
registry.get_by_extension(".png")         # -> [TexturePlugin] (list, may be empty)
registry.get_by_name("texture")           # -> TexturePlugin or None
registry.supported_extensions             # -> {".png", ".jpg", ...}
```

### `pipeline.scanner`

```python
from pipeline.scanner import scan, FingerprintCache, FileStatus

cache = FingerprintCache(Path(".forge-cache/fingerprints.json"))
files = scan(source_dir, supported_extensions, cache)

for f in files:
    f.path         # absolute path
    f.relative     # relative to source_dir
    f.fingerprint  # SHA-256 hex digest
    f.status       # FileStatus.NEW / CHANGED / UNCHANGED
```

## Testing

```bash
# Run pipeline tests
uv run pytest tests/pipeline/ -v

# Run all tests (C + Python)
ctest --test-dir build && uv run pytest tests/pipeline/
```

## Lesson track

The pipeline is built incrementally across the
[asset pipeline lessons](../lessons/assets/):

| Lesson | What it adds to the pipeline |
|---|---|
| [01 — Pipeline Scaffold](../lessons/assets/01-pipeline-scaffold/) | CLI, plugin discovery, scanning, fingerprinting, TOML config |
| [02 — Texture Processing](../lessons/assets/02-texture-processing/) | Image resize, format conversion, mipmaps, metadata sidecars |
| [03 — Mesh Processing](../lessons/assets/03-mesh-processing/) | C tool for vertex/index optimization (meshoptimizer, MikkTSpace) |
| [04 — Procedural Geometry](../lessons/assets/04-procedural-geometry/) | `common/shapes/forge_shapes.h` header-only library |
| [05 — Asset Bundles](../lessons/assets/05-asset-bundles/) | Packing, compression, random-access table of contents |
| [06 — Loading Processed Assets](../lessons/assets/06-loading-processed-assets/) | Header-only C loader for .fmesh and pipeline-processed textures |
| [07 — Materials](../lessons/assets/07-materials/) | PBR material support, .fmat sidecars, multi-primitive meshes |
| [08 — Animations](../lessons/assets/08-animations/) | glTF animation parsing and runtime keyframe evaluation |
| [09 — Scene Hierarchy](../lessons/assets/09-scene-hierarchy/) | Node tree extraction, `.fscene` binary format, `forge-scene-tool` |
| [10 — Animation Loader](../lessons/assets/10-animation-loader/) | glTF animation extraction, `.fanim` binary format |
| [11 — Animation Manifest](../lessons/assets/11-animation-manifest/) | Animation metadata, clip registry, manifest generation |
| [12 — Skinned Animations](../lessons/assets/12-skinned-animations/) | Joint hierarchies, inverse bind matrices, `.fskin` format |
| [13 — Morph Targets](../lessons/assets/13-morph-targets/) | Morph target deltas, weight channels, blend shape pipeline |
| [14 — Web UI Scaffold](../lessons/assets/14-web-ui-scaffold/) | FastAPI backend, Vite + React frontend, asset browser, WebSocket status |
| [15 — Asset Preview](../lessons/assets/15-asset-preview/) | react-three-fiber 3D mesh preview, texture channel isolation, file serving |
| [16 — Import Settings Editor](../lessons/assets/16-import-settings-editor/) | Per-asset `.import.toml` sidecars, schema-driven settings UI, single-asset reprocessing |
| [17 — Texture Atlas Packing](../lessons/assets/17-texture-atlas/) | Guillotine bin-packing, atlas compositing, UV remapping metadata |

## License

[zlib](../LICENSE) — matching SDL.
