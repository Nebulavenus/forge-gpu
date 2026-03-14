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

/* ── Standard inertia tensor coefficients ─────────────────────────────────
 * Named constants for the well-known moment-of-inertia formulas.
 * Box:      I = (1/12) * m * (a² + b²)
 * Sphere:   I = (2/5)  * m * r²
 * Cylinder: I_axial = (1/2) * m * r²
 * Quaternion derivative: dq/dt = (1/2) * ω * q
 */
#define FORGE_PHYSICS_INERTIA_BOX_COEFF      (1.0f / 12.0f)
#define FORGE_PHYSICS_INERTIA_SPHERE_COEFF   (2.0f / 5.0f)
#define FORGE_PHYSICS_INERTIA_CYLINDER_COEFF 0.5f
#define FORGE_PHYSICS_QUAT_DERIV_COEFF       0.5f

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
    if (!(dt > 0.0f) || !isfinite(dt)) {
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
 *   iterations      (int) — solver iterations; clamped to
 *                          [FORGE_PHYSICS_SOLVER_MIN_ITERATIONS,
 *                           FORGE_PHYSICS_SOLVER_MAX_ITERATIONS]
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

/* ══════════════════════════════════════════════════════════════════════════
 * Lesson 04 — Rigid Body State and Orientation
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Constants ─────────────────────────────────────────────────────────────── */

/* Maximum angular velocity magnitude (radians per second).
 *
 * Angular velocities beyond this threshold indicate a numerical explosion.
 * 100 rad/s is roughly 16 revolutions per second — well beyond any
 * reasonable game-object spin rate.
 */
#define FORGE_PHYSICS_MAX_ANGULAR_VELOCITY  100.0f

/* Quaternion renormalization threshold.
 *
 * After each integration step, quaternion drift accumulates. If the
 * quaternion length deviates from 1.0 by more than this threshold,
 * we renormalize. Chosen to be large enough that we skip the sqrt
 * on frames with negligible drift, but small enough that the
 * orientation never visibly distorts.
 */
#define FORGE_PHYSICS_QUAT_RENORM_THRESHOLD  1e-4f

/* ── Types ─────────────────────────────────────────────────────────────────── */

/* A rigid body with position, orientation, and angular dynamics.
 *
 * Extends the particle model with rotation: a rigid body has both linear
 * motion (position, velocity, force) and angular motion (orientation,
 * angular velocity, torque). The shape of the body affects how it responds
 * to off-center forces via its inertia tensor.
 *
 * Static bodies (mass == 0, inv_mass == 0) are unaffected by forces and
 * integration — they act as immovable obstacles.
 *
 * The prev_position and prev_orientation fields store the state before the
 * last integration step, enabling smooth rendering via interpolation
 * between physics ticks (lerp for position, slerp for orientation).
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 9–10 —
 * rigid body state representation, inertia tensor, integration.
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
typedef struct ForgePhysicsRigidBody {
    vec3  position;           /* center of mass, world space (m)            */
    vec3  prev_position;      /* previous position for render interpolation */
    quat  orientation;        /* rotation body→world (unit quaternion)      */
    quat  prev_orientation;   /* previous orientation for interpolation     */
    vec3  velocity;           /* linear velocity (m/s)                      */
    vec3  angular_velocity;   /* angular velocity omega (rad/s, world)      */
    vec3  force_accum;        /* accumulated forces this step (N)           */
    vec3  torque_accum;       /* accumulated torques this step (N·m)        */
    float mass;               /* kg — 0 means static/infinite mass          */
    float inv_mass;           /* 1/mass — 0 for static bodies               */
    float damping;            /* linear velocity damping [0..1]             */
    float angular_damping;    /* angular velocity damping [0..1]            */
    float restitution;        /* coefficient of restitution [0..1]          */
    mat3  inertia_local;      /* inertia tensor, body space (const)           */
    mat3  inv_inertia_local;  /* inverse inertia tensor, body space (const)   */
    mat3  inertia_world;      /* inertia tensor, world space (for gyroscopic) */
    mat3  inv_inertia_world;  /* inverse inertia tensor, world space        */
} ForgePhysicsRigidBody;

/* ── Rigid Body Functions ──────────────────────────────────────────────────── */

/* Create a rigid body with sensible defaults.
 *
 * Sets identity orientation, zero velocities, zero accumulators, and
 * identity inertia (uniform resistance to rotation). Mass of zero creates
 * a static (immovable) body.
 *
 * Parameters:
 *   position    — initial center-of-mass position in world space (m)
 *   mass        — mass in kg; 0 or negative = static body
 *   damping     — linear velocity damping [0..1], clamped
 *   angular_damping — angular velocity damping [0..1], clamped
 *   restitution — coefficient of restitution [0..1], clamped
 *
 * Returns: a fully initialized ForgePhysicsRigidBody with identity
 *   orientation, zero velocities, and identity inertia tensor.
 *
 * Usage:
 *   ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
 *       vec3_create(0, 2, 0), 5.0f, 0.01f, 0.01f, 0.5f);
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline ForgePhysicsRigidBody forge_physics_rigid_body_create(
    vec3 position, float mass, float damping, float angular_damping,
    float restitution)
{
    ForgePhysicsRigidBody rb;

    rb.position         = position;
    rb.prev_position    = position;
    rb.orientation      = quat_identity();
    rb.prev_orientation = quat_identity();
    rb.velocity         = vec3_create(0.0f, 0.0f, 0.0f);
    rb.angular_velocity = vec3_create(0.0f, 0.0f, 0.0f);
    rb.force_accum      = vec3_create(0.0f, 0.0f, 0.0f);
    rb.torque_accum     = vec3_create(0.0f, 0.0f, 0.0f);

    /* Mass: zero, negative, or sub-epsilon = static (matches particle API) */
    if (mass > FORGE_PHYSICS_EPSILON) {
        rb.mass     = mass;
        rb.inv_mass = 1.0f / mass;
    } else {
        rb.mass     = 0.0f;
        rb.inv_mass = 0.0f;
    }

    /* Clamp damping and restitution to [0..1] */
    if (damping < 0.0f) damping = 0.0f;
    if (damping > 1.0f) damping = 1.0f;
    rb.damping = damping;

    if (angular_damping < 0.0f) angular_damping = 0.0f;
    if (angular_damping > 1.0f) angular_damping = 1.0f;
    rb.angular_damping = angular_damping;

    if (restitution < 0.0f) restitution = 0.0f;
    if (restitution > 1.0f) restitution = 1.0f;
    rb.restitution = restitution;

    /* Default: identity inertia (uniform sphere-like response) */
    rb.inertia_local     = mat3_identity();
    rb.inv_inertia_local = mat3_identity();
    rb.inertia_world     = mat3_identity();
    rb.inv_inertia_world = mat3_identity();

    return rb;
}

