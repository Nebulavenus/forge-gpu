# Physics Lesson 06 — Resting Contacts and Friction

## Scope

Rigid body contact detection against planes, Coulomb friction (static and
dynamic), resting contact resolution with iterative solver, and a stacking
preview with simple shapes. First physics lesson with full forge UI overlay.

## Key concepts

1. **Sphere-plane contact detection** for rigid bodies
2. **Box-plane contact detection** (OBB vs plane, up to 8 contact points)
3. **Coulomb friction model** — static vs dynamic friction coefficients
4. **Impulse-based contact resolution** with friction for rigid bodies
5. **Iterative contact solver** — multiple passes for convergence
6. **Resting contact detection** — velocity threshold to zero restitution
7. **Stacking** — multiple bodies at rest on each other
8. **Gyroscopic stability** effects on resting contacts

## Library additions (forge_physics.h)

### New types

- `ForgePhysicsRBContact` — rigid body contact (normal, point, penetration,
  body indices, friction coefficients)

### New functions

- `forge_physics_rb_collide_sphere_plane()` — sphere-plane detection
- `forge_physics_rb_collide_sphere_sphere()` — sphere-sphere rigid body collision detection
- `forge_physics_rb_collide_box_plane()` — OBB-plane corner testing (up to 8 pts)
- `forge_physics_rb_resolve_contact()` — impulse + friction for one contact
- `forge_physics_rb_resolve_contacts()` — iterative solver with configurable passes

### New constants

- `FORGE_PHYSICS_MAX_RB_CONTACTS` (64)
- `FORGE_PHYSICS_DEFAULT_STATIC_FRICTION` (0.6)
- `FORGE_PHYSICS_DEFAULT_DYNAMIC_FRICTION` (0.4)
- `FORGE_PHYSICS_CONTACT_SOLVER_ITERATIONS` (10)
- `FORGE_PHYSICS_BAUMGARTE_FACTOR` (0.2) — positional correction bias
- `FORGE_PHYSICS_PENETRATION_SLOP` (0.01) — overlap tolerance

## Demo program (3 scenes)

1. **Resting Contacts** — Spheres and boxes dropping onto ground, settling
2. **Friction Comparison** — Objects sliding on flat ground with initial velocity
3. **Stacking** — Tower of spheres with body-body collisions, adjustable friction and restitution

## UI controls

- Scene selection buttons (1-3)
- Gravity slider
- Static friction slider
- Dynamic friction slider
- Restitution slider
- Solver iterations slider
- Readouts: contact count, total energy, body count, per-body velocity

## main.c Decomposition (chunked write)

- **Chunk 1** (~400 lines): includes, constants, types, helpers, scene init
- **Chunk 2** (~400 lines): physics step, ground collision, rendering
- **Chunk 3** (~400 lines): UI panel, SDL_AppInit, SDL_AppEvent
- **Chunk 4** (~300 lines): SDL_AppIterate, SDL_AppQuit

## Diagrams (scripts/forge_diagrams/physics/lesson_06.py)

1. `coulomb_friction_cone` — friction cone showing mu_s and mu_d
2. `contact_normal_tangent` — normal/tangent decomposition at contact point
3. `impulse_resolution` — before/after impulse diagram
4. `iterative_solver` — convergence visualization
5. `box_plane_contacts` — contact point generation for box-plane

## Test categories

- Sphere-plane: basic, penetrating, resting, static body, zero radius
- Box-plane: face contact, edge contact, corner contact, rotated box
- Contact resolution: normal impulse, friction impulse, restitution
- Iterative solver: convergence, stacking stability
- Edge cases: zero mass, zero friction, NaN rejection
- Determinism: identical inputs produce identical outputs
