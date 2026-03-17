# Math Library Tests

Automated tests for the forge-gpu math library (`common/math/forge_math.h`).

## What's tested

- **Scalar helpers**: `forge_log2f`, `forge_clampf`, `forge_trilerpf`
- **vec2**: create, add, sub, scale, dot, length, normalize, lerp
- **vec3**: create, add, sub, scale, dot, cross, length, normalize, lerp, trilerp, negate, reflect, rotate\_axis\_angle
- **vec4**: create, add, sub, scale, dot, trilerp
- **mat2**: create, identity, multiply, multiply\_vec2, transpose, determinant, singular\_values, anisotropy\_ratio
- **mat3**: create, identity, multiply, multiply\_vec3, transpose, determinant, inverse, rotate, scale, from\_diagonal
- **mat4**: identity, translate, scale, rotate\_x/y/z, look\_at, perspective, orthographic, multiply, transpose, determinant, inverse, from\_mat3, perspective\_from\_planes
- **quat**: identity, conjugate, normalize, multiply, from\_axis\_angle, from\_euler, to\_mat4, to\_mat3, from\_mat4, slerp, nlerp, rotate\_vec3, euler roundtrips
- **Color**: sRGB/linear conversions, luminance, RGB↔HSL, RGB↔HSV, RGB↔XYZ, XYZ↔xyY, tone mapping (Reinhard, ACES), exposure
- **Hash functions**: Wang, PCG, xxHash32, 2D/3D hash, hash distribution, hash\_to\_float
- **Noise**: Perlin 1D/2D/3D, Simplex 2D, FBM 2D/3D, domain warp, gradient, fade
- **Bezier curves**: quadratic and cubic endpoints, midpoints, tangents, arc length, split, flatten, vec3 variants
- **SDF (2D)**: circle, box, rounded box, segment, union, intersection, subtraction, smooth union/intersection
- **Scalar field**: gradient, Laplacian, heightmap normals

## Running the tests

```bash
# Build and run this suite only
cmake --build build --target test_math
ctest --test-dir build -R math --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Test output

Each test prints:

- ✅ `PASS` if the test succeeds
- ❌ `FAIL` with expected vs actual values if it fails

Example output:

```text
=== forge-gpu Math Library Tests ===
vec2 tests:
  Testing: vec2_create
    PASS
  Testing: vec2_add
    PASS
  ...

=== Test Summary ===
Total:  N
Passed: N
Failed: 0

All tests PASSED!
```

## Exit codes

- **0**: All tests passed
- **1**: One or more tests failed

Use the exit code in CI/CD pipelines to catch regressions.

## Adding new tests

When you add new math functions to `common/math/forge_math.h`:

1. Add a test function to `test_math.c`:

   ```c
   static void test_vec3_my_new_function(void)
   {
       TEST("vec3_my_new_function");
       vec3 input = vec3_create(1.0f, 2.0f, 3.0f);
       vec3 result = vec3_my_new_function(input);
       ASSERT_VEC3_EQ(result, expected_output);
       END_TEST();
   }
   ```

2. Call it from `main()`:

   ```c
   SDL_Log("\nvec3 tests:");
   test_vec3_create();
   // ... other tests
   test_vec3_my_new_function();  // Add here
   ```

3. Rebuild and run:

   ```bash
   cmake --build build --config Debug --target test_math
   build/tests/math/Debug/test_math.exe
   ```

## Test macros

- `ASSERT_FLOAT_EQ(actual, expected)` — Compare floats with epsilon tolerance
- `ASSERT_VEC2_EQ(actual, expected)` — Compare vec2 component-wise
- `ASSERT_VEC3_EQ(actual, expected)` — Compare vec3 component-wise
- `ASSERT_VEC4_EQ(actual, expected)` — Compare vec4 component-wise

All comparisons use `EPSILON = 0.0001f` to account for floating-point rounding.

## Philosophy

**Tests are documentation.** Each test shows how to use a function correctly and
what results to expect. When in doubt about a math function's behavior, read the tests.