/* Set the inertia tensor for a solid box (cuboid).
 *
 * For a uniform-density box with half-extents (hw, hh, hd) and mass m,
 * the principal moments of inertia are:
 *   Ixx = (1/12) * m * ((2*hh)² + (2*hd)²)  = (1/3) * m * (hh² + hd²)
 *   Iyy = (1/12) * m * ((2*hw)² + (2*hd)²)  = (1/3) * m * (hw² + hd²)
 *   Izz = (1/12) * m * ((2*hw)² + (2*hh)²)  = (1/3) * m * (hw² + hh²)
 *
 * The function stores both I and I_inv. The integrator uses I_inv for
 * angular acceleration and I for the gyroscopic term omega x (I * omega).
 *
 * Parameters:
 *   rb           — rigid body (mass must be set first)
 *   half_extents — half-width, half-height, half-depth (m)
 *
 * Usage:
 *   forge_physics_rigid_body_set_inertia_box(&rb,
 *       vec3_create(1.0f, 0.5f, 0.25f));
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 10 —
 * inertia tensor computation for primitive shapes.
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_set_inertia_box(
    ForgePhysicsRigidBody *rb, vec3 half_extents)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!isfinite(half_extents.x) || !isfinite(half_extents.y) ||
        !isfinite(half_extents.z)) return;

    float hw = fabsf(half_extents.x);
    float hh = fabsf(half_extents.y);
    float hd = fabsf(half_extents.z);
    float m  = rb->mass;

    /* Full extents squared */
    float w2 = 4.0f * hw * hw;  /* (2*hw)² */
    float h2 = 4.0f * hh * hh;  /* (2*hh)² */
    float d2 = 4.0f * hd * hd;  /* (2*hd)² */

    float c = FORGE_PHYSICS_INERTIA_BOX_COEFF;
    float Ixx = c * m * (h2 + d2);
    float Iyy = c * m * (w2 + d2);
    float Izz = c * m * (w2 + h2);

    /* Guard against degenerate (zero) inertia */
    if (Ixx < FORGE_PHYSICS_EPSILON) Ixx = FORGE_PHYSICS_EPSILON;
    if (Iyy < FORGE_PHYSICS_EPSILON) Iyy = FORGE_PHYSICS_EPSILON;
    if (Izz < FORGE_PHYSICS_EPSILON) Izz = FORGE_PHYSICS_EPSILON;

    rb->inertia_local     = mat3_from_diagonal(Ixx, Iyy, Izz);
    rb->inv_inertia_local = mat3_from_diagonal(1.0f / Ixx, 1.0f / Iyy, 1.0f / Izz);

    /* Transform to world space using current orientation */
    mat3 R  = quat_to_mat3(rb->orientation);
    mat3 Rt = mat3_transpose(R);
    rb->inertia_world     = mat3_multiply(mat3_multiply(R, rb->inertia_local), Rt);
    rb->inv_inertia_world = mat3_multiply(mat3_multiply(R, rb->inv_inertia_local), Rt);
}

/* Set the inertia tensor for a solid sphere.
 *
 * For a uniform-density sphere of radius r and mass m:
 *   I = (2/5) * m * r² * I₃   (all three principal moments equal)
 *
 * A sphere has the same resistance to rotation around any axis, so the
 * inertia tensor is a scalar multiple of the identity matrix. This means
 * the world-space tensor equals the local-space tensor regardless of
 * orientation — a useful property for debugging.
 *
 * Parameters:
 *   rb     — rigid body (mass must be set first)
 *   radius — sphere radius (m)
 *
 * Usage:
 *   forge_physics_rigid_body_set_inertia_sphere(&rb, 0.5f);
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_set_inertia_sphere(
    ForgePhysicsRigidBody *rb, float radius)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!isfinite(radius)) return;
    radius = fabsf(radius);

    float I = FORGE_PHYSICS_INERTIA_SPHERE_COEFF * rb->mass * radius * radius;
    if (I < FORGE_PHYSICS_EPSILON) I = FORGE_PHYSICS_EPSILON;

    float inv_I = 1.0f / I;
    rb->inertia_local     = mat3_from_diagonal(I, I, I);
    rb->inv_inertia_local = mat3_from_diagonal(inv_I, inv_I, inv_I);

    /* Transform to world space using current orientation */
    mat3 R  = quat_to_mat3(rb->orientation);
    mat3 Rt = mat3_transpose(R);
    rb->inertia_world     = mat3_multiply(mat3_multiply(R, rb->inertia_local), Rt);
    rb->inv_inertia_world = mat3_multiply(mat3_multiply(R, rb->inv_inertia_local), Rt);
}

/* Set the inertia tensor for a solid cylinder (Y-axis aligned).
 *
 * For a uniform-density cylinder of radius r, half-height h, and mass m:
 *   Iyy = (1/2) * m * r²                 (around the symmetry axis)
 *   Ixx = Izz = (1/12) * m * (3*r² + (2*h)²)  (around perpendicular axes)
 *
 * The cylinder is easier to spin around its symmetry axis (Y) than around
 * the perpendicular axes, because mass is concentrated closer to Y.
 *
 * Parameters:
 *   rb     — rigid body (mass must be set first)
 *   radius — cylinder radius (m)
 *   half_h — half-height (m), cylinder extends from -half_h to +half_h on Y
 *
 * Usage:
 *   forge_physics_rigid_body_set_inertia_cylinder(&rb, 1.0f, 0.5f);
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_set_inertia_cylinder(
    ForgePhysicsRigidBody *rb, float radius, float half_h)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!isfinite(radius) || !isfinite(half_h)) return;
    radius = fabsf(radius);
    half_h = fabsf(half_h);

    float m  = rb->mass;
    float r2 = radius * radius;
    float h2 = 4.0f * half_h * half_h;  /* full height squared */

    float Iyy = FORGE_PHYSICS_INERTIA_CYLINDER_COEFF * m * r2;
    float Ixx = FORGE_PHYSICS_INERTIA_BOX_COEFF * m * (3.0f * r2 + h2);
    float Izz = Ixx;  /* same as Ixx by symmetry */

    if (Ixx < FORGE_PHYSICS_EPSILON) Ixx = FORGE_PHYSICS_EPSILON;
    if (Iyy < FORGE_PHYSICS_EPSILON) Iyy = FORGE_PHYSICS_EPSILON;
    if (Izz < FORGE_PHYSICS_EPSILON) Izz = FORGE_PHYSICS_EPSILON;

    rb->inertia_local     = mat3_from_diagonal(Ixx, Iyy, Izz);
    rb->inv_inertia_local = mat3_from_diagonal(1.0f / Ixx, 1.0f / Iyy, 1.0f / Izz);

    /* Transform to world space using current orientation */
    mat3 R  = quat_to_mat3(rb->orientation);
    mat3 Rt = mat3_transpose(R);
    rb->inertia_world     = mat3_multiply(mat3_multiply(R, rb->inertia_local), Rt);
    rb->inv_inertia_world = mat3_multiply(mat3_multiply(R, rb->inv_inertia_local), Rt);
}

/* Accumulate a force applied at the center of mass.
 *
 * Forces at the center of mass produce only linear acceleration (no torque).
 * Gravity is a common example: it acts uniformly on the entire body, which
 * is equivalent to applying it at the center of mass.
 *
 * Forces are accumulated, not applied immediately. Call integrate() to
 * process all accumulated forces, then clear_forces() resets the accumulators.
 *
 * Parameters:
 *   rb    — rigid body (static bodies are skipped)
 *   force — force vector in world space (N)
 *
 * Usage:
 *   vec3 gravity = vec3_create(0, -9.81f * rb.mass, 0);
 *   forge_physics_rigid_body_apply_force(&rb, gravity);
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_apply_force(
    ForgePhysicsRigidBody *rb, vec3 force)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    rb->force_accum = vec3_add(rb->force_accum, force);
}

/* Accumulate a force applied at a world-space point.
 *
 * When a force is applied off-center, it produces both linear force and
 * torque. The torque is τ = r × F, where r is the vector from the center
 * of mass to the application point.
 *
 * This is the general-case force application: collisions, explosions,
 * thrusters, and contact forces all use this form because they act at
 * specific surface points, not at the center of mass.
 *
 * Parameters:
 *   rb       — rigid body (static bodies are skipped)
 *   force    — force vector in world space (N)
 *   world_pt — point of application in world space (m)
 *
 * Usage:
 *   vec3 impact_point = vec3_create(1, 0, 0);
 *   vec3 impact_force = vec3_create(0, 100, 0);
 *   forge_physics_rigid_body_apply_force_at_point(&rb,
 *       impact_force, impact_point);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 10 —
 * force and torque from off-center application.
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_apply_force_at_point(
    ForgePhysicsRigidBody *rb, vec3 force, vec3 world_pt)
{
    if (!rb || rb->inv_mass == 0.0f) return;

    /* Offset from center of mass to application point */
    vec3 offset = vec3_sub(world_pt, rb->position);

    /* Accumulate force (linear) */
    rb->force_accum = vec3_add(rb->force_accum, force);

    /* Accumulate torque: τ = r × F */
    rb->torque_accum = vec3_add(rb->torque_accum, vec3_cross(offset, force));
}

