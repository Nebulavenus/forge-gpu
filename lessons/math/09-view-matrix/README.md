# Math Lesson 09 — View Matrix & Virtual Camera

Building a view matrix from scratch — the transform that makes a virtual camera
work in every 3D engine.

## What you'll learn

- The camera as an inverse transform (world-to-view)
- Extracting forward, right, and up vectors from a quaternion orientation
- Building a view matrix from position + quaternion (for FPS-style cameras)
- Look-at as a special case (for orbit cameras)
- How the view matrix fits into the MVP pipeline
- Equivalence between look-at and quaternion-based view matrices
- Camera movement using extracted basis vectors

## Result

Running the program prints a guided walkthrough of view matrix construction:

```text
=============================================================
  Math Lesson 09 - View Matrix & Virtual Camera
  Building view matrices from scratch
=============================================================

-- 1. The camera as an inverse transform -------------------

  Camera world transform:
    |   1.0000   0.0000   0.0000   0.0000 |
    |   0.0000   1.0000   0.0000   0.0000 |
    |   0.0000   0.0000   1.0000   5.0000 |
    |   0.0000   0.0000   0.0000   1.0000 |

  View matrix (inverse):
    |   1.0000   0.0000   0.0000  -0.0000 |
    |   0.0000   1.0000   0.0000  -0.0000 |
    |   0.0000   0.0000   1.0000  -5.0000 |
    |   0.0000   0.0000   0.0000   1.0000 |

  World * View = Identity? YES -- view is the inverse
  World point = (1.0000, 2.0000, 3.0000)
  In view space = (1.0000, 2.0000, -2.0000, 1.0000)
  Camera is at Z=5, point is at Z=3
  -> In view space, point is at Z=-2.0 (in front of camera)

-- 2. Extracting basis vectors from a quaternion -----------

  Identity quaternion (no rotation):
  Forward = (-0.0000, -0.0000, -1.0000)
  Right   = (1.0000, 0.0000, 0.0000)
  Up      = (0.0000, 1.0000, 0.0000)

  After yaw=45, pitch=30 degrees:
  Forward = (-0.6124, 0.5000, -0.6124)
  Right   = (0.7071, 0.0000, -0.7071)
  Up      = (0.3536, 0.8660, 0.3536)

  Orthonormal basis check:
    dot(forward, right) = 0.000000 (should be ~0)
    dot(forward, up)    = 0.000000 (should be ~0)
    dot(right, up)      = 0.000000 (should be ~0)
    |forward| = 1.0000, |right| = 1.0000, |up| = 1.0000 (all ~1)
    Orthonormal? YES
  Optimized matches quat_rotate_vec3?
    Forward: YES  Right: YES  Up: YES

-- 3. View matrix from position + quaternion ---------------

  Camera position: (3.0, 2.0, 5.0)
  Camera orientation: yaw=30, pitch=-15 degrees
  Forward (where camera looks) = (-0.4830, -0.2588, -0.8365)
  Right   (camera's right side) = (0.8660, 0.0000, -0.5000)
  Up      (above camera's head) = (-0.1294, 0.9659, -0.2241)

  View matrix:
    |   0.8660   0.0000  -0.5000  -0.0981 |
    |  -0.1294   0.9659  -0.2241  -0.4229 |
    |   0.4830   0.2588   0.8365  -6.1491 |
    |   0.0000   0.0000   0.0000   1.0000 |

  Camera pos in view space = (0.0000, 0.0000, 0.0000, 1.0000)
  At origin? YES (camera maps to (0,0,0) in view space)
  Point 3 units ahead of camera:
  World space = (1.5511, 1.2235, 2.4905)
  View space = (-0.0000, 0.0000, -3.0000, 1.0000)
  View-space Z is negative? YES (objects ahead have Z < 0)

-- 4. Look-at as a special case ----------------------------

  Eye:    (0.0, 2.0, 5.0)
  Target: (0.0, 0.0, 0.0)

  View matrix (look-at):
    |   1.0000  -0.0000   0.0000  -0.0000 |
    |   0.0000   0.9285  -0.3714  -0.0000 |
    |  -0.0000   0.3714   0.9285  -5.3852 |
    |   0.0000   0.0000   0.0000   1.0000 |

  Implied forward = (0.0000, -0.3714, -0.9285)
  Implied right   = (1.0000, -0.0000, 0.0000)
  Implied up      = (0.0000, 0.9285, -0.3714)
  Eye in view space = (0.0000, 0.0000, 0.0000, 1.0000)
  At origin? YES
  Target in view space = (0.0000, -0.0000, -5.3852, 1.0000)
  Target at -Z? YES (on the camera's -Z axis)

-- 5. View matrix in the MVP pipeline ----------------------

  Pipeline: Model -> View -> Projection -> NDC
  World vertex      = (1.0000, 0.0000, 0.0000)
  After View        = (1.0000, -0.1693, -5.3825, 1.0000)
  After Projection  = (1.1547, -0.2932, 5.2878, 5.3825)
  After /w (NDC)  = (0.2145, -0.0545, 0.9824)
  Combined MVP gives same NDC? YES
  NDC range: X,Y in [-1,1], Z in [0,1]
  Vertex in valid NDC range? YES (visible on screen)

-- 6. Equivalence: look-at vs quaternion -------------------

  Eye: (3.0, 4.0, 10.0), Target: (0.0, 0.0, 0.0)
  Look-at result  = (2.7777, 1.7562, -12.9692, 1.0000)
  Quaternion result = (2.7777, 1.7562, -12.9692, 1.0000)
  Matrices match? YES -- both methods produce the same view

-- 7. Camera movement demo ---------------------------------

  Simulating camera movement (4 steps):
  Step | Action        | Position                  | Yaw
  -----|---------------|---------------------------|-----
     1 | Walk forward  | (  0.00,  1.60,  -0.03) | 0 deg
     2 | Turn right    | (  0.00,  1.60,  -0.03) | -15 deg
     3 | Walk forward  | (  0.01,  1.60,  -0.07) | -15 deg
     4 | Turn right    | (  0.01,  1.60,  -0.07) | -30 deg

  The view matrix is rebuilt every frame from:
    position    (updated by movement input)
    orientation (updated by mouse/stick input)
  This is how first-person cameras work in every 3D game.
```

