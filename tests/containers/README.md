# Tests — Containers

Tests for the stretchy container library in `common/containers/forge_containers.h`.

## Files

| File | Purpose |
|------|---------|
| `test_containers.c` | Array, hash map, string map, and thread-safe get tests |
| `CMakeLists.txt` | Build target definition |

## Running

```bash
cmake --build build --target test_containers
ctest --test-dir build -R containers
```
