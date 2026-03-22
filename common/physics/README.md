# Physics Library (`common/physics/`)

Header-only physics simulation library for forge-gpu. The physics lessons
teach concepts; this library is what remains when the learning is done. Every
lesson extends it, and it is the primary deliverable of the physics track.

The library must be correct, performant, tested, safe, and valid.
Every function implements a named algorithm, cites its source, handles
degenerate inputs, and has corresponding tests in `tests/physics/`.

## Usage

```c
#include "physics/forge_physics.h"
```

The library depends on:

- `common/math/forge_math.h` — vector, matrix, and quaternion operations
- `common/containers/forge_containers.h` — dynamic arrays for contacts and
  SAP broadphase storage

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
| `forge_physics_collide_particles_all()` | O(n²) all-pairs detection into dynamic array (`**out_contacts`) |
| `forge_physics_collide_particles_step()` | Detection + resolution into dynamic array (`**out_contacts`) |

Types: `ForgePhysicsContact` — contact normal, point, penetration, particle indices.

Constants: `FORGE_PHYSICS_RESTING_THRESHOLD` (0.5 m/s).

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

Constants: `FORGE_PHYSICS_DEFAULT_STATIC_FRICTION` (0.6),
`FORGE_PHYSICS_DEFAULT_DYNAMIC_FRICTION` (0.4),
`FORGE_PHYSICS_CONTACT_SOLVER_ITERATIONS` (10),
`FORGE_PHYSICS_BAUMGARTE_FACTOR` (0.2),
`FORGE_PHYSICS_PENETRATION_SLOP` (0.01 m),
`FORGE_PHYSICS_RB_RESTING_THRESHOLD` (0.5 m/s).

### Lesson 07 — Collision Shapes and Support Functions

| Function | Purpose |
|---|---|
| `forge_physics_shape_sphere()` | Create sphere collision shape |
| `forge_physics_shape_box()` | Create box collision shape |
| `forge_physics_shape_capsule()` | Create capsule collision shape (Y-axis) |
| `forge_physics_shape_is_valid()` | Validate shape type and dimensions |
| `forge_physics_rigid_body_set_inertia_capsule()` | Set inertia tensor for capsule (cylinder + 2 hemispheres) |
| `forge_physics_rigid_body_set_inertia_from_shape()` | Dispatch to correct per-shape inertia setter |
| `forge_physics_shape_support()` | Farthest point on shape in a direction (GJK foundation) |
| `forge_physics_shape_compute_aabb()` | World-space AABB from shape, position, orientation |
| `forge_physics_aabb_overlap()` | Boolean AABB overlap test (broadphase primitive) |
| `forge_physics_aabb_expand()` | Grow AABB by uniform margin |
| `forge_physics_aabb_center()` | Center point of AABB |
| `forge_physics_aabb_extents()` | Half-extents of AABB |

Types: `ForgePhysicsShapeType` (enum), `ForgePhysicsCollisionShape` (tagged union),
`ForgePhysicsAABB` (min/max pair).

Constants: `FORGE_PHYSICS_SHAPE_MIN_DIM` (1e-5),
`FORGE_PHYSICS_CAPSULE_HEMI_CENTROID_FRAC` (3/8).

### Lesson 08 — Sweep-and-Prune Broadphase

| Function | Purpose |
|---|---|
| `forge_physics_sap_init()` | Zero-initialize a SAP world (sets pointers to NULL) |
| `forge_physics_sap_destroy()` | Free dynamic endpoint and pair arrays |
| `forge_physics_sap_select_axis()` | Choose sweep axis with greatest AABB center variance |
| `forge_physics_sap_update()` | Populate endpoints, insertion-sort, sweep, output pairs |
| `forge_physics_sap_pair_count()` | Return number of overlapping pairs |
| `forge_physics_sap_get_pairs()` | Return pointer to pairs array |
| `forge_physics_vec3_axis()` | Extract a vec3 component by axis index (0=X, 1=Y, 2=Z) |

Types: `ForgePhysicsSAPEndpoint`, `ForgePhysicsSAPPair`,
`ForgePhysicsSAPWorld` (dynamic arrays via `forge_containers.h`).

### Lesson 09 — GJK Intersection Testing

| Function | Purpose |
|---|---|
| `forge_physics_gjk_support()` | Minkowski difference support point (sup_A(d) − sup_B(−d)) |
| `forge_physics_gjk_intersect()` | GJK boolean intersection test between two convex shapes |
| `forge_physics_gjk_test_bodies()` | Convenience: extract pos/orient from rigid bodies, call gjk_intersect |

