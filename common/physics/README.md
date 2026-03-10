# Physics Library (`common/physics/`)

Header-only physics simulation library for forge-gpu. The physics lessons
teach concepts; this library is what remains when the learning is done. Every
lesson extends it, and it is the primary deliverable of the physics track.

The library must be robust, correct, performant, tested, safe, and valid.
Every function implements a named algorithm, cites its source, handles
degenerate inputs, and has corresponding tests in `tests/physics/`.

## Usage

```c
#include "physics/forge_physics.h"
```

The library depends on `common/math/forge_math.h` for vector, matrix, and
quaternion operations.

## API Reference

*The physics library grows with each lesson. API documentation will be added
as functions are implemented.*

### Planned API (from Physics Lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 01 — Point Particles | `forge_physics_integrate()`, `forge_physics_apply_gravity()`, `forge_physics_apply_drag()` | Particle dynamics, symplectic Euler |
| 02 — Springs | `forge_physics_spring_force()`, `forge_physics_constraint_distance()` | Hooke's law, distance constraints |
| 03 — Particle Collisions | `forge_physics_collide_sphere_sphere()`, `forge_physics_collide_sphere_plane()` | Collision detection and impulse response |
| 04 — Rigid Body State | `forge_physics_rigid_body_create()`, `forge_physics_rigid_body_integrate()` | Rigid body state, inertia, orientation |
| 05 — Forces and Torques | `forge_physics_apply_force_at_point()`, `forge_physics_apply_torque()` | Force application, angular dynamics |
| 06–14 | *See [PLAN.md](../../PLAN.md)* | Contacts, GJK/EPA, constraints, solver |

## Design

- **Header-only** — `static inline` functions, no separate compilation unit
- **Uses forge_math** — Vectors (`vec3`), quaternions (`quat`), matrices (`mat4`)
- **Naming** — `forge_physics_` prefix for functions, `ForgePhysics` for types
- **No allocations** — Functions operate on caller-owned data
- **Deterministic** — Fixed timestep input produces identical output
- **Numerically safe** — No unguarded divisions, no unvalidated normalizations,
  values with physical bounds are clamped
- **Tested** — Every function has tests for correctness, edge cases,
  conservation, determinism, and collision-specific behavior in
  `tests/physics/`

## Where It's Used

| Lesson | What it uses |
|---|---|
| *Coming soon* | See [PLAN.md](../../PLAN.md) for the roadmap |
