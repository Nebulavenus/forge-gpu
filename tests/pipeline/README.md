# Pipeline Tests

Tests for the asset pipeline: both the Python pipeline library (`pipeline/`)
and the C runtime loader (`common/pipeline/forge_pipeline.h`).

## Files

| File | Language | Description |
|------|----------|-------------|
| `test_pipeline.c` | C | Tests the `.fmesh` binary loader, free functions, and utility accessors from `forge_pipeline.h` |
| `test_config.py` | Python | TOML configuration loading and `PipelineConfig` validation |
| `test_scanner.py` | Python | Directory scanning, SHA-256 fingerprinting, and NEW/CHANGED/UNCHANGED classification |
| `test_plugin.py` | Python | `AssetPlugin` base class, `PluginRegistry`, and extension-based dispatch |
| `test_texture.py` | Python | Texture plugin: resize, format conversion, mipmap generation |
| `test_mesh.py` | Python | Mesh plugin: OBJ/glTF processing via `forge_mesh_tool` |
| `test_bundler.py` | Python | Bundle packing, compression, and random-access table of contents |
| `test_animation.py` | Python | Animation plugin: keyframe extraction and `.fanim` output |
| `test_scene.py` | Python | Scene plugin: scene hierarchy extraction via `forge_scene_tool` |
| `__init__.py` | Python | Package marker |
| `CMakeLists.txt` | CMake | C test build target |

## Running

```bash
# Run Python pipeline tests
uv run pytest tests/pipeline/ -v

# Run the C pipeline loader test
cmake --build build --target test_pipeline
ctest --test-dir build -R pipeline --output-on-failure

# Run all tests (C + Python)
cmake --build build
ctest --test-dir build && uv run pytest tests/pipeline/
```

## Exit codes

Python tests use pytest conventions (exit 0 = all passed, non-zero = failures).
The C binary exits 0 on pass and 1 on failure.
