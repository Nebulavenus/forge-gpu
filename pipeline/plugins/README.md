# Pipeline Plugins

Built-in asset processing plugins for the forge pipeline. Each plugin
handles one asset type, implementing the `AssetPlugin` interface from
`pipeline.plugin`.

## Plugins

| Plugin | File | Extensions | Output | External tool |
|--------|------|------------|--------|---------------|
| `texture` | `texture.py` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` | Resized image + mipmaps + optional `.ktx2`/`.astc` | `basisu`, `astcenc` (optional) |
| `mesh` | `mesh.py` | `.obj`, `.gltf`, `.glb` | `.fmesh` binary | `forge-mesh-tool` |
| `scene` | `scene.py` | `.gltf`, `.glb` | `.fscene` binary | `forge-scene-tool` |
| `animation` | `animation.py` | `.gltf`, `.glb` | `.fanim` binaries + `.fanims` manifest | `forge-anim-tool` |
| `atlas` | `atlas.py` | _(none — post-processing)_ | `atlas.png` + `atlas.json` | — |

## How plugins are discovered

The `PluginRegistry` imports every `.py` file in this directory (skipping
files starting with `_`), finds all `AssetPlugin` subclasses, and
registers them. Adding a new plugin is one file — no registration code
needed.

## Plugin interface

```python
from pathlib import Path
from pipeline.plugin import AssetPlugin, AssetResult

class MyPlugin(AssetPlugin):
    name = "my_type"
    extensions = [".xyz"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        # Transform source into processed output under output_dir.
        # settings comes from [my_type] in pipeline.toml.
        output_path = output_dir / f"{source.stem}.processed"
        output_path.write_bytes(source.read_bytes())
        return AssetResult(source=source, output=output_path, metadata={})
```

## Settings

Each plugin reads its configuration from a `[plugin_name]` section in
`pipeline.toml`. Per-asset overrides use `.import.toml` sidecar files
alongside the source asset. See `pipeline.import_settings` for the
three-layer merge (schema defaults, global, per-asset).

## External tools

The mesh, scene, and animation plugins shell out to compiled C tools
(`forge-mesh-tool`, `forge-scene-tool`, `forge-anim-tool`). If a tool
is not installed, the plugin logs a warning and returns
`metadata={"processed": False}` — the pipeline continues without
failing.

The texture plugin optionally calls `basisu` (Basis Universal) or
`astcenc` (ASTC Encoder) for GPU texture compression. Without these
tools, textures are processed normally and compression is skipped.
