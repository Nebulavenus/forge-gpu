/*
 * forge_physics.h — Physics library for forge-gpu
 *
 * A learning-focused physics library for particle simulation and collision
 * detection. Every function is documented with the underlying physical law,
 * usage examples, and cross-references to lessons and textbooks.
 *
 * Design principles:
 *   - Header-only with static inline functions — no link-time surprises
 *   - No heap allocation — all state is caller-owned
 *   - Deterministic: same inputs always produce same outputs
 *   - Guard all divisions and normalizations against degenerate inputs
 *
 * Depends on: common/math/forge_math.h (vec3 type and operations)
 *
 * See: lessons/physics/ for lessons teaching each concept.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_PHYSICS_H
#define FORGE_PHYSICS_H

#include "math/forge_math.h"
#include <math.h>      /* fabsf, sqrtf */
#include <stdbool.h>   /* bool */

/* ── Constants ─────────────────────────────────────────────────────────────── */

/* Maximum velocity magnitude (units per second).
 *
 * Velocities beyond this threshold indicate a numerical explosion —
 * integration has diverged. Clamping here prevents NaN propagation and
 * keeps the simulation visually stable while signalling that the timestep
 * is too large or forces are too extreme.
 *
 * 500 m/s is roughly Mach 1.5 — well beyond any reasonable game-object
 * speed, so legitimate motion is never affected.
 */
#define FORGE_PHYSICS_MAX_VELOCITY  500.0f

/* Small epsilon for floating-point comparisons and degenerate-case guards.
 *
 * Used to avoid division by zero and to detect near-zero vectors before
 * normalization. Chosen as 1e-6 to be comfortably above single-precision
 * rounding noise (~1e-7) while small enough not to swallow real values.
 */
#define FORGE_PHYSICS_EPSILON       1e-6f

/* ── Types ─────────────────────────────────────────────────────────────────── */

/* A point particle with position, velocity, and physical properties.
 *
 * This is the fundamental simulation object in particle physics. Each
 * particle carries its own mass, damping, restitution, and collision
 * radius, making it self-contained for integration and collision response.
 *
 * Static particles (mass == 0, inv_mass == 0) are unaffected by forces
 * and integration — they act as immovable obstacles.
 *
 * The prev_position field stores the position before the last integration
 * step, enabling smooth rendering via interpolation between physics ticks.
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 3 —
 * "The Laws of Motion", particle representation.
 *
 * See: Physics Lesson 01 — Point Particles
 */
typedef struct ForgePhysicsParticle {
    vec3  position;       /* current world position */
    vec3  prev_position;  /* position before last integration (for render interpolation) */
    vec3  velocity;       /* linear velocity (units per second) */
    vec3  force_accum;    /* accumulated forces this frame, cleared after integration */
    float mass;           /* mass in kilograms; 0 means static/immovable */
    float inv_mass;       /* 1/mass, precomputed; 0 for static particles */
    float damping;        /* velocity damping per frame [0..1], 0 = no damping, 1 = full stop */
    float restitution;    /* coefficient of restitution [0..1], 0 = inelastic, 1 = perfectly elastic */
    float radius;         /* collision radius for sphere-plane tests (meters) */
} ForgePhysicsParticle;

/* ── Particle creation ─────────────────────────────────────────────────────── */

/* Create a particle with valid initial state.
 *
 * Initializes all fields to safe defaults: zero velocity, zero forces,
 * prev_position equal to position. A mass of zero creates a static
 * (immovable) particle — its inv_mass is set to zero so all force-based
 * functions skip it automatically.
 *
 * Algorithm:
 *   inv_mass = (mass > epsilon) ? 1.0 / mass : 0.0
 *   damping  = clamp(damping, 0, 1)
 *   restitution = clamp(restitution, 0, 1)
 *
 * Parameters:
 *   position    (vec3)  — initial world position; any finite value
 *   mass        (float) — mass in kg; 0 or negative = static particle
 *   damping     (float) — velocity damping [0..1]; clamped if out of range
 *   restitution (float) — bounce factor [0..1]; clamped if out of range
 *   radius      (float) — collision sphere radius in meters; clamped >= 0
 *
 * Returns:
 *   A fully initialized ForgePhysicsParticle with zero velocity and forces.
 *
 * Usage:
 *   ForgePhysicsParticle ball = forge_physics_particle_create(
 *       vec3_create(0.0f, 5.0f, 0.0f),  // start 5m above origin
 *       1.0f,   // 1 kg
 *       0.01f,  // slight damping
 *       0.8f,   // fairly bouncy
 *       0.5f    // 0.5m radius
 *   );
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 3 —
 * particle initialization and inverse mass representation.
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline ForgePhysicsParticle forge_physics_particle_create(
    vec3 position, float mass, float damping, float restitution, float radius)
{
    ForgePhysicsParticle p;

    p.position      = position;
    p.prev_position = position;
    p.velocity      = vec3_create(0.0f, 0.0f, 0.0f);
    p.force_accum   = vec3_create(0.0f, 0.0f, 0.0f);

    /* Mass and inverse mass — zero or negative mass means static. */
    if (mass > FORGE_PHYSICS_EPSILON) {
        p.mass     = mass;
        p.inv_mass = 1.0f / mass;
    } else {
        p.mass     = 0.0f;
        p.inv_mass = 0.0f;
    }

    /* Clamp damping to [0, 1]. */
    if (damping < 0.0f)      damping = 0.0f;
    else if (damping > 1.0f) damping = 1.0f;
    p.damping = damping;

    /* Clamp restitution to [0, 1]. */
    if (restitution < 0.0f)      restitution = 0.0f;
    else if (restitution > 1.0f) restitution = 1.0f;
    p.restitution = restitution;

    /* Clamp radius to non-negative. */
    p.radius = (radius > 0.0f) ? radius : 0.0f;

    return p;
}