/* Accumulate a torque (moment of force) directly.
 *
 * Torque causes angular acceleration without any linear acceleration.
 * Use this for abstract rotational effects (motors, gyroscopic precession)
 * that don't correspond to a specific surface force.
 *
 * Parameters:
 *   rb     — rigid body (static bodies are skipped)
 *   torque — torque vector in world space (N·m)
 *
 * Usage:
 *   vec3 motor_torque = vec3_create(0, 10, 0);
 *   forge_physics_rigid_body_apply_torque(&rb, motor_torque);
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_apply_torque(
    ForgePhysicsRigidBody *rb, vec3 torque)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    rb->torque_accum = vec3_add(rb->torque_accum, torque);
}

/* Update derived data: normalize orientation and recompute world-space
 * inverse inertia tensor.
 *
 * Two operations are performed:
 *   1. Normalize rb->orientation to counteract floating-point drift.
 *   2. Rotate the body-space inverse inertia tensor to world space:
 *
 *        I_world_inv = R * I_local_inv * R^T
 *
 * The inertia tensor is stored in body (local) space because it is constant
 * for a rigid body — it only depends on shape and mass distribution, not
 * orientation. R is the 3×3 rotation matrix from the current orientation
 * quaternion. Because R is orthonormal, R^T = R^-1, so this is a similarity
 * transform that preserves the tensor's eigenvalues (principal moments)
 * while rotating the principal axes to match the body's current orientation.
 *
 * Parameters:
 *   rb — rigid body to update (orientation is normalized as a side effect)
 *
 * Note: called automatically at the end of forge_physics_rigid_body_integrate().
 * Manual calls are only needed if you modify orientation or inertia directly.
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 10 —
 * computing the world-space inertia tensor.
 *
 * See: Physics Lesson 04 - Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_update_derived(
    ForgePhysicsRigidBody *rb)
{
    if (!rb || rb->inv_mass == 0.0f) return;

    /* Ensure unit quaternion — the integrator normalizes conditionally,
     * but update_derived() may also be called after manual orientation
     * changes, so normalize unconditionally here for safety. */
    rb->orientation = quat_normalize(rb->orientation);

    /* Rotation matrix from current orientation */
    mat3 R  = quat_to_mat3(rb->orientation);
    mat3 Rt = mat3_transpose(R);

    /* I_world_inv = R * I_local_inv * R^T */
    rb->inv_inertia_world = mat3_multiply(
        mat3_multiply(R, rb->inv_inertia_local), Rt);

    /* I_world = R * I_local * R^T   (needed for gyroscopic term omega x (I*omega)) */
    rb->inertia_world = mat3_multiply(
        mat3_multiply(R, rb->inertia_local), Rt);
}

/* Integrate rigid body state forward by dt seconds.
 *
 * Full symplectic Euler integration for rigid bodies:
 *
 *   1. Guard: skip static bodies (inv_mass == 0)
 *   2. Save prev_position and prev_orientation (for render interpolation)
 *   3. Linear acceleration:  a = F * inv_mass
 *   4. Angular acceleration: alpha = I_world_inv * (tau - omega x (I_world * omega))
 *   5. Update velocity:      v += a * dt
 *   6. Update omega:         omega += alpha * dt
 *   7. Apply damping:        v *= damping^dt,  omega *= angular_damping^dt
 *   8. Clamp velocities to prevent numerical explosion
 *   9. Update position:      pos += v * dt
 *  10. Update orientation:   q += 0.5 * dt * (omega_quat * q), renormalize
 *      if drift exceeds threshold
 *  11. Update derived data (normalize orientation, recompute world-space
 *      inertia)
 *  12. Clear accumulators
 *
 * The quaternion derivative uses the kinematic equation:
 *   dq/dt = 0.5 * omega_quat * q
 * where omega_quat is the quaternion (0, omega_x, omega_y, omega_z). We use first-order
 * Euler integration: q_new = q + dq/dt * dt, then renormalize.
 *
 * Damping uses exponential decay: v *= damping^dt. This is frame-rate
 * independent - a damping value of 0.99 means "retain 99% of velocity
 * per second," regardless of timestep size.
 *
 * Parameters:
 *   rb - rigid body to integrate
 *   dt - timestep in seconds (should be fixed, e.g. 1/60)
 *
 * Usage:
 *   const float FIXED_DT = 1.0f / 60.0f;
 *   forge_physics_rigid_body_apply_force(&rb, gravity);
 *   forge_physics_rigid_body_integrate(&rb, FIXED_DT);
 *   (force/torque accumulators are cleared automatically)
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 10 -
 * rigid body integration loop.
 *
 * See: Physics Lesson 04 - Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_integrate(
    ForgePhysicsRigidBody *rb, float dt)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!(dt > 0.0f) || !isfinite(dt)) return;  /* rejects <= 0, NaN, +inf */

    /* ── Save previous state for render interpolation ─────────────────── */
    rb->prev_position    = rb->position;
    rb->prev_orientation = rb->orientation;

    /* ── Linear acceleration: a = F / m ───────────────────────────────── */
    vec3 linear_acc = vec3_scale(rb->force_accum, rb->inv_mass);

    /* ── Angular acceleration: Euler's equation ────────────────────────── */
    /* α = I_world_inv * (τ - ω × (I_world * ω))
     * The gyroscopic term ω × (Iω) accounts for the coupling between
     * angular velocity and the inertia tensor. Without it, non-spherical
     * bodies precess and tumble incorrectly. */
    vec3 Iw = mat3_multiply_vec3(rb->inertia_world, rb->angular_velocity);
    vec3 gyro = vec3_cross(rb->angular_velocity, Iw);
    vec3 angular_acc = mat3_multiply_vec3(rb->inv_inertia_world,
        vec3_sub(rb->torque_accum, gyro));

    /* ── Update velocities ────────────────────────────────────────────── */
    rb->velocity = vec3_add(rb->velocity, vec3_scale(linear_acc, dt));
    rb->angular_velocity = vec3_add(rb->angular_velocity, vec3_scale(angular_acc, dt));

    /* ── Apply exponential damping (frame-rate independent) ───────────── */
    /* Retention semantics: v *= pow(damping, dt).
     *   damping = 1.0 → no damping (retain 100% per second)
     *   damping = 0.99 → light damping (retain 99% per second)
     *   damping = 0.0 → instant stop
     * Note: this differs from the particle system which uses removal
     * semantics (v *= 1 - drag).  Here, higher values mean less damping. */
    /* Clamp damping to [0..1] in case fields were modified after create */
    float damp     = rb->damping;
    float ang_damp = rb->angular_damping;
    if (damp < 0.0f)     damp = 0.0f;
    if (damp > 1.0f)     damp = 1.0f;
    if (ang_damp < 0.0f) ang_damp = 0.0f;
    if (ang_damp > 1.0f) ang_damp = 1.0f;

    rb->velocity = vec3_scale(rb->velocity, powf(damp, dt));
    rb->angular_velocity = vec3_scale(rb->angular_velocity,
                                       powf(ang_damp, dt));

    /* ── Clamp velocities to prevent numerical explosion ──────────────── */
    float v_len = vec3_length(rb->velocity);
    if (v_len > FORGE_PHYSICS_MAX_VELOCITY) {
        rb->velocity = vec3_scale(
            vec3_normalize(rb->velocity),
            FORGE_PHYSICS_MAX_VELOCITY);
    }

    float w_len = vec3_length(rb->angular_velocity);
    if (w_len > FORGE_PHYSICS_MAX_ANGULAR_VELOCITY) {
        rb->angular_velocity = vec3_scale(
            vec3_normalize(rb->angular_velocity),
            FORGE_PHYSICS_MAX_ANGULAR_VELOCITY);
    }

    /* ── Update position: x += v * dt ─────────────────────────────────── */
    rb->position = vec3_add(rb->position, vec3_scale(rb->velocity, dt));

    /* ── Update orientation: q += 0.5 * dt * (ω_quat * q) ────────────── */
    if (w_len > FORGE_PHYSICS_EPSILON) {
        quat omega_quat = quat_create(
            0.0f, rb->angular_velocity.x,
            rb->angular_velocity.y, rb->angular_velocity.z);
        quat dq = quat_multiply(omega_quat, rb->orientation);

        float half_dt = FORGE_PHYSICS_QUAT_DERIV_COEFF * dt;
        rb->orientation.w += half_dt * dq.w;
        rb->orientation.x += half_dt * dq.x;
        rb->orientation.y += half_dt * dq.y;
        rb->orientation.z += half_dt * dq.z;

        /* Renormalize to prevent quaternion drift */
        float q_len_sq = quat_length_sq(rb->orientation);
        if (fabsf(q_len_sq - 1.0f) > FORGE_PHYSICS_QUAT_RENORM_THRESHOLD) {
            rb->orientation = quat_normalize(rb->orientation);
        }
    }

    /* ── Update derived data (world-space inertia tensor) ─────────────── */
    forge_physics_rigid_body_update_derived(rb);

    /* ── Clear force and torque accumulators ───────────────────────────── */
    rb->force_accum  = vec3_create(0.0f, 0.0f, 0.0f);
    rb->torque_accum = vec3_create(0.0f, 0.0f, 0.0f);
}