Types: `ForgePhysicsGJKVertex` (Minkowski point + per-shape support points),
`ForgePhysicsGJKSimplex` (1–4 vertex evolving simplex),
`ForgePhysicsGJKResult` (intersecting flag, simplex for EPA, iteration count).

Constants: `FORGE_PHYSICS_GJK_MAX_ITERATIONS` (64),
`FORGE_PHYSICS_GJK_EPSILON` (1e-6).

### Lesson 10 — EPA Penetration Depth

| Function | Purpose |
|---|---|
| `forge_physics_epa()` | EPA penetration depth, contact normal, and contact points from GJK simplex |
| `forge_physics_epa_bodies()` | Convenience: extract pos/orient from rigid bodies, call epa |
| `forge_physics_gjk_epa_contact()` | Combined GJK+EPA pipeline producing a ForgePhysicsRBContact |

Types: `ForgePhysicsEPAFace` (polytope triangle with precomputed normal/dist),
`ForgePhysicsEPAEdge` (silhouette edge for polytope expansion),
`ForgePhysicsEPAResult` (valid flag, normal, depth, contact points, iterations).

Constants: `FORGE_PHYSICS_EPA_MAX_ITERATIONS` (64),
`FORGE_PHYSICS_EPA_MAX_VERTICES` (128), `FORGE_PHYSICS_EPA_MAX_FACES` (256),
`FORGE_PHYSICS_EPA_EPSILON` (1e-6).

### Lesson 11 — Contact Manifold

| Function | Purpose |
|---|---|
| `forge_physics_manifold_pair_key()` | Pack a body pair into a canonical uint64 cache key |
| `forge_physics_manifold_contact_id()` | Compute persistent contact ID from geometric features |
| `forge_physics_manifold_world_to_local()` | Transform world-space point to body-local space |
| `forge_physics_manifold_local_to_world()` | Transform body-local point to world space |
| `forge_physics_clip_polygon()` | Sutherland-Hodgman: clip polygon against one half-plane |
| `forge_physics_manifold_box_face()` | Get 4 world-space vertices and normal of a box face |
| `forge_physics_manifold_ref_face_box()` | Find box face most aligned with a direction |
| `forge_physics_manifold_incident_face_box()` | Find box face most anti-aligned with a direction |
| `forge_physics_manifold_reduce()` | Reduce >4 contacts to 4 maximizing contact area |
| `forge_physics_manifold_generate()` | Generate 1–4 contact manifold from EPA result via clipping |
| `forge_physics_manifold_cache_update()` | Insert/merge manifold into cache with warm-starting |
| `forge_physics_manifold_cache_prune()` | Remove stale manifolds not in this frame's broadphase |
| `forge_physics_manifold_cache_free()` | Free all manifold cache memory |
| `forge_physics_manifold_to_rb_contacts()` | Convert manifold to ForgePhysicsRBContact array |
| `forge_physics_gjk_epa_manifold()` | Full GJK+EPA+manifold pipeline in one call |

Types: `ForgePhysicsManifoldContact`, `ForgePhysicsManifold`,
`ForgePhysicsManifoldCacheEntry`

### Lesson 12 — Impulse-Based Resolution

| Function | Purpose |
|---|---|
| `forge_physics_si_tangent_basis()` | Build stable orthonormal tangent frame from contact normal |
| `forge_physics_si_prepare()` | Precompute effective masses, bias terms, initialize from warm-start |
| `forge_physics_si_warm_start()` | Apply cached impulses to body velocities before iteration |
| `forge_physics_si_solve_velocities()` | One velocity iteration with accumulated impulse clamping |
| `forge_physics_si_store_impulses()` | Write accumulated impulses back to manifolds for next-frame cache |
| `forge_physics_si_solve()` | Complete solver: prepare + warm-start + N iterations + store |
| `forge_physics_si_correct_positions()` | Push penetrating bodies apart along contact normal |
| `forge_physics_si_rb_contacts_to_manifold()` | Convert RBContact array to manifold for SI solver input |
| `forge_physics_rigid_body_integrate_velocities()` | Velocity-only integration (for split-step solvers) |
| `forge_physics_rigid_body_integrate_positions()` | Position-only integration (for split-step solvers) |

Types: `ForgePhysicsSIConstraint` (precomputed per-contact data),
`ForgePhysicsSIManifold` (constraint data for one manifold pair).

Constants: `FORGE_PHYSICS_SI_DEFAULT_ITERATIONS` (10).

### Lesson 13 — Constraint Solver

