# Tools

Standalone command-line tools for the forge-gpu asset pipeline. These are
compiled C programs that perform performance-critical processing and are
invoked as subprocesses by the Python pipeline.

## Contents

| Tool   | Description                                              |
|--------|----------------------------------------------------------|
| `mesh/` | Mesh processing: deduplication, optimization, tangent generation, LOD simplification. Reads OBJ/glTF, writes `.fmesh` binary format. |
| `anim/` | Animation processing: glTF animation extraction and conversion. |

## Building

Tools are built as part of the main CMake project:

```bash
cmake --build build --target forge_mesh_tool
cmake --build build --target forge_anim_tool
```
