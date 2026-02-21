/*
 * physics.c — A second translation unit that includes my_vec.h
 *
 * This file exists to demonstrate the one-definition rule.  Both main.c
 * and physics.c include my_vec.h.  Because every function in my_vec.h is
 * 'static inline', each .c file gets its own private copy of the vector
 * functions, and the linker sees no conflict.
 *
 * If the functions in my_vec.h were declared without 'static' (just plain
 * functions), the linker would see two public definitions of Vec2_create,
 * Vec2_add, etc. — one from main.o and one from physics.o — and report
 * a "multiple definition" error.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "physics.h"  /* includes my_vec.h -> Vec2 and all Vec2_* functions */

/* Gravitational acceleration in m/s^2 (downward, so negative Y). */
#define GRAVITY_ACCEL -9.8f

/* 'static const' keeps this private to this translation unit. */
static const Vec2 GRAVITY = { 0.0f, GRAVITY_ACCEL };

Vec2 physics_apply_gravity(Vec2 velocity, float dt)
{
    /* velocity += gravity * dt
     * Vec2_scale and Vec2_add come from my_vec.h. */
    return Vec2_add(velocity, Vec2_scale(GRAVITY, dt));
}

Vec2 physics_update_position(Vec2 position, Vec2 velocity, float dt)
{
    /* position += velocity * dt */
    return Vec2_add(position, Vec2_scale(velocity, dt));
}
