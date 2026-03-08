# Lesson 02 — Texture Processing

Turn the scaffold's no-op texture plugin into a real image processor: load
source images with Pillow, resize them to fit GPU texture limits, convert
formats, generate a complete mipmap chain, and write metadata sidecar files
for reproducible builds.

## What you'll learn

- Load and process image files with Pillow (PNG, JPG, TGA, BMP)
- Resize images to fit within a configurable maximum size while preserving
  aspect ratio (Lanczos resampling)
- Convert between image formats (TGA/BMP source to PNG/JPG output)
- Generate mipmap chains — progressively halved images from full size to 1x1
- Write `.meta.json` sidecar files that record processing settings and output
  dimensions
- Wire a real plugin into the pipeline's processing loop (not just scanning)
- Skip unchanged assets with content-hash fingerprinting from Lesson 01

## Result

```text
$ cd lessons/assets/02-texture-processing
$ forge-pipeline -v

pipeline: Loaded config from pipeline.toml
pipeline: Loaded 2 plugin(s)
pipeline:   mesh          .obj, .gltf, .glb
pipeline:   texture       .png, .jpg, .jpeg, .tga, .bmp
pipeline: Scanned 3 file(s) in assets/raw — 3 new, 0 changed, 0 unchanged

Scanned 3 file(s) in assets/raw:
  3 new
  0 changed
  0 unchanged

  [NEW]      textures/checker.png  (texture)
  [NEW]      textures/gradient.png  (texture)
  [NEW]      textures/solid.tga  (texture)

Processing 3 file(s)...

Done: 3 processed, 0 failed, 0 unchanged.
```

After processing, the output directory contains the base images, mip chains,
and metadata sidecars:

```text
assets/processed/textures/
  checker.png              # base image (mip 0)
  checker_mip1.png         # 128x128
  checker_mip2.png         # 64x64
  ...
  checker_mip8.png         # 1x1
  checker.meta.json        # processing metadata
  gradient.png
  gradient_mip1.png
  ...
  gradient.meta.json
  solid.png                # converted from TGA to PNG
  solid_mip1.png
  ...
  solid.meta.json
```

Running a second time with no file changes:

```text
$ forge-pipeline
All files up to date — nothing to process.
```

## Architecture

![Texture pipeline data flow](assets/texture-pipeline.png)

The texture plugin receives a source file path, an output directory, and a
settings dictionary from the `[texture]` section of `pipeline.toml`. It
processes each image through five stages:

1. **Load** — Open the source file with Pillow and normalize to RGB or RGBA
2. **Resize** — Clamp width and height to `max_size`, preserving aspect ratio
3. **Save base** — Write the processed image in the configured output format
4. **Generate mipmaps** — Create progressively halved copies down to 1x1
5. **Write metadata** — Record source info, output dimensions, mip levels, and
   settings in a `.meta.json` sidecar

## Configuration

The `[texture]` section in `pipeline.toml` controls processing:

```toml
[texture]
max_size = 2048          # Clamp width and height to this limit
generate_mipmaps = true  # Create mip chain alongside the base image
output_format = "png"    # Output format: png, jpg, or bmp
jpg_quality = 90         # JPEG quality (1-100, only for jpg output)
```

All settings have sensible defaults — you can omit the entire section and
the plugin will process at 2048 max size with PNG output and mipmaps enabled.

## Resizing — preserving aspect ratio

When a source image exceeds `max_size` on either axis, the plugin scales it
down proportionally. The key is computing a single scale factor from the
*more constrained* axis:

```python
scale = min(max_size / width, max_size / height)
new_w = max(1, int(width * scale))
new_h = max(1, int(height * scale))
```

A 4000x2000 image with `max_size = 1024` becomes 1024x512 — the width hits
the limit first, and the height scales proportionally. Images already within
limits pass through unchanged.

The plugin uses Lanczos resampling (`Image.LANCZOS`), which produces the
sharpest downscaled results at the cost of slightly more computation. For
game assets where texture quality matters, this is the right trade-off.

## Mipmaps — why and how

### Why mipmaps matter

When a 1024x1024 texture is displayed on a surface that covers only 16x16
pixels on screen, the GPU must sample from a texture far larger than the
rendered size. Without mipmaps this causes:

