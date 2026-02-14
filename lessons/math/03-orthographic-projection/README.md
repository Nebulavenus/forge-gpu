# Math Lesson 03 — Orthographic Projection

Map a rectangular box to the screen without perspective distortion.

## What you'll learn

- What orthographic projection is and how it differs from perspective
- The math behind `mat4_orthographic(left, right, bottom, top, near, far)`
- Why w stays 1 (no perspective divide needed)
- Common use cases: 2D rendering, shadow maps, isometric views

## Result

The demo projects three points at the same XY position but different depths
through both orthographic and perspective projections, showing how orthographic
preserves apparent size while perspective shrinks distant objects.

**Example output:**

```text
==============================================================
  Orthographic Projection
==============================================================

Orthographic projection maps a rectangular box in view space
to the NDC cube. Unlike perspective, distant objects stay the
same size -- parallel lines remain parallel.

1. TEST POINTS IN VIEW SPACE
--------------------------------------------------------------
  Camera at (0, 0, 10) looking at origin.
  Three points at the same XY but different depths:

  Near  (z_world =  9):        (  3.000,   2.000,  -1.000,   1.000)
  Mid   (z_world =  0):        (  3.000,   2.000, -10.000,   1.000)
  Far   (z_world = -8):        (  3.000,   2.000, -18.000,   1.000)

2. ORTHOGRAPHIC PROJECTION
--------------------------------------------------------------
  Box: X=[-5, 5], Y=[-5, 5], near=0.1, far=20

  Clip space (w is always 1 -- no perspective divide!):
  Near clip:                   (  0.600,   0.400,   0.045,   1.000)
  Mid  clip:                   (  0.600,   0.400,   0.497,   1.000)
  Far  clip:                   (  0.600,   0.400,   0.899,   1.000)

  NDC (same as clip since w=1):
  Near NDC:                    (  0.600,   0.400,   0.045)
  Mid  NDC:                    (  0.600,   0.400,   0.497)
  Far  NDC:                    (  0.600,   0.400,   0.899)

  Key observation: X and Y are IDENTICAL for all three points.
  Depth does not affect apparent size. This is the defining
  property of orthographic projection.

3. PERSPECTIVE PROJECTION (for comparison)
--------------------------------------------------------------
  FOV=60 degrees, aspect=1.0, near=0.1, far=20

  Clip space (note w varies -- this is what causes foreshortening):
  Near clip:                   (  5.196,   3.464,   0.905,   1.000)
  Mid  clip:                   (  5.196,   3.464,   9.950,  10.000)
  Far  clip:                   (  5.196,   3.464,  17.990,  18.000)

  NDC (after dividing by w):
  Near NDC:                    (  5.196,   3.464,   0.905)
  Mid  NDC:                    (  0.520,   0.346,   0.995)
  Far  NDC:                    (  0.289,   0.192,   0.999)

  Key observation: X and Y CHANGE with depth. Farther points
  appear closer to the center. This is perspective foreshortening.

4. SIDE-BY-SIDE COMPARISON
--------------------------------------------------------------
  Same three points, X coordinate in NDC:

  Point        Orthographic X     Perspective X
  -----        --------------     -------------
  Near (z= 9)  0.600              5.196
  Mid  (z= 0)  0.600              0.520
  Far  (z=-8)  0.600              0.289

  Orthographic: same X at every depth (no foreshortening)
  Perspective:  X shrinks with distance (foreshortening)

5. COMMON USE CASE: 2D RENDERING
--------------------------------------------------------------
  Orthographic projection for a 1920x1080 screen.
  Maps pixel coordinates directly to NDC.

  Pixel -> NDC:
    (0, 0)         -> (-1.0, -1.0)    bottom-left
    (1920, 1080)   -> (1.0, 1.0)     top-right
    (960, 540)     -> (0.0, 0.0)     center

  This is how 2D games, UIs, and text rendering work:
  specify positions in pixels, let the orthographic matrix
  handle the mapping to GPU coordinates.

==============================================================
  Summary
==============================================================

  Orthographic projection:
    * No foreshortening -- size is independent of depth
    * w stays 1 (no perspective divide needed)
    * Parallel lines in the scene remain parallel on screen
    * Maps an axis-aligned box to NDC

  Use orthographic for:
    * 2D games and UI rendering
    * Shadow map generation
    * CAD and architectural visualization
    * Isometric/top-down views

  Use perspective for:
    * 3D scenes with realistic depth perception
    * First-person / third-person cameras

  See lessons/math/03-orthographic-projection/README.md
```

