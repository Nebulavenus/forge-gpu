# Tests

Unit and integration tests for forge-gpu libraries and tools. Each subdirectory
corresponds to a module in `common/` or `pipeline/`.

## Structure

| Directory    | Language | What it tests                          |
|-------------|----------|----------------------------------------|
| `arena/`    | C        | Arena allocator (`common/arena/`)      |
| `gltf/`     | C        | glTF 2.0 parser (`common/gltf/`)       |
| `math/`     | C        | Math library (`common/math/`)          |
| `obj/`      | C        | OBJ parser (`common/obj/`)             |
| `physics/`  | C        | Physics library (`common/physics/`)    |
| `raster/`   | C        | CPU rasterizer (`common/raster/`)      |
| `scene/`    | C        | Scene renderer (`common/scene/`)       |
| `shapes/`   | C        | Procedural geometry (`common/shapes/`) |
| `ui/`       | C        | UI library (`common/ui/`)              |
| `audio/`    | C        | Audio library (`common/audio/`)        |
| `containers/` | C      | Stretchy containers (`common/containers/`) |
| `pipeline/` | Python   | Asset pipeline (`pipeline/`)           |

## Running

```bash
# Build and run all C tests
cmake --build build
ctest --test-dir build

# Run a single C test suite
cmake --build build --target test_math
ctest --test-dir build -R math

# Run Python pipeline tests
uv run pytest tests/pipeline/ -v
```