/* ── Force application ─────────────────────────────────────────────────────── */

/* Add gravitational force to a particle's force accumulator.
 *
 * Applies Newton's law of gravitation for uniform fields: F = m * g.
 * The gravity vector is typically (0, -9.81, 0) for Earth-surface
 * simulations, but any direction and magnitude are supported.
 *
 * Static particles (inv_mass == 0) are skipped — they are immovable.
 *
 * Algorithm:
 *   force_accum += mass * gravity
 *
 * Parameters:
 *   p       (ForgePhysicsParticle*) — target particle; must not be NULL
 *   gravity (vec3)                  — gravitational acceleration vector (m/s^2);
 *                                     typically (0, -9.81, 0) for Earth gravity
 *
 * Usage:
 *   vec3 gravity = vec3_create(0.0f, -9.81f, 0.0f);
 *   forge_physics_apply_gravity(&ball, gravity);
 *
 * Reference: Newton, "Principia Mathematica", Law II — F = ma applied
 * to uniform gravitational fields.
 * See also: Millington, "Game Physics Engine Development", Ch. 3 —
 * "The Force of Gravity".
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline void forge_physics_apply_gravity(ForgePhysicsParticle *p, vec3 gravity)
{
    /* Static particles are unaffected by forces. */
    if (p->inv_mass == 0.0f) {
        return;
    }

    /* F = m * g, added to the force accumulator. */
    p->force_accum = vec3_add(p->force_accum, vec3_scale(gravity, p->mass));
}

/* Add a linear drag force opposing the particle's velocity.
 *
 * Models simplified aerodynamic drag as a linear function of velocity:
 * F_drag = -drag_coeff * v. This is a first-order approximation suitable
 * for slow-moving objects. For high-speed objects, quadratic drag
 * (proportional to |v|^2) is more physically accurate, but linear drag
 * is simpler and sufficient for most game simulations.
 *
 * Static particles (inv_mass == 0) are skipped.
 *
 * Algorithm:
 *   force_accum += -drag_coeff * velocity
 *
 * Parameters:
 *   p          (ForgePhysicsParticle*) — target particle; must not be NULL
 *   drag_coeff (float)                — drag coefficient; >= 0. Higher values
 *                                       produce stronger resistance. Typical
 *                                       range: 0.01 to 1.0
 *
 * Usage:
 *   forge_physics_apply_drag(&ball, 0.1f);  // light air resistance
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 3 —
 * "Drag" (simplified model).
 * See also: Stokes' law for the physical basis of linear drag in viscous flow.
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline void forge_physics_apply_drag(ForgePhysicsParticle *p, float drag_coeff)
{
    /* Static particles are unaffected by forces. */
    if (p->inv_mass == 0.0f) {
        return;
    }

    /* Negative drag would inject energy — clamp to zero. */
    if (drag_coeff < 0.0f) drag_coeff = 0.0f;

    /* F_drag = -drag * v */
    vec3 drag_force = vec3_scale(p->velocity, -drag_coeff);
    p->force_accum = vec3_add(p->force_accum, drag_force);
}

