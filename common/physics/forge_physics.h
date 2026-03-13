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
#include <math.h>      /* fabsf, sqrtf, fminf */
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

/* Solver iteration bounds for forge_physics_constraints_solve().
 *
 * At least 1 iteration is needed to make any progress; above 100 the
 * per-frame cost dominates without meaningful convergence gain for
 * real-time use.
 */
#define FORGE_PHYSICS_SOLVER_MIN_ITERATIONS  1
#define FORGE_PHYSICS_SOLVER_MAX_ITERATIONS  100

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

/* A spring connecting two particles with Hooke's law and velocity damping.
 *
 * Models an elastic connection that resists changes in distance from its
 * rest length. The spring exerts a restoring force proportional to
 * displacement (Hooke's law) and a damping force proportional to relative
 * velocity along the spring axis.
 *
 * Fields:
 *   a, b        — indices into a particle array
 *   rest_length — natural (equilibrium) length in meters
 *   stiffness   — spring constant k in N/m (higher = stiffer)
 *   damping     — damping coefficient b in Ns/m (higher = more energy loss)
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 6 —
 * "Springs and spring-like things".
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
typedef struct ForgePhysicsSpring {
    int   a;            /* index of first particle */
    int   b;            /* index of second particle */
    float rest_length;  /* natural length (meters); >= 0 */
    float stiffness;    /* spring constant k (N/m); >= 0 */
    float damping;      /* damping coefficient b (Ns/m); >= 0 */
} ForgePhysicsSpring;

/* A distance constraint that projects particles to maintain a target distance.
 *
 * Unlike springs, distance constraints use position-based dynamics (PBD)
 * to directly move particles toward the target distance. This produces
 * stiffer, more stable connections — well suited for ropes, chains, and
 * cloth where oscillation is undesirable.
 *
 * Fields:
 *   a, b       — indices into a particle array
 *   distance   — target distance in meters
 *   stiffness  — constraint stiffness [0..1]; 1 = fully rigid
 *
 * Reference: Müller et al., "Position Based Dynamics" (2006) — Section 3,
 * constraint projection.
 * See also: Jakobsen, "Advanced Character Physics" (GDC 2001) — Verlet
 * integration with distance constraints.
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
typedef struct ForgePhysicsDistanceConstraint {
    int   a;            /* index of first particle */
    int   b;            /* index of second particle */
    float distance;     /* target distance (meters); >= 0 */
    float stiffness;    /* constraint stiffness [0..1]; 1 = rigid */
} ForgePhysicsDistanceConstraint;

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

/* ── Spring creation and application ───────────────────────────────────────── */

