# Physics Library Scaling

The physics library (`common/physics/forge_physics.h`) uses dynamic arrays
from `forge_containers.h` for all variable-size storage. Arrays grow as
needed with one ceiling: SAP body indices are `uint16_t`, capping the
broadphase at 65 535 bodies per world.

## Dynamic arrays (library-managed)

| Buffer | Container | Grows when |
|---|---|---|
| `ForgePhysicsSAPWorld.endpoints` | `forge_arr` | Body count changes |
| `ForgePhysicsSAPWorld.pairs` | `forge_arr` | New overlapping pairs detected |

The SAP world must be initialized with `forge_physics_sap_init()` and freed
with `forge_physics_sap_destroy()`. Between frames, `sap_update` clears and
repopulates the pairs array — the backing allocation is preserved across
frames so capacity stabilizes after warmup.

## Dynamic arrays (caller-managed)

These functions write contacts into a caller-owned dynamic array:

| Function | Output | Ownership |
|---|---|---|
| `forge_physics_collide_particles_all()` | `ForgePhysicsContact **out_contacts` | Caller calls `forge_arr_free()` |
| `forge_physics_collide_particles_step()` | `ForgePhysicsContact **out_contacts` | Caller calls `forge_arr_free()` |

When using `collide_particles_all()` directly, callers should call
`forge_arr_set_length(contacts, 0)` each frame to clear the array without
releasing the allocation, then pass `&contacts` to the detection function.
The convenience wrapper `collide_particles_step()` clears automatically
each call — no manual reset needed.

## Fixed-size buffers (caller-owned)

These functions write into a caller-provided buffer with a `max_contacts`
parameter — the caller chooses the buffer size:

| Function | Max contacts per call | Sizing guideline |
|---|---|---|
| `forge_physics_rb_collide_box_plane()` | Up to 8 (geometry-bounded) | 8 × body count |
| `forge_physics_rb_collide_sphere_plane()` | 1 | N/A |
| `forge_physics_rb_collide_sphere_sphere()` | 1 | N/A |

## Temporary allocations

| Allocation | Size | Lifetime |
|---|---|---|
| `sap_update` active set | `count × (sizeof(uint16_t) + sizeof(int))` | Arena — reset each `sap_update` call, backing memory reused across frames |

## Constants reference

| Constant | Value | Purpose |
|---|---|---|
| `FORGE_PHYSICS_MAX_VELOCITY` | 500.0 | Velocity clamp |
| `FORGE_PHYSICS_MAX_ANGULAR_VELOCITY` | 100.0 | Angular velocity clamp |
| `FORGE_PHYSICS_EPSILON` | 1e-6 | Floating-point comparison guard |
