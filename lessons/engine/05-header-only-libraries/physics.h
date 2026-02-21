/*
 * physics.h — Declarations for a small physics module
 *
 * This header demonstrates a common multi-file pattern:
 *   physics.h  declares the public API (what other files can call)
 *   physics.c  implements the functions (one definition in one .c file)
 *
 * Notice that physics.h includes my_vec.h because its function signatures
 * use the Vec2 type.  When main.c includes both my_vec.h and physics.h,
 * the include guard in my_vec.h prevents it from being processed twice.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef PHYSICS_H
#define PHYSICS_H

#include "my_vec.h"  /* Vec2 type — needed in function signatures below */

/* Apply gravity to a velocity vector over a time step.
 * Returns the updated velocity. */
Vec2 physics_apply_gravity(Vec2 velocity, float dt);

/* Update a position given a velocity and time step.
 * Returns the new position. */
Vec2 physics_update_position(Vec2 position, Vec2 velocity, float dt);

#endif /* PHYSICS_H */