Each section demonstrates one concept with concrete numbers and verification.
The summary (section 8) prints a reference card of all new functions.

## Prerequisites

- [Math Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/) — the
  model/world/view/clip space pipeline
- [Math Lesson 05 — Matrices](../05-matrices/) — matrix multiplication,
  transpose, inverse
- [Math Lesson 06 — Projections](../06-projections/) — projection matrix
  (the "P" in MVP)
- [Math Lesson 08 — Orientation](../08-orientation/) — quaternions,
  `quat_from_euler`, `quat_to_mat4`

## Key Concepts

### The view matrix is an inverse transform

![View transform](assets/view_transform.png)

A camera has a position and an orientation in the world, just like any other
object. Its **world transform** is:

$$
\text{Camera world transform} = T(\text{pos}) \cdot R(\text{orient})
$$

The **view matrix** is the inverse of this. Instead of placing the camera in the
world, it moves the *entire world* so the camera ends up at the origin looking
down -Z:

$$
V = (T \cdot R)^{-1} = R^{-1} \cdot T^{-1} = R^T \cdot T(-\text{pos})
$$

Why the transpose? Rotation matrices are **orthonormal** — their columns are
perpendicular unit vectors — so the inverse equals the transpose ($R^{-1} = R^T$).
An orthonormal matrix is one where every column has length 1 and all columns are
perpendicular to each other. This property is what makes the transpose shortcut
work: the dot products that define matrix multiplication naturally "undo" the
rotation when the matrix is transposed. This is much cheaper than computing a
general inverse.

### Extracting basis vectors from a quaternion

![Camera basis vectors](assets/camera_basis_vectors.png)

A quaternion orientation encodes three directions — the camera's local coordinate
axes:

```text
      Up (+Y)
       ^
       |
       |  Forward (-Z, into the screen)
       | /
       |/
-------+--------> Right (+X)
```

| Vector | Default (identity quat) | Function |
|--------|------------------------|----------|
| Forward | (0, 0, -1) | `quat_forward(q)` |
| Right | (1, 0, 0) | `quat_right(q)` |
| Up | (0, 1, 0) | `quat_up(q)` |

**Why is forward -Z and not +Z?** It comes from the right-hand rule. When you
sit in front of your monitor, everyone agrees on two axes: +X goes right, +Y
goes up. The third axis is determined by the coordinate system's handedness. In
a right-handed system, if you curl the fingers of your right hand from +X toward
+Y, your thumb points in the +Z direction — **out of the screen, toward you:**

```text
        +Y (up)
         |
         |
         |
         +-------> +X (right)
        /
       /
      +Z (out of screen, toward you)
```

Since +Z points toward the viewer, the scene behind the screen is at negative Z.
The camera looks *into* the screen, which is the **-Z direction**.

A left-handed coordinate system (used by traditional DirectX) flips this: +Z
goes into the screen, so the camera looks down +Z. Neither convention is better —
it's a choice. We use right-handed because it matches Vulkan, OpenGL, and
standard math textbooks.

| Convention | +Z direction | Camera looks at | Used by |
|-----------|-------------|----------------|---------|
| Right-handed (ours) | Out of screen | -Z | OpenGL, Vulkan, math textbooks |
| Left-handed | Into screen | +Z | Traditional DirectX |

