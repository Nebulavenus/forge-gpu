# forge-mesh-tool

CLI mesh processing tool that reads OBJ or glTF/GLB files and produces
optimized `.fmesh` binary files with a `.meta.json` sidecar. The tool
performs vertex deduplication, GPU-friendly index/vertex reordering,
MikkTSpace tangent generation, and LOD simplification.

The Python pipeline plugin (`pipeline/plugins/mesh.py`) invokes this tool as
a subprocess. [Asset Lesson 03](../../lessons/assets/03-mesh-processing/)
teaches how it works.

## Usage

```bash
forge_mesh_tool <input> <output.fmesh> [options]
```

### Options

| Flag                          | Description                              |
|-------------------------------|------------------------------------------|
| `--lod-levels 1.0,0.5,0.25`  | LOD target ratios (comma-separated)      |
| `--no-deduplicate`            | Skip vertex deduplication                |
| `--no-tangents`               | Skip tangent generation                  |
| `--no-optimize`               | Skip index/vertex optimization           |
| `--verbose`                   | Print processing statistics              |

## Dependencies

- [meshoptimizer](https://github.com/zeux/meshoptimizer) v0.22 (fetched by CMake)
- [MikkTSpace](https://github.com/mmikk/MikkTSpace) (fetched by CMake)
- SDL3, cJSON

## Files

- `main.c` -- Tool implementation
- `CMakeLists.txt` -- Build configuration with FetchContent for dependencies
