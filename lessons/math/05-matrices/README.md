# Math Lesson 05 — Matrices

## What you'll learn

- What a matrix is (rows, columns, dimensions)
- Column-major vs row-major storage
- Matrix-vector multiplication (the row-dot-column algorithm, step by step)
- Matrix-matrix multiplication (worked examples with 3x3)
- What the columns of a matrix mean (basis vectors, coordinate frames)
- Orthonormal basis vectors
- Identity matrix
- Non-commutativity (A\*B != B\*A) and associativity
- Transpose
- Determinant (area/volume scaling)
- Inverse (undoing a transform)
- Special matrices: diagonal, orthogonal, symmetric

## Result

After running this lesson you will understand how matrices work as mathematical
objects — multiplication, transpose, determinant, inverse — before using them as
transforms. This is the foundation for everything in 3D graphics.

## The math

### 1. What is a matrix?

A matrix is a rectangular grid of numbers arranged in **rows** and **columns**.

A 3x3 matrix (3 rows, 3 columns):

```text
| a  b  c |
| d  e  f |
| g  h  i |
```

In graphics we use:

- **2x2** for 2D rotation/scale (rarely used directly)
- **3x3** for 2D transforms, normal matrices, and the rotation/scale part of 4x4
- **4x4** for full 3D transforms (rotation + scale + translation + projection)

#### Element notation

`M[row][col]` — row first, then column. For the matrix above:
`M[0][0] = a`, `M[0][1] = b`, `M[1][0] = d`, etc.

#### Column-major storage

forge_math.h stores matrices in **column-major** order, matching HLSL's layout.
This means columns are contiguous in memory:

```text
mat3 storage:                      mat4 storage:
  m[0..2] = column 0                m[0..3]   = column 0
  m[3..5] = column 1                m[4..7]   = column 1
  m[6..8] = column 2                m[8..11]  = column 2
                                     m[12..15] = column 3

Access element at row r, column c:
  mat3: m[c * 3 + r]
  mat4: m[c * 4 + r]
```

To access row 1, column 2 of a mat3: `m[2 * 3 + 1] = m[7]`.

**Why column-major?** It matches HLSL shaders, so matrices can be uploaded to
the GPU without transposing. It also matches mathematical convention where
`v' = M * v` multiplies on the left.

### 2. Matrix-vector multiplication

To compute `result = M * v`, take the **dot product** of each row of M with v:

```text
| a  b  c |   | x |     | a*x + b*y + c*z |
| d  e  f | * | y |  =  | d*x + e*y + f*z |
| g  h  i |   | z |     | g*x + h*y + i*z |
```

**Worked example:**

```text
| 2  0  1 |   | 1 |     | 2*1 + 0*2 + 1*3 |     | 5 |
| 0  3  0 | * | 2 |  =  | 0*1 + 3*2 + 0*3 |  =  | 6 |
| 1  0  2 |   | 3 |     | 1*1 + 0*2 + 2*3 |     | 7 |
```

**Another way to think about it** — as a linear combination of columns:

```text
M * v = v.x * column0 + v.y * column1 + v.z * column2
      = 1*(2,0,1) + 2*(0,3,0) + 3*(1,0,2)
      = (2,0,1) + (0,6,0) + (3,0,6)
      = (5, 6, 7)
```

Both perspectives give the same answer. The column view leads directly to the
next insight.

**Code:** `mat3_multiply_vec3(m, v)` and `mat4_multiply_vec4(m, v)`

### 3. What the columns mean — basis vectors

**Key insight:** Each column of a matrix tells you where the corresponding
standard basis vector ends up after the transformation.

- Column 0 = where (1, 0, 0) goes = the new X axis
- Column 1 = where (0, 1, 0) goes = the new Y axis
- Column 2 = where (0, 0, 1) goes = the new Z axis

When you multiply a basis vector by a matrix, you simply extract that column:

```text
| a  d  g |   | 1 |     | a |
| b  e  h | * | 0 |  =  | b |   ← column 0
| c  f  i |   | 0 |     | c |
```

**Example:** A 45-degree rotation matrix

```text
| 0.707  -0.707  0 |
| 0.707   0.707  0 |
| 0       0      1 |
```

- Column 0: (0.707, 0.707, 0) — the X axis rotated 45 degrees
- Column 1: (-0.707, 0.707, 0) — the Y axis rotated 45 degrees
- Column 2: (0, 0, 1) — the Z axis unchanged (this is a 2D rotation)

The columns form a new **coordinate frame** — they describe the rotated axes.

For a 4x4 transform matrix, there's one more column:

- Column 3 = where (0, 0, 0, 1) goes = translation (where the origin moves)

### 4. Orthonormal basis vectors

A set of vectors is **orthonormal** when:

- **Orthogonal** — they are mutually perpendicular (dot product = 0)
- **Normal** — each has unit length (length = 1)

The standard basis {(1,0,0), (0,1,0), (0,0,1)} is orthonormal.

