# OBJ Parser Tests

Automated tests for `common/obj/forge_obj.h`.

Tests write small Wavefront OBJ files to a temp directory, parse them, and
verify vertex positions, normals, UV coordinates, and triangle counts.

## Files

| File | Description |
|------|-------------|
| `test_obj.c` | All OBJ parser tests |
| `CMakeLists.txt` | Build target definition |

## Running

```bash
# Build and run this suite
cmake --build build --target test_obj
ctest --test-dir build -R obj --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