These are computed by rotating the default directions by the quaternion. The
optimized functions expand the rotation formula for each specific input vector,
avoiding the general `quat_rotate_vec3` overhead.

Together these three vectors form an **orthonormal basis** — a set of three
vectors that are mutually perpendicular (every pair has a dot product of zero)
and each has unit length. An orthonormal basis defines a complete coordinate
frame: any point in 3D space can be described as a combination of these three
directions. The camera's basis vectors define what "right," "up," and "forward"
mean from the camera's perspective.

### Building the view matrix

The view matrix combines a rotation and a translation:

```text
View = | right.x    right.y    right.z    -dot(right, pos)   |
       | up.x       up.y       up.z       -dot(up, pos)      |
       | -fwd.x     -fwd.y     -fwd.z      dot(fwd, pos)     |
       |  0          0          0           1                |
```

The rotation part (3x3 upper-left) has the camera's basis vectors as **rows**
(not columns). This is because it's the *transpose* of the camera's rotation
matrix — the inverse rotation.

The translation column uses dot products (`-dot(right, pos)`,
`-dot(up, pos)`, `dot(fwd, pos)`) instead of just `-pos`. This is because the
translation must be in the *rotated* coordinate system: `R^T * (-pos)`. The dot
products compute exactly this rotation-then-negate in one step.

### Look-at: a special case

`mat4_look_at(eye, target, up)` builds a view matrix by computing the camera's
orientation from two points:

1. `forward = normalize(target - eye)` — direction to the target
2. `right = normalize(cross(forward, world_up))` — perpendicular to forward
   and world up (the cross product produces a vector perpendicular to both
   inputs — see [Math Lesson 01 — Vectors](../01-vectors/) for details)
3. `up' = cross(right, forward)` — true up (perpendicular to both)

This produces the same matrix as `mat4_view_from_quat` when given equivalent
inputs. Look-at is convenient for orbit cameras and cutscenes. However, it
cannot represent roll (camera tilt) because it always derives "up" from the world
up direction.

### Two approaches compared

| Method | Input | Best for | Limitations |
|--------|-------|----------|-------------|
| `mat4_look_at` | eye + target + world up | Orbit cameras, cutscenes | No roll |
| `mat4_view_from_quat` | position + quaternion | FPS cameras, flight sims | Need to manage quaternion |

### Camera movement with basis vectors

In a game, the camera moves every frame. The extracted basis vectors tell the
camera which direction to move:

```text
Forward/back (W/S):  position += quat_forward(orientation) * speed * dt
Strafe left/right (A/D):  position += quat_right(orientation) * speed * dt
Fly up/down (Space/Ctrl):  position += quat_up(orientation) * speed * dt
```

The `dt` (delta time) factor ensures movement speed is independent of frame
rate — a camera moving at 5 units/second moves the same distance whether the
game runs at 30 FPS or 120 FPS. Delta time is simply the elapsed time since the
last frame, typically measured in seconds.

Each frame follows this pattern:

```mermaid
flowchart LR
    A["Update orientation\n(mouse input)"] --> B["Extract basis\nvectors"]
    B --> C["Move position\n(keyboard)"]
    C --> D["Build view matrix"]
    D --> E["Upload MVP\nto GPU"]
```

1. **Update orientation** from mouse/stick input (adjust yaw and pitch)
2. **Extract basis vectors** — `quat_forward`, `quat_right`
3. **Move position** along those directions based on keyboard input
4. **Rebuild view matrix** — `mat4_view_from_quat(position, orientation)`
5. **Upload MVP** to the GPU — `MVP = proj * view * model`

The view matrix is rebuilt every frame. This is cheap (a few dot products and
multiplications) and ensures the camera state always matches the latest input.

### The MVP pipeline

The view matrix is the "V" in MVP (Model-View-Projection):

```mermaid
flowchart LR
    A["Model Space"] -- "Model" --> B["World Space"]
    B -- "View" --> C["View Space"]
    C -- "Projection" --> D["Clip Space"]
    D -- "/w" --> E["NDC"]

    style C stroke:#4fc3f7,stroke-width:3px
```

On the GPU, vertices are transformed by the combined matrix:

```text
MVP = Projection * View * Model
clip_pos = MVP * vec4(position, 1.0)
```

The view matrix transforms every object in the scene. It only changes when the
camera moves or rotates.

## The Math

### Why the camera is an inverse

Consider a camera at position `(0, 0, 5)` looking at the origin. A vertex at
`(0, 0, 3)` is 2 units in front of the camera. In view space, that vertex
should be at `(0, 0, -2)` — negative Z because the camera looks down -Z.

The view matrix achieves this by moving everything by -5 along Z:

```text
Before (world):        After (view):
  cam at Z=5             cam at Z=0
  vertex at Z=3          vertex at Z=-2

  |---[v]---[cam]-->    |--[v]---[cam]-->
  0    3     5           -2   0
```

