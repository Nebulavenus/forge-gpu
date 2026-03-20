# Physics Support Function Validation Removal

Documents the removal of NaN/Inf input validation from two hot-path physics
functions and the rationale behind it.

## Summary

`forge_physics_shape_support()` and `forge_physics_gjk_support()` in
`common/physics/forge_physics.h` had per-call input validation that checked
positions, directions, and quaternions for NaN, Infinity, zero-length, and
non-unit values. This validation was removed for performance. The functions
now require callers to pass valid, finite inputs with normalized quaternions.
Note: `forge_physics_shape_support()` retains its zero-length direction
fallback (returns the shape center); `forge_physics_gjk_support()` does not
check direction length — callers must pass non-zero directions.

## Functions affected

### `forge_physics_shape_support()`

Computes the farthest point on a collision shape's surface in a given
direction. This is the geometric core of the GJK algorithm.

**Validation removed:**

- `forge_isfinite()` checks on all 3 components of `pos` (position)
- `forge_isfinite()` checks on all 3 components of `dir` (direction)
- `forge_isfinite()` checks on all 4 components of `orient` (quaternion)
- Quaternion length-squared finiteness and epsilon check
- Quaternion normalization (`quat_normalize` when length deviates from 1.0)
- Direction length finiteness check
- Fallback computation (`pos` if finite, else origin)

**Validation retained:**

- Null/invalid shape check (`!shape || !forge_physics_shape_is_valid(shape)`)
- Zero-length direction check (`dir_len < FORGE_PHYSICS_EPSILON`)

**Total removed:** 13 `forge_isfinite` calls + 1 `quat_length_sq` + 1
`SDL_fabsf` + 1 conditional `quat_normalize` + 1 `vec3_length` finiteness
check.

### `forge_physics_gjk_support()`

Computes the Minkowski difference support vertex: `support_A(d) -
support_B(-d)`. Wraps two calls to `forge_physics_shape_support()`.

**Validation removed:**

- `forge_isfinite()` checks on all 6 position components (`pos_a`, `pos_b`)
- `forge_isfinite()` checks on all 3 direction components
- `vec3_length_squared(dir)` finiteness and epsilon check
- `gjk_validate_quat_()` on both quaternions (finiteness, length, normalization)
- `forge_isfinite()` checks on all 3 output `v.point` components (overflow guard)

**Validation retained:**

- Null/invalid shape check for both shapes
- NaN-sentinel return for null/invalid shapes

**Total removed:** 12 `forge_isfinite` calls on inputs + 2 quaternion
validations (8 `forge_isfinite` + 2 `quat_length_sq` + 2 conditional
`quat_normalize`) + 3 `forge_isfinite` on output = ~25 comparison
instructions per call.

## Performance rationale

These two functions sit in the innermost loop of GJK and EPA collision
detection. Call frequency for a typical frame:

| Scenario | `shape_support` calls | `gjk_support` calls |
|---|---|---|
| 50 SAP pairs, 10 GJK iterations | 2000 | 1000 |
| + 10 EPA calls, 20 iterations | 2400 | 1200 |

For a sphere (the simplest shape), the actual support computation is
`pos + normalize(dir) * radius` — approximately 8 arithmetic instructions.
The removed validation was 13+ comparison instructions, exceeding the
computation itself. For boxes (~15 instructions) and capsules (~25
instructions), the validation was 50-85% of the function body.

### Absolute cost

At 1000 `gjk_support` calls per frame with ~25 guard instructions each:
25,000 instructions removed. At 3 GHz with IPC of 4, this is ~2
microseconds per frame — 0.012% of a 60 Hz frame budget. The cost is
negligible in isolation but the guard-to-work ratio was disproportionate.

### Branch prediction

All validation branches followed the "never taken" pattern — the guards
only trigger on corrupted data. Modern CPUs predict these with >99.99%
accuracy, so the branch latency itself was near-zero. The cost was
purely in instruction fetch, decode, and retire bandwidth.

## Caller responsibility

Input validation is now the responsibility of the outer entry points:

- **`forge_physics_gjk_intersect()`** validates positions, shapes, and
  orientations before entering the GJK iteration loop. Located in
  `common/physics/forge_physics.h`.

- **`forge_physics_epa()`** validates all inputs (GJK result, shapes,
  positions, orientations) before expanding the polytope. Located in
  `common/physics/forge_physics.h`.

- **`forge_physics_shape_compute_aabb()`** retains its own NaN/Inf
  validation and quaternion normalization — it was not modified.

### Quaternion normalization