## Key concepts

- **No foreshortening** — An object 100 meters away appears the same size as
  one 1 meter away. Only the Z (depth) value changes.
- **w stays 1** — Unlike perspective projection, orthographic does not encode
  depth into the w component. Clip space coordinates are already in NDC.
- **Parallel lines stay parallel** — Railroad tracks don't converge. This is
  the visual signature of orthographic projection.

## The Math

### What the matrix does

Orthographic projection maps an axis-aligned box in view space to the NDC cube:

```text
View space box:                  NDC cube:
  X: [left, right]       ->       X: [-1, +1]
  Y: [bottom, top]       ->       Y: [-1, +1]
  Z: [-near, -far]       ->       Z: [0, 1]
```

The Z range uses 0-to-1 depth (Vulkan/Metal/D3D12 convention), matching our
`mat4_perspective` function.

### Deriving the matrix

Each axis is mapped independently with a simple scale-and-translate:

**X axis:** Map `[left, right]` to `[-1, 1]`

```text
x_ndc = 2/(right-left) * x  -  (right+left)/(right-left)
```

**Y axis:** Map `[bottom, top]` to `[-1, 1]`

```text
y_ndc = 2/(top-bottom) * y  -  (top+bottom)/(top-bottom)
```

**Z axis:** Map `[-near, -far]` to `[0, 1]`

In our right-handed coordinate system, the camera looks down -Z. Points at the
near plane are at z = -near, points at the far plane are at z = -far.

```text
z_ndc = 1/(near-far) * z  +  near/(near-far)
```

### The complete matrix

```text
| 2/(r-l)   0         0          -(r+l)/(r-l) |
| 0         2/(t-b)   0          -(t+b)/(t-b) |
| 0         0         1/(n-f)    n/(n-f)       |
| 0         0         0          1             |
```

Where r=right, l=left, t=top, b=bottom, n=near, f=far.

Notice the bottom row is `[0, 0, 0, 1]` — this means w is always 1, so there
is no perspective divide. This is the fundamental difference from the perspective
matrix, which has `[0, 0, -1, 0]` in the bottom row.

### Symmetric case

When the box is centered (left = -right, bottom = -top), the translation terms
vanish and the matrix simplifies to pure scaling:

```text
| 1/right   0        0          0        |
| 0         1/top    0          0        |
| 0         0        1/(n-f)    n/(n-f)  |
| 0         0        0          1        |
```

## Where it's used

Graphics and game programming uses orthographic projection for:

- **2D games and UI** — Map pixel coordinates directly to NDC
- **Shadow mapping** — Directional lights use orthographic projection from the
  light's point of view to render a depth map
- **Isometric views** — Strategy games, city builders, and some RPGs
- **CAD / architecture** — Accurate measurements require no perspective distortion
- **Minimap rendering** — Top-down orthographic view of the game world

**In forge-gpu lessons:**

- [Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/) explains the full
  transformation pipeline (model -> world -> view -> clip -> NDC -> screen)
- [GPU Lesson 03 — Uniforms & Motion](../../gpu/03-uniforms-and-motion/) uses
  `mat4_perspective` for 3D; orthographic would be used for 2D overlays

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\math\03-orthographic-projection\Debug\03-orthographic-projection.exe

# Linux / macOS
./build/lessons/math/03-orthographic-projection/03-orthographic-projection
```

The demo prints a comparison of orthographic vs perspective projection for the
same set of 3D points, showing how depth affects (or doesn't affect) apparent
position.

## Exercises

1. **Asymmetric box:** Change the projection to
   `mat4_orthographic(0, 20, -5, 5, 0.1, 20)` and observe how the center of
   the visible volume shifts in NDC. When would you want an asymmetric box?

2. **2D coordinate system:** Create an orthographic projection for a 640x480
   window where (0, 0) is the top-left corner. (Hint: flip bottom and top.)

3. **Depth precision:** Create both an orthographic and perspective projection
   with near=0.1 and far=1000. Transform points at z=-1, z=-10, z=-100, and
   z=-500. Compare how depth precision is distributed across the range.

## Further reading

- [Math Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/) — The full
  transformation pipeline that orthographic projection fits into
- [Math library DESIGN.md](../../../common/math/DESIGN.md) — Coordinate system
  and matrix conventions used by `mat4_orthographic`