| Function | Purpose |
|---|---|
| `forge_physics_joint_ball_socket()` | Create a ball-socket (point) joint — 3 DOF removed |
| `forge_physics_joint_hinge()` | Create a hinge (revolute) joint — 5 DOF removed |
| `forge_physics_joint_slider()` | Create a slider (prismatic) joint — 5 DOF removed |
| `forge_physics_joint_prepare()` | Precompute K matrices, Baumgarte bias for all joints |
| `forge_physics_joint_warm_start()` | Apply cached impulses to body velocities |
| `forge_physics_joint_solve_velocities()` | One Gauss-Seidel iteration for joint constraints |
| `forge_physics_joint_store_impulses()` | Write accumulated impulses back to joints |
| `forge_physics_joint_correct_positions()` | Positional correction for joint drift |
| `forge_physics_joint_solve()` | Complete solver: prepare + warm-start + N iterations + store + position correction |

Types: `ForgePhysicsJoint` (persistent joint definition),
`ForgePhysicsJointType` (BALL_SOCKET / HINGE / SLIDER),
`ForgePhysicsJointSolverData` (per-step precomputed workspace).

Constants: `FORGE_PHYSICS_JOINT_BAUMGARTE` (0.1),
`FORGE_PHYSICS_JOINT_SLOP` (0.005).

### Planned API (from Physics Lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 14–15 | *TBD* | Stacking stability, advanced constraints |

## Design

- **Header-only** — `static inline` functions, no separate compilation unit
- **Uses forge_math** — Vectors (`vec3`), quaternions (`quat`), matrices (`mat4`)
- **Uses forge_containers** — Dynamic arrays for contacts and SAP broadphase
- **Naming** — `forge_physics_` prefix for functions, `ForgePhysics` for types
- **No hardcoded limits** — Contact buffers and SAP arrays grow dynamically.
  SAP body indices are `uint16_t`, capping broadphase at 65 535 bodies per world
- **Init/destroy lifecycle** — SAP worlds must be initialized with
  `forge_physics_sap_init()` and freed with `forge_physics_sap_destroy()`.
  Particle contact arrays are caller-owned dynamic arrays freed with
  `forge_arr_free()`.
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
| [Physics L07](../../lessons/physics/07-collision-shapes/) | `ForgePhysicsCollisionShape`, `forge_physics_shape_sphere()`, `forge_physics_shape_box()`, `forge_physics_shape_capsule()`, `forge_physics_shape_support()`, `forge_physics_shape_compute_aabb()`, `forge_physics_aabb_overlap()`, `forge_physics_rigid_body_set_inertia_from_shape()` |
| [Physics L08](../../lessons/physics/08-sweep-and-prune/) | `ForgePhysicsSAPWorld`, `forge_physics_sap_init()`, `forge_physics_sap_destroy()`, `forge_physics_sap_update()`, `forge_physics_sap_select_axis()`, `forge_physics_sap_pair_count()`, `forge_physics_sap_get_pairs()` |
| [Physics L09](../../lessons/physics/09-gjk-intersection/) | `ForgePhysicsGJKResult`, `forge_physics_gjk_support()`, `forge_physics_gjk_intersect()`, `forge_physics_gjk_test_bodies()` |
| [Physics L10](../../lessons/physics/10-epa-penetration-depth/) | `ForgePhysicsEPAResult`, `forge_physics_epa()`, `forge_physics_epa_bodies()`, `forge_physics_gjk_epa_contact()` |
| [Physics L11](../../lessons/physics/11-contact-manifold/) | `ForgePhysicsManifold`, `forge_physics_gjk_epa_manifold()`, `forge_physics_manifold_cache_update()`, `forge_physics_manifold_cache_prune()`, `forge_physics_manifold_to_rb_contacts()` |
| [Physics L12](../../lessons/physics/12-impulse-based-resolution/) | `ForgePhysicsSIManifold`, `forge_physics_si_prepare()`, `forge_physics_si_warm_start()`, `forge_physics_si_solve_velocities()`, `forge_physics_si_store_impulses()`, `forge_physics_si_correct_positions()` |
| [Physics L13](../../lessons/physics/13-constraint-solver/) | `ForgePhysicsJoint`, `ForgePhysicsJointSolverData`, `forge_physics_joint_ball_socket()`, `forge_physics_joint_hinge()`, `forge_physics_joint_slider()`, `forge_physics_joint_prepare()`, `forge_physics_joint_warm_start()`, `forge_physics_joint_solve_velocities()`, `forge_physics_joint_store_impulses()`, `forge_physics_joint_correct_positions()`, `forge_physics_joint_solve()` |
