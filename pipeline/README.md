# forge-pipeline

A plugin-based asset processing pipeline for game and graphics projects.
Scan source files, fingerprint them by content hash, and process only what
changed — with plugins for each asset type.

Part of [forge-gpu](https://github.com/RosyGameStudio/forge-gpu). Learn how
it works in the [asset pipeline lessons](../lessons/assets/).

## Installation

```bash
# From the forge-gpu repository root
pip install -e ".[dev]"

# Or install directly (once published)
pip install forge-pipeline
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

The pipeline has four components:

| Component | Module | Purpose |
|---|---|---|
| Configuration | `pipeline.config` | Parse TOML, produce typed `PipelineConfig` dataclass |
| Plugin system | `pipeline.plugin` | `AssetPlugin` base class, `PluginRegistry`, file-based discovery |
| Scanner | `pipeline.scanner` | Walk directories, SHA-256 fingerprint, classify NEW/CHANGED/UNCHANGED |
| Bundler | `pipeline.bundler` | Pack processed assets into compressed bundles with random-access TOC |
| CLI | `pipeline.__main__` | `argparse` entry point tying everything together |
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

| Plugin | Extensions | Status |
|---|---|---|
| `texture` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` | Resize, format convert, mipmaps, metadata |
| `mesh` | `.obj`, `.gltf`, `.glb` | Scaffold (no-op) |
| `animation` | `.gltf`, `.glb` | Animation extraction and keyframe processing |
| `scene` | `.gltf`, `.glb` | Scene hierarchy extraction via `forge-scene-tool` |

The texture plugin was built in Lesson 02.  Mesh optimization with
meshoptimizer and MikkTSpace is added in Lesson 03.  The animation plugin
was added in Lesson 08.

## Web UI

Browse assets in the browser:

```bash
python -m pipeline serve
```

Opens at `http://localhost:8000`. API docs at `http://localhost:8000/api/docs`.

Options:

```bash
python -m pipeline serve --port 3000          # custom port
python -m pipeline serve --host 0.0.0.0       # listen on all interfaces
```

For frontend development with hot reload:

```bash
# Terminal 1 — backend
python -m pipeline serve

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
registry.get_by_extension(".png")         # -> TexturePlugin or None
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
pytest tests/pipeline/ -v

# Run all tests (C + Python)
ctest --test-dir build && pytest tests/pipeline/
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

## License

[zlib](../LICENSE) — matching SDL.
