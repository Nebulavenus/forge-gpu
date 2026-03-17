# Tools

Standalone command-line tools for the forge-gpu asset pipeline. These are
compiled C programs that perform performance-critical processing and are
invoked as subprocesses by the Python pipeline.

## Contents

| Directory  | Description                                              |
|------------|----------------------------------------------------------|
| `common/`  | Shared header-only helpers (binary I/O) used by all tools. |
| `mesh/`    | Mesh processing: deduplication, optimization, tangent generation, LOD simplification. Reads OBJ/glTF, writes `.fmesh` binary format. |
| `anim/`    | Animation processing: glTF animation extraction and conversion. |
| `scene/`   | Scene hierarchy extraction: reads glTF/GLB, writes `.fscene` binary with node tree and `.meta.json` sidecar. |
| `texture/` | Texture compression: KTX2 to `.ftex` transcoding via Basis Universal (BC7/BC5). |

## Building

Tools are built as part of the main CMake project:

```bash
cmake --build build --target forge_mesh_tool
cmake --build build --target forge_anim_tool
cmake --build build --target forge_scene_tool
cmake --build build --target forge_texture_tool
```