/* Add an arbitrary force to a particle's force accumulator.
 *
 * This is the general-purpose force applicator. Any force — springs,
 * explosions, wind, player input — is added to the accumulator and
 * resolved during integration. Multiple forces combine additively
 * (principle of superposition).
 *
 * Static particles (inv_mass == 0) are skipped.
 *
 * Algorithm:
 *   force_accum += force
 *
 * Parameters:
 *   p     (ForgePhysicsParticle*) — target particle; must not be NULL
 *   force (vec3)                  — force vector in Newtons; any direction
 *                                   and magnitude
 *
 * Usage:
 *   vec3 wind = vec3_create(2.0f, 0.0f, 0.0f);
 *   forge_physics_apply_force(&ball, wind);
 *
 * Reference: Newton, "Principia Mathematica", Law II — the net force
 * on a body equals the sum of all individual forces.
 * See also: Millington, "Game Physics Engine Development", Ch. 3 —
 * "The Force Accumulator".
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline void forge_physics_apply_force(ForgePhysicsParticle *p, vec3 force)
{
    /* Static particles are unaffected by forces. */
    if (p->inv_mass == 0.0f) {
        return;
    }

    p->force_accum = vec3_add(p->force_accum, force);
}

/* ── Integration ───────────────────────────────────────────────────────────── */

/* Advance a particle by one timestep using symplectic Euler integration.
 *
 * Symplectic Euler (also called semi-implicit Euler) updates velocity
 * before position, which preserves energy better than explicit (forward)
 * Euler and is the standard choice for real-time physics:
 *
 *   1. a = force_accum * inv_mass
 *   2. v = v + a * dt            (velocity updated first)
 *   3. v = v * (1 - damping)     (velocity damping applied)
 *   4. Clamp |v| to MAX_VELOCITY (prevent numerical explosion)
 *   5. x = x + v * dt            (position uses clamped velocity)
 *   6. Clear force_accum
 *
 * Saving prev_position before updating allows the renderer to interpolate
 * between physics states for smooth motion at arbitrary frame rates.
 *
 * Static particles (inv_mass == 0) are skipped entirely. If dt <= 0,
 * the function returns immediately — negative time is not physical.
 *
 * Parameters:
 *   p  (ForgePhysicsParticle*) — target particle; must not be NULL
 *   dt (float)                 — timestep in seconds; must be > 0.
 *                                Typical fixed timestep: 1/60 (0.01667s).
 *                                Values above 1/30 risk instability.
 *
 * Usage:
 *   const float FIXED_DT = 1.0f / 60.0f;
 *   forge_physics_apply_gravity(&ball, gravity);
 *   forge_physics_integrate(&ball, FIXED_DT);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 3 —
 * "The Integration Method" (symplectic Euler).
 * See also: Hairer, Lubich, Wanner — "Geometric Numerical Integration"
 * for why symplectic integrators conserve energy over long simulations.
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline void forge_physics_integrate(ForgePhysicsParticle *p, float dt)
{
    /* Reject non-positive or non-finite timesteps.
     * NaN fails all comparisons, so check explicitly. */
    if (!(dt > 0.0f)) {
        return;
    }

    /* Static particles do not move. */
    if (p->inv_mass == 0.0f) {
        return;
    }

    /* Save previous position for render interpolation. */
    p->prev_position = p->position;

    /* Step 1: Compute acceleration from accumulated forces.
     * a = F / m = F * inv_mass (Newton's second law). */
    vec3 accel = vec3_scale(p->force_accum, p->inv_mass);

    /* Step 2: Update velocity (symplectic Euler — velocity first). */
    p->velocity = vec3_add(p->velocity, vec3_scale(accel, dt));

    /* Step 3: Apply velocity damping.
     * Damping of 0 means no energy loss; damping of 1 means full stop.
     * v *= (1 - damping) each frame. */
    p->velocity = vec3_scale(p->velocity, 1.0f - p->damping);

    /* Step 4: Clamp velocity magnitude to prevent numerical explosions. */
    float speed_sq = vec3_length_squared(p->velocity);
    if (speed_sq > FORGE_PHYSICS_MAX_VELOCITY * FORGE_PHYSICS_MAX_VELOCITY) {
        float speed = sqrtf(speed_sq);
        /* Guard against zero speed (should not happen given the check above,
         * but defensive coding costs nothing). */
        if (speed > FORGE_PHYSICS_EPSILON) {
            p->velocity = vec3_scale(p->velocity,
                                     FORGE_PHYSICS_MAX_VELOCITY / speed);
        }
    }

    /* Step 5: Update position using the new velocity. */
    p->position = vec3_add(p->position, vec3_scale(p->velocity, dt));

    /* Step 6: Clear the force accumulator for the next frame. */
    p->force_accum = vec3_create(0.0f, 0.0f, 0.0f);
}

/* ── Collision detection and response ──────────────────────────────────────── */