Both functions now require unit quaternions. Previously, they normalized
internally via `quat_normalize()` when the quaternion length deviated from
1.0 by more than `FORGE_PHYSICS_EPSILON`. This normalization was removed
because:

1. It was structurally embedded in the NaN/Inf validation block (guarded
   by `forge_isfinite(qlen_sq)` checks).
2. All callers already produce unit quaternions:
   - `forge_physics_rigid_body_integrate()` normalizes `rb->orientation`
     every frame via `forge_physics_rigid_body_update_derived()`.
   - Static bodies use `quat_identity()`, which is unit by construction.
3. `quat_conjugate()` (used inside the box support path) is only the
   inverse for unit quaternions. Non-unit quaternions produce incorrect
   box support points.

Passing a non-unit quaternion to either function is now undefined
behavior — the output will be geometrically incorrect but no crash or
memory corruption will occur.

## Undefined behavior contract

Passing any of the following to the modified functions is undefined
behavior:

- NaN in any position, direction, or quaternion component
- Infinity in any position, direction, or quaternion component
- Non-unit quaternion (length != 1.0)

The functions will produce garbage output. No specific outcome is
guaranteed — the caller may see incorrect intersection results,
wrong penetration depth, or nonsensical contact normals. In practice,
GJK's progress check often detects garbage support points and
terminates early, but this is an implementation artifact, not a
contract.

## Tests removed

14 tests were removed across two test files. All tested behavior that
was removed from the two functions. No tests for other functions were
affected.

### `tests/physics/test_physics_shapes.c` — 6 tests removed

| Test name | What it tested |
|---|---|
| `CS_support_nan_position_returns_origin` | NaN position fallback |
| `CS_support_nan_direction_returns_pos` | NaN direction fallback |
| `CS_support_nan_orientation_returns_pos` | NaN quaternion fallback |
| `CS_support_inf_direction_returns_pos` | Inf direction fallback |
| `CS_support_nonunit_quat_box` | Non-unit quaternion normalization (box) |
| `CS_support_nonunit_quat_capsule` | Non-unit quaternion normalization (capsule) |

### `tests/physics/test_physics_gjk.c` — 8 tests removed

| Test name | What it tested |
|---|---|
| `GJK_support_nan_position` | NaN position sentinel |
| `GJK_support_inf_position` | Inf position sentinel |
| `GJK_support_zero_direction` | Zero-length direction sentinel |
| `GJK_support_near_zero_direction` | Sub-epsilon direction sentinel |
| `GJK_support_nan_direction` | NaN direction sentinel |
| `GJK_support_zero_quat` | Zero quaternion sentinel |
| `GJK_support_overflow` | Position overflow sentinel |
| `GJK_support_dir_overflow` | Direction squared-length overflow sentinel |

### Tests retained

- Null/invalid shape tests for both functions (structural guards kept)
- All AABB NaN/Inf and normalization tests (`forge_physics_shape_compute_aabb`
  was not modified)
- All GJK `intersect`-level NaN/overflow tests (`forge_physics_gjk_intersect`
  still validates inputs)
- All 34 EPA tests (includes non-unit quaternion normalization test)
- Production/stress tests

All physics tests pass.

## Files changed

| File | Changes |
|---|---|
| `common/physics/forge_physics.h` | Removed validation from both functions, added inline comments and file-header note |
| `tests/physics/test_physics_shapes.c` | Removed 6 tests and their runner calls |
| `tests/physics/test_physics_gjk.c` | Removed 8 tests and their runner calls |

## Where the validation still exists

For reference, the following functions in `forge_physics.h` retain full
NaN/Inf validation:

- `forge_physics_particle_create()` — sanitizes NaN position to origin
- `forge_physics_integrate()` — rejects NaN state, restores last-good
- `forge_physics_collide_plane()` — rejects NaN position, normal, radius
- `forge_physics_collide_sphere_sphere()` — rejects NaN/Inf radius
- `forge_physics_rigid_body_create()` — sanitizes NaN position to origin
- `forge_physics_rigid_body_integrate()` — rejects NaN state, restores last-good
- `forge_physics_rb_collide_sphere_plane()` — validates radius and normal
- `forge_physics_rb_collide_box_plane()` — validates half-extents and normal
- `forge_physics_rb_collide_sphere_sphere()` — validates radii
- `forge_physics_rb_resolve_contact()` — validates normal and dt
- `forge_physics_shape_compute_aabb()` — validates position, orientation, normalizes quat
- `forge_physics_gjk_intersect()` — validates positions, orientations, shapes
- `forge_physics_epa()` — validates all inputs including GJK simplex vertices