/* Create a spring connecting two particles.
 *
 * Initializes a spring with the given parameters, clamping rest_length,
 * stiffness, and damping to non-negative values. The indices a and b
 * refer to positions in a particle array managed by the caller.
 *
 * Parameters:
 *   a           (int)   — index of the first particle
 *   b           (int)   — index of the second particle
 *   rest_length (float) — natural spring length in meters; clamped >= 0
 *   stiffness   (float) — spring constant k in N/m; clamped >= 0
 *   damping     (float) — damping coefficient b in Ns/m; clamped >= 0
 *
 * Returns:
 *   A fully initialized ForgePhysicsSpring.
 *
 * Algorithm:
 *   rest_length = max(rest_length, 0)
 *   stiffness   = max(stiffness, 0)
 *   damping     = max(damping, 0)
 *
 * Usage:
 *   ForgePhysicsSpring s = forge_physics_spring_create(0, 1, 2.0f, 50.0f, 1.0f);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 6 —
 * "Springs and spring-like things" (spring representation).
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
static inline ForgePhysicsSpring forge_physics_spring_create(
    int a, int b, float rest_length, float stiffness, float damping)
{
    ForgePhysicsSpring s;
    s.a = a;
    s.b = b;
    s.rest_length = (rest_length > 0.0f) ? rest_length : 0.0f;
    s.stiffness   = (stiffness > 0.0f)   ? stiffness   : 0.0f;
    s.damping     = (damping > 0.0f)     ? damping     : 0.0f;
    return s;
}

/* Apply spring force (Hooke's law + velocity damping) to two particles.
 *
 * Computes the spring force along the axis between particles a and b:
 *
 *   F_spring = k * (|d| - rest_length) * n̂
 *   F_damp   = b * dot(v_rel, n̂) * n̂
 *   F_total  = F_spring + F_damp
 *
 * where d = pos_b - pos_a, n̂ = normalize(d), and v_rel = vel_b - vel_a.
 *
 * The spring force is applied equally and oppositely to both particles
 * (Newton's third law). Static particles (inv_mass == 0) do not
 * accumulate forces but still participate in distance/direction
 * calculations — this lets you anchor a spring to a fixed point.
 *
 * Degenerate cases:
 *   - Coincident particles (|d| < epsilon): no force applied (direction
 *     is undefined)
 *   - Out-of-bounds indices: no force applied
 *
 * Parameters:
 *   spring    (const ForgePhysicsSpring*)    — spring to apply
 *   particles (ForgePhysicsParticle*)        — particle array
 *   count     (int)                          — number of particles
 *
 * Reference: Hooke, "De Potentia Restitutiva" (1678) — F = kx.
 * See also: Millington, "Game Physics Engine Development", Ch. 6 —
 * "The Basic Spring Generator".
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
static inline void forge_physics_spring_apply(
    const ForgePhysicsSpring *spring,
    ForgePhysicsParticle *particles, int count)
{
    if (!spring || !particles || count <= 0) {
        return;
    }

    /* Bounds-check indices. */
    if (spring->a < 0 || spring->a >= count ||
        spring->b < 0 || spring->b >= count) {
        return;
    }

    ForgePhysicsParticle *pa = &particles[spring->a];
    ForgePhysicsParticle *pb = &particles[spring->b];

    /* Both static — no force accumulation possible. */
    if (pa->inv_mass == 0.0f && pb->inv_mass == 0.0f) {
        return;
    }

    /* Direction vector from a to b. */
    vec3 d = vec3_sub(pb->position, pa->position);
    float dist = vec3_length(d);

    /* Skip coincident particles — direction is undefined. */
    if (dist < FORGE_PHYSICS_EPSILON) {
        return;
    }

    /* Unit direction from a toward b. */
    vec3 n = vec3_scale(d, 1.0f / dist);

    /* Hooke's law: F = k * (current_length - rest_length).
     * Positive displacement = extension → force pulls particles together.
     * Negative displacement = compression → force pushes particles apart. */
    float displacement = dist - spring->rest_length;
    float f_spring = spring->stiffness * displacement;

    /* Velocity damping along the spring axis.
     * v_rel = v_b - v_a projected onto the spring direction.
     * Damping opposes relative motion along the spring. */
    vec3 v_rel = vec3_sub(pb->velocity, pa->velocity);
    float f_damp = spring->damping * vec3_dot(v_rel, n);

    /* Total force magnitude along the spring axis. */
    float f_total = f_spring + f_damp;
    vec3 force = vec3_scale(n, f_total);

    /* Apply force to particle a (toward b) if dynamic. */
    if (pa->inv_mass > 0.0f) {
        pa->force_accum = vec3_add(pa->force_accum, force);
    }

    /* Apply equal and opposite force to particle b (toward a) if dynamic. */
    if (pb->inv_mass > 0.0f) {
        pb->force_accum = vec3_sub(pb->force_accum, force);
    }
}

/* ── Distance constraint creation and solving ──────────────────────────────── */

/* Create a distance constraint between two particles.
 *
 * Parameters:
 *   a         (int)   — index of the first particle
 *   b         (int)   — index of the second particle
 *   distance  (float) — target distance in meters; clamped >= 0
 *   stiffness (float) — constraint stiffness [0..1]; clamped to range
 *
 * Returns:
 *   A fully initialized ForgePhysicsDistanceConstraint.
 *
 * Algorithm:
 *   distance  = max(distance, 0)
 *   stiffness = clamp(stiffness, 0, 1)
 *
 * Usage:
 *   ForgePhysicsDistanceConstraint c =
 *       forge_physics_constraint_distance_create(0, 1, 2.0f, 1.0f);
 *
 * Reference: Müller et al., "Position Based Dynamics" (2006) — Section 3,
 * constraint definition and parameterization.
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
static inline ForgePhysicsDistanceConstraint
forge_physics_constraint_distance_create(
    int a, int b, float distance, float stiffness)
{
    ForgePhysicsDistanceConstraint c;
    c.a = a;
    c.b = b;
    c.distance  = (distance > 0.0f) ? distance : 0.0f;

    if (!(stiffness >= 0.0f))  stiffness = 0.0f;   /* catches NaN */
    else if (stiffness > 1.0f) stiffness = 1.0f;
    c.stiffness = stiffness;

    return c;
}

