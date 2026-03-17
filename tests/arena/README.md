# Arena Allocator Tests

Automated tests for `common/arena/forge_arena.h`.

Tests arena creation, bump allocation, alignment guarantees, arena growth
(backing block chaining), reset, and destruction.

## Files

| File | Description |
|------|-------------|
| `test_arena.c` | All arena allocator tests |
| `CMakeLists.txt` | Build target definition |

## Running

```bash
# Build and run this suite
cmake --build build --target test_arena
ctest --test-dir build -R arena --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