The world moves; the camera stays at the origin. This is the key insight.

### Orthonormal basis from a quaternion

When you rotate the three default axes by a quaternion:

```text
forward = quat_rotate_vec3(q, (0, 0, -1))
right   = quat_rotate_vec3(q, (1, 0, 0))
up      = quat_rotate_vec3(q, (0, 1, 0))
```

The results are always mutually perpendicular and unit length, because:

- The input vectors are orthonormal (the standard basis)
- Quaternion rotation preserves angles and lengths
- Therefore the output vectors are also orthonormal

The optimized `quat_forward`, `quat_right`, and `quat_up` functions expand
the rotation formula for each specific input, avoiding unnecessary
multiplications with zero components.

### View matrix derivation

Starting from the camera's world transform `T(pos) * R(quat)`:

```text
Step 1: Invert the rotation
  R^-1 = R^T (because R is orthonormal)
  For a quaternion, R^-1 = conjugate(q) -> mat4

Step 2: Invert the translation
  T^-1 = T(-pos)

Step 3: Combine (note reversed order)
  View = R^-1 * T^-1

Step 4: In matrix form, this gives basis vectors as rows
  with dot-product translations (the R^T * (-pos) computation)
```

### Look-at derivation

`mat4_look_at` computes the same thing starting from geometry instead of a
quaternion:

```text
Step 1: forward = normalize(target - eye)
Step 2: right   = normalize(cross(forward, world_up))
Step 3: up'     = cross(right, forward)  [already unit length]
Step 4: Same matrix structure as mat4_view_from_quat
```

The cross products in steps 2 and 3 use the **Gram-Schmidt process** — a
method for building an orthonormal basis from a set of vectors. Given one
direction (forward), cross products derive two more directions that are each
perpendicular to forward and to each other.

Step 2 might seem surprising: why `cross(forward, world_up)` and not
`cross(world_up, forward)`? Because we want the right vector to point to the
right (positive X in view space). In a right-handed coordinate system,
`cross(forward, up)` gives right. If the camera looks along -Z and up is +Y,
then `cross(-Z, +Y) = +X`, which is indeed right.

## Where it's used

Graphics and game programming uses view matrices for:

- **Every 3D application** — the view matrix is required to render any 3D scene
- **First-person cameras** — position + quaternion from mouse/keyboard input
- **Third-person cameras** — look-at from an offset position toward the player
- **Orbit cameras** — look-at from a rotating position toward a center point
- **Cutscene cameras** — interpolate between positions/orientations with SLERP

**In forge-gpu lessons:**

- [GPU Lesson 06 — Depth & 3D](../../gpu/06-depth-and-3d/) uses `mat4_look_at`
  for a static camera viewing several cubes
- GPU Lesson 07 (upcoming) will use `mat4_view_from_quat` for a first-person
  camera with keyboard and mouse input

## Building

```bash
python scripts/run.py math/09
```

Or build manually:

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\math\09-view-matrix\Debug\09-view-matrix.exe

# Linux / macOS
./build/lessons/math/09-view-matrix/09-view-matrix
```

The program prints a walkthrough of every concept with numerical verification.

## Exercises

1. **Orbit camera:**
   Given a target point, a distance, and yaw/pitch angles, compute the camera
   position on a sphere around the target. Build the view matrix with
   `mat4_look_at`. Verify the target maps to `(0, 0, -distance)` in view space.

2. **FPS camera movement:**
   Implement a loop that reads yaw/pitch from simulated mouse deltas, extracts
   forward/right with `quat_forward`/`quat_right`, moves the position, and
   rebuilds the view matrix each frame. Verify the camera position is always at
   the origin in view space.

3. **View matrix inverse:**
   Given a view matrix, compute its inverse using `mat4_inverse`. Verify that
   the inverse equals the camera's world transform `T(pos) * R(quat)`.

4. **Billboard transform:**
   Given a view matrix, extract the camera's right and up vectors from it
   (they're in the first and second rows of the 3x3 part). Use them to orient
   a quad that always faces the camera.

5. **Smooth camera transition:**
   Use `quat_slerp` to smoothly interpolate between two camera orientations
   over 60 frames. Rebuild the view matrix at each step and verify the camera
   position remains at the origin in view space throughout.

## See also

- [Math Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/) — view space
  in the full transformation pipeline
- [Math Lesson 05 — Matrices](../05-matrices/) — transpose, inverse, orthonormal
  matrices
- [Math Lesson 06 — Projections](../06-projections/) — the projection matrix
  that follows the view matrix
- [Math Lesson 08 — Orientation](../08-orientation/) — quaternions,
  `quat_from_euler`, `quat_to_mat4`, SLERP
- [Math library](../../../common/math/README.md) — `mat4_look_at`,
  `mat4_view_from_quat`, `quat_forward/right/up`
