# Physics Library Tests

Automated tests for `common/physics/forge_physics.h`.

Three test binaries cover different aspects of the physics library: core
particle dynamics, rigid body collision shapes, and the rigid body constraint
solver. A shared header (`test_physics_common.h`) provides the test framework
and common constants.

## Files

| File | Description |
|------|-------------|
| `test_physics.c` | Core tests: particle creation, force application, integration, collision detection, determinism, energy stability |
| `test_physics_shapes.c` | Collision shape tests: AABB, sphere, capsule, OBB, and shape-pair overlap queries |
| `test_physics_rbc.c` | Rigid body constraint tests (prefixed `RBC_`) |
| `test_physics_common.h` | Shared test framework and constants |
| `CMakeLists.txt` | Build target definitions |

## Running

```bash
# Build and run all physics suites
cmake --build build --target test_physics
ctest --test-dir build -R physics --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