/* Test and resolve a sphere-plane collision for a particle.
 *
 * The plane is defined in Hessian normal form: dot(n, x) = d, where n is
 * the outward-facing unit normal and d is the signed distance from the
 * origin to the plane along n. The particle is treated as a sphere of
 * the given radius centered at its position.
 *
 * Penetration occurs when: dot(position, normal) - plane_d < radius.
 * On collision:
 *   1. Push the particle out so it sits exactly on the surface
 *   2. Reflect the velocity component along the normal
 *   3. Scale the reflected component by the coefficient of restitution
 *
 * Algorithm:
 *   dist = dot(pos, normal) - plane_d
 *   if dist < radius:
 *     penetration = radius - dist
 *     pos += normal * penetration            (push out)
 *     v_n = dot(v, normal) * normal          (normal velocity component)
 *     v_t = v - v_n                          (tangential component)
 *     v   = v_t - restitution * v_n          (reflect + restitution)
 *
 * Parameters:
 *   p            (ForgePhysicsParticle*) — target particle; must not be NULL
 *   plane_normal (vec3)                  — outward normal of the plane; must
 *                                          be unit length (or near-unit). If
 *                                          the normal has zero length, the
 *                                          function returns false safely.
 *   plane_d      (float)                — signed distance from origin to plane
 *                                          along the normal. For a ground plane
 *                                          at y=0 with normal (0,1,0), plane_d=0.
 *
 * Returns:
 *   true if a collision was detected and resolved, false otherwise.
 *
 * Usage:
 *   // Ground plane at y = 0, normal pointing up
 *   vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);
 *   if (forge_physics_collide_plane(&ball, ground_normal, 0.0f)) {
 *       // ball bounced off the ground
 *   }
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 7 —
 * "Particle-Plane Collision" (contact generation and resolution).
 * See also: Ericson, "Real-Time Collision Detection", Ch. 5 —
 * "Testing Sphere Against Plane".
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline bool forge_physics_collide_plane(
    ForgePhysicsParticle *p, vec3 plane_normal, float plane_d)
{
    /* Guard against degenerate (zero-length) normals and normalize for
     * robustness — callers may pass non-unit normals by accident. */
    float normal_len_sq = vec3_length_squared(plane_normal);
    if (normal_len_sq < FORGE_PHYSICS_EPSILON) {
        return false;
    }

    /* Static particles are immovable — skip collision response. */
    if (p->inv_mass == 0.0f) {
        return false;
    }

    float inv_len = 1.0f / sqrtf(normal_len_sq);
    plane_normal = vec3_scale(plane_normal, inv_len);
    plane_d *= inv_len;

    /* Signed distance from the particle center to the plane surface. */
    float dist = vec3_dot(p->position, plane_normal) - plane_d;

    /* No collision if the sphere does not penetrate. */
    if (dist >= p->radius) {
        return false;
    }

    /* ── Collision response ────────────────────────────────────────────── */

    /* Step 1: Push the particle out of the plane so it rests on the surface.
     * penetration = how far the sphere has sunk below the plane. */
    float penetration = p->radius - dist;
    p->position = vec3_add(p->position, vec3_scale(plane_normal, penetration));

    /* Step 2: Decompose velocity into normal and tangential components. */
    float v_dot_n = vec3_dot(p->velocity, plane_normal);

    /* Only respond if the particle is moving into the plane (v_dot_n < 0).
     * If it's already moving away, the push-out is sufficient. */
    if (v_dot_n < 0.0f) {
        vec3 v_normal     = vec3_scale(plane_normal, v_dot_n);
        vec3 v_tangential = vec3_sub(p->velocity, v_normal);

        /* Step 3: Reflect the normal component and scale by restitution. */
        p->velocity = vec3_sub(v_tangential,
                               vec3_scale(v_normal, p->restitution));
    }

    return true;
}

/* ── Force accumulator management ──────────────────────────────────────────── */

/* Reset a particle's force accumulator to zero.
 *
 * Call this at the start of each physics step before applying forces,
 * or use it to cancel all pending forces on a particle. The integration
 * function (forge_physics_integrate) clears forces automatically after
 * each step, so manual clearing is only needed for custom force loops
 * or to abort forces mid-frame.
 *
 * Algorithm:
 *   force_accum = (0, 0, 0)
 *
 * Parameters:
 *   p (ForgePhysicsParticle*) — target particle; must not be NULL
 *
 * Usage:
 *   forge_physics_clear_forces(&ball);
 *   forge_physics_apply_gravity(&ball, gravity);
 *   forge_physics_apply_drag(&ball, 0.1f);
 *   forge_physics_integrate(&ball, dt);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 3 —
 * "The Force Accumulator" (clear-accumulate-integrate cycle).
 *
 * See: Physics Lesson 01 — Point Particles
 */
static inline void forge_physics_clear_forces(ForgePhysicsParticle *p)
{
    p->force_accum = vec3_create(0.0f, 0.0f, 0.0f);
}

#endif /* FORGE_PHYSICS_H */