/* Solve a single distance constraint by projecting particle positions.
 *
 * Computes the position correction needed to satisfy the distance
 * constraint and distributes it between the two particles proportionally
 * to their inverse masses (lighter particles move more).
 *
 * Algorithm (position-based dynamics):
 *   d = pos_b - pos_a
 *   dist = |d|
 *   error = dist - target_distance
 *   correction = stiffness * error * n̂
 *   w_total = inv_mass_a + inv_mass_b
 *   pos_a += (inv_mass_a / w_total) * correction
 *   pos_b -= (inv_mass_b / w_total) * correction
 *
 * Static particles (inv_mass == 0) are immovable. If both particles
 * are static, no correction is applied. Coincident particles are
 * skipped (direction is undefined).
 *
 * IMPORTANT — position-only projection:
 *   This function modifies particle positions but does NOT update
 *   velocity. This is the standard PBD contract: the caller's
 *   simulation loop is responsible for the velocity/position
 *   relationship through the integration step. This matches the
 *   Millington two-phase pattern where position correction and
 *   velocity correction are independent passes.
 *
 * Parameters:
 *   constraint (const ForgePhysicsDistanceConstraint*) — constraint to solve
 *   particles  (ForgePhysicsParticle*)                 — particle array
 *   count      (int)                                   — number of particles
 *
 * Reference: Müller et al., "Position Based Dynamics" (2006) — Section 3.3,
 * distance constraint projection.
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
static inline void forge_physics_constraint_solve_distance(
    const ForgePhysicsDistanceConstraint *constraint,
    ForgePhysicsParticle *particles, int count)
{
    if (!constraint || !particles || count <= 0) {
        return;
    }

    /* Bounds-check indices. */
    if (constraint->a < 0 || constraint->a >= count ||
        constraint->b < 0 || constraint->b >= count) {
        return;
    }

    ForgePhysicsParticle *pa = &particles[constraint->a];
    ForgePhysicsParticle *pb = &particles[constraint->b];

    /* Both static — nothing to do. */
    float w_total = pa->inv_mass + pb->inv_mass;
    if (!(w_total > 0.0f)) {   /* both static, or NaN */
        return;
    }

    /* Direction vector from a to b. */
    vec3 d = vec3_sub(pb->position, pa->position);
    float dist = vec3_length(d);

    /* Skip coincident particles — direction is undefined. */
    if (dist < FORGE_PHYSICS_EPSILON) {
        return;
    }

    /* Unit direction from a toward b. */
    vec3 n = vec3_scale(d, 1.0f / dist);

    /* Position error: how far off we are from the target distance. */
    float error = dist - constraint->distance;

    /* Scaled correction along the spring axis. */
    vec3 correction = vec3_scale(n, constraint->stiffness * error);

    /* Mass-weighted distribution: lighter particles move more. */
    float w_a = pa->inv_mass / w_total;
    float w_b = pb->inv_mass / w_total;

    pa->position = vec3_add(pa->position, vec3_scale(correction, w_a));
    pb->position = vec3_sub(pb->position, vec3_scale(correction, w_b));
}

/* Solve multiple distance constraints iteratively (Gauss-Seidel).
 *
 * Applies all constraints repeatedly for the given number of iterations.
 * More iterations produce results closer to the exact solution. Each
 * pass uses the updated positions from the previous pass (Gauss-Seidel
 * style), which converges faster than Jacobi iteration.
 *
 * Parameters:
 *   constraints     (const ForgePhysicsDistanceConstraint*) — constraint array
 *   num_constraints (int) — number of constraints
 *   particles       (ForgePhysicsParticle*) — particle array
 *   num_particles   (int) — number of particles
 *   iterations      (int) — solver iterations; clamped to [1, 100]
 *
 * Usage:
 *   // Solve all constraints with 10 iterations
 *   forge_physics_constraints_solve(constraints, num_constraints,
 *                                   particles, num_particles, 10);
 *
 * Reference: Müller et al., "Position Based Dynamics" (2006) — Section 3.2,
 * Gauss-Seidel iteration for constraint projection.
 *
 * See: Physics Lesson 02 — Springs and Constraints
 */
