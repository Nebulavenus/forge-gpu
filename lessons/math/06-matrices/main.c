/*
 * Math Lesson 06 — Matrices
 *
 * A proper linear algebra lesson covering what matrices are and how they work,
 * before you ever use them for transforms.
 *
 * Sections:
 *   1.  What is a matrix? — Creating and printing matrices
 *   2.  Matrix-vector multiplication — the row-dot-column algorithm
 *   3.  What the columns mean — basis vectors
 *   4.  Orthonormal basis vectors — perpendicular + unit length
 *   5.  Matrix-matrix multiplication — combining matrices
 *   6.  Non-commutativity — A*B != B*A
 *   7.  Associativity — (A*B)*C = A*(B*C)
 *   8.  Identity matrix — the "do nothing" matrix
 *   9.  Transpose — swapping rows and columns
 *   10. Determinant — area/volume scaling
 *   11. Inverse — undoing a transformation
 *   12. Bridge to 4×4 — connecting to transforms
 *   13. Summary
 *
 * New math library additions in this lesson:
 *   mat3 type, mat3_create, mat3_identity, mat3_multiply, mat3_multiply_vec3,
 *   mat3_transpose, mat3_determinant, mat3_inverse, mat3_rotate, mat3_scale,
 *   mat4_transpose, mat4_determinant, mat4_inverse, mat4_from_mat3
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

/* Epsilon for approximate floating-point comparisons */
#define APPROX_EPSILON 0.0001f

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

static void print_vec3(const char *label, vec3 v)
{
    printf("  %-40s (%.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z);
}

static void print_mat3(const char *label, mat3 m)
{
    printf("  %s\n", label);
    printf("    | %8.3f  %8.3f  %8.3f |\n", m.m[0], m.m[3], m.m[6]);
    printf("    | %8.3f  %8.3f  %8.3f |\n", m.m[1], m.m[4], m.m[7]);
    printf("    | %8.3f  %8.3f  %8.3f |\n", m.m[2], m.m[5], m.m[8]);
}

static void print_mat4(const char *label, mat4 m)
{
    printf("  %s\n", label);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[0], m.m[4], m.m[8],  m.m[12]);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[1], m.m[5], m.m[9],  m.m[13]);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[2], m.m[6], m.m[10], m.m[14]);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[3], m.m[7], m.m[11], m.m[15]);
}

