# Pipeline Plugins

Built-in asset type plugins for the forge-pipeline processing system. Each
plugin is an `AssetPlugin` subclass discovered automatically at startup.

## Plugins

| File | Plugin | Extensions | Purpose |
|------|--------|------------|---------|
| `texture.py` | `texture` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` | Resize, format conversion, mipmaps, metadata sidecars |
| `mesh.py` | `mesh` | `.obj`, `.gltf`, `.glb` | Vertex/index optimization via `forge-mesh-tool` |
| `animation.py` | `animation` | `.gltf`, `.glb` | Animation extraction and keyframe processing |
| `scene.py` | `scene` | `.gltf`, `.glb` | Scene hierarchy extraction via `forge-scene-tool` |
| `atlas.py` | `atlas` | (post-processing) | Guillotine bin-pack processed textures into a single atlas |

## Adding a plugin

Create a Python file in this directory with an `AssetPlugin` subclass. The
pipeline discovers it automatically — no registration code needed. See
`pipeline/plugin.py` for the base class and `pipeline/README.md` for details.
