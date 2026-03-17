# Shapes Library Tests

Automated tests for `common/shapes/forge_shapes.h`.

Tests all procedural geometry generators — sphere, torus, capsule, cube, plane,
and others. Verifies vertex counts, index counts, winding order, normal
correctness, and that radius/dimension parameters are respected.

## Files

| File | Description |
|------|-------------|
| `test_shapes.c` | All procedural geometry tests |
| `CMakeLists.txt` | Build target definition |

## Running

```bash
# Build and run this suite
cmake --build build --target test_shapes
ctest --test-dir build -R shapes --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