**Rotation matrices have orthonormal columns.** You can verify this:

```text
45° rotation:
  col0 = (0.707, 0.707, 0)    |col0| = 1.0
  col1 = (-0.707, 0.707, 0)   |col1| = 1.0
  col2 = (0, 0, 1)            |col2| = 1.0

  col0 · col1 = 0.707*(-0.707) + 0.707*0.707 + 0*0 = 0  ✓
  col0 · col2 = 0                                         ✓
  col1 · col2 = 0                                         ✓
```

All lengths = 1, all dot products = 0. Orthonormal!

**Why orthonormality matters:**

- Rotation matrices preserve lengths and angles (no stretching)
- Camera coordinate frames (`mat4_look_at`) produce orthonormal bases
- For orthonormal matrices: **inverse = transpose** (very fast!)
- Normal matrices for lighting are derived from orthonormal bases

### 5. Matrix-matrix multiplication

To compute `C = A * B`, each column of C is A times the corresponding column of B.

Or equivalently: `C[row][col] = dot(A's row, B's column)`.

**Worked example:**

```text
A = | 1  2  0 |    B = | 1  0  0 |
    | 0  1  0 |        | 3  1  0 |
    | 0  0  1 |        | 0  0  1 |

A * B:
  result[0][0] = 1*1 + 2*3 + 0*0 = 7
  result[0][1] = 1*0 + 2*1 + 0*0 = 2
  result[0][2] = 1*0 + 2*0 + 0*1 = 0
  ...

A * B = | 7  2  0 |
        | 3  1  0 |
        | 0  0  1 |
```

**Non-commutativity:** A\*B != B\*A in general. Order matters!

```text
B * A = | 1  2  0 |     ← different from A * B above!
        | 3  7  0 |
        | 0  0  1 |
```

This is why transform order is critical in 3D graphics: "rotate then translate"
produces a different result than "translate then rotate."

**Associativity:** Grouping doesn't matter: (A\*B)\*C = A\*(B\*C). You can combine
matrices in any grouping and get the same result — just don't change the order.

**Code:** `mat3_multiply(a, b)` and `mat4_multiply(a, b)`

### 6. Identity matrix

The identity matrix has 1s on the diagonal and 0s everywhere else:

```text
| 1  0  0 |
| 0  1  0 |
| 0  0  1 |
```

It leaves everything unchanged: **I \* M = M \* I = M** (like multiplying by 1).

Its columns are the standard basis vectors (1,0,0), (0,1,0), (0,0,1) — meaning
it maps each axis to itself.

**Code:** `mat3_identity()` and `mat4_identity()`

### 7. Transpose

The transpose M^T swaps rows and columns: **M^T\[i\]\[j\] = M\[j\]\[i\]**

Visually: mirror across the main diagonal.

```text
M = | 1  2  3 |      M^T = | 1  4  7 |
    | 4  5  6 |             | 2  5  8 |
    | 7  8  9 |             | 3  6  9 |
```

**Properties:**

- `(M^T)^T = M` — double transpose is the original
- `(A*B)^T = B^T * A^T` — transpose reverses multiplication order
- `det(M^T) = det(M)` — transpose doesn't change the determinant

**For rotation matrices:** transpose = inverse. This is a major shortcut —
instead of computing the full inverse (expensive), just transpose (swap elements).

**Code:** `mat3_transpose(m)` and `mat4_transpose(m)`

### 8. Determinant

The determinant tells you how much a matrix scales area (2D) or volume (3D).

| Value | Meaning |
|-------|---------|
| det > 0 | Preserves orientation, scales volume by det |
| det < 0 | Flips orientation (mirror), scales volume by \|det\| |
| det = 0 | Singular — squishes to lower dimension, NOT invertible |
| det = 1 | Preserves volume exactly (rotations!) |

**3x3 formula** (cofactor expansion along first row):

```text
| a  b  c |
| d  e  f |  →  det = a(ei - fh) - b(di - fg) + c(dh - eg)
| g  h  i |
```

**Examples:**

- Identity: det = 1 (no change)
- Scale by 2 in X and Y: det = 2 \* 2 \* 1 = 4 (area quadrupled)
- Any rotation: det = 1 (volume preserved)

**Properties:**

- `det(A * B) = det(A) * det(B)`
- `det(I) = 1`
- `det(A^T) = det(A)`

**Code:** `mat3_determinant(m)` and `mat4_determinant(m)`

### 9. Inverse

The inverse M^-1 **undoes** the transformation: **M \* M^-1 = I**

An inverse only exists when det(M) != 0 (the matrix is non-singular).

**3x3 inverse method:** Compute the adjugate (transpose of cofactor matrix) and
divide by the determinant:

```text
M^-1 = adjugate(M) / det(M)
```

**Verification:** Multiply M by its inverse and check you get identity:

```text
M = | 2  1  0 |     M^-1 = | 0.5   -0.167  0.167 |
    | 0  3  1 |             | 0      0.333 -0.333 |
    | 0  0  1 |             | 0      0      1     |

M * M^-1 ≈ | 1  0  0 |  ✓
            | 0  1  0 |
            | 0  0  1 |
```

