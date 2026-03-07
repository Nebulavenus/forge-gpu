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
| CLI | `pipeline.__main__` | `argparse` entry point tying everything together |

```text
pipeline.toml --> CLI (__main__.py)
                   |
                   +---> Plugin Discovery (plugins/*.py)
                   |        register by name + extension
                   |
                   +---> Scanner (scanner.py)
                            |
                            +-- walk source directory
                            +-- fingerprint (SHA-256)
                            +-- compare against cache
                            +-- classify: NEW / CHANGED / UNCHANGED
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
| `texture` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` | Scaffold (no-op) |
| `mesh` | `.obj`, `.gltf`, `.glb` | Scaffold (no-op) |

Real processing is added in later lessons: texture compression (Lesson 02),
mesh optimization with meshoptimizer and MikkTSpace (Lesson 03).

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
| 02 — Texture Processing | Image import plugin (resize, compress, mipmaps) |
| 03 — Mesh Processing | C tool for vertex/index optimization (meshoptimizer, MikkTSpace) |
| 04 — Procedural Geometry | `common/shapes/forge_shapes.h` header-only library |
| 05 — Asset Bundles | Packing, compression, random-access table of contents |

## License

[zlib](../LICENSE) — matching SDL.