/* ── Lesson 05 — Force Generators ─────────────────────────────────────── */

/* Apply gravitational acceleration to a rigid body.
 *
 * Adds F = m * g to the force accumulator at the center of mass.
 * Gravity acts uniformly on the entire body, which is equivalent to a
 * force at the center of mass — it produces no torque. Static bodies
 * (inv_mass == 0) are unaffected.
 *
 * This is a convenience wrapper around forge_physics_rigid_body_apply_force()
 * that handles the mass multiplication, matching the particle-level
 * forge_physics_apply_gravity().
 *
 * Parameters:
 *   rb      — rigid body to apply gravity to (must not be NULL)
 *   gravity — gravitational acceleration vector (m/s²), typically (0, -9.81, 0)
 *
 * Usage:
 *   forge_physics_rigid_body_apply_gravity(&rb, (vec3){0, -9.81f, 0});
 *
 * Reference: Newton's second law: F = m * a. For gravity, a = g.
 *
 * See: Physics Lesson 05 — Forces and Torques
 */
static inline void forge_physics_rigid_body_apply_gravity(
    ForgePhysicsRigidBody *rb, vec3 gravity)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    rb->force_accum = vec3_add(rb->force_accum,
                               vec3_scale(gravity, rb->mass));
}

/* Apply linear drag (air resistance) to a rigid body.
 *
 * Adds F = -coeff * v to the force accumulator, opposing the current
 * linear velocity. This is a simplified linear drag model — real drag
 * is proportional to v² (quadratic), but linear drag is numerically
 * stable and sufficient for most game physics.
 *
 * Linear drag causes objects to reach a terminal velocity where the
 * drag force equals the applied force (e.g. gravity):
 *   v_terminal = m * g / coeff
 *
 * Parameters:
 *   rb    — rigid body (static bodies are skipped)
 *   coeff — drag coefficient (kg/s). Must be >= 0. Higher values mean
 *           more drag. Typical range: 0.1 to 5.0.
 *
 * Usage:
 *   forge_physics_rigid_body_apply_linear_drag(&rb, 0.5f);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 10 —
 * simplified drag model for rigid bodies.
 *
 * See: Physics Lesson 05 — Forces and Torques
 */
static inline void forge_physics_rigid_body_apply_linear_drag(
    ForgePhysicsRigidBody *rb, float coeff)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!(coeff > 0.0f)) return;  /* skip zero, negative, NaN */

    /* F_drag = -coeff * v */
    rb->force_accum = vec3_add(rb->force_accum,
                               vec3_scale(rb->velocity, -coeff));
}

/* Apply angular drag (rotational air resistance) to a rigid body.
 *
 * Adds torque = -coeff * omega to the torque accumulator, opposing the
 * current angular velocity. This models the resistance that air exerts
 * on a spinning object — without it, objects spin forever (unless the
 * exponential damping field is < 1.0).
 *
 * Angular drag differs from the damping field:
 * - damping is exponential decay (v *= damping^dt) — frame-rate independent
 *   but not physically motivated
 * - angular drag is a force-based model (tau = -k*omega) — participates
 *   in the force accumulator and interacts correctly with other torques
 *
 * Both can be used together. Damping provides baseline energy loss;
 * angular drag adds physically-motivated rotational resistance.
 *
 * Parameters:
 *   rb    — rigid body (static bodies are skipped)
 *   coeff — angular drag coefficient (N·m·s/rad). Must be >= 0.
 *           Typical range: 0.1 to 5.0.
 *
 * Usage:
 *   forge_physics_rigid_body_apply_angular_drag(&rb, 1.0f);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 10.
 *
 * See: Physics Lesson 05 — Forces and Torques
 */
static inline void forge_physics_rigid_body_apply_angular_drag(
    ForgePhysicsRigidBody *rb, float coeff)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!(coeff > 0.0f)) return;  /* skip zero, negative, NaN */

    /* tau_drag = -coeff * omega */
    rb->torque_accum = vec3_add(rb->torque_accum,
                                vec3_scale(rb->angular_velocity, -coeff));
}

/* Apply contact friction to a rigid body sliding on a surface.
 *
 * Friction opposes the tangential component of the contact-point velocity.
 * The contact-point velocity accounts for both linear and rotational motion:
 *   v_contact = v + omega x r    (where r = contact_point - position)
 *
 * The tangential velocity is the projection onto the contact plane:
 *   v_tangent = v_contact - (v_contact . n) * n
 *
 * The friction force opposes v_tangent:
 *   F_friction = -coeff * |v_tangent| * normalize(v_tangent)
 *
 * This is a simplified velocity-proportional friction model. The coefficient
 * combines the friction coefficient with the velocity dependence — in a full
 * contact solver, Coulomb friction (F_friction <= mu * F_normal) is used
 * instead, but for a force-generator approach this form is practical.
 *
 * The force is applied at the contact point, so it also generates
 * torque (causing objects to spin from friction, like a rolling ball
 * slowing down). When the contact point equals the body's position
 * (|r| < epsilon), only the linear velocity contributes and no torque
 * is produced.
 *
 * Parameters:
 *   rb            — rigid body (static bodies are skipped)
 *   normal        — contact surface normal (non-zero; normalized internally)
 *   contact_point — world-space point where friction acts
 *   coeff         — friction coefficient (N·s/m). Must be >= 0.
 *                   Typical range: 0.1 to 10.0.
 *
 * Usage:
 *   vec3 ground_normal = {0, 1, 0};
 *   vec3 contact = rb.position;
 *   contact.y = 0;  // ground contact point
 *   forge_physics_rigid_body_apply_friction(&rb, ground_normal,
 *       contact, 2.0f);
 *
 * Reference: Coulomb friction model, simplified for force-generator use.
 * See Millington, "Game Physics Engine Development", Ch. 15.
 *
 * See: Physics Lesson 05 — Forces and Torques
 */
