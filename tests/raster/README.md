# Raster Library Tests

Automated tests for `common/raster/forge_raster.h`.

Tests the CPU triangle rasterizer: buffer allocation and clearing, edge-function
rasterization, barycentric interpolation, texture sampling, alpha blending,
indexed drawing, and BMP file writing.

## Files

| File | Description |
|------|-------------|
| `test_raster.c` | All rasterizer tests |
| `CMakeLists.txt` | Build target definition |

## Running

```bash
# Build and run this suite
cmake --build build --target test_raster
ctest --test-dir build -R raster --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
