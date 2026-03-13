# Asset Lesson 10 — Animation Loader and Per-Clip Export

Adds a runtime `.fanim` loader to `forge_pipeline.h` and extends
`forge-anim-tool` with per-clip export. After this lesson, gameplay code
can load animation data from processed `.fanim` files without touching the
glTF parser, and artists can edit individual clips without rebuilding the
entire animation set.

## What you'll learn

- How to parse the `.fanim` binary format at runtime — sequential walking
  through clips, samplers, and channels
- Per-sampler memory layout: separately allocated timestamp and value arrays
- Validation strategy: bounds checking at every level (clip count, sampler
  count, keyframe count, channel references)
- Per-clip export: splitting a multi-clip `.fanim` into individual files
- Filename sanitization: converting clip names to safe filesystem identifiers
- The `.fanims` stub manifest: a starting point for named clip lookup
  (Lesson 11 builds this into a full system)

## Result

After completing this lesson:

- `forge_pipeline.h` can load `.fanim` files produced by `forge-anim-tool`
- `forge-anim-tool --split --output-dir <dir>` writes one `.fanim` per clip
  plus a `.fanims` stub manifest
- The animation pipeline plugin defaults to split mode
- Tests validate the loader across valid files, error cases, data integrity,
  free safety, and value validation

Load animation data at runtime:

```c
ForgePipelineAnimFile file;
if (forge_pipeline_load_animation("walk.fanim", &file)) {
    if (file.clip_count > 0) {
        /* file.clips[0].samplers, .channels are ready to use */
        /* ... evaluate keyframes ... */
    }
    forge_pipeline_free_animation(&file);
}
```

Export per-clip files from the command line:

```bash
forge-anim-tool model.gltf --split --output-dir output/
# Creates: output/walk.fanim, output/run.fanim, output/model.fanims
```

## The `.fanim` binary format

Lesson 08 defined the `.fanim` format. The binary layout is sequential —
no offset tables, no random access. The loader walks from header to clips
to samplers to channels in a single pass:

```text
Header (12 bytes):
  magic         4B    "FANM"
  version       u32   1
  clip_count    u32

Per clip:
  name              64B   null-terminated, zero-padded
  duration          f32   maximum timestamp across all samplers
  sampler_count     u32
  channel_count     u32

  Per sampler (header: 12 bytes, then inline data):
    keyframe_count    u32   number of keyframes
    value_components  u32   3 (translation/scale) or 4 (rotation)
    interpolation     u32   0 = LINEAR, 1 = STEP
    timestamps[]      f32 × keyframe_count
    values[]          f32 × (keyframe_count × value_components)

  Per channel (12 bytes):
    target_node       i32   index into scene nodes (-1 if unset)
    target_path       u32   0 = translation, 1 = rotation, 2 = scale
    sampler_index     u32   index into this clip's sampler array
```

## Runtime loader

The loader (`forge_pipeline_load_animation`) reads the file into memory with
`SDL_LoadFile`, validates the header, then walks each clip sequentially.
Per-sampler timestamp and value arrays are individually heap-allocated so
they can be freed independently.

### Validation

Every level has bounds checks:

| Level | Validation |
|---|---|
| Header | Magic bytes, version, file size >= 12 bytes |
| Clip count | <= `FORGE_PIPELINE_MAX_ANIM_CLIPS` (256) |
| Sampler count | <= `FORGE_PIPELINE_MAX_ANIM_SAMPLERS` (512) per clip |
| Channel count | <= `FORGE_PIPELINE_MAX_ANIM_CHANNELS` (512) per clip |
| Keyframe count | <= `FORGE_PIPELINE_MAX_KEYFRAMES` (65536) per sampler |
| Value components | Must be 3 or 4 |
| Channel sampler index | Must be < clip's sampler_count |
| Truncation | Pointer vs. end-of-file check before every read |