static inline void forge_physics_rigid_body_apply_friction(
    ForgePhysicsRigidBody *rb, vec3 normal, vec3 contact_point,
    float coeff)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!(coeff > 0.0f)) return;  /* skip zero, negative, NaN */

    /* Guard and normalize the contact normal */
    float normal_len_sq = vec3_dot(normal, normal);
    if (!(normal_len_sq > FORGE_PHYSICS_EPSILON)) return;
    normal = vec3_scale(normal, 1.0f / sqrtf(normal_len_sq));

    /* Compute contact-point velocity: v_p = v + omega x r */
    vec3 r = vec3_sub(contact_point, rb->position);
    vec3 v_contact;
    if (vec3_length(r) < FORGE_PHYSICS_EPSILON) {
        /* Contact at COM — use linear velocity only (no torque) */
        v_contact = rb->velocity;
    } else {
        v_contact = vec3_add(rb->velocity,
                             vec3_cross(rb->angular_velocity, r));
    }

    /* Project contact-point velocity onto the contact plane */
    float v_dot_n = vec3_dot(v_contact, normal);
    vec3 v_normal = vec3_scale(normal, v_dot_n);
    vec3 v_tangent = vec3_sub(v_contact, v_normal);

    float tang_speed = vec3_length(v_tangent);
    if (tang_speed < FORGE_PHYSICS_EPSILON) return;  /* not sliding */

    /* Friction force opposes tangential velocity */
    vec3 friction_dir = vec3_scale(v_tangent, -1.0f / tang_speed);
    vec3 friction_force = vec3_scale(friction_dir, coeff * tang_speed);

    /* Apply at contact point (generates torque if off-center) */
    forge_physics_rigid_body_apply_force_at_point(rb, friction_force,
                                                   contact_point);
}

/* Clear force and torque accumulators.
 *
 * Call this at the start of each physics step before applying forces,
 * or rely on integrate() which clears accumulators automatically.
 *
 * Parameters:
 *   rb — rigid body to clear
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline void forge_physics_rigid_body_clear_forces(
    ForgePhysicsRigidBody *rb)
{
    if (!rb) return;
    rb->force_accum  = vec3_create(0.0f, 0.0f, 0.0f);
    rb->torque_accum = vec3_create(0.0f, 0.0f, 0.0f);
}

/* Get the 4×4 world transform matrix for rendering.
 *
 * Combines position (translation) and orientation (rotation) into a
 * single model matrix: M = T(pos) * R(orient)
 *
 * This is the matrix you pass to the vertex shader as the model transform.
 * It does not include scale — rigid bodies do not change shape.
 *
 * Parameters:
 *   rb — rigid body
 *
 * Returns:
 *   4×4 model matrix (column-major, suitable for upload to GPU)
 *
 * Usage:
 *   mat4 model = forge_physics_rigid_body_get_transform(&rb);
 *   // upload model to vertex shader as uniform
 *
 * See: Physics Lesson 04 — Rigid Body State and Orientation
 */
