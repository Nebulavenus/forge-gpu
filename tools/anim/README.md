# forge-anim-tool

CLI animation extraction tool that reads a glTF/GLB file, extracts animation
clips, and writes a binary `.fanim` file with a `.meta.json` metadata sidecar.

The Python pipeline plugin (`pipeline/plugins/anim.py`) invokes this tool as
a subprocess. [Asset Lesson 08](../../lessons/assets/08-animations/) teaches
how it works.

## Usage

```bash
forge-anim-tool <input.gltf> <output.fanim> [--verbose]
```

### Options

| Flag        | Description                    |
|-------------|--------------------------------|
| `--verbose` | Print extraction statistics    |

## .fanim binary format

All values are little-endian.

**Header:**

| Field        | Size    | Description                    |
|--------------|---------|--------------------------------|
| `magic`      | 4 bytes | `"FANM"`                       |
| `version`    | u32     | 1                              |
| `clip_count` | u32     | Number of animation clips      |

**Per clip** (immediately after header):

| Field           | Size     | Description                              |
|-----------------|----------|------------------------------------------|
| `name`          | 64 bytes | Null-terminated clip name (zero-padded)  |
| `duration`      | float    | Max timestamp across all samplers        |
| `sampler_count` | u32      | Number of samplers in this clip          |
| `channel_count` | u32      | Number of channels in this clip          |

**Per sampler** (after clip header):

| Field              | Size                                  | Description                              |
|--------------------|---------------------------------------|------------------------------------------|
| `keyframe_count`   | u32                                   | Number of keyframes                      |
| `value_components` | u32                                   | 3 (translation/scale) or 4 (rotation)   |
| `interpolation`    | u32                                   | 0 = LINEAR, 1 = STEP                    |
| `timestamps[]`     | float x keyframe_count                | Keyframe timestamps                      |
| `values[]`         | float x keyframe_count x components   | Keyframe values                          |

**Per channel** (after all samplers for this clip):

| Field           | Size | Description                                       |
|-----------------|------|---------------------------------------------------|
| `target_node`   | i32  | Index into scene nodes (-1 if unset)              |
| `target_path`   | u32  | 0 = translation, 1 = rotation, 2 = scale         |
| `sampler_index` | u32  | Index into this clip's sampler array              |

## Dependencies

- SDL3, cJSON
- `common/gltf/forge_gltf.h` (glTF parser)

## Files

- `main.c` -- Tool implementation
- `CMakeLists.txt` -- Build configuration