/* Check if two mat3 values are approximately equal (for verification) */
static bool mat3_approx_equal(mat3 a, mat3 b)
{
    for (int i = 0; i < 9; i++) {
        if (SDL_fabsf(a.m[i] - b.m[i]) > APPROX_EPSILON) return false;
    }
    return true;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("\n");
    printf("==============================================================\n");
    printf("  Matrices — The Language of Linear Transformations\n");
    printf("==============================================================\n");
    printf("\n");
    printf("A matrix is a grid of numbers arranged in rows and columns.\n");
    printf("In graphics, matrices encode transformations: rotation, scaling,\n");
    printf("translation, projection, and more.\n");
    printf("\n");
    printf("This lesson teaches the math behind matrices using 3x3 examples.\n");
    printf("Once you understand 3x3, the jump to 4x4 is easy.\n");

    /* ── 1. What is a Matrix? ─────────────────────────────────────────── */

    print_header("1. WHAT IS A MATRIX?");

    printf("  A matrix is a rectangular grid of numbers with rows and columns.\n");
    printf("  A 3x3 matrix has 3 rows and 3 columns (9 numbers total).\n\n");

    printf("  We store matrices in column-major order (matching HLSL).\n");
    printf("  m[0..2] = column 0, m[3..5] = column 1, m[6..8] = column 2\n\n");

    mat3 example = mat3_create(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );
    print_mat3("A 3x3 matrix:", example);

    printf("\n  mat3_create() takes values in row-major order (how you'd write\n");
    printf("  it on paper), but stores them column-major internally.\n\n");

    printf("  Column-major storage for this matrix:\n");
    printf("    m[0]=%.0f m[1]=%.0f m[2]=%.0f  (column 0)\n",
           example.m[0], example.m[1], example.m[2]);
    printf("    m[3]=%.0f m[4]=%.0f m[5]=%.0f  (column 1)\n",
           example.m[3], example.m[4], example.m[5]);
    printf("    m[6]=%.0f m[7]=%.0f m[8]=%.0f  (column 2)\n",
           example.m[6], example.m[7], example.m[8]);

    /* ── 2. Matrix-Vector Multiplication ──────────────────────────────── */

    print_header("2. MATRIX-VECTOR MULTIPLICATION");

    printf("  To multiply a matrix M by a vector v, take the dot product\n");
    printf("  of each ROW of M with the vector v.\n\n");

    mat3 m = mat3_create(
        2, 0, 1,
        0, 3, 0,
        1, 0, 2
    );
    vec3 v = vec3_create(1.0f, 2.0f, 3.0f);

    print_mat3("M:", m);
    print_vec3("v:", v);

    printf("\n  Step by step:\n");
    printf("    result.x = row0 . v = 2*1 + 0*2 + 1*3 = 5\n");
    printf("    result.y = row1 . v = 0*1 + 3*2 + 0*3 = 6\n");
    printf("    result.z = row2 . v = 1*1 + 0*2 + 2*3 = 7\n\n");

    vec3 result = mat3_multiply_vec3(m, v);
    print_vec3("M * v =", result);

    printf("\n  Another way to see it: M * v = v.x * col0 + v.y * col1 + v.z * col2\n");
    printf("    = 1*(2,0,1) + 2*(0,3,0) + 3*(1,0,2)\n");
    printf("    = (2,0,1) + (0,6,0) + (3,0,6)\n");
    printf("    = (5, 6, 7)  -- same result!\n");

    /* ── 3. What the Columns Mean — Basis Vectors ─────────────────────── */

    print_header("3. WHAT THE COLUMNS MEAN -- BASIS VECTORS");

    printf("  KEY INSIGHT: Each column of a matrix tells you where the\n");
    printf("  corresponding basis vector ends up after transformation.\n\n");

    printf("  Column 0 = where (1,0,0) goes = the new X axis\n");
    printf("  Column 1 = where (0,1,0) goes = the new Y axis\n");
    printf("  Column 2 = where (0,0,1) goes = the new Z axis\n\n");

    float angle_45 = FORGE_PI / 4.0f;  /* 45 degrees */
    mat3 rot = mat3_rotate(angle_45);
    print_mat3("45-degree rotation matrix:", rot);

    /* Multiply each basis vector to show columns match */
    vec3 ex = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 ey = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 ez = vec3_create(0.0f, 0.0f, 1.0f);

    vec3 new_x = mat3_multiply_vec3(rot, ex);
    vec3 new_y = mat3_multiply_vec3(rot, ey);
    vec3 new_z = mat3_multiply_vec3(rot, ez);

    printf("\n  Multiplying each standard basis vector:\n");
    print_vec3("M * (1,0,0) = column 0:", new_x);
    print_vec3("M * (0,1,0) = column 1:", new_y);
    print_vec3("M * (0,0,1) = column 2:", new_z);

    printf("\n  Compare with the matrix columns above — they match!\n");
    printf("  The rotation moved X to (%.3f, %.3f, 0) and Y to (%.3f, %.3f, 0).\n",
           new_x.x, new_x.y, new_y.x, new_y.y);
    printf("  Z was unchanged (this is a 2D rotation in the XY plane).\n");

    /* ── 4. Orthonormal Basis Vectors ─────────────────────────────────── */

    print_header("4. ORTHONORMAL BASIS VECTORS");

    printf("  'Orthonormal' means two things:\n");
    printf("    Orthogonal: columns are perpendicular (dot product = 0)\n");
    printf("    Normal:     columns have unit length (length = 1)\n\n");

    printf("  Rotation matrices have orthonormal columns. Let's verify:\n\n");

    /* Extract columns of the rotation matrix */
    vec3 col0 = vec3_create(rot.m[0], rot.m[1], rot.m[2]);
    vec3 col1 = vec3_create(rot.m[3], rot.m[4], rot.m[5]);
    vec3 col2 = vec3_create(rot.m[6], rot.m[7], rot.m[8]);

    printf("  Column lengths (should be 1.0):\n");
    printf("    |col0| = %.6f\n", vec3_length(col0));
    printf("    |col1| = %.6f\n", vec3_length(col1));
    printf("    |col2| = %.6f\n\n", vec3_length(col2));

    printf("  Dot products between columns (should be 0.0):\n");
    printf("    col0 . col1 = %.6f\n", vec3_dot(col0, col1));
    printf("    col0 . col2 = %.6f\n", vec3_dot(col0, col2));
    printf("    col1 . col2 = %.6f\n\n", vec3_dot(col1, col2));

    printf("  All lengths = 1, all dot products = 0. Orthonormal!\n\n");

    printf("  Why it matters:\n");
    printf("  - Camera coordinate frames are orthonormal (mat4_look_at)\n");
    printf("  - Rotation matrices preserve lengths and angles\n");
    printf("  - For orthonormal matrices: inverse = transpose (fast!)\n");

    /* ── 5. Matrix-Matrix Multiplication ──────────────────────────────── */

    print_header("5. MATRIX-MATRIX MULTIPLICATION");

    printf("  To multiply A * B, each column of the result is A times\n");
    printf("  the corresponding column of B.\n\n");

    mat3 a = mat3_create(
        1, 2, 0,
        0, 1, 0,
        0, 0, 1
    );
    mat3 b = mat3_create(
        1, 0, 0,
        3, 1, 0,
        0, 0, 1
    );

    print_mat3("A (shear X by Y):", a);
    print_mat3("B (shear Y by X):", b);

    mat3 ab = mat3_multiply(a, b);
    print_mat3("A * B:", ab);

    printf("\n  Each element: result[row][col] = dot(A's row, B's column)\n");
    printf("    Top-left:  1*1 + 2*3 + 0*0 = 7\n");
    printf("    Top-mid:   1*0 + 2*1 + 0*0 = 2\n");

    /* ── 6. Non-Commutativity ─────────────────────────────────────────── */

    print_header("6. NON-COMMUTATIVITY -- A*B != B*A");

    printf("  Matrix multiplication is NOT commutative.\n");
    printf("  A*B and B*A usually give different results.\n\n");

    mat3 ba = mat3_multiply(b, a);
    print_mat3("A * B:", ab);
    print_mat3("B * A:", ba);

    bool same = mat3_approx_equal(ab, ba);
    printf("\n  A*B == B*A?  %s\n", same ? "YES" : "NO -- different!");
    printf("  Order matters! This is why transform order is critical in 3D.\n");

    /* ── 7. Associativity ─────────────────────────────────────────────── */

    print_header("7. ASSOCIATIVITY -- (A*B)*C = A*(B*C)");

    printf("  Although order matters (A*B != B*A), GROUPING doesn't.\n");
    printf("  (A*B)*C always equals A*(B*C).\n\n");

    mat3 c = mat3_rotate(FORGE_PI / 6.0f);  /* 30 degrees */

    mat3 ab_c = mat3_multiply(mat3_multiply(a, b), c);
    mat3 a_bc = mat3_multiply(a, mat3_multiply(b, c));

    print_mat3("(A * B) * C:", ab_c);
    print_mat3("A * (B * C):", a_bc);

    bool assoc = mat3_approx_equal(ab_c, a_bc);
    printf("\n  (A*B)*C == A*(B*C)?  %s\n", assoc ? "YES -- associative!" : "NO");

    /* ── 8. Identity Matrix ───────────────────────────────────────────── */

    print_header("8. IDENTITY MATRIX");

    printf("  The identity matrix has 1s on the diagonal, 0s elsewhere.\n");
    printf("  I * M = M * I = M (like multiplying by 1).\n\n");

    mat3 id = mat3_identity();
    print_mat3("Identity:", id);

    printf("\n  Its columns are the standard basis vectors:\n");
    printf("    col0 = (1, 0, 0)  -- X axis\n");
    printf("    col1 = (0, 1, 0)  -- Y axis\n");
    printf("    col2 = (0, 0, 1)  -- Z axis\n\n");

    mat3 test_m = mat3_create(2, 3, 1, 4, 5, 6, 7, 8, 9);
    mat3 im = mat3_multiply(id, test_m);
    bool id_works = mat3_approx_equal(im, test_m);
    printf("  I * M == M?  %s\n", id_works ? "YES" : "NO");

    /* ── 9. Transpose ─────────────────────────────────────────────────── */

    print_header("9. TRANSPOSE -- SWAPPING ROWS AND COLUMNS");

    printf("  The transpose M^T swaps rows and columns: M^T[i][j] = M[j][i]\n");
    printf("  Visually: mirror across the main diagonal.\n\n");

    mat3 orig = mat3_create(1, 2, 3, 4, 5, 6, 7, 8, 9);
    mat3 trans = mat3_transpose(orig);

    print_mat3("Original M:", orig);
    print_mat3("Transpose M^T:", trans);

    printf("\n  Properties:\n");

    /* (M^T)^T = M */
    mat3 double_t = mat3_transpose(trans);
    bool prop1 = mat3_approx_equal(double_t, orig);
    printf("    (M^T)^T = M?  %s\n", prop1 ? "YES" : "NO");

    /* (A*B)^T = B^T * A^T */
    mat3 ab_t = mat3_transpose(mat3_multiply(a, b));
    mat3 bt_at = mat3_multiply(mat3_transpose(b), mat3_transpose(a));
    bool prop2 = mat3_approx_equal(ab_t, bt_at);
    printf("    (A*B)^T = B^T * A^T?  %s\n", prop2 ? "YES" : "NO");

    printf("\n  For rotation matrices, transpose = inverse (much faster!):\n");
    mat3 rot_t = mat3_transpose(rot);
    mat3 rot_product = mat3_multiply(rot, rot_t);
    bool rot_inv = mat3_approx_equal(rot_product, id);
    printf("    R * R^T = I?  %s\n", rot_inv ? "YES" : "NO");

    /* ── 10. Determinant ──────────────────────────────────────────────── */

    print_header("10. DETERMINANT -- AREA AND VOLUME SCALING");

    printf("  The determinant tells you how much a matrix scales area/volume.\n\n");

    printf("  Key values:\n");
    printf("    det > 0: preserves orientation, scales volume by det\n");
    printf("    det < 0: flips orientation (mirror)\n");
    printf("    det = 0: singular — squishes to lower dimension\n");
    printf("    det = 1: preserves volume exactly (rotations!)\n\n");

    float det_id = mat3_determinant(id);
    printf("  det(Identity) = %.1f  (no change)\n", det_id);

    mat3 scale2 = mat3_scale(vec2_create(2.0f, 2.0f));
    float det_scale = mat3_determinant(scale2);
    printf("  det(Scale 2x2) = %.1f  (area quadrupled: 2*2*1)\n", det_scale);

    float det_rot = mat3_determinant(rot);
    printf("  det(Rotation) = %.1f  (volume preserved)\n", det_rot);

    mat3 singular = mat3_create(1, 2, 3, 4, 5, 6, 5, 7, 9);
    float det_sing = mat3_determinant(singular);
    printf("  det(Singular) = %.1f  (squished to 2D — not invertible)\n", det_sing);

    printf("\n  Properties:\n");
    printf("    det(A * B) = det(A) * det(B)\n");

    float det_a = mat3_determinant(a);
    float det_b = mat3_determinant(b);
    float det_ab = mat3_determinant(ab);
    printf("    det(A)=%.1f  det(B)=%.1f  det(A)*det(B)=%.1f  det(A*B)=%.1f\n",
           det_a, det_b, det_a * det_b, det_ab);

    /* ── 11. Inverse ──────────────────────────────────────────────────── */

    print_header("11. INVERSE -- UNDOING A TRANSFORMATION");

    printf("  The inverse M^-1 undoes M: M * M^-1 = I\n");
    printf("  Only exists when det(M) != 0.\n\n");

    mat3 invertible = mat3_create(
        2, 1, 0,
        0, 3, 1,
        0, 0, 1
    );
    mat3 inv = mat3_inverse(invertible);

    print_mat3("M:", invertible);
    print_mat3("M^-1:", inv);

    mat3 check = mat3_multiply(invertible, inv);
    print_mat3("M * M^-1 (should be Identity):", check);

    bool is_identity = mat3_approx_equal(check, id);
    printf("\n  M * M^-1 = I?  %s\n", is_identity ? "YES" : "NO");

    printf("\n  For rotations, inverse = transpose (fast shortcut!):\n");
    mat3 rot_inverse = mat3_inverse(rot);
    mat3 rot_transpose = mat3_transpose(rot);
    bool rot_eq = mat3_approx_equal(rot_inverse, rot_transpose);
    printf("    R^-1 == R^T?  %s\n", rot_eq ? "YES" : "NO");

    printf("\n  Property: (A*B)^-1 = B^-1 * A^-1  (reversed order!)\n");
    mat3 ab_inv = mat3_inverse(mat3_multiply(a, b));
    mat3 b_inv_a_inv = mat3_multiply(mat3_inverse(b), mat3_inverse(a));
    bool inv_prop = mat3_approx_equal(ab_inv, b_inv_a_inv);
    printf("    (A*B)^-1 == B^-1 * A^-1?  %s\n", inv_prop ? "YES" : "NO");

    /* ── 12. Bridge to 4×4 ────────────────────────────────────────────── */

    print_header("12. BRIDGE TO 4x4 MATRICES");

    printf("  Everything above works the same for 4x4 matrices.\n");
    printf("  The extra dimension adds translation:\n\n");

    printf("  4x4 columns:\n");
    printf("    Column 0: X axis direction (rotation + scale)\n");
    printf("    Column 1: Y axis direction (rotation + scale)\n");
    printf("    Column 2: Z axis direction (rotation + scale)\n");
    printf("    Column 3: Translation (where the origin moves)\n\n");

    /* Demonstrate mat4_from_mat3 */
    mat3 rot3 = mat3_rotate(FORGE_PI / 4.0f);
    mat4 rot4 = mat4_from_mat3(rot3);
    print_mat3("3x3 rotation:", rot3);
    print_mat4("Embedded in 4x4 (mat4_from_mat3):", rot4);

    /* Show mat4 transpose, determinant, inverse work too */
    printf("\n  mat4 transpose, determinant, inverse work the same way:\n");
    mat4 t4 = mat4_translate(vec3_create(3.0f, 4.0f, 5.0f));
    mat4 combo = mat4_multiply(t4, rot4);
    printf("    det(rotation4x4) = %.1f\n", mat4_determinant(rot4));
    printf("    det(translate * rotate) = %.1f\n", mat4_determinant(combo));

    mat4 combo_inv = mat4_inverse(combo);
    mat4 combo_check = mat4_multiply(combo, combo_inv);

    /* Verify it's approximately identity */
    bool is_id4 = true;
    mat4 id4 = mat4_identity();
    for (int i = 0; i < 16; i++) {
        if (SDL_fabsf(combo_check.m[i] - id4.m[i]) > APPROX_EPSILON) {
            is_id4 = false;
            break;
        }
    }
    printf("    (T*R) * (T*R)^-1 = I?  %s\n", is_id4 ? "YES" : "NO");

    printf("\n  For the full transform pipeline (Model, View, Projection),\n");
    printf("  see: lessons/math/02-coordinate-spaces/\n");
    printf("  For using transforms in practice with the GPU,\n");
    printf("  see: lessons/gpu/06-depth-and-3d/\n");

    /* ── Summary ──────────────────────────────────────────────────────── */

    printf("\n");
    printf("==============================================================\n");
    printf("  Summary\n");
    printf("==============================================================\n");
    printf("\n");
    printf("  What a matrix IS:\n");
    printf("    * Grid of numbers (rows x columns)\n");
    printf("    * Column-major storage: m[col * 3 + row]\n");
    printf("    * Columns = where basis vectors go (new coordinate frame)\n");
    printf("\n");
    printf("  Multiplication:\n");
    printf("    * M * v: dot each row of M with v (transforms a vector)\n");
    printf("    * A * B: each column of result = A * column of B\n");
    printf("    * NOT commutative (A*B != B*A) — order matters!\n");
    printf("    * IS associative ((A*B)*C = A*(B*C))\n");
    printf("\n");
    printf("  Special operations:\n");
    printf("    * Identity: I*M = M*I = M (diagonal of 1s)\n");
    printf("    * Transpose: swap rows/columns (rotation inverse!)\n");
    printf("    * Determinant: area/volume scaling factor\n");
    printf("    * Inverse: M * M^-1 = I (undo the transform)\n");
    printf("\n");
    printf("  Orthonormal matrices (rotations):\n");
    printf("    * Columns are perpendicular (dot = 0) and unit length\n");
    printf("    * det = 1 (preserve volume)\n");
    printf("    * Inverse = transpose (fast!)\n");
    printf("\n");
    printf("  3x3 functions: mat3_create, mat3_identity, mat3_multiply,\n");
    printf("    mat3_multiply_vec3, mat3_transpose, mat3_determinant,\n");
    printf("    mat3_inverse, mat3_rotate, mat3_scale\n");
    printf("\n");
    printf("  4x4 additions: mat4_transpose, mat4_determinant,\n");
    printf("    mat4_inverse, mat4_from_mat3\n");
    printf("\n");
    printf("  See: lessons/math/06-matrices/README.md\n");
    printf("  See: lessons/math/02-coordinate-spaces/ (transforms + MVP)\n");
    printf("  See: lessons/gpu/06-depth-and-3d/ (using transforms in practice)\n");
    printf("  See: common/math/README.md (full API reference)\n");
    printf("\n");

    SDL_Quit();
    return 0;
}
