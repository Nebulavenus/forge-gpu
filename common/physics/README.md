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

### Lesson 04 — Rigid Body State and Orientation

| Function | Purpose |
|---|---|
| `forge_physics_rigid_body_create()` | Create a rigid body with position, mass, damping, angular damping, restitution |
| `forge_physics_rigid_body_set_inertia_box()` | Set inertia tensor for a solid box (cuboid) |
| `forge_physics_rigid_body_set_inertia_sphere()` | Set inertia tensor for a solid sphere |
| `forge_physics_rigid_body_set_inertia_cylinder()` | Set inertia tensor for a solid cylinder (Y-axis) |
| `forge_physics_rigid_body_apply_force()` | Accumulate force at center of mass |
| `forge_physics_rigid_body_apply_force_at_point()` | Accumulate force at world point (generates torque) |
| `forge_physics_rigid_body_apply_torque()` | Accumulate torque directly |
| `forge_physics_rigid_body_update_derived()` | Recompute world-space inertia tensor from orientation |
| `forge_physics_rigid_body_integrate()` | Full symplectic Euler integration (linear + angular, with gyroscopic term) |
| `forge_physics_rigid_body_clear_forces()` | Zero force and torque accumulators |
| `forge_physics_rigid_body_get_transform()` | Get 4×4 model matrix for rendering |

Types: `ForgePhysicsRigidBody` — position, orientation (quaternion), velocities, inertia tensors,
force/torque accumulators, mass properties.

Constants: `FORGE_PHYSICS_MAX_ANGULAR_VELOCITY` (100 rad/s),
`FORGE_PHYSICS_QUAT_RENORM_THRESHOLD` (1e-4).

### Lesson 05 — Forces and Torques

| Function | Purpose |
|---|---|
| `forge_physics_rigid_body_apply_gravity()` | Apply gravitational acceleration (F = m * g at COM) |
| `forge_physics_rigid_body_apply_linear_drag()` | Apply velocity-proportional drag (F = -k * v) |
| `forge_physics_rigid_body_apply_angular_drag()` | Apply angular velocity drag (tau = -k * omega) |
| `forge_physics_rigid_body_apply_friction()` | Apply contact friction opposing tangential contact-point velocity |

### Lesson 06 — Resting Contacts and Friction

| Function | Purpose |
|---|---|
| `forge_physics_rb_collide_sphere_plane()` | Sphere-plane contact detection (signed distance) |
| `forge_physics_rb_collide_sphere_sphere()` | Sphere-sphere rigid body collision detection |
| `forge_physics_rb_collide_box_plane()` | OBB-plane contact detection (up to 8 corner contacts) |
| `forge_physics_rb_resolve_contact()` | Single contact resolution with Coulomb friction and Baumgarte stabilization |
| `forge_physics_rb_resolve_contacts()` | Iterative solver for multiple contacts (sequential impulse) |

Types: `ForgePhysicsRBContact` — contact point, normal, penetration depth, body indices,
static and dynamic friction coefficients.

Constants: `FORGE_PHYSICS_MAX_RB_CONTACTS` (64),
`FORGE_PHYSICS_DEFAULT_STATIC_FRICTION` (0.6),
`FORGE_PHYSICS_DEFAULT_DYNAMIC_FRICTION` (0.4),
`FORGE_PHYSICS_CONTACT_SOLVER_ITERATIONS` (10),
`FORGE_PHYSICS_BAUMGARTE_FACTOR` (0.2),
`FORGE_PHYSICS_PENETRATION_SLOP` (0.01 m),
`FORGE_PHYSICS_RB_RESTING_THRESHOLD` (0.5 m/s).

### Planned API (from Physics Lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 07–14 | *See [PLAN.md](../../PLAN.md)* | Contacts, GJK/EPA, constraints, solver |

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
| [Physics L04](../../lessons/physics/04-rigid-body-state/) | `ForgePhysicsRigidBody`, `forge_physics_rigid_body_create()`, `forge_physics_rigid_body_integrate()`, inertia setters, force/torque application |
| [Physics L05](../../lessons/physics/05-forces-and-torques/) | `forge_physics_rigid_body_apply_gravity()`, `forge_physics_rigid_body_apply_linear_drag()`, `forge_physics_rigid_body_apply_angular_drag()`, `forge_physics_rigid_body_apply_friction()` |
| [Physics L06](../../lessons/physics/06-resting-contacts-and-friction/) | `ForgePhysicsRBContact`, `forge_physics_rb_collide_sphere_plane()`, `forge_physics_rb_collide_sphere_sphere()`, `forge_physics_rb_collide_box_plane()`, `forge_physics_rb_resolve_contact()`, `forge_physics_rb_resolve_contacts()` |
