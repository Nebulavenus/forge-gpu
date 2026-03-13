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

### Lesson 01 — Point Particles

| Function | Purpose |
|---|---|
| `forge_physics_particle_create()` | Create a particle with position, mass, damping, restitution, radius |
| `forge_physics_apply_gravity()` | Add gravitational force to accumulator |
| `forge_physics_apply_drag()` | Add linear drag force opposing velocity |
| `forge_physics_apply_force()` | Add an arbitrary force to accumulator |
| `forge_physics_integrate()` | Symplectic Euler integration (velocity-first) |
| `forge_physics_collide_plane()` | Sphere-plane collision detection and response |
| `forge_physics_clear_forces()` | Reset force accumulator to zero |

### Lesson 02 — Springs and Constraints

| Function | Purpose |
|---|---|
| `forge_physics_spring_create()` | Create a spring with rest length, stiffness, and damping |
| `forge_physics_spring_apply()` | Apply Hooke's law + velocity damping between two particles |
| `forge_physics_constraint_distance_create()` | Create a distance constraint with target distance and stiffness |
| `forge_physics_constraint_solve_distance()` | Solve a single distance constraint via position projection |
| `forge_physics_constraints_solve()` | Gauss-Seidel multi-pass solver for multiple distance constraints |

### Lesson 03 — Particle Collisions

| Function | Purpose |
|---|---|
| `forge_physics_collide_sphere_sphere()` | Detect overlap between two particle spheres, fill contact |
| `forge_physics_resolve_contact()` | Apply impulse + positional correction for one contact |
| `forge_physics_resolve_contacts()` | Resolve an array of contacts (single pass) |
| `forge_physics_collide_particles_all()` | O(n²) all-pairs sphere-sphere detection |
| `forge_physics_collide_particles_step()` | Convenience: detect all + resolve all in one call |

Types: `ForgePhysicsContact` — contact normal, point, penetration, particle indices.

Constants: `FORGE_PHYSICS_MAX_CONTACTS` (256), `FORGE_PHYSICS_RESTING_THRESHOLD` (0.5 m/s).

### Planned API (from Physics Lessons)

| Lesson | Functions | Purpose |
|---|---|---|
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
| [Physics L01](../../lessons/physics/01-point-particles/) | `ForgePhysicsParticle`, integrate, gravity, drag, collide_plane |
| [Physics L02](../../lessons/physics/02-springs-and-constraints/) | `ForgePhysicsSpring`, `ForgePhysicsDistanceConstraint`, `forge_physics_spring_apply()`, `forge_physics_constraint_solve_distance()`, `forge_physics_constraints_solve()` |
| [Physics L03](../../lessons/physics/03-particle-collisions/) | `ForgePhysicsContact`, `forge_physics_collide_sphere_sphere()`, `forge_physics_resolve_contact()`, `forge_physics_collide_particles_step()` |
