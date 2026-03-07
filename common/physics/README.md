# Physics Library (`common/physics/`)

Header-only physics simulation library for forge-gpu. Built lesson by lesson,
covering particle dynamics, rigid body simulation, collision detection, and
contact resolution.

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
| 04+ | *See [PLAN.md](../../PLAN.md)* | Rigid bodies, GJK/EPA, constraint solver |

## Design

- **Header-only** — `static inline` functions, no separate compilation unit
- **Uses forge_math** — Vectors (`vec3`), quaternions (`quat`), matrices (`mat4`)
- **Naming** — `forge_physics_` prefix for functions, `ForgePhysics` for types
- **No allocations** — Functions operate on caller-owned data
- **Deterministic** — Fixed timestep input produces identical output

## Where It's Used

| Lesson | What it uses |
|---|---|
| *Coming soon* | See [PLAN.md](../../PLAN.md) for the roadmap |