static inline mat4 forge_physics_rigid_body_get_transform(
    const ForgePhysicsRigidBody *rb)
{
    if (!rb) return mat4_identity();

    mat4 rotation    = quat_to_mat4(rb->orientation);
    mat4 translation = mat4_translate(rb->position);
    return mat4_multiply(translation, rotation);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Lesson 06 — Resting Contacts and Friction
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Constants ─────────────────────────────────────────────────────────────── */

/* Maximum number of rigid body contacts per step.
 *
 * Each box-plane collision can produce up to 8 contacts (all corners below
 * the plane). With N bodies, the worst case for plane contacts alone is
 * 8*N. 64 is generous for the small scenes in these lessons.
 */
#define FORGE_PHYSICS_MAX_RB_CONTACTS 64

/* Default Coulomb friction coefficients (reference values).
 *
 * Static friction must be >= dynamic friction. These are combined between
 * two bodies using the geometric mean: mu = sqrt(mu_a * mu_b).
 * Provided as reference defaults — callers pass friction explicitly.
 *
 * Reference: typical rubber-on-concrete ~ 0.6 static / 0.4 dynamic.
 */
#define FORGE_PHYSICS_DEFAULT_STATIC_FRICTION   0.6f
#define FORGE_PHYSICS_DEFAULT_DYNAMIC_FRICTION  0.4f

/* Default number of contact solver iterations (reference value).
 *
 * Each iteration re-evaluates and resolves all contacts. More iterations
 * improve convergence for stacked bodies at the cost of CPU time. 10 is
 * a good balance for real-time stacking of 5-8 bodies.
 * Provided as a reference default — callers pass the count explicitly.
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 */
#define FORGE_PHYSICS_CONTACT_SOLVER_ITERATIONS 10

/* Baumgarte stabilization factor.
 *
 * Controls how aggressively penetration is corrected via velocity bias.
 * Higher values correct faster but can cause jitter. 0.2 is standard.
 *
 * The velocity bias is: v_bias = (beta / dt) * max(penetration - slop, 0)
 *
 * Reference: Baumgarte, "Stabilization of Constraints and Integrals of
 * Motion in Dynamical Systems" (1972).
 */
#define FORGE_PHYSICS_BAUMGARTE_FACTOR  0.2f

/* Penetration slop — overlap below this threshold is ignored.
 *
 * Prevents jitter from micro-corrections. Objects are allowed to overlap
 * by this much without corrective velocity being applied.
 */
#define FORGE_PHYSICS_PENETRATION_SLOP  0.01f

/* Resting velocity threshold for rigid body contacts.
 *
 * When the closing velocity is below this threshold, restitution is zeroed
 * to prevent micro-bouncing of resting objects.
 */
#define FORGE_PHYSICS_RB_RESTING_THRESHOLD  0.5f

/* ── Types ─────────────────────────────────────────────────────────────────── */

/* A contact between two rigid bodies (or a body and a plane).
 *
 * Stores the geometric data needed to compute and apply contact impulses:
 * the contact point, surface normal, penetration depth, and friction
 * coefficients.
 *
 * For body-plane contacts, body_b is set to -1 (the plane is implicitly
 * static with infinite mass).
 *
 * The normal always points from body B toward body A. For plane contacts,
 * the normal is the plane's outward normal.
 *
 * Fields:
 *   point        — world-space contact point (on the contact surface)
 *   normal       — unit normal from B toward A
 *   penetration  — overlap depth (m), >= 0
 *   body_a       — index of first body (the one the normal points toward)
 *   body_b       — index of second body (-1 for plane/static environment)
 *   static_friction  — combined static friction coefficient [0..inf)
 *   dynamic_friction — combined dynamic friction coefficient [0..inf)
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 *
 * See: Physics Lesson 06 — Resting Contacts and Friction
 */
typedef struct ForgePhysicsRBContact {
    vec3  point;              /* world-space contact point (m) — torque arm   */
    vec3  normal;             /* unit normal from B toward A                  */
    float penetration;        /* overlap depth (m), >= 0                      */
    int   body_a;             /* index of body A in the body array            */
    int   body_b;             /* index of body B, or -1 for plane             */
    float static_friction;    /* static friction coefficient, >= 0            */
    float dynamic_friction;   /* dynamic friction coefficient, >= 0           */
} ForgePhysicsRBContact;

/* ── Rigid Body Contact Detection ──────────────────────────────────────────── */

/* Detect contact between a rigid body sphere and an infinite plane.
 *
 * Tests whether a sphere (defined by the body's position and the given
 * radius) intersects the plane defined by a point and normal. If contact
 * is detected, fills out a ForgePhysicsRBContact.
 *
 * Algorithm:
 *   signed_dist = dot(sphere_center - plane_point, plane_normal)
 *   penetration = radius - signed_dist
 *   if penetration > 0: contact exists
 *   contact_point = sphere_center - plane_normal * signed_dist
 *
 * Parameters:
 *   body         — the rigid body (sphere shape)
 *   body_idx     — index of this body in the body array
 *   radius       — sphere radius (m), must be > 0
 *   plane_point  — any point on the plane (m)
 *   plane_normal — outward normal of the plane (must be unit length)
 *   mu_s         — static friction coefficient for this contact
 *   mu_d         — dynamic friction coefficient for this contact
 *   out          — output contact (filled on collision)
 *
 * Returns:
 *   true if contact was detected.
 *
 * Usage:
 *   ForgePhysicsRBContact c;
 *   vec3 ground_pt = {0, 0, 0};
 *   vec3 ground_n  = {0, 1, 0};
 *   if (forge_physics_rb_collide_sphere_plane(&body, 0, 0.5f,
 *           ground_pt, ground_n, 0.6f, 0.4f, &c)) { ... }
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 5.2.2.
 *
 * See: Physics Lesson 06 — Resting Contacts and Friction
 */
static inline bool forge_physics_rb_collide_sphere_plane(
    const ForgePhysicsRigidBody *body, int body_idx,
    float radius,
    vec3 plane_point, vec3 plane_normal,
    float mu_s, float mu_d,
    ForgePhysicsRBContact *out)
{
    if (!body || !out) return false;
    if (!(radius > FORGE_PHYSICS_EPSILON)) return false;
    float normal_len_sq = vec3_length_squared(plane_normal);
    if (!(normal_len_sq > FORGE_PHYSICS_EPSILON)) return false;
    plane_normal = vec3_scale(plane_normal, 1.0f / sqrtf(normal_len_sq));

    /* Signed distance from sphere center to plane */
    vec3 diff = vec3_sub(body->position, plane_point);
    float signed_dist = vec3_dot(diff, plane_normal);

    float penetration = radius - signed_dist;
    if (penetration <= 0.0f) return false;

    out->normal       = plane_normal;
    out->penetration  = penetration;
    out->body_a       = body_idx;
    out->body_b       = -1;  /* plane = static environment */

    /* Contact point: projection of sphere center onto the plane */
    out->point = vec3_sub(body->position,
                          vec3_scale(plane_normal, signed_dist));

    /* Friction coefficients */
    out->static_friction  = (mu_s >= 0.0f) ? mu_s : 0.0f;
    out->dynamic_friction = (mu_d >= 0.0f) ? mu_d : 0.0f;

    return true;
}

/* Detect contacts between a rigid body box (OBB) and an infinite plane.
 *
 * Tests all 8 corners of the oriented bounding box against the plane. Each
 * corner below the plane surface generates a contact. This can produce
 * 0 to 8 contacts (commonly 1 for corner, 2 for edge, 4 for face).
 *
 * The box is defined by its center (body position), orientation (body
 * quaternion), and half-extents.
 *
 * Algorithm:
 *   For each of the 8 corners (±hx, ±hy, ±hz):
 *     world_corner = position + R * local_corner
 *     signed_dist = dot(world_corner - plane_point, plane_normal)
 *     if signed_dist < 0:
 *       generate contact at world_corner with penetration = -signed_dist
 *
 * Parameters:
 *   body         — the rigid body (box shape)
 *   body_idx     — index of this body in the body array
 *   half_extents — half-width, half-height, half-depth (m)
 *   plane_point  — any point on the plane (m)
 *   plane_normal — outward normal of the plane (must be unit length)
 *   mu_s         — static friction coefficient
 *   mu_d         — dynamic friction coefficient
 *   out          — output contact array (must have room for up to 8 entries)
 *   max_contacts — capacity of the output array
 *
 * Returns:
 *   Number of contacts generated (0 to min(8, max_contacts)).
 *
 * Usage:
 *   ForgePhysicsRBContact contacts[8];
 *   int n = forge_physics_rb_collide_box_plane(&body, 0,
 *       vec3_create(1, 0.5f, 0.5f),
 *       vec3_create(0, 0, 0), vec3_create(0, 1, 0),
 *       0.6f, 0.4f, contacts, 8);
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 5.2.3 —
 * OBB vs plane intersection.
 *
 * See: Physics Lesson 06 — Resting Contacts and Friction
 */
static inline int forge_physics_rb_collide_box_plane(
    const ForgePhysicsRigidBody *body, int body_idx,
    vec3 half_extents,
    vec3 plane_point, vec3 plane_normal,
    float mu_s, float mu_d,
    ForgePhysicsRBContact *out, int max_contacts)
{
    if (!body || !out || max_contacts <= 0) return 0;
    float normal_len_sq = vec3_length_squared(plane_normal);
    if (!(normal_len_sq > FORGE_PHYSICS_EPSILON)) return 0;
    plane_normal = vec3_scale(plane_normal, 1.0f / sqrtf(normal_len_sq));

    /* Rotation matrix from body orientation */
    mat3 R = quat_to_mat3(body->orientation);

    /* Clamp friction coefficients */
    if (mu_s < 0.0f) mu_s = 0.0f;
    if (mu_d < 0.0f) mu_d = 0.0f;

    float hx = fabsf(half_extents.x);
    float hy = fabsf(half_extents.y);
    float hz = fabsf(half_extents.z);

    /* Signs for all 8 corners */
    float signs[8][3] = {
        {-1, -1, -1}, {-1, -1,  1}, {-1,  1, -1}, {-1,  1,  1},
        { 1, -1, -1}, { 1, -1,  1}, { 1,  1, -1}, { 1,  1,  1}
    };

    int count = 0;

    for (int i = 0; i < 8 && count < max_contacts; i++) {
        /* Local-space corner */
        vec3 local_corner = vec3_create(
            signs[i][0] * hx,
            signs[i][1] * hy,
            signs[i][2] * hz);

        /* Transform to world space */
        vec3 world_corner = vec3_add(body->position,
                                      mat3_multiply_vec3(R, local_corner));

        /* Signed distance from corner to plane */
        vec3 diff = vec3_sub(world_corner, plane_point);
        float signed_dist = vec3_dot(diff, plane_normal);

        if (signed_dist < 0.0f) {
            /* Project penetrating corner onto the plane surface so
             * friction lever arms are computed at the contact surface,
             * not below it. */
            out[count].point        = vec3_sub(
                world_corner,
                vec3_scale(plane_normal, signed_dist));
            out[count].normal       = plane_normal;
            out[count].penetration  = -signed_dist;
            out[count].body_a       = body_idx;
            out[count].body_b       = -1;
            out[count].static_friction  = mu_s;
            out[count].dynamic_friction = mu_d;
            count++;
        }
    }

    return count;
}

/* Detect collision between two sphere-shaped rigid bodies.
 *
 * Tests whether spheres with given radii overlap. If so, computes the
 * contact point (midpoint of the overlap region), normal (from B toward A),
 * and penetration depth.
 *
 * This is the rigid body version of forge_physics_collide_sphere_sphere()
 * (Lesson 03). It outputs a ForgePhysicsRBContact instead of a particle
 * contact, enabling impulse-based resolution with angular velocity.
 *
 * Parameters:
 *   a         — first rigid body (must not be NULL)
 *   idx_a     — index of body A in the bodies array
 *   radius_a  — collision radius of body A (must be > 0)
 *   b         — second rigid body (must not be NULL)
 *   idx_b     — index of body B in the bodies array
 *   radius_b  — collision radius of body B (must be > 0)
 *   mu_s      — static friction coefficient for the contact (>= 0)
 *   mu_d      — dynamic friction coefficient for the contact (>= 0)
 *   out       — receives the contact if overlap detected (must not be NULL)
 *
 * Returns: true if the spheres overlap, false otherwise.
 *
 * Usage:
 *   ForgePhysicsRBContact c;
 *   if (forge_physics_rb_collide_sphere_sphere(
 *           &bodies[0], 0, 0.5f, &bodies[1], 1, 0.5f,
 *           0.6f, 0.4f, &c)) {
 *       forge_physics_rb_resolve_contact(&c, bodies, 2, dt);
 *   }
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 4.3
 * See: Physics Lesson 03 — Particle Collisions (particle version)
 * See: Physics Lesson 06 — Resting Contacts and Friction (rigid body usage)
 */
static inline bool forge_physics_rb_collide_sphere_sphere(
    const ForgePhysicsRigidBody *a, int idx_a, float radius_a,
    const ForgePhysicsRigidBody *b, int idx_b, float radius_b,
    float mu_s, float mu_d, ForgePhysicsRBContact *out)
{
    if (!a || !b || !out) return false;
    if (radius_a <= 0.0f || radius_b <= 0.0f) return false;

    /* Two static bodies cannot collide. */
    if (a->inv_mass == 0.0f && b->inv_mass == 0.0f) return false;

    /* Vector from B toward A. */
    vec3 d = vec3_sub(a->position, b->position);
    float dist_sq = vec3_dot(d, d);
    float sum_radii = radius_a + radius_b;

    /* No overlap — spheres are separated. */
    if (dist_sq >= sum_radii * sum_radii) return false;

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
    out->body_a      = idx_a;
    out->body_b      = idx_b;
    out->static_friction  = (mu_s >= 0.0f) ? mu_s : 0.0f;
    out->dynamic_friction = (mu_d >= 0.0f) ? mu_d : 0.0f;

    /* Contact point: midpoint of the overlap region. */
    out->point = vec3_add(b->position,
        vec3_scale(normal, radius_b - out->penetration * 0.5f));

    return true;
}

/* ── Rigid Body Contact Resolution ─────────────────────────────────────────── */

/* Resolve a single rigid body contact with impulse-based response and
 * Coulomb friction.
 *
 * Computes and applies a normal impulse to prevent penetration, plus a
 * tangential friction impulse that opposes sliding. The friction impulse
 * is clamped by the Coulomb friction cone:
 *   |j_tangent| <= mu * |j_normal|
 *
 * If the tangential impulse exceeds the static friction limit, dynamic
 * friction is used instead. This produces the correct static-to-dynamic
 * transition: objects stick until the applied force exceeds mu_s * N,
 * then slide with mu_d * N resistance.
 *
 * Algorithm:
 *   1. Compute contact-point velocity for each body:
 *      v_p = v + omega x r   (r = contact_point - position)
 *   2. Relative velocity: v_rel = v_a - v_b (at contact point)
 *   3. Normal closing velocity: v_n = dot(v_rel, normal)
 *   4. If separating (v_n > 0) and no Baumgarte bias, skip (clamped below)
 *   5. Compute restitution: e = min(e_a, e_b), zero if v_n < threshold
 *   6. Effective mass along normal:
 *      m_eff = inv_mass_a + inv_mass_b
 *            + dot(normal, (I_inv_a * (r_a x normal)) x r_a)
 *            + dot(normal, (I_inv_b * (r_b x normal)) x r_b)
 *   7. Normal impulse: j_n = -(1 + e) * v_n / m_eff
 *      Apply Baumgarte bias: j_n += (beta/dt) * max(pen - slop, 0) / m_eff
 *   8. Apply normal impulse to velocities
 *   9. Recompute tangential velocity after normal impulse
 *  10. Compute friction impulse magnitude, clamped by Coulomb cone
 *  11. Apply friction impulse
 *
 * Parameters:
 *   contact — the contact to resolve
 *   bodies  — rigid body array
 *   count   — number of bodies in the array
 *   dt      — physics timestep (for Baumgarte bias)
 *
 * Usage:
 *   forge_physics_rb_resolve_contact(&contacts[i], bodies, num_bodies, dt);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 * See also: Tonge, "Iterative Rigid Body Solvers" (GDC 2013).
 *
 * See: Physics Lesson 06 — Resting Contacts and Friction
 */
static inline void forge_physics_rb_resolve_contact(
    const ForgePhysicsRBContact *contact,
    ForgePhysicsRigidBody *bodies, int count,
    float dt)
{
    if (!contact || !bodies || count <= 0) return;
    if (!(dt > 0.0f) || !isfinite(dt)) return;

    int ia = contact->body_a;
    int ib = contact->body_b;

    if (ia < 0 || ia >= count) return;

    /* body_b must be -1 (plane sentinel) or a valid index.  Reject any
     * other out-of-range value so upstream bugs are surfaced early. */
    if (ib != -1 && (ib < 0 || ib >= count)) return;

    ForgePhysicsRigidBody *a = &bodies[ia];
    ForgePhysicsRigidBody *b = (ib >= 0) ? &bodies[ib] : NULL;

    float inv_mass_a = a->inv_mass;
    float inv_mass_b = b ? b->inv_mass : 0.0f;
    bool dynamic_a = inv_mass_a > 0.0f;
    bool dynamic_b = b && inv_mass_b > 0.0f;

    /* Both static — nothing to do */
    if (!dynamic_a && !dynamic_b) return;

    vec3 n = contact->normal;
    float n_len_sq = vec3_length_squared(n);
    if (!(n_len_sq > FORGE_PHYSICS_EPSILON) || !isfinite(n_len_sq)) return;
    n = vec3_scale(n, 1.0f / sqrtf(n_len_sq));

    /* Offsets from COM to contact point */
    vec3 r_a = vec3_sub(contact->point, a->position);
    vec3 r_b = b ? vec3_sub(contact->point, b->position)
                 : vec3_create(0, 0, 0);

    /* ── Contact-point velocities ─────────────────────────────────── */
    vec3 v_a = vec3_add(a->velocity, vec3_cross(a->angular_velocity, r_a));
    vec3 v_b = b ? vec3_add(b->velocity, vec3_cross(b->angular_velocity, r_b))
                 : vec3_create(0, 0, 0);
    vec3 v_rel = vec3_sub(v_a, v_b);

    /* Normal closing velocity */
    float v_n = vec3_dot(v_rel, n);

    /* ── Effective mass along the normal ──────────────────────────── */
    /* m_eff_inv = 1/m_a + 1/m_b + n . ((I_a_inv * (r_a x n)) x r_a)
     *                              + n . ((I_b_inv * (r_b x n)) x r_b)
     * Angular terms are only included for dynamic bodies — static bodies
     * (inv_mass == 0) must not contribute angular effective mass. */
    vec3 r_a_cross_n = vec3_cross(r_a, n);
    vec3 r_b_cross_n = vec3_cross(r_b, n);

    float m_eff_inv = inv_mass_a;
    if (dynamic_a) {
        vec3 ang_a = vec3_cross(
            mat3_multiply_vec3(a->inv_inertia_world, r_a_cross_n), r_a);
        m_eff_inv += vec3_dot(ang_a, n);
    }

    if (b) {
        m_eff_inv += inv_mass_b;
        if (dynamic_b) {
            vec3 ang_b = vec3_cross(
                mat3_multiply_vec3(b->inv_inertia_world, r_b_cross_n), r_b);
            m_eff_inv += vec3_dot(ang_b, n);
        }
    }

    if (m_eff_inv < FORGE_PHYSICS_EPSILON) return;

    /* ── Normal impulse ───────────────────────────────────────────── */
    float e = a->restitution;
    if (b) e = fminf(e, b->restitution);

    /* Kill restitution for resting contacts */
    if (fabsf(v_n) < FORGE_PHYSICS_RB_RESTING_THRESHOLD) {
        e = 0.0f;
    }

    /* Baumgarte velocity bias for penetration correction */
    float bias = 0.0f;
    if (dt > FORGE_PHYSICS_EPSILON) {
        float pen_excess = contact->penetration - FORGE_PHYSICS_PENETRATION_SLOP;
        if (pen_excess > 0.0f) {
            bias = (FORGE_PHYSICS_BAUMGARTE_FACTOR / dt) * pen_excess;
        }
    }

    float j_n = (-(1.0f + e) * v_n + bias) / m_eff_inv;

    /* Clamp: normal impulse must be non-negative (push apart, never pull) */
    if (j_n < 0.0f) j_n = 0.0f;

    /* Apply normal impulse */
    vec3 impulse_n = vec3_scale(n, j_n);

    a->velocity = vec3_add(a->velocity, vec3_scale(impulse_n, inv_mass_a));
    if (dynamic_a) {
        a->angular_velocity = vec3_add(a->angular_velocity,
            mat3_multiply_vec3(a->inv_inertia_world,
                               vec3_cross(r_a, impulse_n)));
    }

    if (b) {
        b->velocity = vec3_sub(b->velocity, vec3_scale(impulse_n, inv_mass_b));
        if (dynamic_b) {
            b->angular_velocity = vec3_sub(b->angular_velocity,
                mat3_multiply_vec3(b->inv_inertia_world,
                                   vec3_cross(r_b, impulse_n)));
        }
    }

    /* ── Friction impulse ─────────────────────────────────────────── */

    /* Recompute relative velocity after normal impulse */
    v_a = vec3_add(a->velocity, vec3_cross(a->angular_velocity, r_a));
    v_b = b ? vec3_add(b->velocity, vec3_cross(b->angular_velocity, r_b))
             : vec3_create(0, 0, 0);
    v_rel = vec3_sub(v_a, v_b);

    /* Tangential velocity: project out the normal component */
    float v_n2 = vec3_dot(v_rel, n);
    vec3 v_tangent = vec3_sub(v_rel, vec3_scale(n, v_n2));
    float tang_speed = vec3_length(v_tangent);

    if (tang_speed > FORGE_PHYSICS_EPSILON) {
        vec3 tang_dir = vec3_scale(v_tangent, 1.0f / tang_speed);

        /* Effective mass along tangent direction */
        vec3 r_a_cross_t = vec3_cross(r_a, tang_dir);
        vec3 r_b_cross_t = vec3_cross(r_b, tang_dir);

        float m_eff_t_inv = inv_mass_a;
        if (dynamic_a) {
            m_eff_t_inv += vec3_dot(
                vec3_cross(mat3_multiply_vec3(a->inv_inertia_world,
                                              r_a_cross_t), r_a),
                tang_dir);
        }

        if (b) {
            m_eff_t_inv += inv_mass_b;
            if (dynamic_b) {
                m_eff_t_inv += vec3_dot(
                    vec3_cross(mat3_multiply_vec3(b->inv_inertia_world,
                                                  r_b_cross_t), r_b),
                    tang_dir);
            }
        }

        if (m_eff_t_inv > FORGE_PHYSICS_EPSILON) {
            float j_t = -tang_speed / m_eff_t_inv;

            /* Sanitize friction coefficients: clamp to >= 0, enforce
             * dynamic <= static (Coulomb model requires mu_d <= mu_s) */
            float mu_s = contact->static_friction;
            float mu_d = contact->dynamic_friction;
            if (!(mu_s >= 0.0f)) mu_s = 0.0f;  /* catches NaN too */
            if (!(mu_d >= 0.0f)) mu_d = 0.0f;
            if (mu_d > mu_s) mu_d = mu_s;

            /* Coulomb friction cone: clamp tangential impulse */
            float friction_limit;
            if (fabsf(j_t) <= mu_s * j_n) {
                /* Within static friction cone — full stop */
                friction_limit = j_t;
            } else {
                /* Exceeded static friction — use dynamic friction */
                friction_limit = (j_t < 0.0f ? -1.0f : 1.0f)
                               * mu_d * j_n;
            }

            vec3 impulse_t = vec3_scale(tang_dir, friction_limit);

            a->velocity = vec3_add(a->velocity,
                                    vec3_scale(impulse_t, inv_mass_a));
            if (dynamic_a) {
                a->angular_velocity = vec3_add(a->angular_velocity,
                    mat3_multiply_vec3(a->inv_inertia_world,
                                       vec3_cross(r_a, impulse_t)));
            }

            if (b) {
                b->velocity = vec3_sub(b->velocity,
                                        vec3_scale(impulse_t, inv_mass_b));
                if (dynamic_b) {
                    b->angular_velocity = vec3_sub(b->angular_velocity,
                        mat3_multiply_vec3(b->inv_inertia_world,
                                           vec3_cross(r_b, impulse_t)));
                }
            }
        }
    }
}

/* Resolve an array of rigid body contacts with iterative solving.
 *
 * Runs multiple passes over the contact array, resolving each contact
 * on every pass. Iterative solving improves convergence for scenarios
 * with multiple simultaneous contacts (stacking, resting on surfaces).
 *
 * A single pass is equivalent to sequential impulse solving. Each
 * additional pass allows impulse changes from one contact to propagate
 * to neighboring contacts, improving the global solution.
 *
 * Algorithm:
 *   for iter in [0, iterations):
 *     for each contact:
 *       resolve_contact(contact, bodies, count, dt)
 *
 * Parameters:
 *   contacts      — array of contacts to resolve
 *   num_contacts  — number of contacts in the array
 *   bodies        — rigid body array (modified in place)
 *   num_bodies    — number of bodies in the array
 *   iterations    — number of solver passes (clamped to
 *                    [FORGE_PHYSICS_SOLVER_MIN_ITERATIONS,
 *                     FORGE_PHYSICS_SOLVER_MAX_ITERATIONS])
 *   dt            — physics timestep (for Baumgarte bias)
 *
 * Usage:
 *   forge_physics_rb_resolve_contacts(contacts, num_contacts,
 *       bodies, num_bodies, 10, PHYSICS_DT);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 *
 * See: Physics Lesson 06 — Resting Contacts and Friction
 */
static inline void forge_physics_rb_resolve_contacts(
    const ForgePhysicsRBContact *contacts, int num_contacts,
    ForgePhysicsRigidBody *bodies, int num_bodies,
    int iterations, float dt)
{
    if (!contacts || num_contacts <= 0 || !bodies || num_bodies <= 0) return;

    /* Clamp iterations */
    if (iterations < FORGE_PHYSICS_SOLVER_MIN_ITERATIONS)
        iterations = FORGE_PHYSICS_SOLVER_MIN_ITERATIONS;
    if (iterations > FORGE_PHYSICS_SOLVER_MAX_ITERATIONS)
        iterations = FORGE_PHYSICS_SOLVER_MAX_ITERATIONS;

    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < num_contacts; i++) {
            forge_physics_rb_resolve_contact(&contacts[i], bodies,
                                              num_bodies, dt);
        }
    }
}

#endif /* FORGE_PHYSICS_H */
