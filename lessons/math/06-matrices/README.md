# Math Lesson 06 — Matrices

## What you'll learn

- How 4x4 matrices represent transforms (translation, rotation, scaling)
- The identity matrix — the "do nothing" transform
- Translation and why `w=1` means position and `w=0` means direction
- Uniform and non-uniform scaling
- Rotation around each axis (X, Y, Z)
- Matrix composition — combining transforms (order matters!)
- The MVP pipeline — Model, View, Projection end-to-end

## Result

After running this lesson you will understand how each type of mat4 transform
works, how to compose them, and how the full Model-View-Projection pipeline
transforms a 3D point from object space all the way to screen coordinates.

## Key concepts

### Column-major storage

forge_math.h stores matrices in **column-major** order, matching HLSL's default
layout:

```text
Memory: m[0] m[1] m[2] m[3]  m[4] m[5] m[6] m[7]  m[8] ...

As a matrix:
| m[0]  m[4]  m[8]   m[12] |    | Xx  Yx  Zx  Tx |
| m[1]  m[5]  m[9]   m[13] |  = | Xy  Yy  Zy  Ty |
| m[2]  m[6]  m[10]  m[14] |    | Xz  Yz  Zz  Tz |
| m[3]  m[7]  m[11]  m[15] |    | 0   0   0   1  |
```

Columns 0-2 are the X, Y, Z axes (rotation + scale). Column 3 is translation.

### Transform types

| Function | What it does | Example |
|----------|-------------|---------|
| `mat4_identity()` | No change — leaves points as-is | Starting point for composing |
| `mat4_translate(v)` | Moves points by offset `v` | Positioning objects in the world |
| `mat4_scale(v)` | Stretches each axis independently | Making objects bigger/smaller |
| `mat4_scale_uniform(s)` | Stretches all axes equally | Zooming in/out |
| `mat4_rotate_x/y/z(a)` | Rotates around one axis | Spinning, orbiting, tilting |

### Composition order matters

```c
mat4 C = mat4_multiply(A, B);  /* Apply B first, then A */
```

Translate-then-rotate and rotate-then-translate produce **different results**:

```text
Rotate then translate:  (1,0,0) → rotate → (0,1,0) → translate → (5,1,0)
Translate then rotate:  (1,0,0) → translate → (6,0,0) → rotate → (0,6,0)
```

### The MVP pipeline

Every vertex in a 3D scene goes through three transforms:

```text
Object Space  ──Model──>  World Space  ──View──>  Camera Space  ──Projection──>  Clip Space
```

Composed as a single matrix: `MVP = Projection * View * Model`

In the vertex shader: `clip_pos = MVP * vertex_position`

After the vertex shader, the GPU performs the **perspective divide** (dividing
by `w`) to get **Normalized Device Coordinates** (NDC):

```text
NDC.x ∈ [-1, 1]  — left to right
NDC.y ∈ [-1, 1]  — bottom to top
NDC.z ∈ [0, 1]   — near to far (depth)
```

### The view matrix

`mat4_look_at(eye, target, up)` creates a camera by:

1. Computing a coordinate frame (forward, right, up)
2. Building a rotation that aligns world axes with camera axes
3. Translating the world so the camera is at the origin

### The projection matrix

`mat4_perspective(fov, aspect, near, far)` creates perspective foreshortening:

- Distant objects appear smaller (divide by distance)
- Field of view controls how wide the camera "sees"
- Aspect ratio prevents stretching on non-square windows
- Near/far planes define the visible depth range

## Functions used from forge_math.h

| Function | Description |
|----------|-------------|
| `mat4_identity()` | Create identity matrix |
| `mat4_translate(v)` | Create translation matrix |
| `mat4_scale(v)` | Create non-uniform scale matrix |
| `mat4_scale_uniform(s)` | Create uniform scale matrix |
| `mat4_rotate_x(a)` | Rotate around X axis |
| `mat4_rotate_y(a)` | Rotate around Y axis |
| `mat4_rotate_z(a)` | Rotate around Z axis |
| `mat4_multiply(A, B)` | Compose two matrices (B first, then A) |
| `mat4_multiply_vec4(M, v)` | Transform a point or direction |
| `mat4_look_at(eye, target, up)` | Create view matrix |
| `mat4_perspective(fov, asp, n, f)` | Create perspective projection |

## Building

```bash
python scripts/run.py math/06
```

Requires SDL3 and a C99 compiler (see project root README for full setup).

## Exercises

1. **Rotation composition**: Create a rotation that rotates 45 degrees around Y,
   then 30 degrees around X. Multiply them in both orders and compare. Which
   order gives the "look down and to the side" effect?

2. **Scale then translate vs translate then scale**: Scale by 2, then translate
   by (5, 0, 0). Now reverse the order. What's different and why?

3. **Orthographic vs perspective**: Replace `mat4_perspective` with
   `mat4_orthographic` in the MVP section. How does the clip-space result differ?
   What happens to the `w` component?

4. **Camera orbit**: Modify the eye position to orbit around the origin.
   Use `eye.x = 3 * cos(angle)`, `eye.z = 3 * sin(angle)` and trace the
   same cube corner. How does the view-space position change?

## See also

- [Math Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/) — the theory behind transforms
- [Math Lesson 03 — Orthographic Projection](../03-orthographic-projection/) — orthographic as an alternative
- [GPU Lesson 06 — Depth Buffer & 3D Transforms](../../gpu/06-depth-and-3d/) — using MVP in practice
- [Math library API](../../../common/math/README.md) — full function reference
