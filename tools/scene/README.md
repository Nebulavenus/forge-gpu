# forge-scene-tool

Command-line tool that reads a glTF/GLB file, extracts the node hierarchy, and
writes a compact `.fscene` binary plus a `.meta.json` metadata sidecar.

The Python pipeline invokes this tool as a subprocess via the `scene` plugin.
[Asset Lesson 09](../../lessons/assets/09-scene-hierarchy/) teaches how it works.

## Files

| File | Purpose |
|------|---------|
| `main.c` | Tool implementation -- glTF parsing, `.fscene` binary writer, `.meta.json` output |
| `CMakeLists.txt` | Build configuration |

## Building

```bash
cmake --build build --target forge_scene_tool
```

## Usage

```bash
./build/forge_scene_tool input.gltf output.fscene
```

The tool produces two output files:

- `output.fscene` -- binary node hierarchy (nodes, mesh table, children array)
- `output.meta.json` -- human-readable metadata sidecar

## Binary format

The `.fscene` format stores the scene hierarchy in a flat, random-access layout
(all little-endian). See the comment at the top of `main.c` for the full
specification.
