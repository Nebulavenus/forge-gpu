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

#endif /* FORGE_PHYSICS_H */