On any error, the loader frees all partially-allocated data and returns
false. `forge_pipeline_free_animation` is safe to call on a zeroed, NULL,
or already-freed struct.

## Per-clip export

The `--split` flag changes `forge-anim-tool` from writing a single `.fanim`
(all clips packed together) to writing one `.fanim` per clip:

```bash
# Legacy mode (Lesson 08) — all clips in one file:
forge-anim-tool CesiumMan.gltf output.fanim

# Split mode (Lesson 10) — one file per clip:
forge-anim-tool CesiumMan.gltf --split --output-dir output/
```

### Filename sanitization

Clip names from glTF files can contain spaces, special characters, and
mixed case. The tool sanitizes names for filesystem use:

- Uppercase → lowercase
- Spaces and special characters → underscores
- Consecutive underscores collapsed
- Leading/trailing underscores stripped
- Empty names → `"clip"`

Examples: `"Walk Cycle"` → `walk_cycle`, `"Idle (Loop)"` → `idle_loop`

### Stub manifest

In split mode, the tool also writes a `.fanims` JSON file listing the
exported clips. This is a starting point — Lesson 11 extends it into a
full manifest with loop flags, tags, and named lookup.

## Pipeline plugin

The animation plugin (`pipeline/plugins/animation.py`) now defaults to
split mode. The `[animation]` section in `pipeline.toml` controls the
behaviour:

```toml
[animation]
split = true    # Per-clip export (default)
# split = false  # Legacy single-file mode
```

## Key concepts

- **Sequential binary format** — `.fanim` files are read in a single pass
  from header to clips to samplers to channels. No offset tables or random
  access. This keeps the writer and loader simple.
- **Per-sampler allocation** — each sampler's timestamps and values are
  individually heap-allocated. This allows the loader to handle variable-size
  data without a fixed upper bound on keyframe count.
- **Defensive validation** — bounds checks at every level (clip count,
  sampler count, keyframe count, value components, channel sampler index,
  interpolation mode, target path). Truncation is detected by comparing the
  read pointer against end-of-file before every read.
- **Name deduplication** — sanitized clip names can collide (e.g. "Walk
  Cycle" and "Walk-Cycle" both become `walk_cycle`). The exporter appends
  numeric suffixes (`_1`, `_2`) on collision so no file is silently
  overwritten.
- **Manifest as source of truth** — the pipeline plugin reads the `.fanims`
  manifest to determine which clip files were produced, rather than scanning
  the output directory (which could contain stale files from a previous run).

## Building

Build the animation tool and tests:

```bash
cmake --build build --target forge_anim_tool
cmake --build build --target test_pipeline
```

Run the animation loader tests:

```bash
ctest --test-dir build -R pipeline
```

Test per-clip export:

```bash
./build/tools/anim/forge_anim_tool assets/models/CesiumMan/CesiumMan.gltf \
    --split --output-dir /tmp/cesiumman/ --verbose
ls /tmp/cesiumman/*.fanim
```

The pipeline plugin defaults to split mode. To use legacy single-file mode,
set `split = false` in the `[animation]` section of `pipeline.toml`.

## Exercises

1. **Add a `--clip` filter** — extend `forge-anim-tool` to accept
   `--clip <name>` and export only matching clips. Useful for re-exporting
   a single edited animation.

2. **Duration validation** — add a test that verifies the loaded duration
   matches the maximum timestamp across all samplers in the clip.

3. **Loader statistics** — add a function that returns total keyframe count
   and memory usage for a loaded `ForgePipelineAnimFile`. Use it for debug
   UI showing animation memory budgets.

## Cross-references

- [Asset Lesson 08 — Animations](../08-animations/) — glTF animation parsing
  and the `.fanim` binary format
- [Asset Lesson 11 — Animation Manifest](../11-animation-manifest/) — JSON
  manifest with named clip lookup (next lesson)
- `common/gltf/forge_gltf_anim.h` — runtime keyframe evaluation (slerp,
  binary search)
