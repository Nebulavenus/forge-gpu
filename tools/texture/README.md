# forge-texture-tool

Build-time KTX2 to `.ftex` transcoder. Reads a UASTC KTX2 file (produced by
basisu), transcodes each mip level to BC7 or BC5 using the Basis Universal
transcoder, and writes a `.ftex` file containing GPU-ready compressed blocks.

The `.ftex` format is a simple binary layout that the runtime can load with a
single `SDL_LoadFile` + pointer arithmetic — no transcoding or third-party
library needed at runtime.

The Python pipeline plugin (`pipeline/plugins/texture.py`) invokes this tool
as a subprocess. [Asset Lesson 05](../../lessons/assets/05-asset-bundles/)
and [GPU Lesson 42](../../lessons/gpu/42-pipeline-texture-compression/) teach
how the compression pipeline works end to end.

## Usage

```bash
forge_texture_tool <input.ktx2> <output.ftex> <format>
```

### Formats

| Format       | Description                                         |
|--------------|-----------------------------------------------------|
| `bc7_srgb`   | BC7 in sRGB color space (base color, emissive)      |
| `bc7_unorm`  | BC7 in linear space (metallic-roughness, occlusion) |
| `bc5_unorm`  | BC5 two-channel (normal maps: RG only, Z reconstructed in shader) |

## Build

Built automatically by CMake when the `forge_texture_tool` target is enabled.
Requires the Basis Universal C wrapper (`basisu_c_wrapper.h`) from
`third_party/`.

## Files

| File             | Description                           |
|------------------|---------------------------------------|
| `main.c`         | Tool entry point and transcoding logic |
| `CMakeLists.txt` | Build configuration                   |
