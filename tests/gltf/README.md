# glTF Parser Tests

Automated tests for `common/gltf/forge_gltf.h` and `forge_gltf_anim.h`.

Tests write small glTF + binary (.bin) files to a temp directory, parse them,
and verify the output — vertices, indices, materials, nodes, scene hierarchy,
transforms, and animation data. A separate overflow test exercises parser
robustness against malformed input. A fuzz driver is provided for fuzzing
with libFuzzer.

## Files

| File | Description |
|------|-------------|
| `test_gltf.c` | Core parser tests (geometry, materials, nodes, animations) |
| `test_gltf_overflow.c` | Robustness tests against malformed / truncated glTF input |
| `fuzz_gltf.c` | libFuzzer driver for the glTF parser |
| `CMakeLists.txt` | Build target definitions |

## Running

```bash
# Build and run this suite
cmake --build build --target test_gltf
ctest --test-dir build -R gltf --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