**For rotation matrices:** inverse = transpose (the fast path!).

**Properties:**

- `(A * B)^-1 = B^-1 * A^-1` — inverse reverses multiplication order
- `(M^-1)^-1 = M`

**Code:** `mat3_inverse(m)` and `mat4_inverse(m)`

### 10. Special matrices

**Diagonal matrix** — Only diagonal elements are non-zero. Represents independent
axis scaling:

```text
| sx  0   0  |
| 0   sy  0  |     Scales X by sx, Y by sy, Z by sz
| 0   0   sz |
```

**Symmetric matrix** — M = M^T (equal to its own transpose). Common in physics
(inertia tensors, stress tensors).

**Orthogonal matrix** — M^T \* M = I (transpose is the inverse). Key properties:

- Columns are orthonormal
- Preserves lengths and angles
- det = +1 (rotation) or det = -1 (rotation + reflection)
- All rotation matrices are orthogonal

### 11. Bridge to transforms

Now you know the math — matrices can encode translation, rotation, scale, and
more. For 4x4 matrices:

```text
| Xx  Yx  Zx  Tx |     Columns 0-2: X, Y, Z axes (rotation + scale)
| Xy  Yy  Zy  Ty |     Column 3: Translation (where the origin goes)
| Xz  Yz  Zz  Tz |
| 0   0   0   1  |
```

To see how these combine into the Model-View-Projection pipeline, see
[Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/).

To see matrices used in practice on the GPU, see
[GPU Lesson 06 — Depth Buffer & 3D Transforms](../../gpu/06-depth-and-3d/).

## New functions added to forge_math.h

### mat3 (3x3 matrices)

| Function | Description |
|----------|-------------|
| `mat3_create(9 floats)` | Create from row-major values (stored column-major) |
| `mat3_identity()` | 3x3 identity matrix |
| `mat3_multiply(a, b)` | 3x3 matrix multiplication |
| `mat3_multiply_vec3(m, v)` | Transform a vec3 by a mat3 |
| `mat3_transpose(m)` | Swap rows and columns |
| `mat3_determinant(m)` | Compute determinant |
| `mat3_inverse(m)` | Compute inverse via adjugate/determinant |
| `mat3_rotate(angle)` | 2D rotation in XY plane |
| `mat3_scale(v)` | 2D scale (vec2 for XY, Z stays 1) |

### mat4 additions

| Function | Description |
|----------|-------------|
| `mat4_transpose(m)` | Swap rows and columns |
| `mat4_determinant(m)` | 4x4 determinant via cofactor expansion |
| `mat4_inverse(m)` | 4x4 inverse via adjugate/determinant |
| `mat4_from_mat3(m)` | Embed mat3 into upper-left of mat4 |

## Building

```bash
python scripts/run.py math/05
```

Requires SDL3 and a C99 compiler (see project root README for full setup).

## Exercises

1. **Verify multiplication by hand**: Pick a 3x3 matrix and a vector. Compute
   M \* v by hand (row-dot-column), then check your answer with the program.

2. **Basis vector extraction**: Create a rotation matrix with `mat3_rotate()` at
   different angles (30, 60, 90 degrees). Print the columns and verify they match
   where the X and Y axes should point.

3. **Determinant of composition**: Create two scale matrices with different scale
   factors. Verify that `det(A * B) = det(A) * det(B)`.

4. **Inverse verification**: Create a matrix that combines rotation and scaling.
   Compute its inverse and verify that M \* M^-1 is approximately the identity.

5. **Transpose = inverse for rotations**: Create several rotation matrices at
   different angles. For each, verify that `mat3_transpose(R)` gives the same
   result as `mat3_inverse(R)`. Then try a scale matrix — does it still hold?

6. **Non-commutativity exploration**: Create a rotation and a scale matrix.
   Compute A\*B and B\*A. Print both results and explain geometrically why they
   differ.

7. **Singular matrices**: Create a matrix where one row is a multiple of another.
   Compute its determinant (should be 0) and try to invert it. What does the
   library return?

8. **4x4 bridge**: Use `mat4_from_mat3()` to promote a 3x3 rotation to 4x4.
   Then multiply it with a translation matrix. Verify that the 4x4 determinant
   is still 1.0.

## See also

- [Math Lesson 01 — Vectors](../01-vectors/) — dot product, cross product, normalization
- [Math Lesson 02 — Coordinate Spaces](../02-coordinate-spaces/) — transforms, MVP pipeline
- [GPU Lesson 06 — Depth Buffer & 3D Transforms](../../gpu/06-depth-and-3d/) — using matrices in practice
- [Math library API](../../../common/math/README.md) — full function reference
- [3Blue1Brown — Essence of Linear Algebra](https://www.youtube.com/playlist?list=PLZHQObOWTQDPD3MizzM2xVFitgF8hE_ab) — excellent visual explanations
