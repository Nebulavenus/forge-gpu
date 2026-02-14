# Math Library Tests

Automated tests for the forge-gpu math library (`common/math/forge_math.h`).

## What's tested

- **vec2**: create, add, sub, scale, dot, length, normalize, lerp (8 tests)
- **vec3**: create, add, sub, scale, dot, cross, length, normalize, lerp (9 tests)
- **vec4**: create, add, dot (3 tests)
- **mat4**: identity, translate, scale, rotate_z, multiply (6 tests)

**Total: 26 tests**

## Running the tests

### Build and run directly

```bash
cmake --build build --config Debug --target test_math
build/tests/math/Debug/test_math.exe
```

### Run via CTest

```bash
cd build
ctest -C Debug --output-on-failure
```

CTest runs all registered tests and provides a summary.

## Test output

Each test prints:
- ✅ `PASS` if the test succeeds
- ❌ `FAIL` with expected vs actual values if it fails

Example output:
```
=== forge-gpu Math Library Tests ===
vec2 tests:
  Testing: vec2_create
    PASS
  Testing: vec2_add
    PASS
  ...

=== Test Summary ===
Total:  26
Passed: 26
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

## Future additions

- Matrix inverse, transpose
- Quaternion operations
- Projection matrices (perspective, orthographic)
- More edge cases (zero vectors, degenerate matrices, etc.)

Add tests as you add functionality — keep the math library reliable!