static inline void forge_physics_constraints_solve(
    const ForgePhysicsDistanceConstraint *constraints, int num_constraints,
    ForgePhysicsParticle *particles, int num_particles,
    int iterations)
{
    if (!constraints || !particles) {
        return;
    }

    /* Nothing to solve. */
    if (num_constraints <= 0 || num_particles <= 0) {
        return;
    }

    /* Clamp iterations to a safe range. */
    if (iterations < FORGE_PHYSICS_SOLVER_MIN_ITERATIONS) {
        iterations = FORGE_PHYSICS_SOLVER_MIN_ITERATIONS;
    }
    if (iterations > FORGE_PHYSICS_SOLVER_MAX_ITERATIONS) {
        iterations = FORGE_PHYSICS_SOLVER_MAX_ITERATIONS;
    }

    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < num_constraints; i++) {
            forge_physics_constraint_solve_distance(
                &constraints[i], particles, num_particles);
        }
    }
}

/* ── Collision detection and response ──────────────────────────────────────── */

/* A contact point between two colliding particles.
 *
 * Stores the geometric and physical information needed to resolve a
 * sphere-sphere collision: the contact normal, contact point, penetration
 * depth, and the indices of the two particles involved.
 *
 * Fields:
 *   normal      — unit direction from particle B toward particle A
 *   point       — contact point (midpoint of the overlap region)
 *   penetration — overlap depth >= 0
 *   particle_a  — index of the first particle in the pair
 *   particle_b  — index of the second particle in the pair
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 7 —
 * contact data representation.
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
typedef struct ForgePhysicsContact {
    vec3  normal;       /* unit direction from B toward A — determines impulse sign */
    vec3  point;        /* contact point [m] — midpoint of overlap region           */
    float penetration;  /* overlap depth [m], >= 0                                  */
    int   particle_a;   /* index of first particle, must be in [0, count)           */
    int   particle_b;   /* index of second particle, must be in [0, count)          */
} ForgePhysicsContact;

/* Maximum number of contacts returned by forge_physics_collide_particles_all().
 *
 * For N particles the theoretical maximum is N*(N-1)/2 contacts. This
 * limit caps the contact buffer at 256 entries, which is sufficient for
 * ~23 mutually-colliding particles. If the scene exceeds this limit,
 * additional contacts are silently dropped — increase this constant or
 * implement spatial partitioning (see Physics Lesson 07).
 */
#define FORGE_PHYSICS_MAX_CONTACTS 256

/* Closing velocity threshold below which restitution is zeroed.
 *
 * When two particles collide with a relative closing velocity below
 * this threshold, the coefficient of restitution is set to zero to
 * prevent micro-bouncing (jitter). This is standard practice in
 * real-time physics engines.
 *
 * 0.5 m/s is a reasonable default — slow enough that visible bouncing
 * would look like noise, fast enough not to swallow real bounces.
 */
#define FORGE_PHYSICS_RESTING_THRESHOLD 0.5f

