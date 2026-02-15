# Math Lesson 03 — Bilinear Interpolation

The math behind LINEAR texture filtering: two nested lerps blending
the 4 nearest texels into a smooth result.

## What you'll learn

- How **linear interpolation (lerp)** blends two values with a parameter t
- How **bilinear interpolation** extends lerp to 2D with three lerps
- How the GPU uses bilinear interpolation for **LINEAR texture filtering**
- The difference between **LINEAR** and **NEAREST** filtering
- How **UV coordinates** map to texel coordinates and fractional blend weights

## Result

A console program that walks through bilinear interpolation step by step,
from the 1D lerp building block to a full texture sampling example with
color blending.

**Example output:**

```text
==============================================================
  Bilinear Interpolation
==============================================================

Bilinear interpolation blends four values on a 2D grid based
on a fractional position. It's what the GPU does when a texture
sampler uses LINEAR filtering.

1. LINEAR INTERPOLATION (LERP) REFRESHER
--------------------------------------------------------------
  lerp(a, b, t) = a + t * (b - a)
  Blends between two values based on t in [0, 1].

  a = 10.0,  b = 30.0

  t = 0.00  ->  lerp = 10.0   (100% a)
  t = 0.25  ->  lerp = 15.0   ( 75% a + 25% b)
  t = 0.50  ->  lerp = 20.0   ( 50% a + 50% b)
  t = 0.75  ->  lerp = 25.0   ( 25% a + 75% b)
  t = 1.00  ->  lerp = 30.0   (100% b)

  Lerp is the 1D building block. Bilinear interpolation
  extends it to 2D by doing three lerps.
```

**Important:** Copy output directly from running the program — don't manually
type it. The full output includes sections on step-by-step bilerp, special
cases, texture sampling, color blending, and nearest vs linear comparison.

## Key concepts

- **Lerp (linear interpolation)** — Blends two values: `lerp(a, b, t) = a + t * (b - a)`. When t=0 you get a, when t=1 you get b, when t=0.5 you get the midpoint
- **Bilinear interpolation** — Three lerps that blend four corner values on a 2D grid based on a fractional position (tx, ty)
- **Texel coordinates** — UV coordinates scaled to pixel positions in a texture. The integer part identifies the 4 nearest texels; the fractional part becomes the blend weights
- **LINEAR filtering** — The GPU's bilinear interpolation mode: smooth blending between texels. Gives smooth gradients for photographic textures
- **NEAREST filtering** — Picks the single closest texel with no blending. Gives sharp, pixelated edges ideal for pixel art

## The math

### Linear interpolation (1D)

Lerp blends between two values **a** and **b** using a parameter **t**:

```text
lerp(a, b, t) = a + t * (b - a)

    a ============*=============== b
    t=0          t=0.5           t=1
```

When t=0, the result is exactly **a**. When t=1, it's exactly **b**. Values
in between give a proportional blend. This is the same lerp from
[Math Lesson 01 — Vectors](../01-vectors/), but applied to scalars.

### Bilinear interpolation (2D)

Bilinear interpolation blends four corner values on a 2D grid. Given
corners c00, c10, c01, c11 and a fractional position (tx, ty):

```text
    c01 ----------- c11
     |               |
     |    * (tx,ty)  |
     |               |
    c00 ----------- c10
```

**Algorithm — three lerps:**

1. **Lerp along the bottom edge:** `bot = lerp(c00, c10, tx)`
2. **Lerp along the top edge:** `top = lerp(c01, c11, tx)`
3. **Lerp vertically between results:** `result = lerp(bot, top, ty)`

```text
    c01 ---[top]---- c11        Step 1: lerp bottom (tx)
     |       |        |         Step 2: lerp top (tx)
     |    [result]    |         Step 3: lerp vertically (ty)
     |       |        |
    c00 ---[bot]---- c10
```

**Special cases:**

- At any corner (e.g., tx=0, ty=0), returns exactly that corner's value
- At the center (tx=0.5, ty=0.5), returns the average of all four corners

### How the GPU uses it for texture filtering

When you sample a texture at a UV coordinate that falls between texel centers,
the GPU needs to decide what color to return. With **LINEAR** filtering, it:

1. **Converts UV to texel coordinates:** multiply by texture dimensions
2. **Splits into integer + fraction:** integer identifies the 4 nearest
   texels; fraction becomes the blend weights (tx, ty)
3. **Gathers 4 texels:** the 2x2 neighborhood around the sample point
4. **Bilinearly interpolates:** blends the 4 texel colors using (tx, ty)

```text
    UV = (0.375, 0.625) on a 4x4 texture

    Texel coords: (0.375 * 3, 0.625 * 3) = (1.125, 1.875)
    Integer part: (1, 1)        -> bottom-left texel
    Fraction:     (0.125, 0.875) -> blend weights

    tex[2][1]=150 --- tex[2][2]=200     The sample point is
         |      *          |            near the top-left texel,
    tex[1][1]=125 --- tex[1][2]=175     so it gets more weight.
```

With **NEAREST** filtering, the GPU simply picks whichever texel center is
closest — no blending at all. This is faster but produces visible "staircase"
artifacts on photographic textures.

## Where it's used

Graphics and game programming uses bilinear interpolation for:

- **Texture filtering** — LINEAR sampler mode blends between texels
- **Heightmap sampling** — Terrain engines interpolate height between grid points
- **Lightmap lookup** — Pre-baked lighting stored in textures, sampled smoothly
- **Image scaling** — Resizing images (the "bilinear" resize mode)
- **UV mapping** — Any time you sample a 2D grid at fractional coordinates

**In forge-gpu lessons:**

- [GPU Lesson 04 — Textures & Samplers](../../gpu/04-textures-and-samplers/)
  uses LINEAR filtering, which is bilinear interpolation of the 4 nearest
  texels. Exercise 1 lets you switch to NEAREST to see the difference.

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\math\03-bilinear-interpolation\Debug\03-bilinear-interpolation.exe

# Linux / macOS
./build/lessons/math/03-bilinear-interpolation/03-bilinear-interpolation
```

Or use the run script:

```bash
python scripts/run.py math/03
```

The demo walks through lerp, bilinear interpolation step-by-step, texture
sampling, color blending, and a nearest vs linear comparison.

## Exercises

1. **Verify order independence** — Bilinear interpolation gives the same
   result whether you lerp horizontally first then vertically, or vertically
   first then horizontally. Modify the demo to do both orderings and confirm
   the results match.

2. **Heightmap terrain** — Create a small 8x8 grid of height values. Sample
   at 0.1 increments across the surface using `forge_bilerpf` and print
   a grid of interpolated heights. Compare with nearest-neighbor sampling.

3. **Trilinear interpolation** — Extend bilinear to 3D: given 8 corner values
   of a cube and (tx, ty, tz), compute the blended result. This is two
   bilinear interpolations followed by one lerp — the same structure the GPU
   uses for trilinear mipmap filtering (coming in Math Lesson 04).

## Further reading

- [Math Lesson 01 — Vectors](../01-vectors/) — Lerp for vectors (`vec2_lerp`, `vec3_lerp`)
- [GPU Lesson 04 — Textures & Samplers](../../gpu/04-textures-and-samplers/) — LINEAR vs NEAREST filtering in practice
- [common/math/README.md](../../../common/math/README.md) — Math library API reference
