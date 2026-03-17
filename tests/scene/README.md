# Scene Renderer Tests

Automated tests for `common/scene/forge_scene.h`.

Tests are split across three binaries linked from a shared common header.
Most tests exercise the pure-math and configuration layer — no GPU device
required. GPU-integration tests (group 9) require a Vulkan-capable device or
Lavapipe and are skipped gracefully when unavailable.

## Files

| File | Description |
|------|-------------|
| `test_scene.c` | Groups 1–9: config defaults, light VP math, camera math, inline accessors, struct layout sizes, error handling |
| `test_scene_model.c` | Groups 10–17: model loading, scene hierarchy traversal, per-primitive material binding, mesh instancing |
| `test_scene_skinned.c` | Skinned model loading, joint hierarchy, inverse bind matrices |
| `test_scene_transparency.c` | Groups 27–30: transparent draw collection, centroid depth sorting, two-pass draw splitting |
| `test_scene_common.h` | Shared test framework, extern declarations, and common helpers |
| `CMakeLists.txt` | Build target definitions |

## Running

```bash
# Build and run all scene suites
cmake --build build --target test_scene
ctest --test-dir build -R scene --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## GPU tests

Group 9 requires a Vulkan device. On headless Linux, install Lavapipe:

```bash
sudo apt install mesa-vulkan-drivers
```

The tests detect the absence of a GPU device and skip gracefully.

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