/* Detect a sphere-sphere collision between two particles.
 *
 * Tests whether the bounding spheres of particles a and b overlap.
 * If they do, fills out a ForgePhysicsContact with the collision
 * geometry: normal (from B toward A), contact point (midpoint of the
 * overlap region), and penetration depth.
 *
 * Algorithm:
 *   d = pos_a - pos_b
 *   dist = |d|
 *   sum_radii = radius_a + radius_b
 *   if dist < sum_radii:
 *     normal = normalize(d)           (from B toward A)
 *     penetration = sum_radii - dist
 *     point = pos_b + normal * (radius_b - penetration / 2)
 *
 * Edge cases:
 *   - Either radius is non-positive: no collision possible, returns false
 *   - Both particles are static (inv_mass == 0): skip, returns false
 *   - Coincident centers (dist < epsilon): uses arbitrary normal (0, 1, 0)
 *     to resolve the degenerate case deterministically
 *
 * Parameters:
 *   a     (const ForgePhysicsParticle*) — first particle; must not be NULL
 *   b     (const ForgePhysicsParticle*) — second particle; must not be NULL
 *   idx_a (int)                         — index of particle a; must be in
 *                                         [0, num_particles) for resolve calls
 *   idx_b (int)                         — index of particle b; must be in
 *                                         [0, num_particles) for resolve calls
 *   out   (ForgePhysicsContact*)        — output contact; filled on collision;
 *                                         must not be NULL
 *
 * Returns:
 *   true if a collision was detected and out was filled, false otherwise.
 *
 * Usage:
 *   ForgePhysicsContact contact;
 *   if (forge_physics_collide_sphere_sphere(&pa, &pb, 0, 1, &contact)) {
 *       // handle collision
 *   }
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 4.3 —
 * sphere-sphere intersection test.
 * See also: Millington, "Game Physics Engine Development", Ch. 7 —
 * "Generating contacts".
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline bool forge_physics_collide_sphere_sphere(
    const ForgePhysicsParticle *a, const ForgePhysicsParticle *b,
    int idx_a, int idx_b, ForgePhysicsContact *out)
{
    /* Zero-radius particles cannot collide. */
    if (a->radius <= 0.0f || b->radius <= 0.0f) {
        return false;
    }

    /* Two static particles cannot move apart — skip. */
    if (a->inv_mass == 0.0f && b->inv_mass == 0.0f) {
        return false;
    }

    /* Vector from B toward A. */
    vec3 d = vec3_sub(a->position, b->position);
    float dist_sq = vec3_dot(d, d);
    float sum_radii = a->radius + b->radius;

    /* No overlap — spheres are separated. */
    if (dist_sq >= sum_radii * sum_radii) {
        return false;
    }

    float dist = sqrtf(dist_sq);
    vec3 normal;

    if (dist < FORGE_PHYSICS_EPSILON) {
        /* Coincident centers — use arbitrary upward normal. */
        normal = vec3_create(0.0f, 1.0f, 0.0f);
        dist = 0.0f;
    } else {
        /* Normal from B toward A. */
        normal = vec3_scale(d, 1.0f / dist);
    }

    out->normal      = normal;
    out->penetration = sum_radii - dist;
    out->particle_a  = idx_a;
    out->particle_b  = idx_b;

    /* Contact point: midpoint of the overlap region.
     * Located along the line from B toward A, at B's surface minus
     * half the penetration depth. */
    out->point = vec3_add(b->position,
        vec3_scale(normal, b->radius - out->penetration * 0.5f));

    return true;
}

