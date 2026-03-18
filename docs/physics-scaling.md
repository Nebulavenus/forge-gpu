# Physics Library Scaling

The physics library (`common/physics/forge_physics.h`) uses fixed-size arrays
for all internal storage. There are no heap allocations — all buffers are
either embedded in structs (compile-time capacity) or caller-owned (caller
chooses size and allocation strategy).

## Fixed-size buffers

| Buffer | Capacity | Approx. Size | Location |
|---|---|---|---|
| `ForgePhysicsSAPWorld.endpoints` | 512 (256 × 2) | 4 KB | Struct field |
| `ForgePhysicsSAPWorld.pairs` | 4,096 | 16 KB | Struct field |
| `sap_update` `active[]` | 256 | 256 B | Stack local |

## Caller-owned buffers

These functions take a pointer and capacity — the caller decides how to
allocate (stack, heap, arena):

| Buffer | Suggested cap | Used by |
|---|---|---|
| `ForgePhysicsContact[]` (particle) | `FORGE_PHYSICS_MAX_CONTACTS` (256) | `forge_physics_collide_particles_all/step` |
| `ForgePhysicsContact[]` (rigid body) | `FORGE_PHYSICS_MAX_RB_CONTACTS` (64) | Rigid body collision functions |

## Scaling bottleneck

The primary constraint is `FORGE_PHYSICS_SAP_MAX_BODIES = 256`. The SAP
world struct embeds its endpoint and pair arrays at compile time, so this
cap cannot be changed at runtime.

256 bodies is sufficient for the physics lessons and small games. For larger
scenes (thousands of bodies), the SAP world would need to switch from inline
arrays to dynamically allocated storage — `sap_init` would take a
`max_bodies` parameter, and a `sap_destroy` function would be added for
cleanup. Alternatively, a spatial hash grid or BVH would replace the SAP
broadphase entirely.

## Constants reference

| Constant | Value | Purpose |
|---|---|---|
| `FORGE_PHYSICS_SAP_MAX_BODIES` | 256 | Max bodies in broadphase |
| `FORGE_PHYSICS_SAP_MAX_PAIRS` | 4,096 | Max overlapping pairs (overflow flagged via `pair_overflow`) |
| `FORGE_PHYSICS_MAX_CONTACTS` | 256 | Suggested particle contact buffer size |
| `FORGE_PHYSICS_MAX_RB_CONTACTS` | 64 | Suggested rigid body contact buffer size |
| `FORGE_PHYSICS_MAX_VELOCITY` | 500.0 | Velocity clamp |
| `FORGE_PHYSICS_MAX_ANGULAR_VELOCITY` | 100.0 | Angular velocity clamp |