- **Aliasing** — shimmering patterns on distant or angled surfaces
- **Cache thrash** — the GPU reads scattered texels from a large texture,
  wasting memory bandwidth
- **Wasted bandwidth** — transferring full-resolution data that will be
  averaged down anyway

Mipmaps solve this by pre-computing smaller versions. The GPU picks the mip
level closest to the rendered size, giving correct filtering with minimal
bandwidth.

### Mip chain structure

Each mip level halves both dimensions until the smallest axis reaches 1:

| Level | Size | Pixels |
|-------|------|--------|
| 0 (base) | 256x256 | 65,536 |
| 1 | 128x128 | 16,384 |
| 2 | 64x64 | 4,096 |
| 3 | 32x32 | 1,024 |
| 4 | 16x16 | 256 |
| 5 | 8x8 | 64 |
| 6 | 4x4 | 16 |
| 7 | 2x2 | 4 |
| 8 | 1x1 | 1 |

The total number of mip levels for a texture is:

```text
levels = 1 + floor(log2(max(width, height)))
```

A 256x256 texture has 9 levels. A 512x256 texture has 10 levels (driven by
the larger axis). The entire mip chain uses only 33% more memory than the
base level alone — a small cost for significant rendering quality improvement.

### How the plugin generates mipmaps

```python
current = img  # start with the base image
for level in range(1, levels):
    mip_w = max(1, width >> level)   # halve via bit shift
    mip_h = max(1, height >> level)
    current = current.resize((mip_w, mip_h), Image.LANCZOS)
    current.save(f"{stem}_mip{level}{ext}")
```

Each level is resized from the *previous* level, not from the original. This
is called a *mip chain* or *image pyramid*. Resizing from the previous level
is faster and avoids aliasing artifacts that can occur when downsampling by
large factors in a single step.

## Metadata sidecar files

Every processed texture gets a `.meta.json` sidecar:

```json
{
  "source": "checker.png",
  "output": "checker.png",
  "original_width": 256,
  "original_height": 256,
  "output_width": 256,
  "output_height": 256,
  "mip_levels": [
    {"level": 0, "width": 256, "height": 256},
    {"level": 1, "width": 128, "height": 128},
    {"level": 2, "width": 64, "height": 64}
  ],
  "settings": {
    "max_size": 512,
    "generate_mipmaps": true,
    "output_format": "png"
  }
}
```

Sidecar files serve three purposes:

1. **Reproducibility** — Record exactly which settings produced each output.
   Change a setting and re-run to see the difference.
2. **Asset loading** — GPU code can read the sidecar to learn the mip count
   and dimensions without parsing the image files.
3. **Debugging** — When a texture looks wrong, check the sidecar to see if
   it was resized, what format was used, and how many mip levels were
   generated.

## Processing loop

Lesson 01 built the scanner but only reported what would be processed. This
lesson adds the processing loop to `__main__.py`:

```python
for f in to_process:
    plugin = registry.get_by_extension(f.extension)
    settings = config.plugin_settings.get(plugin.name, {})
    output_subdir = config.output_dir / f.relative.parent

    result = plugin.process(f.path, output_subdir, settings)
```

The pipeline creates the output directory structure mirroring the source tree,
so `assets/raw/textures/hero.png` produces output in
`assets/processed/textures/hero.png`.

The fingerprint cache is updated *after* successful processing. If processing
fails partway through, the failed files remain marked as NEW/CHANGED and will
be retried on the next run. This prevents a failed build from silently
skipping broken assets.

## Format conversion

The plugin converts any supported input format to the configured output
format. This is useful for standardizing on a single format across the
project:

| Input | Output (`output_format = "png"`) | Notes |
|-------|----------------------------------|-------|
| `.png` | `.png` | Lossless round-trip |
| `.jpg` | `.png` | Decompresses JPEG, saves as lossless PNG |
| `.tga` | `.png` | Converts Targa to PNG (smaller, widely supported) |
| `.bmp` | `.png` | Converts uncompressed BMP to compressed PNG |

When outputting JPEG, RGBA images are automatically converted to RGB (JPEG
does not support alpha channels). The `jpg_quality` setting controls the
compression level.

## Building

