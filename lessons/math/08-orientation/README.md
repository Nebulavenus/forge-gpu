# Math Lesson 08 — Orientation

Four representations of 3D rotation and how to convert between them.

## What you'll learn

- Euler angles: pitch, yaw, roll — and why rotation order matters
- Gimbal lock: what it is, why it happens, and how to avoid it
- Rotation matrices: Rx, Ry, Rz and their properties
- Rodrigues' rotation formula: rotating around any arbitrary axis
- Axis-angle: the most intuitive rotation representation
- Quaternions: identity, conjugate, inverse, multiplication, double cover
- Rotating a vector by a quaternion (the sandwich product)
- Converting between all four representations (round-trips)
- SLERP and NLERP: smooth interpolation between orientations

## Result

Running the program prints a guided walkthrough of every rotation
representation:

```text
=============================================================
  Math Lesson 08 — Orientation
  Four representations of 3D rotation
=============================================================

-- 1. Euler angles -- pitch, yaw, and roll ----------------
  ...45° yaw, 30° pitch applied to forward vector...

-- 2. Gimbal lock -- why Euler angles break ---------------
  ...different Euler triplets producing the same rotation...

-- 6. Quaternion basics -----------------------------------
  ...identity, conjugate, inverse, double cover...

-- 10. SLERP -- smooth rotation interpolation -------------
  ...constant-speed vs variable-speed interpolation table...
```

Each section demonstrates one concept with concrete numbers.

## Prerequisites

- [Math Lesson 01 — Vectors](../01-vectors/) — dot product, cross product,
  normalization
- [Math Lesson 05 — Matrices](../05-matrices/) — matrix multiplication,
  transpose, determinant

## Key Concepts

### Euler angles

Three angles describing orientation — the most intuitive representation:

- **Yaw**: rotation around Y axis (look left/right)
- **Pitch**: rotation around X axis (look up/down)
- **Roll**: rotation around Z axis (tilt head)

```text
        +Y (yaw)
         |
         |  +Z (roll, toward camera)
         | /
         |/
---------+--------> +X (pitch)
```

**Rotation order matters.** We use intrinsic Y-X-Z:

1. Yaw first (rotate around world Y)
2. Pitch second (rotate around the yawed X axis)
3. Roll third (rotate around the pitched Z axis)

In matrix form: `R = R_y(yaw) * R_x(pitch) * R_z(roll)`

This is the standard convention for game cameras and aircraft
(heading-elevation-bank).

### Gimbal lock

When pitch = +/-90 degrees, yaw and roll rotate around the **same axis** — you lose a
degree of freedom. This is called gimbal lock.

```text
Normal (3 independent axes):     Gimbal lock (2 axes aligned):
  Yaw  --+                        Yaw  --+
          |                               |
  Pitch --+-- Roll                 Pitch --+-- Roll  <- same axis!
```

At pitch = 90 degrees, the rotation matrix only depends on `(yaw - roll)`, not on
yaw and roll separately. Two different Euler triplets produce the same
orientation.

**This is why quaternions are preferred for runtime orientation.**

### Rotation matrices

Each basis-axis rotation matrix rotates in the plane perpendicular to
that axis:

```text
Rx(t): rotates YZ plane      Ry(t): rotates XZ plane      Rz(t): rotates XY plane
| 1    0     0  |             |  cos  0  sin |             | cos  -sin  0 |
| 0   cos  -sin |             |   0   1   0  |             | sin   cos  0 |
| 0   sin   cos |             | -sin  0  cos |             |  0     0   1 |
```

Properties of rotation matrices:

- **Orthonormal**: columns are perpendicular unit vectors
- **Determinant = 1**: volume is preserved
- **Inverse = transpose**: `R^-1 = R^T` (cheap to invert)

### Rodrigues' rotation formula

Rotates a vector around **any** axis (not just X, Y, or Z):

```text
v' = v*cos(t) + (k x v)*sin(t) + k*(k.v)*(1 - cos(t))
```

where `k` is the unit rotation axis and `t` is the angle.

The formula decomposes the vector into components parallel and perpendicular
to the axis. The parallel part stays fixed; the perpendicular part rotates.

### Axis-angle

Stores a rotation as an axis (unit vector) plus an angle (scalar). This is
the most natural way to describe a rotation:

> "Rotate 45 degrees around the Y axis."

4 values (3 for axis + 1 for angle). No gimbal lock. But hard to compose
or interpolate, so it's mainly an input/interface format — convert to
quaternions for computation.

### Quaternions

A quaternion `q = w + xi + yj + zk` is a 4-component number where
`i, j, k` are imaginary units satisfying:

```text
i*i = j*j = k*k = i*j*k = -1
i*j = k    j*k = i    k*i = j     (cyclic, like cross product)
j*i = -k   k*j = -i   i*k = -j   (anti-commutative)
```

A unit quaternion (length = 1) represents a rotation of angle `t` around
axis `(ax, ay, az)`:

```text
q = (cos(t/2),  sin(t/2)*ax,  sin(t/2)*ay,  sin(t/2)*az)
```

Key properties:

