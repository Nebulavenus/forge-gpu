# Lesson 05 — Mipmaps

## What you'll learn

- Why textures need mipmaps (aliasing when surfaces are viewed at a distance)
- Creating textures with multiple mip levels (`num_levels = log2(size) + 1`)
- Auto-generating mipmaps with `SDL_GenerateMipmapsForGPUTexture`
- Comparing sampler mipmap modes (trilinear, bilinear+nearest, no mipmaps)
- Procedural texture generation (no external image files)
- UV tiling to make aliasing visible

## Result

![Lesson 05 screenshot](assets/screenshot.png)

A pulsing checkerboard quad that cycles between three sampler modes when you
press **SPACE**:

1. **Trilinear** — smooth mip transitions, no aliasing
2. **Bilinear + nearest mip** — smooth within levels, visible "pops" between
3. **No mipmaps** — severe aliasing, flickering checkerboard

The quad scales up and down with a sine wave so you can observe the mip
level transitions in real time.

## Key concepts

### The aliasing problem

When a texture is viewed at a distance, many texels map to a single screen
pixel. Without mipmaps, the GPU samples only a few texels and misses the rest,
causing shimmering and moire patterns. This lesson uses an 8×8 checkerboard
tiled 2× (16 visible squares per axis) to make aliasing obvious while
still looking like a recognisable checkerboard up close.

### Creating a mipmapped texture

Two key differences from Lesson 04:

```c
/* 1. Usage flags — COLOR_TARGET is required for mipmap generation */
tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                 SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

/* 2. Multiple mip levels */
tex_info.num_levels = (int)forge_log2f((float)size) + 1;  /* 9 for 256×256 */
```

### Generating mipmaps

After uploading the base level (mip 0), call `SDL_GenerateMipmapsForGPUTexture`
**outside any render or copy pass**:

```c
/* Upload base level in a copy pass */
SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
SDL_UploadToGPUTexture(copy, &src, &dst, false);
SDL_EndGPUCopyPass(copy);

/* Generate all other mip levels — MUST be outside any pass */
SDL_GenerateMipmapsForGPUTexture(cmd, texture);

SDL_SubmitGPUCommandBuffer(cmd);
```

The GPU internally renders into each mip level, downsampling from the level
above. This is why `COLOR_TARGET` usage is required.

### Sampler mipmap modes

The sampler controls how the GPU uses mipmaps:

```c
/* Trilinear: blend between two adjacent mip levels (smooth) */
info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
info.min_lod     = 0.0f;
info.max_lod     = 1000.0f;  /* Allow all mip levels */

/* No mipmaps: force level 0 only (causes aliasing) */
info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
info.max_lod     = 0.0f;     /* Clamp to base level */
```

### Procedural checkerboard

Instead of loading an image file, we generate the texture in C:

```c
int tile_x = x / tile_size;
int tile_y = y / tile_size;
Uint8 color = ((tile_x + tile_y) % 2 == 0) ? 255 : 0;
```

This creates a clean black-and-white pattern that makes mip level transitions
very visible.

### UV tiling

The vertex shader multiplies UVs by a scale factor:

```hlsl
output.uv = input.uv * uv_scale;
```

With `uv_scale = 2`, the texture repeats 2 times across the quad. Combined with
the sampler's `REPEAT` address mode, this creates a dense pattern that
emphasizes aliasing artifacts.

## What changed from Lesson 04

| Concept | Lesson 04 | Lesson 05 |
|---------|-----------|-----------|
| Texture source | PNG file from disk | Procedural checkerboard |
| Mip levels | 1 (no mipmaps) | 9 (full chain for 256×256) |
| Texture usage | `SAMPLER` only | `SAMPLER \| COLOR_TARGET` |
| Mipmap generation | None | `SDL_GenerateMipmapsForGPUTexture` |
| Samplers | 1 (linear) | 3 (trilinear, bilinear+nearest, none) |
| Uniforms | time, aspect | time, aspect, uv_scale, padding |
| Animation | Rotation | Scale pulsing (sine wave) |
| Interaction | None | SPACE to cycle samplers |

## Shaders

| File | Purpose |
|------|---------|
| `quad.vert.hlsl` | Scales UVs to tile the texture and pulses the quad size with a sine wave to show mipmap transitions |
| `quad.frag.hlsl` | Samples the texture — the GPU automatically selects the mip level based on screen-space derivatives |

## Building and running

```bash
cmake -B build
cmake --build build --config Debug
python scripts/run.py 05
```

Press **SPACE** to cycle between sampler modes. Watch the edges of the
checkerboard pattern — trilinear is smooth, no-mipmaps is a mess.

## AI skill

This lesson has a matching Claude Code skill:
[`mipmaps`](../../../.claude/skills/mipmaps/SKILL.md) — invoke it with
`/mipmaps` or copy it into your own project's `.claude/skills/` directory.
It distils the mipmap creation, sampler configuration, and procedural texture
patterns from this lesson into a reusable reference.

## Exercises

1. **LOD bias**: Add `mip_lod_bias = 1.0` to the trilinear sampler. The
   texture will appear blurrier because the GPU shifts toward smaller mip
   levels. Try negative values too — what happens?

2. **Max LOD clamping**: Set `max_lod = 2.0` on the trilinear sampler. Now
   the GPU never uses mip levels beyond 2 (64×64 for our texture). When the
   quad shrinks, you'll see aliasing return because the smallest mip level
   isn't small enough.

3. **Colored mipmaps**: Instead of auto-generating mipmaps, manually upload
   different colors to each mip level (red for level 0, green for level 1,
   etc.). This shows exactly which mip level the GPU picks at each distance.

4. **Photographic texture**: Replace the procedural checkerboard with the
   brick wall texture from Lesson 04. Copy the `textures/` folder and the
   `load_texture` function from Lesson 04's `main.c`. How do mipmaps affect
   a real-world texture? The difference is subtler than the checkerboard, but
   zoom out and you'll still see aliasing without mipmaps.

5. **Anisotropic filtering**: Research `max_anisotropy` in the sampler
   create info. Anisotropic filtering improves quality when surfaces are
   viewed at steep angles (which our flat quad doesn't show well — try
   rotating the quad in 3D for a more dramatic effect).

## See also

- [Math Lesson 04 — Mipmaps & LOD](../../math/04-mipmaps-and-lod/) — the math behind mip selection
- [Lesson 04 — Textures & Samplers](../04-textures-and-samplers/) — texture basics
- [Math Lesson 03 — Bilinear Interpolation](../../math/03-bilinear-interpolation/) — the filtering building block
- [Math library](../../../common/math/README.md) — `forge_log2f`, `forge_trilerpf`, etc.