```bash
# From the forge-gpu repository root
pip install -e ".[dev]"
```

This installs the `forge-pipeline` CLI with Pillow for image processing and
pytest + ruff for development.

## Running

### Try it out

```bash
cd lessons/assets/02-texture-processing
forge-pipeline -v              # process sample textures
forge-pipeline                 # second run — all unchanged
forge-pipeline --dry-run       # scan only, do not process
```

### Inspect the output

```bash
cat assets/processed/textures/checker.meta.json
ls assets/processed/textures/
```

### Run the tests

```bash
# From the repository root
pytest tests/pipeline/ -v
```

22 new tests covering dimension clamping, mip count calculation, full
processing (resize, mipmaps, format conversion, metadata), error handling,
and edge cases (RGBA, TGA input, nested output directories).

## Key concepts

### Plugin lifecycle

In Lesson 01, plugins were scaffolds — they registered their extensions but
did nothing. This lesson shows the full plugin lifecycle:

1. **Register** — Plugin declares `name` and `extensions` (unchanged from L01)
2. **Discover** — Registry finds and imports the plugin (unchanged from L01)
3. **Configure** — Plugin reads its `[texture]` settings from TOML
4. **Process** — Plugin receives a source path and settings, produces outputs
5. **Report** — Plugin returns an `AssetResult` with output path and metadata

### Incremental builds

The fingerprint cache from Lesson 01 drives incremental processing. Only
files classified as NEW or CHANGED are passed to plugins. The cache is
updated only after successful processing, so failed files are retried
automatically.

### Image processing pipeline

The texture plugin demonstrates a common pattern in asset pipelines: a
sequence of transformations applied to each file. Each step (load, resize,
convert, mipmap, metadata) is independent and testable, but they compose
into a complete processing workflow.

## Where it connects

| Track | Connection |
|---|---|
| [GPU Lesson 04 — Textures & Samplers](../../gpu/04-textures-and-samplers/) | Processed textures are ready for `SDL_CreateGPUTexture` — mipmaps map directly to mip levels in the GPU texture |
| [GPU Lesson 05 — Mipmaps](../../gpu/05-mipmaps/) | Explains how the GPU uses the mip chain this plugin generates; sampler mipmap mode selects between levels |
| [Asset Lesson 01 — Pipeline Scaffold](../01-pipeline-scaffold/) | Built the scanner, fingerprinting, and plugin discovery that this lesson extends |
| [Asset Lesson 03 — Mesh Processing](../03-mesh-processing/) | Next lesson: the same plugin pattern applied to 3D models with compiled C tools |

## Exercises

1. **Add WebP output** — Pillow supports WebP. Add `"webp"` to the format
   map and test it. WebP offers better compression than PNG with optional
   lossy mode — compare file sizes.

2. **Power-of-two resize** — Many older GPUs require textures with
   power-of-two dimensions. Add a `power_of_two = true` setting that rounds
   the resized dimensions up to the nearest power of two.

3. **Channel statistics** — Add min/max/mean values per channel (R, G, B, A)
   to the metadata sidecar. This is useful for debugging — a normal map with
   an unexpected min/max likely has encoding issues.

4. **Atlas packing** — Write a new plugin that combines multiple small
   textures into a single atlas texture. Output a JSON file mapping each
   source texture to its UV rectangle in the atlas.

5. **Compression estimation** — For each texture, estimate the compressed
   GPU memory usage if it were stored as BC7 (8 bits/pixel) vs. the
   uncompressed size (32 bits/pixel RGBA). Add this to the metadata sidecar
   and print a summary showing total memory savings.

## Further reading

- [Pipeline API reference](../../../pipeline/README.md) — Full API docs and
  usage guide
- [Pillow documentation](https://pillow.readthedocs.io/) — The image
  processing library used by the texture plugin
- [GPU Lesson 05 — Mipmaps](../../gpu/05-mipmaps/) — How the GPU uses mip
  chains for texture filtering
- [Texture filtering (Learn OpenGL)](https://learnopengl.com/Getting-started/Textures) —
  Visual explanation of nearest vs. linear filtering and mipmap selection
- [BC7 texture compression](https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc7-format) —
  The GPU-native compressed format that a production pipeline would target