/* Resolve a single contact by applying impulse-based velocity response
 * and positional correction.
 *
 * Computes the closing velocity along the contact normal, determines the
 * impulse magnitude using the coefficient of restitution, and distributes
 * velocity and position changes proportional to each particle's inverse
 * mass.
 *
 * Algorithm:
 *   v_rel     = v_a - v_b
 *   v_closing = dot(v_rel, normal)
 *   if v_closing > 0: return                (already separating)
 *   e = min(restitution_a, restitution_b)
 *   if |v_closing| < RESTING_THRESHOLD: e = 0
 *   j = -(1 + e) * v_closing / (inv_mass_a + inv_mass_b)
 *   v_a += j * inv_mass_a * normal
 *   v_b -= j * inv_mass_b * normal
 *   // Positional correction: distribute penetration by inverse mass ratio
 *   total_inv = inv_mass_a + inv_mass_b
 *   pos_a += (inv_mass_a / total_inv) * penetration * normal
 *   pos_b -= (inv_mass_b / total_inv) * penetration * normal
 *
 * Parameters:
 *   contact   (const ForgePhysicsContact*) — the contact to resolve;
 *                                            must not be NULL
 *   particles (ForgePhysicsParticle*)      — particle array; must not be NULL
 *   count     (int)                        — number of particles in the array;
 *                                            contact indices must be in bounds
 *
 * Usage:
 *   forge_physics_resolve_contact(&contact, particles, num_particles);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 7 —
 * "Resolving contacts" (impulse-based response with positional projection).
 * See also: Baraff & Witkin, "Physically Based Modeling" (SIGGRAPH 1997) —
 * impulse derivation for frictionless collisions.
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline void forge_physics_resolve_contact(
    const ForgePhysicsContact *contact,
    ForgePhysicsParticle *particles, int count)
{
    if (!contact || !particles || count <= 0) {
        return;
    }

    /* Bounds-check indices. */
    if (contact->particle_a < 0 || contact->particle_a >= count ||
        contact->particle_b < 0 || contact->particle_b >= count) {
        return;
    }

    ForgePhysicsParticle *pa = &particles[contact->particle_a];
    ForgePhysicsParticle *pb = &particles[contact->particle_b];

    float inv_mass_sum = pa->inv_mass + pb->inv_mass;

    /* Both static — nothing to resolve. */
    if (inv_mass_sum < FORGE_PHYSICS_EPSILON) {
        return;
    }

    /* ── Velocity resolution ──────────────────────────────────────── */

    /* Relative velocity of A with respect to B. */
    vec3 v_rel = vec3_sub(pa->velocity, pb->velocity);

    /* Closing velocity along the contact normal.
     * Negative means approaching, positive means separating. */
    float v_closing = vec3_dot(v_rel, contact->normal);

    /* Already separating — no impulse needed. */
    if (v_closing > 0.0f) {
        /* Still apply positional correction if penetrating. */
        if (contact->penetration > FORGE_PHYSICS_EPSILON) {
            float ratio_a = pa->inv_mass / inv_mass_sum;
            float ratio_b = pb->inv_mass / inv_mass_sum;
            pa->position = vec3_add(pa->position,
                vec3_scale(contact->normal, contact->penetration * ratio_a));
            pb->position = vec3_sub(pb->position,
                vec3_scale(contact->normal, contact->penetration * ratio_b));
        }
        return;
    }

    /* Combined restitution: use the minimum of the two particles.
     * This ensures that a perfectly inelastic object (e=0) dominates. */
    float e = fminf(pa->restitution, pb->restitution);

    /* Kill restitution for low-velocity contacts to prevent jitter. */
    if (fabsf(v_closing) < FORGE_PHYSICS_RESTING_THRESHOLD) {
        e = 0.0f;
    }

    /* Impulse magnitude: j = -(1 + e) * v_closing / (inv_mass_a + inv_mass_b)
     *
     * Derivation: conservation of momentum with Newton's restitution law
     * gives the impulse that produces the desired post-collision closing
     * velocity of -e * v_closing. */
    float j = -(1.0f + e) * v_closing / inv_mass_sum;

    /* Apply impulse to velocities — proportional to inverse mass. */
    vec3 impulse = vec3_scale(contact->normal, j);
    pa->velocity = vec3_add(pa->velocity, vec3_scale(impulse, pa->inv_mass));
    pb->velocity = vec3_sub(pb->velocity, vec3_scale(impulse, pb->inv_mass));

    /* ── Positional correction ────────────────────────────────────── */

    /* Push particles apart to eliminate overlap.
     * Correction is distributed proportional to inverse mass so that
     * heavier (lower inv_mass) particles move less. */
    if (contact->penetration > FORGE_PHYSICS_EPSILON) {
        float ratio_a = pa->inv_mass / inv_mass_sum;
        float ratio_b = pb->inv_mass / inv_mass_sum;
        pa->position = vec3_add(pa->position,
            vec3_scale(contact->normal, contact->penetration * ratio_a));
        pb->position = vec3_sub(pb->position,
            vec3_scale(contact->normal, contact->penetration * ratio_b));
    }
}