| Property | Formula | Meaning |
|----------|---------|---------|
| Identity | (1, 0, 0, 0) | No rotation |
| Conjugate | (w, -x, -y, -z) | Reverse rotation |
| Inverse | conjugate / length^2 | Undo rotation (= conjugate for unit quat) |
| Multiply | q1 * q2 | Compose rotations (apply q2 first) |
| Rotate | `q * v * q*` | Apply rotation to vector v |
| Double cover | q and -q are the same rotation | Two quaternions per rotation |

### Why half-angle?

The half-angle in `cos(t/2)` comes from the double-cover property: a 360-degree
rotation maps to `q = (-1, 0, 0, 0)`, while 720 degrees brings you back to
`(1, 0, 0, 0)`. The quaternion traverses its unit sphere **twice** for every
full turn of 3D space.

### Quaternion multiplication

Composes rotations like matrix multiplication. The formula expands the product
of two quaternions using the `i, j, k` multiplication rules:

```text
(a * b).w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
(a * b).x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y
(a * b).y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x
(a * b).z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
```

Like matrices: `C = A * B` means "apply B first, then A."

### Rotating a vector with a quaternion

The "sandwich product" `v' = q * v * q*` rotates vector `v` by quaternion `q`.
The vector is treated as a pure quaternion `(0, v.x, v.y, v.z)`.

An optimized formula avoids constructing intermediate quaternions:

```text
v' = v + 2*w*(u x v) + 2*(u x (u x v))
```

where `u = (q.x, q.y, q.z)` is the vector part of `q`.

### SLERP — Spherical Linear Interpolation

SLERP interpolates between two orientations along the shortest arc on the
unit sphere, producing constant angular velocity:

```text
slerp(a, b, t) = a * sin((1-t)*T) / sin(T)  +  b * sin(t*T) / sin(T)
where T = acos(dot(a, b))
```

- `t = 0` returns `a`, `t = 1` returns `b`
- Constant speed (uniform angular velocity)
- Shortest path (negates one quaternion if needed)
- Falls back to NLERP for very small angles

**NLERP** (Normalized Linear Interpolation) is cheaper: lerp + normalize.
Same path but non-constant speed. Often good enough for games.

### Conversions between representations

```text
Euler angles <----> Quaternion <----> Rotation matrix
                       ^
                       |
                   Axis-angle
```

| Conversion | Function | Notes |
|-----------|----------|-------|
| Euler -> Quaternion | `quat_from_euler(yaw, pitch, roll)` | Intrinsic Y-X-Z order |
| Quaternion -> Euler | `quat_to_euler(q)` | Returns (yaw, pitch, roll) in vec3 |
| Axis-angle -> Quaternion | `quat_from_axis_angle(axis, angle)` | Axis must be unit length |
| Quaternion -> Axis-angle | `quat_to_axis_angle(q, &axis, &angle)` | Angle in [0, 2pi] |
| Quaternion -> Matrix | `quat_to_mat4(q)` | For the GPU / MVP pipeline |
| Matrix -> Quaternion | `quat_from_mat4(m)` | Shepperd's method |

### When to use each representation

| Representation | Best for | Avoid for |
|---------------|----------|-----------|
| Euler angles | User input/display, debug output | Runtime orientation, interpolation |
| Rotation matrix | GPU transforms, MVP pipeline | Storage, interpolation |
| Axis-angle | Code-level rotation specification | Composition, storage |
| Quaternion | Storage, composition, interpolation | Direct user display |

**Typical pipeline in a game:**

```text
User input -> Euler angles
              -> quat_from_euler(yaw, pitch, roll)
                 -> quaternion (store, compose, interpolate)
                    -> quat_to_mat4(q)
                       -> mat4 (upload to GPU as part of MVP)
```

## Building

```bash
python scripts/run.py math/08
```

Or build manually:

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\math\08-orientation\Debug\08-orientation.exe

# Linux / macOS
./build/lessons/math/08-orientation/08-orientation
```

## Exercises

1. **Quaternion from two vectors:**
   Given two unit vectors `a` and `b`, find the quaternion that rotates
   `a` to `b`. Hint: the axis is `cross(a, b)` and the angle is
   `acos(dot(a, b))`.

2. **Camera orbit:**
   Implement an orbit camera: given a target point and a distance, use
   quaternions to rotate the camera around the target. Apply yaw and
   pitch from mouse input using `quat_from_axis_angle`.

3. **Verify double cover:**
   Create quaternion `q` and its negation `-q`. Show that `quat_to_mat4(q)`
   and `quat_to_mat4(-q)` produce the same rotation matrix.

4. **Gimbal lock detector:**
   Write a function that takes Euler angles and returns true if the
   pitch is within a threshold of +/-90 degrees (near gimbal lock).

5. **SLERP animation:**
   Generate two random rotations and SLERP between them with 60 steps.
   Verify that the angle between consecutive steps is constant (within
   floating-point tolerance).

6. **Matrix round-trip:**
   Create a non-trivial quaternion, convert to `mat4`, then back to `quat`
   with `quat_from_mat4`. Verify both quaternions rotate a test vector
   to the same result.

## See also

- [Math Lesson 01 — Vectors](../01-vectors/) — cross product, normalization
- [Math Lesson 05 — Matrices](../05-matrices/) — rotation matrices, transpose
- [Math Lesson 06 — Projections](../06-projections/) — the MVP pipeline that
  uses rotation matrices
- [Math library](../../../common/math/README.md) — `quat_*` functions,
  `vec3_rotate_axis_angle`