/* Resolve an array of contacts in a single pass.
 *
 * Iterates over the contact array and resolves each contact individually
 * by applying impulse-based velocity response and positional correction.
 * A single pass is sufficient for scenes with few simultaneous contacts;
 * for dense stacking scenarios, multiple solver iterations improve
 * convergence (see Physics Lesson 07).
 *
 * Algorithm:
 *   for each contact in [0, num_contacts):
 *     forge_physics_resolve_contact(contact, particles, num_particles)
 *
 * Parameters:
 *   contacts      (const ForgePhysicsContact*) — array of contacts to resolve;
 *                                                must not be NULL if num_contacts > 0
 *   num_contacts  (int)                        — number of contacts in the array
 *   particles     (ForgePhysicsParticle*)      — particle array; must not be NULL
 *   num_particles (int)                        — number of particles in the array
 *
 * Usage:
 *   forge_physics_resolve_contacts(contacts, num_contacts,
 *                                  particles, num_particles);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 7 —
 * "The Contact Resolver" (iterative resolution loop).
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline void forge_physics_resolve_contacts(
    const ForgePhysicsContact *contacts, int num_contacts,
    ForgePhysicsParticle *particles, int num_particles)
{
    if (!contacts || num_contacts <= 0 || !particles || num_particles <= 0) {
        return;
    }

    for (int i = 0; i < num_contacts; i++) {
        forge_physics_resolve_contact(&contacts[i], particles, num_particles);
    }
}

/* Detect all sphere-sphere collisions among an array of particles.
 *
 * Performs an O(n^2) all-pairs test, checking every unique particle pair
 * for overlap. Detected contacts are written into the caller-provided
 * contacts array up to max_contacts. If more collisions exist than
 * max_contacts allows, additional contacts are silently dropped.
 *
 * For large particle counts, replace this brute-force approach with
 * spatial partitioning (grid, octree) — see Physics Lesson 07.
 *
 * Algorithm:
 *   count = 0
 *   for i in [0, n-1):
 *     for j in [i+1, n):
 *       if collide_sphere_sphere(i, j):
 *         contacts[count++] = result
 *         if count == max_contacts: return count
 *   return count
 *
 * Parameters:
 *   particles     (const ForgePhysicsParticle*) — particle array; must not be NULL
 *   num_particles (int)                         — number of particles
 *   contacts      (ForgePhysicsContact*)        — output contact buffer; must not
 *                                                 be NULL and have room for at least
 *                                                 max_contacts entries
 *   max_contacts  (int)                         — capacity of the contacts buffer
 *
 * Returns:
 *   The number of contacts detected and written into the buffer.
 *
 * Usage:
 *   ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
 *   int n = forge_physics_collide_particles_all(
 *       particles, num_particles, contacts, FORGE_PHYSICS_MAX_CONTACTS);
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 11 —
 * brute-force collision detection and the need for broad-phase pruning.
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline int forge_physics_collide_particles_all(
    const ForgePhysicsParticle *particles, int num_particles,
    ForgePhysicsContact *contacts, int max_contacts)
{
    if (!particles || num_particles <= 1 || !contacts || max_contacts <= 0) {
        return 0;
    }

    int count = 0;

    for (int i = 0; i < num_particles - 1; i++) {
        for (int j = i + 1; j < num_particles; j++) {
            if (count >= max_contacts) {
                return count;
            }
            if (forge_physics_collide_sphere_sphere(
                    &particles[i], &particles[j], i, j, &contacts[count])) {
                count++;
            }
        }
    }

    return count;
}

/* Detect and resolve all particle collisions in one step.
 *
 * Convenience function that performs both the detection and resolution
 * phases: first runs O(n^2) all-pairs sphere-sphere detection, then
 * resolves all detected contacts with impulse-based response and
 * positional correction.
 *
 * Algorithm:
 *   num_contacts = collide_particles_all(particles, contacts)
 *   resolve_contacts(contacts, num_contacts, particles)
 *   return num_contacts
 *
 * Parameters:
 *   particles     (ForgePhysicsParticle*) — particle array; must not be NULL.
 *                                           Velocities and positions are modified
 *                                           in place during resolution.
 *   num_particles (int)                   — number of particles
 *   contacts      (ForgePhysicsContact*) — scratch buffer for detected contacts;
 *                                           must have room for max_contacts entries
 *   max_contacts  (int)                  — capacity of the contacts buffer
 *
 * Returns:
 *   The number of contacts detected and resolved.
 *
 * Usage:
 *   ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
 *   int n = forge_physics_collide_particles_step(
 *       particles, num_particles, contacts, FORGE_PHYSICS_MAX_CONTACTS);
 *   SDL_Log("Resolved %d collisions this frame", n);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 7 —
 * the complete contact resolution pipeline (detect → resolve).
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline int forge_physics_collide_particles_step(
    ForgePhysicsParticle *particles, int num_particles,
    ForgePhysicsContact *contacts, int max_contacts)
{
    int num_contacts = forge_physics_collide_particles_all(
        particles, num_particles, contacts, max_contacts);

    if (num_contacts > 0) {
        forge_physics_resolve_contacts(
            contacts, num_contacts, particles, num_particles);
    }

    return num_contacts;
}

#endif /* FORGE_PHYSICS_H */
