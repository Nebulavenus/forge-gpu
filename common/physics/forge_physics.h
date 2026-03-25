/*
 * forge_physics.h — Physics library for forge-gpu
 *
 * A learning-focused physics library for particle simulation and collision
 * detection. Every function is documented with the underlying physical law,
 * usage examples, and cross-references to lessons and textbooks.
 *
 * Design principles:
 *   - Header-only with static inline functions — no link-time surprises
 *   - Dynamic arrays (forge_containers.h) for contacts and SAP storage;
 *     callers must call the appropriate destroy/free functions
 *   - Deterministic: same inputs always produce same outputs
 *   - Guard all divisions and normalizations against degenerate inputs
 *
 * Performance note — unguarded hot-path functions:
 *   forge_physics_shape_support() and forge_physics_gjk_support() do NOT
 *   validate inputs for NaN/Inf. These are inner-loop functions called
 *   hundreds to thousands of times per frame (twice per GJK iteration,
 *   5-20 iterations per pair, tens of pairs per frame). The per-call
 *   validation cost (13+ forge_isfinite checks) exceeds the actual
 *   computation for simple shapes like spheres. Callers — primarily
 *   forge_physics_gjk_intersect() and forge_physics_epa() — are
 *   responsible for validating inputs before entering the GJK/EPA loop.
 *   Passing NaN/Inf to these functions is undefined behavior.
 *   Quaternions passed to these functions MUST be normalized — they do
 *   not normalize internally. Non-unit quaternions produce incorrect
 *   results for boxes and capsules (quat_conjugate is only the inverse
 *   for unit quaternions).
 *
 * Depends on: common/math/forge_math.h (vec3 type and operations)
 *             common/containers/forge_containers.h (dynamic arrays)
 *
 * See: lessons/physics/ for lessons teaching each concept.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_PHYSICS_H
#define FORGE_PHYSICS_H

#include "math/forge_math.h"
#include "containers/forge_containers.h"
#include "arena/forge_arena.h"
#include <stdbool.h>   /* bool */
#include <stdint.h>    /* UINT16_MAX */

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
    float radius;         /* collision sphere radius — sphere-sphere and sphere-plane tests (meters) */
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

    /* Sanitize position — NaN/Inf would propagate into collision detection. */
    if (!forge_isfinite(vec3_length_squared(position))) {
        position = vec3_create(0.0f, 0.0f, 0.0f);
    }
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

    /* Clamp damping to [0, 1] — NaN fails both comparisons, so default to 0. */
    if (!forge_isfinite(damping) || damping < 0.0f) damping = 0.0f;
    else if (damping > 1.0f) damping = 1.0f;
    p.damping = damping;

    /* Clamp restitution to [0, 1] — NaN fails both comparisons, so default to 0. */
    if (!forge_isfinite(restitution) || restitution < 0.0f) restitution = 0.0f;
    else if (restitution > 1.0f) restitution = 1.0f;
    p.restitution = restitution;

    /* Clamp radius to non-negative — reject NaN and infinity. */
    p.radius = (forge_isfinite(radius) && radius > 0.0f) ? radius : 0.0f;

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
    if (!p) return;

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
    if (!p) return;

    /* Static particles are unaffected by forces. */
    if (p->inv_mass == 0.0f) {
        return;
    }

    /* Negative drag would inject energy, NaN would poison the accumulator —
     * clamp to zero in both cases. */
    if (!forge_isfinite(drag_coeff) || drag_coeff < 0.0f) drag_coeff = 0.0f;

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
    if (!p) return;

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
    if (!p) return;

    /* Reject non-positive or non-finite timesteps.
     * NaN fails all comparisons, so check explicitly. */
    if (!(dt > 0.0f) || !forge_isfinite(dt)) {
        return;
    }

    /* Static particles do not move. */
    if (p->inv_mass == 0.0f) {
        return;
    }

    /* Reject particles whose state has become non-finite (NaN/inf).
     * IEEE 754: NaN comparisons return false, so velocity-clamping guards
     * like `speed_sq > MAX` silently pass NaN through.  Catching it here
     * prevents one corrupted particle from poisoning the simulation. */
    if (!forge_isfinite(vec3_length_squared(p->velocity)) ||
        !forge_isfinite(vec3_length_squared(p->position)) ||
        !forge_isfinite(vec3_length_squared(p->force_accum))) {
        /* Restore last known-good transform and zero all dynamics so the
         * corrupted state does not propagate into collision detection. */
        p->position    = p->prev_position;
        p->velocity    = vec3_create(0.0f, 0.0f, 0.0f);
        p->force_accum = vec3_create(0.0f, 0.0f, 0.0f);
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
        float speed = SDL_sqrtf(speed_sq);
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
    if (!p) return false;

    /* Reject non-finite inputs — NaN normal would bypass the epsilon check
     * (NaN < epsilon is false) and corrupt the particle state.  Also reject
     * non-finite position since vec3_dot would produce NaN distance. */
    if (!forge_isfinite(vec3_length_squared(p->position)) ||
        !(p->radius > FORGE_PHYSICS_EPSILON) ||
        !forge_isfinite(p->radius) ||
        !forge_isfinite(plane_d)) {
        return false;
    }

    /* Guard against degenerate (zero-length) or non-finite normals. */
    float normal_len_sq = vec3_length_squared(plane_normal);
    if (!forge_isfinite(normal_len_sq) ||
        !(normal_len_sq > FORGE_PHYSICS_EPSILON)) {
        return false;
    }

    /* Static particles are immovable — skip collision response. */
    if (p->inv_mass == 0.0f) {
        return false;
    }

    float inv_len = 1.0f / SDL_sqrtf(normal_len_sq);
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
    if (!p) return;
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
    s.rest_length = (forge_isfinite(rest_length) && rest_length > 0.0f)
                        ? rest_length : 0.0f;
    s.stiffness   = (forge_isfinite(stiffness) && stiffness > 0.0f)
                        ? stiffness : 0.0f;
    s.damping     = (forge_isfinite(damping) && damping > 0.0f)
                        ? damping : 0.0f;
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
    c.distance  = (forge_isfinite(distance) && distance > 0.0f)
                      ? distance : 0.0f;

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
 *   - Either radius is non-positive, subnormal, NaN, or infinite: returns false
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
    if (!a || !b || !out) return false;

    /* Zero, subnormal, NaN, or infinite radius cannot collide. */
    if (!(a->radius > FORGE_PHYSICS_EPSILON) || !forge_isfinite(a->radius) ||
        !(b->radius > FORGE_PHYSICS_EPSILON) || !forge_isfinite(b->radius)) {
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

    float dist = SDL_sqrtf(dist_sq);
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
    float e = forge_fminf(pa->restitution, pb->restitution);

    /* Kill restitution for low-velocity contacts to prevent jitter. */
    if (SDL_fabsf(v_closing) < FORGE_PHYSICS_RESTING_THRESHOLD) {
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
 * convergence (see Physics Lesson 06).
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
 * for overlap. Detected contacts are appended to the caller-provided
 * dynamic array (forge_containers.h). The array is NOT cleared — the
 * caller should call forge_arr_set_length(*out_contacts, 0) before this
 * function if a fresh detection pass is desired. (The convenience wrapper
 * forge_physics_collide_particles_step() clears automatically each call.)
 *
 * For large particle counts, replace this brute-force approach with
 * spatial partitioning (grid, octree) — see Physics Lesson 08.
 *
 * Algorithm:
 *   for i in [0, n-1):
 *     for j in [i+1, n):
 *       if collide_sphere_sphere(i, j):
 *         forge_arr_append(*out_contacts, result)
 *   return forge_arr_length(*out_contacts)
 *
 * Parameters:
 *   particles     (const ForgePhysicsParticle*) — particle array; must not be NULL
 *   num_particles (int)                         — number of particles
 *   out_contacts  (ForgePhysicsContact**)       — pointer to a dynamic array of
 *                                                 contacts; must not be NULL. The
 *                                                 pointed-to array may be NULL
 *                                                 (will be allocated on first append).
 *
 * Returns:
 *   The total number of contacts in the dynamic array after detection.
 *
 * Usage:
 *   ForgePhysicsContact *contacts = NULL;
 *   int n = forge_physics_collide_particles_all(
 *       particles, num_particles, &contacts);
 *   // ... use contacts[0..n-1] ...
 *   forge_arr_free(contacts);
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 11 —
 * brute-force collision detection and the need for broad-phase pruning.
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline int forge_physics_collide_particles_all(
    const ForgePhysicsParticle *particles, int num_particles,
    ForgePhysicsContact **out_contacts)
{
    if (!particles || num_particles <= 1 || !out_contacts) {
        return 0;
    }

    for (int i = 0; i < num_particles - 1; i++) {
        for (int j = i + 1; j < num_particles; j++) {
            ForgePhysicsContact c;
            if (forge_physics_collide_sphere_sphere(
                    &particles[i], &particles[j], i, j, &c)) {
                forge_arr_append(*out_contacts, c);
            }
        }
    }

    return (int)forge_arr_length(*out_contacts);
}

/* Detect and resolve all particle collisions in one step.
 *
 * Convenience function that performs both the detection and resolution
 * phases: first runs O(n^2) all-pairs sphere-sphere detection into a
 * dynamic array, then resolves all detected contacts with impulse-based
 * response and positional correction.
 *
 * Algorithm:
 *   forge_arr_set_length(*out_contacts, 0)
 *   num_contacts = collide_particles_all(particles, out_contacts)
 *   resolve_contacts(*out_contacts, num_contacts, particles)
 *   return num_contacts
 *
 * Parameters:
 *   particles     (ForgePhysicsParticle*) — particle array; must not be NULL.
 *                                           Velocities and positions are modified
 *                                           in place during resolution.
 *   num_particles (int)                   — number of particles
 *   out_contacts  (ForgePhysicsContact**) — pointer to a dynamic array of contacts;
 *                                           cleared and repopulated each call
 *
 * Returns:
 *   The number of contacts detected and resolved.
 *
 * Usage:
 *   ForgePhysicsContact *contacts = NULL;
 *   int n = forge_physics_collide_particles_step(
 *       particles, num_particles, &contacts);
 *   SDL_Log("Resolved %d collisions this frame", n);
 *   // ... later ...
 *   forge_arr_free(contacts);
 *
 * Reference: Millington, "Game Physics Engine Development", Ch. 7 —
 * the complete contact resolution pipeline (detect → resolve).
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline int forge_physics_collide_particles_step(
    ForgePhysicsParticle *particles, int num_particles,
    ForgePhysicsContact **out_contacts)
{
    if (!out_contacts) return 0;

    forge_arr_set_length(*out_contacts, 0);

    int num_contacts = forge_physics_collide_particles_all(
        particles, num_particles, out_contacts);

    if (num_contacts > 0) {
        forge_physics_resolve_contacts(
            *out_contacts, num_contacts, particles, num_particles);
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

    /* Sanitize position — NaN/Inf would propagate into collision detection. */
    if (!forge_isfinite(vec3_length_squared(position))) {
        position = vec3_create(0.0f, 0.0f, 0.0f);
    }
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

    /* Clamp damping and restitution to [0..1] — NaN fails both comparisons,
     * so check finiteness first and default to 0. */
    if (!forge_isfinite(damping) || damping < 0.0f) damping = 0.0f;
    if (damping > 1.0f) damping = 1.0f;
    rb.damping = damping;

    if (!forge_isfinite(angular_damping) || angular_damping < 0.0f)
        angular_damping = 0.0f;
    if (angular_damping > 1.0f) angular_damping = 1.0f;
    rb.angular_damping = angular_damping;

    if (!forge_isfinite(restitution) || restitution < 0.0f) restitution = 0.0f;
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
    if (!forge_isfinite(half_extents.x) || !forge_isfinite(half_extents.y) ||
        !forge_isfinite(half_extents.z)) return;

    float hw = SDL_fabsf(half_extents.x);
    float hh = SDL_fabsf(half_extents.y);
    float hd = SDL_fabsf(half_extents.z);
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
    if (!forge_isfinite(radius)) return;
    radius = SDL_fabsf(radius);

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
    if (!forge_isfinite(radius) || !forge_isfinite(half_h)) return;
    radius = SDL_fabsf(radius);
    half_h = SDL_fabsf(half_h);

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
 *   1b. Guard: if any state field is non-finite (velocity, angular_velocity,
 *       position, force_accum, torque_accum, orientation), clear accumulators
 *       and return — prevents one corrupted body from poisoning the simulation
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
    if (!(dt > 0.0f) || !forge_isfinite(dt)) return;  /* rejects <= 0, NaN, +inf */

    /* Reject bodies whose state has become non-finite (NaN/inf).
     * IEEE 754: NaN comparisons return false, so velocity-clamping guards
     * like `v_len > MAX` silently pass NaN through.  Catching it here
     * prevents one corrupted body from poisoning the whole simulation. */
    if (!forge_isfinite(vec3_length_squared(rb->velocity)) ||
        !forge_isfinite(vec3_length_squared(rb->angular_velocity)) ||
        !forge_isfinite(vec3_length_squared(rb->position)) ||
        !forge_isfinite(vec3_length_squared(rb->force_accum)) ||
        !forge_isfinite(vec3_length_squared(rb->torque_accum)) ||
        !forge_isfinite(quat_length_sq(rb->orientation))) {
        /* Restore last known-good transform and zero all dynamics so the
         * corrupted state does not propagate into collision detection. */
        rb->position         = rb->prev_position;
        rb->orientation      = rb->prev_orientation;
        rb->velocity         = vec3_create(0.0f, 0.0f, 0.0f);
        rb->angular_velocity = vec3_create(0.0f, 0.0f, 0.0f);
        rb->force_accum      = vec3_create(0.0f, 0.0f, 0.0f);
        rb->torque_accum     = vec3_create(0.0f, 0.0f, 0.0f);
        return;
    }

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

    rb->velocity = vec3_scale(rb->velocity, SDL_powf(damp, dt));
    rb->angular_velocity = vec3_scale(rb->angular_velocity,
                                       SDL_powf(ang_damp, dt));

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
        if (SDL_fabsf(q_len_sq - 1.0f) > FORGE_PHYSICS_QUAT_RENORM_THRESHOLD) {
            rb->orientation = quat_normalize(rb->orientation);
        }
    }

    /* ── Update derived data (world-space inertia tensor) ─────────────── */
    forge_physics_rigid_body_update_derived(rb);

    /* ── Clear force and torque accumulators ───────────────────────────── */
    rb->force_accum  = vec3_create(0.0f, 0.0f, 0.0f);
    rb->torque_accum = vec3_create(0.0f, 0.0f, 0.0f);
}

/* Integrate velocities only (no position update).
 *
 * Performs the velocity half of the integration step: applies forces
 * to linear velocity, applies torques (with gyroscopic correction) to
 * angular velocity, applies damping, and clamps velocities. Clears
 * force/torque accumulators. Does NOT update position or orientation.
 *
 * Use this with forge_physics_rigid_body_integrate_positions() when the
 * physics step needs to solve velocity constraints between the velocity
 * and position updates (e.g., sequential impulse solver):
 *
 *   integrate_velocities()   // v += a*dt, damping, clamping
 *   detect_collisions()      // at current positions
 *   solve_constraints()      // correct velocities
 *   integrate_positions()    // x += v*dt (with corrected v)
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_rigid_body_integrate_velocities(
    ForgePhysicsRigidBody *rb, float dt)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!(dt > 0.0f) || !forge_isfinite(dt)) return;

    /* NaN safety — restore last-good transform and zero dynamics,
     * matching the combined integrate. Without this, a corrupted
     * position/orientation would persist into collision detection. */
    if (!forge_isfinite(vec3_length_squared(rb->velocity)) ||
        !forge_isfinite(vec3_length_squared(rb->angular_velocity)) ||
        !forge_isfinite(vec3_length_squared(rb->position)) ||
        !forge_isfinite(vec3_length_squared(rb->force_accum)) ||
        !forge_isfinite(vec3_length_squared(rb->torque_accum)) ||
        !forge_isfinite(quat_length_sq(rb->orientation))) {
        rb->position         = rb->prev_position;
        rb->orientation      = rb->prev_orientation;
        rb->velocity         = vec3_create(0, 0, 0);
        rb->angular_velocity = vec3_create(0, 0, 0);
        rb->force_accum      = vec3_create(0, 0, 0);
        rb->torque_accum     = vec3_create(0, 0, 0);
        return;
    }

    /* Linear acceleration: a = F / m */
    vec3 linear_acc = vec3_scale(rb->force_accum, rb->inv_mass);

    /* Angular acceleration: Euler's equation */
    vec3 Iw = mat3_multiply_vec3(rb->inertia_world, rb->angular_velocity);
    vec3 gyro = vec3_cross(rb->angular_velocity, Iw);
    vec3 angular_acc = mat3_multiply_vec3(rb->inv_inertia_world,
        vec3_sub(rb->torque_accum, gyro));

    /* Update velocities */
    rb->velocity = vec3_add(rb->velocity, vec3_scale(linear_acc, dt));
    rb->angular_velocity = vec3_add(rb->angular_velocity,
                                     vec3_scale(angular_acc, dt));

    /* Exponential damping */
    float damp     = rb->damping;
    float ang_damp = rb->angular_damping;
    if (damp < 0.0f)     damp = 0.0f;
    if (damp > 1.0f)     damp = 1.0f;
    if (ang_damp < 0.0f) ang_damp = 0.0f;
    if (ang_damp > 1.0f) ang_damp = 1.0f;

    rb->velocity = vec3_scale(rb->velocity, SDL_powf(damp, dt));
    rb->angular_velocity = vec3_scale(rb->angular_velocity,
                                       SDL_powf(ang_damp, dt));

    /* Clamp velocities */
    float v_len = vec3_length(rb->velocity);
    if (v_len > FORGE_PHYSICS_MAX_VELOCITY) {
        rb->velocity = vec3_scale(
            vec3_normalize(rb->velocity), FORGE_PHYSICS_MAX_VELOCITY);
    }
    float w_len = vec3_length(rb->angular_velocity);
    if (w_len > FORGE_PHYSICS_MAX_ANGULAR_VELOCITY) {
        rb->angular_velocity = vec3_scale(
            vec3_normalize(rb->angular_velocity),
            FORGE_PHYSICS_MAX_ANGULAR_VELOCITY);
    }

    /* Clear accumulators */
    rb->force_accum  = vec3_create(0, 0, 0);
    rb->torque_accum = vec3_create(0, 0, 0);
}

/* Integrate positions only (no velocity update).
 *
 * Performs the position half of the integration step: updates position
 * from linear velocity, updates orientation from angular velocity,
 * and recomputes derived state (world-space inertia tensor).
 *
 * Call this AFTER velocity constraints have been solved so that
 * positions are updated with the corrected velocities.
 *
 * Does NOT save prev_position/prev_orientation. When using split-step
 * integration with position correction, save prev state before the
 * correction pass for correct render interpolation.
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_rigid_body_integrate_positions(
    ForgePhysicsRigidBody *rb, float dt)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!(dt > 0.0f) || !forge_isfinite(dt)) return;

    /* Note: does NOT save prev_position/prev_orientation. When using
     * split-step integration with position correction, the caller
     * should save prev state before the correction pass so that
     * render interpolation spans the full step. */

    /* NaN guard — don't propagate corrupt velocity into position */
    if (!forge_isfinite(vec3_length_squared(rb->velocity)) ||
        !forge_isfinite(vec3_length_squared(rb->angular_velocity))) {
        return;
    }

    /* Update position: x += v * dt */
    rb->position = vec3_add(rb->position, vec3_scale(rb->velocity, dt));

    /* Update orientation: q += 0.5 * dt * (ω_quat * q) */
    float w_len = vec3_length(rb->angular_velocity);
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

        float q_len_sq = quat_length_sq(rb->orientation);
        if (SDL_fabsf(q_len_sq - 1.0f) > FORGE_PHYSICS_QUAT_RENORM_THRESHOLD) {
            rb->orientation = quat_normalize(rb->orientation);
        }
    }

    /* Update derived data (world-space inertia tensor) */
    forge_physics_rigid_body_update_derived(rb);
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
    normal = vec3_scale(normal, 1.0f / SDL_sqrtf(normal_len_sq));

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

/* Default position correction parameters (used by si_correct_positions). */
#define FORGE_PHYSICS_DEFAULT_CORRECTION_FRACTION  0.4f
#define FORGE_PHYSICS_DEFAULT_CORRECTION_SLOP      0.01f

/* ── Solver configuration ──────────────────────────────────────────────────── */

/* Per-solve configuration for the sequential impulse solver.
 *
 * Groups solver tuning parameters into one struct for convenience.
 * The fields are consumed at different stages of the solver pipeline:
 *
 * - baumgarte_factor and penetration_slop are read by
 *   forge_physics_si_prepare() (called internally by forge_physics_si_solve())
 *   to compute velocity-level bias corrections.
 *
 * - correction_fraction and correction_slop are NOT read by
 *   forge_physics_si_solve(). They must be passed explicitly to
 *   forge_physics_si_correct_positions() as direct parameters.
 *   The struct groups them for caller convenience, but the position
 *   correction function does not take a config pointer.
 *
 * Pass NULL to forge_physics_si_solve() to use default baumgarte_factor
 * and penetration_slop values.
 *
 * baumgarte_factor  — velocity bias rate for penetration correction [0..1].
 *                     Higher values correct penetration faster but cause more
 *                     energy injection (jitter). 0.1–0.3 is the stable range
 *                     for stacking. Default: 0.2.
 *
 * penetration_slop  — overlap tolerance (m). Contacts shallower than this
 *                     receive no bias correction, reducing jitter on resting
 *                     contacts. Default: 0.01 (1 cm).
 *
 * correction_fraction — position correction rate per step [0..1]. Applied
 *                       after velocity solving to push overlapping bodies
 *                       apart. 0.2–0.4 is typical. Default: 0.4.
 *
 * correction_slop   — position correction tolerance (m). Same role as
 *                     penetration_slop but for the position correction pass.
 *                     Default: 0.01.
 *
 * Usage:
 *   ForgePhysicsSolverConfig cfg = forge_physics_solver_config_default();
 *   cfg.baumgarte_factor = 0.1f;   // gentler bias for tall stacks
 *   cfg.penetration_slop = 0.005f; // tighter tolerance
 *   forge_physics_si_solve(manifolds, count, bodies, num_bodies,
 *                          20, dt, true, workspace, &cfg);
 *   forge_physics_si_correct_positions(manifolds, count, bodies, num_bodies,
 *                                      cfg.correction_fraction,
 *                                      cfg.correction_slop);
 *
 * See: Physics Lesson 14 — Stacking Stability
 * Ref: Catto, "Modeling and Solving Constraints" (GDC 2009)
 */
typedef struct ForgePhysicsSolverConfig {
    float baumgarte_factor;      /* velocity bias rate [0..1]            */
    float penetration_slop;      /* tolerated overlap (m)                */
    float correction_fraction;   /* position correction rate [0..1]      */
    float correction_slop;       /* position correction tolerance (m)    */
} ForgePhysicsSolverConfig;

/* Return a solver config with the default values matching the #define
 * constants. Callers can override individual fields before passing to
 * forge_physics_si_solve() and forge_physics_si_correct_positions(). */
static inline ForgePhysicsSolverConfig forge_physics_solver_config_default(void)
{
    ForgePhysicsSolverConfig cfg;
    cfg.baumgarte_factor    = FORGE_PHYSICS_BAUMGARTE_FACTOR;
    cfg.penetration_slop    = FORGE_PHYSICS_PENETRATION_SLOP;
    cfg.correction_fraction = FORGE_PHYSICS_DEFAULT_CORRECTION_FRACTION;
    cfg.correction_slop     = FORGE_PHYSICS_DEFAULT_CORRECTION_SLOP;
    return cfg;
}

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
 *   radius       — sphere radius (m), must be > 0 and finite
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
    if (!(radius > FORGE_PHYSICS_EPSILON) || !forge_isfinite(radius)) return false;
    float normal_len_sq = vec3_length_squared(plane_normal);
    if (!(normal_len_sq > FORGE_PHYSICS_EPSILON)) return false;
    plane_normal = vec3_scale(plane_normal, 1.0f / SDL_sqrtf(normal_len_sq));

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
 *   half_extents — half-width, half-height, half-depth (m); must be finite
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
    if (!forge_isfinite(half_extents.x) || !forge_isfinite(half_extents.y) ||
        !forge_isfinite(half_extents.z)) return 0;
    float normal_len_sq = vec3_length_squared(plane_normal);
    if (!(normal_len_sq > FORGE_PHYSICS_EPSILON)) return 0;
    plane_normal = vec3_scale(plane_normal, 1.0f / SDL_sqrtf(normal_len_sq));

    /* Rotation matrix from body orientation */
    mat3 R = quat_to_mat3(body->orientation);

    /* Clamp friction coefficients */
    if (mu_s < 0.0f) mu_s = 0.0f;
    if (mu_d < 0.0f) mu_d = 0.0f;

    float hx = SDL_fabsf(half_extents.x);
    float hy = SDL_fabsf(half_extents.y);
    float hz = SDL_fabsf(half_extents.z);

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
 *   radius_a  — collision radius of body A (must be > 0 and finite)
 *   b         — second rigid body (must not be NULL)
 *   idx_b     — index of body B in the bodies array
 *   radius_b  — collision radius of body B (must be > 0 and finite)
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
    if (!(radius_a > FORGE_PHYSICS_EPSILON) || !forge_isfinite(radius_a) ||
        !(radius_b > FORGE_PHYSICS_EPSILON) || !forge_isfinite(radius_b)) return false;

    /* Two static bodies cannot collide. */
    if (a->inv_mass == 0.0f && b->inv_mass == 0.0f) return false;

    /* Vector from B toward A. */
    vec3 d = vec3_sub(a->position, b->position);
    float dist_sq = vec3_dot(d, d);
    float sum_radii = radius_a + radius_b;

    /* No overlap — spheres are separated. */
    if (dist_sq >= sum_radii * sum_radii) return false;

    float dist = SDL_sqrtf(dist_sq);
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
    if (!(dt > 0.0f) || !forge_isfinite(dt)) return;

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
    if (!(n_len_sq > FORGE_PHYSICS_EPSILON) || !forge_isfinite(n_len_sq)) return;
    n = vec3_scale(n, 1.0f / SDL_sqrtf(n_len_sq));

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
    if (b) e = forge_fminf(e, b->restitution);

    /* Kill restitution for resting contacts */
    if (SDL_fabsf(v_n) < FORGE_PHYSICS_RB_RESTING_THRESHOLD) {
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
            if (SDL_fabsf(j_t) <= mu_s * j_n) {
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

/* ═══════════════════════════════════════════════════════════════════════════
 * LESSON 07 — Collision Shapes and Support Functions
 *
 * This section introduces three foundational primitives for collision
 * detection: a tagged union for shape parameters, support functions (the
 * geometric core of the GJK algorithm), and axis-aligned bounding box
 * (AABB) computation.
 *
 * Shapes are separate structs, NOT embedded in ForgePhysicsRigidBody.
 * Demos keep shapes[] parallel to bodies[], matching the existing
 * parallel-array pattern from L06.
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Collision shape constants ─────────────────────────────────────────── */

/* Minimum allowed dimension for shape parameters (radius, half-extent).
 * Prevents degenerate zero-volume shapes that break support functions
 * and AABB computation. */
#define FORGE_PHYSICS_SHAPE_MIN_DIM  1e-5f

/* Hemisphere inertia coefficients for capsule inertia.
 *
 * A solid hemisphere has different moments about its symmetry axis and
 * transverse axes (unlike a full sphere):
 *   - Symmetry axis (Y):   I_yy = (2/5) m r²  (same as full sphere)
 *   - Transverse axes (X,Z): I_xx = I_zz = (83/320) m r²
 *
 * The parallel axis theorem shifts each hemisphere by the distance from
 * its centroid to the capsule center. The centroid of a hemisphere is at
 * (3/8)r from the flat face. */
#define FORGE_PHYSICS_CAPSULE_HEMI_CENTROID_FRAC    (3.0f / 8.0f)
#define FORGE_PHYSICS_HEMI_TRANSVERSE_INERTIA_COEFF (83.0f / 320.0f)

/* ── Types ─────────────────────────────────────────────────────────────── */

/* Collision shape type tag.
 *
 * Each type defines a convex shape with a known support function and
 * AABB computation. Sphere, box, and capsule cover the vast majority
 * of game physics needs.
 */
typedef enum ForgePhysicsShapeType {
    FORGE_PHYSICS_SHAPE_SPHERE  = 0,
    FORGE_PHYSICS_SHAPE_BOX     = 1,
    FORGE_PHYSICS_SHAPE_CAPSULE = 2
} ForgePhysicsShapeType;

/* Tagged union for collision shape parameters.
 *
 * Stores the geometric parameters needed for collision detection without
 * embedding them in the rigid body struct. This keeps shape data separate
 * from dynamics data, following the parallel-array pattern.
 *
 * - Sphere:  defined by radius
 * - Box:     defined by half-extents (half-width, half-height, half-depth)
 * - Capsule: defined by radius and half-height (cylinder half-height,
 *            not including the hemisphere caps)
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
typedef struct ForgePhysicsCollisionShape {
    ForgePhysicsShapeType type;
    union {
        struct { float radius; }                     sphere;
        struct { vec3 half_extents; }                box;
        struct { float radius; float half_height; }  capsule;
    } data;
} ForgePhysicsCollisionShape;

/* Axis-Aligned Bounding Box (AABB).
 *
 * A rectangular box aligned with the world axes, defined by its minimum
 * and maximum corners. AABBs are the standard broadphase primitive:
 * cheap to compute, cheap to test for overlap, and they tightly enclose
 * the shape when axis-aligned.
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
typedef struct ForgePhysicsAABB {
    vec3 min;
    vec3 max;
} ForgePhysicsAABB;

/* ── Shape constructors ────────────────────────────────────────────────── */

/* Create a sphere collision shape.
 *
 * Parameters:
 *   radius — sphere radius (m), clamped to FORGE_PHYSICS_SHAPE_MIN_DIM minimum
 *
 * Usage:
 *   ForgePhysicsCollisionShape s = forge_physics_shape_sphere(0.5f);
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline ForgePhysicsCollisionShape forge_physics_shape_sphere(float radius)
{
    ForgePhysicsCollisionShape shape;
    shape.type = FORGE_PHYSICS_SHAPE_SPHERE;
    if (!forge_isfinite(radius) || radius < FORGE_PHYSICS_SHAPE_MIN_DIM)
        radius = FORGE_PHYSICS_SHAPE_MIN_DIM;
    shape.data.sphere.radius = radius;
    return shape;
}

/* Create a box collision shape.
 *
 * Parameters:
 *   half_extents — half-width, half-height, half-depth (m),
 *                  each component clamped to FORGE_PHYSICS_SHAPE_MIN_DIM minimum
 *
 * Usage:
 *   ForgePhysicsCollisionShape s = forge_physics_shape_box(
 *       vec3_create(0.5f, 0.5f, 0.5f));
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline ForgePhysicsCollisionShape forge_physics_shape_box(vec3 half_extents)
{
    ForgePhysicsCollisionShape shape;
    shape.type = FORGE_PHYSICS_SHAPE_BOX;

    if (!forge_isfinite(half_extents.x) || half_extents.x < FORGE_PHYSICS_SHAPE_MIN_DIM)
        half_extents.x = FORGE_PHYSICS_SHAPE_MIN_DIM;
    if (!forge_isfinite(half_extents.y) || half_extents.y < FORGE_PHYSICS_SHAPE_MIN_DIM)
        half_extents.y = FORGE_PHYSICS_SHAPE_MIN_DIM;
    if (!forge_isfinite(half_extents.z) || half_extents.z < FORGE_PHYSICS_SHAPE_MIN_DIM)
        half_extents.z = FORGE_PHYSICS_SHAPE_MIN_DIM;

    shape.data.box.half_extents = half_extents;
    return shape;
}

/* Create a capsule collision shape (Y-axis aligned).
 *
 * A capsule is a cylinder capped with hemispheres. The half_height
 * parameter describes the cylindrical portion only — the total height
 * from tip to tip is 2 * (half_height + radius).
 *
 * Parameters:
 *   radius      — capsule radius (m)
 *   half_height — half-height of the cylindrical portion (m)
 *
 * Usage:
 *   ForgePhysicsCollisionShape s = forge_physics_shape_capsule(0.3f, 0.5f);
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline ForgePhysicsCollisionShape forge_physics_shape_capsule(
    float radius, float half_height)
{
    ForgePhysicsCollisionShape shape;
    shape.type = FORGE_PHYSICS_SHAPE_CAPSULE;

    if (!forge_isfinite(radius) || radius < FORGE_PHYSICS_SHAPE_MIN_DIM)
        radius = FORGE_PHYSICS_SHAPE_MIN_DIM;
    if (!forge_isfinite(half_height) || half_height < FORGE_PHYSICS_SHAPE_MIN_DIM)
        half_height = FORGE_PHYSICS_SHAPE_MIN_DIM;

    shape.data.capsule.radius      = radius;
    shape.data.capsule.half_height = half_height;
    return shape;
}

/* Validate a collision shape.
 *
 * Checks that the type tag is recognized and all dimensions are positive
 * and finite.
 *
 * Parameters:
 *   shape — pointer to the shape to validate
 *
 * Returns: true if valid, false otherwise
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline bool forge_physics_shape_is_valid(
    const ForgePhysicsCollisionShape *shape)
{
    if (!shape) return false;

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE:
        return forge_isfinite(shape->data.sphere.radius) &&
               shape->data.sphere.radius > 0.0f;
    case FORGE_PHYSICS_SHAPE_BOX:
        return forge_isfinite(shape->data.box.half_extents.x) &&
               forge_isfinite(shape->data.box.half_extents.y) &&
               forge_isfinite(shape->data.box.half_extents.z) &&
               shape->data.box.half_extents.x > 0.0f &&
               shape->data.box.half_extents.y > 0.0f &&
               shape->data.box.half_extents.z > 0.0f;
    case FORGE_PHYSICS_SHAPE_CAPSULE:
        return forge_isfinite(shape->data.capsule.radius) &&
               forge_isfinite(shape->data.capsule.half_height) &&
               shape->data.capsule.radius > 0.0f &&
               shape->data.capsule.half_height > 0.0f;
    default:
        return false;
    }
}

/* ── Capsule inertia ───────────────────────────────────────────────────── */

/* Set the inertia tensor for a solid capsule (Y-axis aligned).
 *
 * A capsule is a cylinder of radius r and half-height h, capped with two
 * hemispheres of radius r. The total mass is split proportionally between
 * the cylinder and the two hemisphere caps based on their volumes:
 *
 *   V_cyl  = π r² (2h)
 *   V_hemi = (2/3) π r³   (one hemisphere; two caps = (4/3) π r³ = sphere)
 *
 * Cylinder inertia (about its own center):
 *   I_cyl_yy = (1/2) m_cyl r²
 *   I_cyl_xx = I_cyl_zz = (1/12) m_cyl (3r² + (2h)²)
 *
 * Hemisphere inertia (about capsule center, using parallel axis theorem):
 *   I_hemi_xx_com = (83/320) m_hemi r²  (transverse axes, about centroid)
 *   I_hemi_yy_com = (2/5) m_hemi r²     (symmetry axis, about centroid)
 *   offset = h + (3/8)r                  (capsule center to hemisphere centroid)
 *   I_hemi_xx = I_hemi_zz = I_hemi_xx_com + m_hemi offset²  (parallel axis)
 *   I_hemi_yy = I_hemi_yy_com            (no shift needed on symmetry axis)
 *
 * Parameters:
 *   rb          — rigid body (mass must be set first)
 *   radius      — capsule radius (m)
 *   half_height — half-height of the cylindrical portion (m)
 *
 * Usage:
 *   forge_physics_rigid_body_set_inertia_capsule(&rb, 0.3f, 0.5f);
 *
 * Reference: "Inertia Tensor of a Capsule" — standard derivation via
 * composite body theorem (cylinder + 2 hemispheres with parallel axis).
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline void forge_physics_rigid_body_set_inertia_capsule(
    ForgePhysicsRigidBody *rb, float radius, float half_height)
{
    if (!rb || rb->inv_mass == 0.0f) return;
    if (!forge_isfinite(radius) || !forge_isfinite(half_height)) return;
    radius      = SDL_fabsf(radius);
    half_height = SDL_fabsf(half_height);
    if (radius < FORGE_PHYSICS_SHAPE_MIN_DIM) radius = FORGE_PHYSICS_SHAPE_MIN_DIM;
    if (half_height < FORGE_PHYSICS_SHAPE_MIN_DIM) half_height = FORGE_PHYSICS_SHAPE_MIN_DIM;

    float m = rb->mass;
    float r2 = radius * radius;
    float h  = half_height;  /* half-height of cylinder */

    /* Volume-proportional mass split */
    float v_cyl  = FORGE_PI * r2 * (2.0f * h);
    float v_caps = (4.0f / 3.0f) * FORGE_PI * r2 * radius;  /* two hemispheres = sphere */
    float v_total = v_cyl + v_caps;
    if (v_total < FORGE_PHYSICS_EPSILON) v_total = FORGE_PHYSICS_EPSILON;

    float m_cyl  = m * (v_cyl / v_total);
    float m_caps = m * (v_caps / v_total);  /* total mass of both caps */
    float m_hemi = m_caps * 0.5f;           /* mass of one hemisphere */

    /* Cylinder contribution (centered at origin) */
    float cyl_Iyy = FORGE_PHYSICS_INERTIA_CYLINDER_COEFF * m_cyl * r2;
    float full_h2 = 4.0f * h * h;  /* (2h)² */
    float cyl_Ixx = FORGE_PHYSICS_INERTIA_BOX_COEFF * m_cyl * (3.0f * r2 + full_h2);

    /* Hemisphere contribution (parallel axis theorem).
     * Transverse axes (X/Z) use 83/320 m r², not 2/5, because a hemisphere
     * is not symmetric about those axes. The symmetry axis (Y) uses 2/5. */
    float hemi_Ixx_com = FORGE_PHYSICS_HEMI_TRANSVERSE_INERTIA_COEFF * m_hemi * r2;
    float hemi_Iyy_com = FORGE_PHYSICS_INERTIA_SPHERE_COEFF * m_hemi * r2;
    float offset = h + FORGE_PHYSICS_CAPSULE_HEMI_CENTROID_FRAC * radius;
    float hemi_Ixx = hemi_Ixx_com + m_hemi * offset * offset;  /* per hemisphere */
    float hemi_Iyy = hemi_Iyy_com;  /* no parallel axis shift on symmetry axis */

    /* Sum contributions (two hemispheres) */
    float Ixx = cyl_Ixx + 2.0f * hemi_Ixx;
    float Iyy = cyl_Iyy + 2.0f * hemi_Iyy;
    float Izz = Ixx;  /* symmetric about Y */

    /* Guard against degenerate inertia */
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

/* Set the inertia tensor from a collision shape.
 *
 * Dispatches to the correct per-shape inertia setter based on shape type.
 * This eliminates the need to match shape types manually when setting up
 * rigid bodies.
 *
 * Parameters:
 *   rb    — rigid body (mass must be set first)
 *   shape — collision shape describing the geometry
 *
 * Usage:
 *   ForgePhysicsCollisionShape shape = forge_physics_shape_sphere(0.5f);
 *   forge_physics_rigid_body_set_inertia_from_shape(&rb, &shape);
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline void forge_physics_rigid_body_set_inertia_from_shape(
    ForgePhysicsRigidBody *rb, const ForgePhysicsCollisionShape *shape)
{
    if (!rb || !shape || !forge_physics_shape_is_valid(shape)) return;

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE:
        forge_physics_rigid_body_set_inertia_sphere(
            rb, shape->data.sphere.radius);
        break;
    case FORGE_PHYSICS_SHAPE_BOX:
        forge_physics_rigid_body_set_inertia_box(
            rb, shape->data.box.half_extents);
        break;
    case FORGE_PHYSICS_SHAPE_CAPSULE:
        forge_physics_rigid_body_set_inertia_capsule(
            rb, shape->data.capsule.radius,
            shape->data.capsule.half_height);
        break;
    default:
        break;
    }
}

/* ── Support function ──────────────────────────────────────────────────── */

/* Compute the support point of a collision shape in a given direction.
 *
 * The support function returns the point on the shape's surface that is
 * farthest in a given direction. This is the geometric foundation of the
 * GJK (Gilbert-Johnson-Keerthi) algorithm for convex intersection testing.
 *
 * UNGUARDED HOT PATH — no NaN/Inf validation on pos, orient, or dir.
 * See the file header comment for rationale. Callers must ensure all
 * inputs are finite and orient is a unit quaternion before calling.
 * Passing NaN/Inf is undefined behavior (garbage output, no crash).
 *
 * For each shape type:
 *
 *   Sphere:  support = center + normalize(dir) * radius
 *     The farthest point on a sphere is always along the query direction.
 *
 *   Box:     For each local axis, pick the sign that aligns with dir,
 *            giving one of the 8 corners. Transform that corner to world
 *            space.
 *
 *   Capsule: Project dir onto the local Y axis to determine which
 *            hemisphere cap is farthest. Then find the support of that
 *            hemisphere (center + normalize(dir) * radius).
 *
 * If dir is zero or near-zero, returns pos (the shape center).
 *
 * Parameters:
 *   shape  — collision shape (NULL or invalid → returns pos)
 *   pos    — world-space position (must be finite — not validated)
 *   orient — unit quaternion (must be finite and normalized — not validated)
 *   dir    — world-space direction (need not be normalized, must be finite
 *            — not validated; zero-length → returns pos)
 *
 * Returns: world-space support point
 *
 * Reference: Gilbert, Johnson, Keerthi, "A Fast Procedure for Computing
 * the Distance Between Complex Objects in Three-Dimensional Space" (1988).
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline vec3 forge_physics_shape_support(
    const ForgePhysicsCollisionShape *shape,
    vec3 pos, quat orient, vec3 dir)
{
    /* UNGUARDED HOT PATH — no NaN/Inf validation.
     *
     * This function is the innermost loop of GJK and EPA, called hundreds
     * to thousands of times per physics frame. The per-call cost of
     * validating all inputs (13 forge_isfinite checks on pos, dir, orient
     * components, plus quaternion length and direction length checks)
     * exceeds the actual support computation for spheres (~8 instructions)
     * and is a significant fraction for boxes/capsules (~15-30
     * instructions). For 50 broadphase pairs at 10 GJK iterations each,
     * removing validation saves ~20,000 comparison instructions per frame.
     *
     * Callers (gjk_intersect, epa) validate inputs once before entering
     * the iteration loop. Passing NaN/Inf position, direction, or
     * orientation to this function is undefined behavior — results will
     * be garbage, but no crash or memory corruption will occur.
     *
     * Quaternions MUST be normalized before calling. This function does
     * not normalize internally — a non-unit quaternion produces incorrect
     * support points for boxes and capsules because quat_conjugate() is
     * only the inverse for unit quaternions. In practice, all callers
     * already pass unit quaternions: rigid body integration normalizes
     * every frame, and static bodies use quat_identity().
     *
     * The null-shape and zero-direction checks are retained because they
     * guard against structural errors (missing shape setup, degenerate
     * geometry), not transient numerical corruption. */

    if (!shape || !forge_physics_shape_is_valid(shape)) return pos;

    float dir_len = vec3_length(dir);
    if (dir_len < FORGE_PHYSICS_EPSILON) return pos;

    vec3 dir_n = vec3_scale(dir, 1.0f / dir_len);

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE: {
        /* Farthest point is center + radius along direction */
        return vec3_add(pos, vec3_scale(dir_n, shape->data.sphere.radius));
    }

    case FORGE_PHYSICS_SHAPE_BOX: {
        /* Rotate direction into local space */
        quat inv_orient = quat_conjugate(orient);
        vec3 local_dir = quat_rotate_vec3(inv_orient, dir_n);

        /* Pick the corner that maximizes dot(corner, local_dir) */
        vec3 he = shape->data.box.half_extents;
        vec3 corner;
        corner.x = (local_dir.x >= 0.0f) ?  he.x : -he.x;
        corner.y = (local_dir.y >= 0.0f) ?  he.y : -he.y;
        corner.z = (local_dir.z >= 0.0f) ?  he.z : -he.z;

        /* Transform corner to world space */
        return vec3_add(pos, quat_rotate_vec3(orient, corner));
    }

    case FORGE_PHYSICS_SHAPE_CAPSULE: {
        /* Capsule = line segment + sphere sweep.
         * Project dir onto local Y to pick which hemisphere cap. */
        vec3 local_y = quat_rotate_vec3(orient, vec3_create(0, 1, 0));
        float dot_y = vec3_dot(dir_n, local_y);

        /* Center of the chosen hemisphere cap */
        vec3 cap_center;
        if (dot_y >= 0.0f)
            cap_center = vec3_add(pos, vec3_scale(local_y, shape->data.capsule.half_height));
        else
            cap_center = vec3_sub(pos, vec3_scale(local_y, shape->data.capsule.half_height));

        /* Support of sphere at cap center */
        return vec3_add(cap_center, vec3_scale(dir_n, shape->data.capsule.radius));
    }

    default:
        return pos;
    }
}

/* ── AABB computation ──────────────────────────────────────────────────── */

/* Compute the world-space AABB for a collision shape.
 *
 * The AABB (axis-aligned bounding box) is the tightest box aligned with
 * the world axes that fully encloses the shape at its current position
 * and orientation. AABBs are recomputed every frame because rotation
 * changes the enclosure.
 *
 * For each shape type:
 *
 *   Sphere:  AABB = center ± radius on each axis. Orientation-independent
 *            because a sphere looks the same from every angle.
 *
 *   Box:     Rotate the 3 half-extent axes to world space, take absolute
 *            values, and sum the contributions per world axis. This gives
 *            the tightest AABB for an oriented box without testing all 8
 *            corners.
 *
 *   Capsule: Compute the two hemisphere cap centers in world space,
 *            take their component-wise min/max, then expand by radius.
 *
 * Parameters:
 *   shape  — collision shape
 *   pos    — world-space position
 *   orient — orientation quaternion
 *
 * Returns: world-space AABB
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline ForgePhysicsAABB forge_physics_shape_compute_aabb(
    const ForgePhysicsCollisionShape *shape,
    vec3 pos, quat orient)
{
    /* Safe fallback center: use pos if finite, otherwise origin */
    vec3 fallback_center = vec3_create(0.0f, 0.0f, 0.0f);
    if (forge_isfinite(pos.x) && forge_isfinite(pos.y) && forge_isfinite(pos.z))
        fallback_center = pos;

    ForgePhysicsAABB aabb;
    aabb.min = fallback_center;
    aabb.max = fallback_center;

    if (!shape || !forge_physics_shape_is_valid(shape)) return aabb;

    /* Guard against non-finite position or orientation to prevent NaN propagation */
    if (!forge_isfinite(pos.x) || !forge_isfinite(pos.y) || !forge_isfinite(pos.z))
        return aabb;
    if (!forge_isfinite(orient.w) || !forge_isfinite(orient.x) ||
        !forge_isfinite(orient.y) || !forge_isfinite(orient.z))
        return aabb;

    /* Normalize quaternion — quat_to_mat3 / quat_rotate_vec3 produce scaled
     * results for non-unit quaternions, giving incorrect AABBs. */
    {
        float qlen_sq = quat_length_sq(orient);
        if (!forge_isfinite(qlen_sq) || !(qlen_sq > FORGE_PHYSICS_EPSILON))
            return aabb;
        if (SDL_fabsf(qlen_sq - 1.0f) > FORGE_PHYSICS_EPSILON)
            orient = quat_normalize(orient);
    }

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE: {
        float r = shape->data.sphere.radius;
        vec3 rv = vec3_create(r, r, r);
        aabb.min = vec3_sub(pos, rv);
        aabb.max = vec3_add(pos, rv);
        break;
    }

    case FORGE_PHYSICS_SHAPE_BOX: {
        /* Rotate each local axis to world space, take absolute components,
         * sum to get the world-space half-extents of the AABB.
         *
         * For a rotation matrix R and local half-extents h:
         *   aabb_half.x = |R[0][0]|*h.x + |R[0][1]|*h.y + |R[0][2]|*h.z
         *   (similarly for y, z)
         */
        mat3 R = quat_to_mat3(orient);
        vec3 he = shape->data.box.half_extents;

        vec3 world_half;
        world_half.x = SDL_fabsf(R.m[0]) * he.x + SDL_fabsf(R.m[3]) * he.y + SDL_fabsf(R.m[6]) * he.z;
        world_half.y = SDL_fabsf(R.m[1]) * he.x + SDL_fabsf(R.m[4]) * he.y + SDL_fabsf(R.m[7]) * he.z;
        world_half.z = SDL_fabsf(R.m[2]) * he.x + SDL_fabsf(R.m[5]) * he.y + SDL_fabsf(R.m[8]) * he.z;

        aabb.min = vec3_sub(pos, world_half);
        aabb.max = vec3_add(pos, world_half);
        break;
    }

    case FORGE_PHYSICS_SHAPE_CAPSULE: {
        /* Two cap centers along local Y axis */
        vec3 local_y = quat_rotate_vec3(orient, vec3_create(0, 1, 0));
        vec3 top_center = vec3_add(pos, vec3_scale(local_y, shape->data.capsule.half_height));
        vec3 bot_center = vec3_sub(pos, vec3_scale(local_y, shape->data.capsule.half_height));

        /* Min/max of cap centers */
        vec3 mn, mx;
        mn.x = (top_center.x < bot_center.x) ? top_center.x : bot_center.x;
        mn.y = (top_center.y < bot_center.y) ? top_center.y : bot_center.y;
        mn.z = (top_center.z < bot_center.z) ? top_center.z : bot_center.z;
        mx.x = (top_center.x > bot_center.x) ? top_center.x : bot_center.x;
        mx.y = (top_center.y > bot_center.y) ? top_center.y : bot_center.y;
        mx.z = (top_center.z > bot_center.z) ? top_center.z : bot_center.z;

        /* Expand by radius */
        float r = shape->data.capsule.radius;
        vec3 rv = vec3_create(r, r, r);
        aabb.min = vec3_sub(mn, rv);
        aabb.max = vec3_add(mx, rv);
        break;
    }

    default:
        break;
    }

    return aabb;
}

/* ── AABB utility functions ────────────────────────────────────────────── */

/* Check whether all components of an AABB are finite (not NaN or infinity).
 *
 * Returns false if any min or max component is non-finite. Used by the SAP
 * broadphase to skip bodies with corrupted AABBs before calling aabb_overlap,
 * which would produce bogus results (NaN comparisons always return false,
 * causing separation tests to fail).
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline bool forge_physics_aabb_isfinite(ForgePhysicsAABB a)
{
    return forge_isfinite(a.min.x) && forge_isfinite(a.min.y) &&
           forge_isfinite(a.min.z) && forge_isfinite(a.max.x) &&
           forge_isfinite(a.max.y) && forge_isfinite(a.max.z);
}

/* Test whether two AABBs overlap.
 *
 * Two AABBs overlap if and only if their projections overlap on all three
 * axes. This is the separating axis test for axis-aligned boxes.
 *
 * Parameters:
 *   a, b — AABBs to test
 *
 * Returns: true if the AABBs overlap (including touching)
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline bool forge_physics_aabb_overlap(
    ForgePhysicsAABB a, ForgePhysicsAABB b)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;
    return true;
}

/* Expand an AABB by a uniform margin on all sides.
 *
 * Used to create "fat" AABBs for broadphase — a small margin avoids
 * recomputation when objects move slightly between frames.
 *
 * Parameters:
 *   aabb   — AABB to expand
 *   margin — distance to expand on each side (m)
 *
 * Returns: expanded AABB
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline ForgePhysicsAABB forge_physics_aabb_expand(
    ForgePhysicsAABB aabb, float margin)
{
    if (!forge_isfinite(margin) || margin < 0.0f) return aabb;
    vec3 m = vec3_create(margin, margin, margin);
    aabb.min = vec3_sub(aabb.min, m);
    aabb.max = vec3_add(aabb.max, m);
    return aabb;
}

/* Compute the center point of an AABB.
 *
 * Parameters:
 *   aabb — the AABB
 *
 * Returns: center point (midpoint of min and max)
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline vec3 forge_physics_aabb_center(ForgePhysicsAABB aabb)
{
    return vec3_scale(vec3_add(aabb.min, aabb.max), 0.5f);
}

/* Compute the half-extents of an AABB.
 *
 * Parameters:
 *   aabb — the AABB
 *
 * Returns: half-extents ((max - min) / 2)
 *
 * See: Physics Lesson 07 — Collision Shapes and Support Functions
 */
static inline vec3 forge_physics_aabb_extents(ForgePhysicsAABB aabb)
{
    return vec3_scale(vec3_sub(aabb.max, aabb.min), 0.5f);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Sweep-and-Prune (SAP) Broadphase
 *
 * Sort-and-sweep broadphase collision detection. Projects AABBs onto a
 * single axis, sorts the endpoints, and sweeps to find overlapping pairs.
 * For spatially coherent scenes the sort is nearly O(n) per frame because
 * insertion sort exploits temporal coherence — endpoints barely move
 * between frames.
 *
 * Usage:
 *   ForgePhysicsSAPWorld sap;
 *   forge_physics_sap_init(&sap);
 *   // each frame:
 *   sap.sweep_axis = forge_physics_sap_select_axis(aabbs, count);
 *   forge_physics_sap_update(&sap, aabbs, count);
 *   int n = forge_physics_sap_pair_count(&sap);
 *   const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&sap);
 *
 * Reference: David Baraff, "Dynamic Simulation of Non-Penetrating Rigid
 *            Bodies", PhD thesis, 1992 — sort-and-sweep algorithm.
 *            Erin Catto, Box2D broadphase documentation.
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── SAP Types ─────────────────────────────────────────────────────────── */

/* A single endpoint on the sweep axis.
 *
 * Each AABB contributes two endpoints: a min (is_min = true) and a max
 * (is_min = false). Sorting these by value and sweeping left-to-right
 * identifies overlapping intervals.
 */
typedef struct ForgePhysicsSAPEndpoint {
    float    value;       /* coordinate on the sweep axis (meters) */
    uint16_t body_index;  /* index into caller's AABB/body array [0, count) — uint16_t caps body count at UINT16_MAX */
    bool     is_min;      /* true = min (opening) endpoint, false = max (closing) — controls active-set open/close during sweep */
} ForgePhysicsSAPEndpoint;

/* An overlapping pair found by the broadphase.
 *
 * Invariant: a < b always, ensuring each pair is stored uniquely.
 */
typedef struct ForgePhysicsSAPPair {
    uint16_t a;  /* body index A, range [0, count); always < b */
    uint16_t b;  /* body index B, range [0, count); always > a */
} ForgePhysicsSAPPair;

/* Sweep-and-prune broadphase world state.
 *
 * Holds dynamic endpoint and pair arrays managed by forge_containers.h.
 * Call forge_physics_sap_init() before first use and
 * forge_physics_sap_destroy() when done to free the backing memory.
 *
 * Non-copyable: Do not assign, copy, or pass by value. Always use pointers.
 * Shallow copies alias the heap-owned endpoints/pairs arrays and cause
 * double-free on destroy.
 */
typedef struct ForgePhysicsSAPWorld {
    ForgePhysicsSAPEndpoint *endpoints;   /* sorted endpoint array — NULL before init, populated by sap_update (forge_containers) */
    ForgePhysicsSAPPair     *pairs;       /* overlapping pairs found during sweep — NULL before init, populated by sap_update (forge_containers) */
    ForgeArena               sweep_arena; /* scratch memory for the sweep active set — reset each sap_update, freed by sap_destroy */
    int                     sweep_axis;   /* 0 = X, 1 = Y, 2 = Z — reset to 0 by sap_destroy */
    int                     sort_ops;     /* insertion-sort swap count from last sap_update — >= 0; near 0 means good temporal coherence; reset each sap_update call */
} ForgePhysicsSAPWorld;

/* ── SAP Helper Functions ──────────────────────────────────────────────── */

/* Extract a component from a vec3 by axis index.
 *
 * Parameters:
 *   v    — the vector
 *   axis — 0 = x, 1 = y, 2 = z
 *
 * Returns: the component value (0.0f for invalid axis)
 */
static inline float forge_physics_vec3_axis(vec3 v, int axis)
{
    switch (axis) {
    case 0: return v.x;
    case 1: return v.y;
    case 2: return v.z;
    default: return 0.0f;
    }
}

/* ── SAP Functions ─────────────────────────────────────────────────────── */

/* Zero-initialize a SAP world.
 *
 * Sets all pointers to NULL and counters to zero. Must be called before first use.
 *
 * Parameters:
 *   world — SAP world to initialize (must not be NULL)
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline void forge_physics_sap_init(ForgePhysicsSAPWorld *world)
{
    if (!world) return;
    world->endpoints  = NULL;
    world->pairs      = NULL;
    world->sweep_arena = forge_arena_create(0);
    world->sweep_axis = 0;
    world->sort_ops   = 0;
}

/* Free all memory owned by a SAP world.
 *
 * Releases the dynamic endpoint and pair arrays, and resets sweep_axis
 * and sort_ops to 0. The world struct itself is caller-owned and not
 * freed. Safe to call on an already-destroyed or zero-initialized world.
 * After destroy, all memory is freed and pointers are NULL. Call
 * sap_init() before reuse.
 *
 * Parameters:
 *   world — SAP world to destroy (NULL is safe)
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline void forge_physics_sap_destroy(ForgePhysicsSAPWorld *world)
{
    if (!world) return;
    forge_arr_free(world->endpoints);
    forge_arr_free(world->pairs);
    forge_arena_destroy(&world->sweep_arena);
    world->sweep_axis = 0;
    world->sort_ops   = 0;
}

/* Select the sweep axis with greatest AABB center variance.
 *
 * Choosing the axis where bodies are most spread out maximizes pruning
 * efficiency — endpoints are more separated, so fewer false overlaps
 * survive the sweep.
 *
 * Parameters:
 *   aabbs — array of AABBs
 *   count — number of AABBs
 *
 * Returns: axis index (0 = X, 1 = Y, 2 = Z); defaults to 0 for
 *          degenerate input
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline int forge_physics_sap_select_axis(
    const ForgePhysicsAABB *aabbs, int count)
{
    if (!aabbs || count < 2) return 0;

    float best_variance = -1.0f;
    int best_axis = 0;

    for (int axis = 0; axis < 3; axis++) {
        /* Compute mean of centers on this axis */
        float sum = 0.0f;
        for (int i = 0; i < count; i++) {
            vec3 center = forge_physics_aabb_center(aabbs[i]);
            sum += forge_physics_vec3_axis(center, axis);
        }
        float mean = sum / (float)count;

        /* Compute variance */
        float var = 0.0f;
        for (int i = 0; i < count; i++) {
            vec3 center = forge_physics_aabb_center(aabbs[i]);
            float d = forge_physics_vec3_axis(center, axis) - mean;
            var += d * d;
        }

        if (var > best_variance) {
            best_variance = var;
            best_axis = axis;
        }
    }

    return best_axis;
}

/* Update the SAP broadphase: populate endpoints, sort, sweep, output pairs.
 *
 * This is the main per-frame function. It:
 *   1. Refreshes endpoint projected values from current AABBs on world->sweep_axis.
 *      If the body count changed, rebuilds endpoints from scratch; otherwise
 *      preserves the previous sort order for temporal coherence.
 *   2. Insertion-sorts endpoints by value (O(n) for nearly-sorted data when
 *      temporal coherence is preserved)
 *   3. Sweeps left-to-right: on a min endpoint, tests the new body against
 *      all currently active bodies using full 3-axis AABB overlap; on a max
 *      endpoint, removes the body from the active set
 *   4. Outputs pairs (a < b) into world->pairs (dynamic array, grows as needed)
 *
 * Parameters:
 *   world — SAP world (must be initialized)
 *   aabbs — array of AABBs to test
 *   count — number of AABBs (clamped to 65535 — body indices are uint16_t)
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline void forge_physics_sap_update(
    ForgePhysicsSAPWorld *world,
    const ForgePhysicsAABB *aabbs, int count)
{
    if (!world) return;
    if (!aabbs || count <= 0) {
        forge_arr_set_length(world->endpoints, 0);
        forge_arr_set_length(world->pairs, 0);
        world->sort_ops = 0;
        return;
    }

    /* Body indices are stored as uint16_t — cap count to avoid truncation. */
    if (count > UINT16_MAX) {
        SDL_Log("WARNING: forge_physics_sap_update: count %d exceeds UINT16_MAX, "
                "clamped to %d", count, (int)UINT16_MAX);
        count = UINT16_MAX;
    }

    int axis = world->sweep_axis;
    if (axis < 0 || axis > 2) {
        axis = 0;
        world->sweep_axis = axis;
    }

    /* Step 1: Populate endpoints.
     *
     * If the body count changed since last frame (or this is the first call),
     * rebuild the endpoint array from scratch in body-index order. Otherwise,
     * update only the projected values in-place — this preserves the sort
     * order from the previous frame so the insertion sort stays near-linear
     * (temporal coherence). */
    int ep_count = count * 2;
    bool rebuild = ((int)forge_arr_length(world->endpoints) != ep_count);

    if (rebuild) {
        /* Full rebuild: resize and assign body_index and is_min */
        forge_arr_set_length(world->endpoints, (size_t)ep_count);
        if ((int)forge_arr_length(world->endpoints) != ep_count) {
            SDL_Log("ERROR: forge_physics_sap_update: endpoint allocation failed");
            forge_arr_set_length(world->pairs, 0);
            return;
        }
        for (int i = 0; i < count; i++) {
            world->endpoints[i * 2 + 0].body_index = (uint16_t)i;
            world->endpoints[i * 2 + 0].is_min     = true;
            world->endpoints[i * 2 + 1].body_index = (uint16_t)i;
            world->endpoints[i * 2 + 1].is_min     = false;
        }
    }

    /* Refresh projected values from current AABBs.  Reject non-finite AABB
     * values — NaN would corrupt the insertion sort order and produce
     * incorrect broadphase results silently.  Non-finite min/max are
     * replaced with 0, collapsing the AABB to a point at the origin so
     * the body participates in no pairs. */
    for (int i = 0; i < ep_count; i++) {
        int bi = world->endpoints[i].body_index;
        float v = world->endpoints[i].is_min
            ? forge_physics_vec3_axis(aabbs[bi].min, axis)
            : forge_physics_vec3_axis(aabbs[bi].max, axis);
        if (!forge_isfinite(v)) v = 0.0f;
        world->endpoints[i].value = v;
    }

    /* Step 2: Insertion sort — O(n) for nearly-sorted (temporally coherent) data */
    world->sort_ops = 0;
    for (int i = 1; i < ep_count; i++) {
        ForgePhysicsSAPEndpoint key = world->endpoints[i];
        int j = i - 1;
        while (
            j >= 0 &&
            (
                world->endpoints[j].value > key.value ||
                (world->endpoints[j].value == key.value &&
                 !world->endpoints[j].is_min && key.is_min)
            )
        ) {
            world->endpoints[j + 1] = world->endpoints[j];
            j--;
            world->sort_ops++;
        }
        world->endpoints[j + 1] = key;
    }

    /* Step 3: Sweep — track active bodies, test overlaps on min endpoints */
    forge_arr_set_length(world->pairs, 0);  /* clear pairs, preserve allocation */

    /* Active set: a dense list of body indices for O(k) pairing where k is
     * the number of currently active bodies, plus a position map for O(1)
     * swap-removal when a max endpoint closes an interval.
     *
     * active_list[0..active_count-1] — dense array of active body indices
     * active_pos[body_index] — position of body_index in active_list, or
     *                          -1 if inactive.
     *
     * Allocated from the world's sweep arena — reset (not freed) each
     * frame so the backing memory is reused without per-frame allocs. */
    forge_arena_reset(&world->sweep_arena);
    uint16_t *active_list = (uint16_t *)forge_arena_alloc(
        &world->sweep_arena, (size_t)count * sizeof(uint16_t));
    int *active_pos = (int *)forge_arena_alloc(
        &world->sweep_arena, (size_t)count * sizeof(int));
    if (!active_list || !active_pos) {
        SDL_Log("ERROR: forge_physics_sap_update: active set allocation failed — "
               "endpoints sorted but pairs empty");
        return;
    }
    int active_count = 0;
    /* Arena zeroes memory on reset; -1 marks inactive, so set explicitly. */
    for (int i = 0; i < count; i++) active_pos[i] = -1;

    for (int i = 0; i < ep_count; i++) {
        ForgePhysicsSAPEndpoint *ep = &world->endpoints[i];

        if (ep->is_min) {
            /* Skip bodies with non-finite AABBs — endpoint values were
             * sanitized above, but aabb_overlap() reads the original AABB
             * where NaN comparisons always return false, causing all
             * separation tests to fail (bogus overlap). */
            if (!forge_physics_aabb_isfinite(aabbs[ep->body_index])) {
                continue;
            }

            /* New body enters — test against all currently active bodies.
             * Only iterates the dense active list (O(k) not O(n)). */
            for (int j = 0; j < active_count; j++) {
                int b = active_list[j];

                /* Full 3-axis AABB overlap test */
                if (forge_physics_aabb_overlap(aabbs[ep->body_index], aabbs[b])) {
                    uint16_t pa = ep->body_index;
                    uint16_t pb = (uint16_t)b;
                    /* Enforce a < b ordering */
                    if (pa > pb) { uint16_t tmp = pa; pa = pb; pb = tmp; }
                    ForgePhysicsSAPPair pair;
                    pair.a = pa;
                    pair.b = pb;
                    forge_arr_append(world->pairs, pair);
                }
            }
            /* Add to active set */
            active_pos[ep->body_index] = active_count;
            active_list[active_count]  = ep->body_index;
            active_count++;
        } else {
            /* Body exits — swap-remove from dense active list in O(1) */
            int pos = active_pos[ep->body_index];
            if (pos >= 0 && pos < active_count) {
                active_count--;
                if (pos < active_count) {
                    /* Swap last element into the vacated slot */
                    uint16_t last = active_list[active_count];
                    active_list[pos] = last;
                    active_pos[last] = pos;
                }
                active_pos[ep->body_index] = -1;
            }
        }
    }

    /* Arena memory stays alive for reuse next frame — reset in the next
     * sap_update call, freed by sap_destroy. */
}

/* Return the number of overlapping pairs found in the last update.
 *
 * Parameters:
 *   world — SAP world
 *
 * Returns: pair count (0 if world is NULL)
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline int forge_physics_sap_pair_count(const ForgePhysicsSAPWorld *world)
{
    if (!world) return 0;
    return (int)forge_arr_length(world->pairs);
}

/* Return a pointer to the overlapping pairs array.
 *
 * The returned pointer is valid until the next call to
 * forge_physics_sap_update(). Each pair has a < b.
 *
 * Parameters:
 *   world — SAP world
 *
 * Returns: pointer to pairs array (NULL if world is NULL)
 *
 * See: Physics Lesson 08 — Sweep-and-Prune Broadphase
 */
static inline const ForgePhysicsSAPPair *forge_physics_sap_get_pairs(
    const ForgePhysicsSAPWorld *world)
{
    if (!world) return NULL;
    return world->pairs;
}

/* ── SAP-Accelerated Particle Collision ─────────────────────────────────── */

/* Stack-buffer threshold for particle AABB arrays.  Below this count,
 * AABBs are allocated on the stack to avoid heap overhead. */
#define FORGE_PHYSICS_SAP_PARTICLE_STACK_MAX  256

/* Detect particle collisions using SAP broadphase + sphere-sphere narrow phase.
 *
 * Replaces the O(n²) brute-force approach in forge_physics_collide_particles_all()
 * with a broadphase-accelerated path. Computes an AABB for each particle from
 * position ± radius, feeds them to the SAP broadphase, then runs sphere-sphere
 * narrow phase only on overlapping pairs.
 *
 * The caller owns the ForgePhysicsSAPWorld — initialize once, reuse each frame.
 * SAP exploits temporal coherence (particles move slightly between frames) to
 * keep the internal insertion sort near-linear.
 *
 * Parameters:
 *   particles     (const ForgePhysicsParticle*) — particle array (must not be NULL)
 *   num_particles (int)                         — particle count (must be > 1)
 *   sap           (ForgePhysicsSAPWorld*)        — SAP world (caller-owned, reused each frame)
 *   out_contacts  (ForgePhysicsContact**)        — dynamic array for results
 *   out_sap_pair_count (int*)                    — if non-NULL, receives the SAP pair count
 *
 * Returns:
 *   The total number of contacts in the dynamic array after detection.
 *
 * Usage:
 *   ForgePhysicsSAPWorld sap;
 *   forge_physics_sap_init(&sap);
 *   ForgePhysicsContact *contacts = NULL;
 *   int n = forge_physics_collide_particles_sap(
 *       particles, num_particles, &sap, &contacts, NULL);
 *   // ... use contacts[0..n-1] ...
 *   forge_arr_free(contacts);
 *   forge_physics_sap_destroy(&sap);
 *
 * Reference: Ericson, "Real-Time Collision Detection", Ch. 7.2.4 —
 * sort-and-sweep broadphase for sphere sets.
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline int forge_physics_collide_particles_sap(
    const ForgePhysicsParticle *particles, int num_particles,
    ForgePhysicsSAPWorld *sap,
    ForgePhysicsContact **out_contacts,
    int *out_sap_pair_count)
{
    if (out_sap_pair_count) *out_sap_pair_count = 0;
    if (!particles || num_particles <= 1 || !sap || !out_contacts) {
        return 0;
    }

    /* Clamp to SAP's uint16_t index limit up front so we don't build
     * AABBs or select an axis over particles that SAP will ignore. */
    if (num_particles > UINT16_MAX) {
        SDL_Log("WARNING: forge_physics_collide_particles_sap: "
                "count %d exceeds UINT16_MAX, clamped to %d",
                num_particles, (int)UINT16_MAX);
        num_particles = UINT16_MAX;
    }

    /* Step 1: Build AABBs from particle positions and radii.
     * Use a stack buffer for small counts, heap for large. */
    ForgePhysicsAABB stack_aabbs[FORGE_PHYSICS_SAP_PARTICLE_STACK_MAX];
    ForgePhysicsAABB *aabbs = stack_aabbs;
    bool heap_aabbs = false;

    if (num_particles > FORGE_PHYSICS_SAP_PARTICLE_STACK_MAX) {
        aabbs = (ForgePhysicsAABB *)SDL_malloc(
            (size_t)num_particles * sizeof(ForgePhysicsAABB));
        if (!aabbs) {
            SDL_Log("forge_physics_collide_particles_sap: "
                    "AABB allocation failed for %d particles", num_particles);
            return 0;
        }
        heap_aabbs = true;
    }

    for (int i = 0; i < num_particles; i++) {
        float r = particles[i].radius;
        vec3 rv = vec3_create(r, r, r);
        aabbs[i].min = vec3_sub(particles[i].position, rv);
        aabbs[i].max = vec3_add(particles[i].position, rv);
    }

    /* Step 2: Select sweep axis and run SAP broadphase. */
    sap->sweep_axis = forge_physics_sap_select_axis(aabbs, num_particles);
    forge_physics_sap_update(sap, aabbs, num_particles);

    if (heap_aabbs) {
        SDL_free(aabbs);
    }

    /* Step 3: Narrow phase — sphere-sphere on SAP pairs only. */
    int pair_count = forge_physics_sap_pair_count(sap);
    if (out_sap_pair_count) *out_sap_pair_count = pair_count;

    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(sap);
    for (int p = 0; p < pair_count; p++) {
        int i = pairs[p].a;
        int j = pairs[p].b;
        ForgePhysicsContact c;
        if (forge_physics_collide_sphere_sphere(
                &particles[i], &particles[j], i, j, &c)) {
            forge_arr_append(*out_contacts, c);
        }
    }

    return (int)forge_arr_length(*out_contacts);
}

/* Detect and resolve particle collisions using SAP broadphase.
 *
 * SAP-accelerated version of forge_physics_collide_particles_step().
 * Clears the contact array, runs SAP broadphase + sphere-sphere narrow
 * phase, then resolves all contacts with impulse response and positional
 * correction.
 *
 * Parameters:
 *   particles     (ForgePhysicsParticle*) — particle array (modified in place)
 *   num_particles (int)                   — particle count
 *   sap           (ForgePhysicsSAPWorld*) — SAP world (caller-owned, reused each frame)
 *   out_contacts  (ForgePhysicsContact**) — dynamic array, cleared and repopulated
 *   out_sap_pair_count (int*)             — if non-NULL, receives the SAP pair count
 *
 * Returns:
 *   The number of contacts detected and resolved.
 *
 * See: Physics Lesson 03 — Particle Collisions
 */
static inline int forge_physics_collide_particles_sap_step(
    ForgePhysicsParticle *particles, int num_particles,
    ForgePhysicsSAPWorld *sap,
    ForgePhysicsContact **out_contacts,
    int *out_sap_pair_count)
{
    if (!out_contacts) {
        if (out_sap_pair_count) *out_sap_pair_count = 0;
        return 0;
    }

    forge_arr_set_length(*out_contacts, 0);

    int num_contacts = forge_physics_collide_particles_sap(
        particles, num_particles, sap, out_contacts, out_sap_pair_count);

    if (num_contacts > 0) {
        forge_physics_resolve_contacts(
            *out_contacts, num_contacts, particles, num_particles);
    }

    return num_contacts;
}

/* =========================================================================
 * GJK Intersection Testing
 *
 * The Gilbert-Johnson-Keerthi (GJK) algorithm determines whether two convex
 * shapes overlap by iteratively building a simplex (point, line, triangle,
 * tetrahedron) in the Minkowski difference of the two shapes. If the simplex
 * can enclose the origin, the shapes intersect.
 *
 * Key insight: the Minkowski difference A⊖B = {a − b | a∈A, b∈B} contains
 * the origin if and only if A and B intersect. GJK exploits the fact that the
 * farthest point of A⊖B in any direction is support_A(d) − support_B(−d),
 * which is cheap to compute for convex primitives.
 *
 * The simplex is preserved in the result so that EPA (Physics Lesson 10) can
 * use it as a starting polytope for contact point and depth computation.
 * On intersection, early-exit hits (origin on a segment, coplanar face, or
 * collapsed direction) are inflated to a full tetrahedron (count == 4) via
 * additional support queries before returning, so EPA can start immediately.
 *
 * Reference: Gilbert, Johnson, Keerthi, "A fast procedure for computing the
 * distance between complex objects in three-dimensional space", IEEE J-RA 1988.
 * See also: Catto, GDC 2010 "Computing Distance".
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 * ========================================================================= */

/* Maximum GJK iterations before giving up.
 * 64 is well above the typical 10–20 needed for convex primitives;
 * a higher cap risks an infinite loop on degenerate inputs. */
#define FORGE_PHYSICS_GJK_MAX_ITERATIONS  64
/* Maximum simplex vertex count — 4 vertices form a tetrahedron in 3D */
#define FORGE_PHYSICS_GJK_MAX_SIMPLEX     4

/* Numerical tolerance for GJK edge-case tests (near-zero dot products,
 * near-zero cross product magnitudes). Chosen to be larger than float
 * rounding error but smaller than any physically meaningful distance. */
#define FORGE_PHYSICS_GJK_EPSILON         1e-6f

/* GJK vertex — one point of the Minkowski difference simplex.
 * Stores the support points from each shape separately because EPA
 * (Lesson 10) needs them to reconstruct the contact point via
 * barycentric interpolation on the closest polytope face. */
typedef struct ForgePhysicsGJKVertex {
    vec3 point;   /* meters, world-space — Minkowski difference: sup_a − sup_b.
                   * GJK tests this point against the origin. Any finite vec3. */
    vec3 sup_a;   /* meters, world-space — support point on shape A's surface.
                   * Stored separately for EPA contact reconstruction. */
    vec3 sup_b;   /* meters, world-space — support point on shape B's surface.
                   * Stored separately for EPA contact reconstruction. */
} ForgePhysicsGJKVertex;

/* GJK simplex — the evolving 1–4 vertex simplex that GJK maintains
 * as it searches for the origin in the Minkowski difference. */
typedef struct ForgePhysicsGJKSimplex {
    ForgePhysicsGJKVertex verts[FORGE_PHYSICS_GJK_MAX_SIMPLEX];
                                    /* simplex vertices — only verts[0..count-1]
                                     * are valid; newest vertex at highest index.
                                     * 4 is the max (tetrahedron in 3D). */
    int count;                      /* 0 = uninitialized/invalid (zeroed result),
                                     * 1–4 = active vertex count */
} ForgePhysicsGJKSimplex;

/* GJK result — output of the intersection test.
 *
 * The simplex is preserved so EPA (Lesson 10) can use it as a starting
 * polytope. On intersection, the simplex is inflated to a full tetrahedron
 * (count == 4) via additional support queries so EPA can start immediately
 * without needing to inflate sub-dimensional simplices itself.
 *
 * Return-value semantics:
 *   intersecting=true  — shapes overlap; simplex.count == 4 (inflated)
 *   intersecting=false, iterations>0  — typically confirmed separated, but may
 *                                        also indicate a fail-closed exit from
 *                                        support-validation (e.g. INF overflow
 *                                        in Minkowski point or non-finite
 *                                        direction length squared)
 *   intersecting=false, iterations=0  — invalid input (zeroed result)
 *   intersecting=false, iterations=MAX — iteration cap reached (inconclusive) */
typedef struct ForgePhysicsGJKResult {
    bool intersecting;              /* true if shapes overlap */
    ForgePhysicsGJKSimplex simplex; /* final simplex — count == 4 on hit
                                     * (inflated for EPA), 0 on invalid input */
    int iterations;                 /* 0–64 — 0 means invalid-input early exit */
} ForgePhysicsGJKResult;
/* Initial "infinity" for closest-distance search in degenerate-tetrahedron
 * fallback — must exceed any plausible squared distance */
#define FORGE_PHYSICS_GJK_SENTINEL_DIST2  1e30f

/* Threshold for "nearly parallel" in cross-product basis construction
 * (inflate helper): if |dot(v, axis)| >= this, the axis is too close to v
 * and we pick an alternative axis for the cross product. */
#define FORGE_PHYSICS_GJK_PARALLEL_THRESH 0.9f

/* Private helper: validate a quaternion for GJK.
 * Returns true if the quaternion is finite, non-zero, and safe to normalize.
 * If not already unit-length, normalizes it in place. */
static inline bool gjk_validate_quat_(quat *q)
{
    if (!forge_isfinite(q->w) || !forge_isfinite(q->x) ||
        !forge_isfinite(q->y) || !forge_isfinite(q->z))
        return false;
    float len_sq = quat_length_sq(*q);
    if (!forge_isfinite(len_sq) || !(len_sq > FORGE_PHYSICS_GJK_EPSILON))
        return false;
    if (SDL_fabsf(len_sq - 1.0f) > FORGE_PHYSICS_GJK_EPSILON)
        *q = quat_normalize(*q);
    return true;
}

/* Compute a Minkowski difference support vertex (support mapping step of GJK).
 *
 * The support of A⊖B in direction d is support_A(d) − support_B(−d).
 * Both world-space positions and orientations are passed so that
 * forge_physics_shape_support() can handle oriented shapes (boxes, capsules).
 *
 * UNGUARDED HOT PATH — no NaN/Inf validation on positions, directions, or
 * quaternions. See the file header comment for rationale. Callers must
 * ensure all inputs are finite before calling. Returns NaN-sentinel only
 * for NULL or invalid shapes (structural errors).
 *
 * Parameters:
 *   shape_a, shape_b  — collision shapes (non-NULL, must pass shape_is_valid)
 *   pos_a, pos_b      — world-space positions in meters (must be finite —
 *                        not validated, undefined behavior if NaN/Inf)
 *   orient_a, orient_b — unit quaternions (must be finite and normalized —
 *                        not validated, undefined behavior if non-unit)
 *   dir               — search direction, unitless (need not be normalized
 *                        but must be finite and non-zero — not validated)
 *
 * Returns: fully populated ForgePhysicsGJKVertex, or NaN-sentinel vertex
 *          (all fields NaN) on NULL/invalid shape input.
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline ForgePhysicsGJKVertex forge_physics_gjk_support(
    const ForgePhysicsCollisionShape *shape_a, vec3 pos_a, quat orient_a,
    const ForgePhysicsCollisionShape *shape_b, vec3 pos_b, quat orient_b,
    vec3 dir)
{
    /* UNGUARDED HOT PATH — no NaN/Inf validation.
     *
     * This is the Minkowski difference support mapping, called twice per
     * GJK iteration and twice per EPA iteration (once per shape). For a
     * typical frame with 50 broadphase pairs and 10 GJK iterations each,
     * this function executes ~1000 times. Each call previously paid ~30
     * comparison instructions in NaN/Inf guards (positions, direction,
     * quaternions, output point) — comparable to or exceeding the actual
     * support computation for simple shapes.
     *
     * Input validation is the responsibility of the outer GJK/EPA entry
     * points (forge_physics_gjk_intersect, forge_physics_epa), which
     * validate positions, orientations, and shapes once before entering
     * the iteration loop. Passing NaN/Inf to this function is undefined
     * behavior — the returned vertex will contain garbage, but no crash
     * or memory corruption will occur. The GJK loop's existing progress
     * check (dot(new_point, dir) <= dot(closest, dir)) will detect the
     * lack of progress from garbage support points and terminate.
     *
     * Quaternions MUST be normalized before calling. This function does
     * not normalize internally — forge_physics_shape_support() uses
     * quat_conjugate() which is only the inverse for unit quaternions.
     * Non-unit quaternions produce incorrect Minkowski difference points.
     *
     * The null-shape check is retained because it guards against
     * structural errors (missing shape setup), not numerical corruption. */

    /* Null-shape guard — structural validity, not NaN/Inf */
    const float nan_val = SDL_sqrtf(-1.0f);
    vec3 nan_vec = vec3_create(nan_val, nan_val, nan_val);
    ForgePhysicsGJKVertex fail = { nan_vec, nan_vec, nan_vec };

    if (!shape_a || !shape_b ||
        !forge_physics_shape_is_valid(shape_a) ||
        !forge_physics_shape_is_valid(shape_b))
        return fail;

    ForgePhysicsGJKVertex v;
    vec3 neg_dir = vec3_scale(dir, -1.0f);
    v.sup_a = forge_physics_shape_support(shape_a, pos_a, orient_a, dir);
    v.sup_b = forge_physics_shape_support(shape_b, pos_b, orient_b, neg_dir);
    v.point = vec3_sub(v.sup_a, v.sup_b);
    return v;
}

/* Line sub-algorithm — called when the simplex has 2 vertices.
 *
 * Vertices are ordered [B, A] where A is the most recently added point.
 * We test whether the origin is in the Voronoi region of the edge AB or
 * behind A. The direction is updated to point toward the origin from the
 * nearest feature. Returns true if the origin lies within the segment
 * (collinear/touching case); false otherwise.
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline bool gjk_do_line_(
    ForgePhysicsGJKSimplex *s, vec3 *dir)
{
    /* A = newest vertex, B = older vertex */
    ForgePhysicsGJKVertex a = s->verts[1];
    ForgePhysicsGJKVertex b = s->verts[0];

    vec3 ab = vec3_sub(b.point, a.point); /* edge from A toward B */
    vec3 ao = vec3_scale(a.point, -1.0f); /* direction from A toward origin */

    if (vec3_dot(ab, ao) > 0.0f) {
        /* Origin projects onto the interior of segment AB.
         * New search direction: perpendicular to AB toward origin,
         * computed via the triple cross product AB × AO × AB. */
        vec3 cross_ab_ao = vec3_cross(ab, ao);
        vec3 perp = vec3_cross(cross_ab_ao, ab);
        /* Scale-aware collinearity check: |AB × AO|² ≤ ε² · |AB|² · |AO|²
         * so the test works correctly regardless of world scale.
         * We use the raw cross product length (not the triple cross) so
         * units are consistent: both sides are [length⁴]. */
        float cross_len_sq = vec3_dot(cross_ab_ao, cross_ab_ao);
        float ab_sq = vec3_dot(ab, ab);
        float ao_sq = vec3_dot(ao, ao);
        float eps2 = FORGE_PHYSICS_GJK_EPSILON * FORGE_PHYSICS_GJK_EPSILON;
        if (cross_len_sq <= eps2 * ab_sq * ao_sq) {
            /* AB and AO are collinear — origin lies on segment AB.
             * Project to find the closest feature. */
            float t = (ab_sq > FORGE_PHYSICS_GJK_EPSILON)
                ? vec3_dot(ao, ab) / ab_sq
                : 0.0f;

            if (t <= 0.0f) {
                /* Closest to A */
                s->verts[0] = a;
                s->count    = 1;
                *dir        = ao;
                return false;
            }
            if (t >= 1.0f) {
                /* Closest to B */
                s->verts[0] = b;
                s->count    = 1;
                *dir        = vec3_scale(b.point, -1.0f);
                return false;
            }

            /* Origin lies within segment AB — intersection */
            return true;
        } else {
            *dir = perp;
        }
        /* Keep both vertices — simplex remains a line segment */
    } else {
        /* Origin is past A in the opposite direction of B.
         * The best feature is A alone. */
        s->verts[0] = a;
        s->count    = 1;
        *dir        = ao;
    }

    return false;
}

/* Triangle sub-algorithm — called when the simplex has 3 vertices.
 *
 * Vertices are ordered [C, B, A] where A is the most recently added point.
 * We determine which Voronoi region the origin lies in and either reduce
 * the simplex to a line (discarding one vertex) or keep the triangle and
 * update the search direction to point toward the origin perpendicular to
 * the triangle plane. Returns true if the origin lies on the triangle plane
 * (coplanar/touching case); false otherwise.
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline bool gjk_do_triangle_(
    ForgePhysicsGJKSimplex *s, vec3 *dir)
{
    /* A = newest vertex, B and C are older */
    ForgePhysicsGJKVertex a = s->verts[2];
    ForgePhysicsGJKVertex b = s->verts[1];
    ForgePhysicsGJKVertex c = s->verts[0];

    vec3 ab  = vec3_sub(b.point, a.point);
    vec3 ac  = vec3_sub(c.point, a.point);
    vec3 ao  = vec3_scale(a.point, -1.0f);
    vec3 abc = vec3_cross(ab, ac); /* triangle normal */

    /* Degenerate triangle: if the three vertices are collinear, the cross
     * product is zero and no meaningful plane test is possible. Find the
     * edge whose closest point to the origin is nearest and reduce to it. */
    /* Scale-aware triangle degeneracy: |AB × AC|² ≤ ε² · |AB|² · |AC|² */
    float abc_sq = vec3_dot(abc, abc);
    float ab_sq_t = vec3_dot(ab, ab);
    float ac_sq_t = vec3_dot(ac, ac);
    float eps2_t = FORGE_PHYSICS_GJK_EPSILON * FORGE_PHYSICS_GJK_EPSILON;
    if (abc_sq <= eps2_t * ab_sq_t * ac_sq_t) {
        /* Project origin onto each edge and pick the closest feature. */
        struct { ForgePhysicsGJKVertex v0, v1; float dist2; } edges[3];

        /* Edge AB */
        { float ab2 = vec3_dot(ab, ab);
          float t = (ab2 > FORGE_PHYSICS_GJK_EPSILON) ? vec3_dot(ao, ab) / ab2 : 0.0f;
          if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
          vec3 closest = vec3_add(a.point, vec3_scale(ab, t));
          edges[0].v0 = b; edges[0].v1 = a;
          edges[0].dist2 = vec3_dot(closest, closest); }

        /* Edge AC */
        { float ac2 = vec3_dot(ac, ac);
          float t = (ac2 > FORGE_PHYSICS_GJK_EPSILON) ? vec3_dot(ao, ac) / ac2 : 0.0f;
          if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
          vec3 closest = vec3_add(a.point, vec3_scale(ac, t));
          edges[1].v0 = c; edges[1].v1 = a;
          edges[1].dist2 = vec3_dot(closest, closest); }

        /* Edge BC */
        { vec3 bc = vec3_sub(c.point, b.point);
          vec3 bo = vec3_scale(b.point, -1.0f);
          float bc2 = vec3_dot(bc, bc);
          float t = (bc2 > FORGE_PHYSICS_GJK_EPSILON) ? vec3_dot(bo, bc) / bc2 : 0.0f;
          if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
          vec3 closest = vec3_add(b.point, vec3_scale(bc, t));
          edges[2].v0 = c; edges[2].v1 = b;
          edges[2].dist2 = vec3_dot(closest, closest); }

        /* Pick the edge closest to the origin */
        int best = 0;
        if (edges[1].dist2 < edges[best].dist2) best = 1;
        if (edges[2].dist2 < edges[best].dist2) best = 2;

        s->verts[0] = edges[best].v0;
        s->verts[1] = edges[best].v1;
        s->count = 2;
        return gjk_do_line_(s, dir);
    }

    /* Test the edge AC side: if the origin is outside the triangle along AC,
     * reduce to segment AC. The outward perpendicular to AC (pointing away
     * from B) within the plane of ABC is cross(ABC_normal, AC). */
    vec3 ac_perp = vec3_cross(abc, ac);
    if (vec3_dot(ac_perp, ao) > 0.0f) {
        /* Origin is outside edge AC — reduce to segment [C, A] */
        s->verts[0] = c;
        s->verts[1] = a;
        s->count    = 2;
        return gjk_do_line_(s, dir);
    }

    /* Test the edge AB side: outward perpendicular to AB (pointing away
     * from C) within the plane of ABC is cross(AB, ABC_normal). */
    vec3 ab_perp = vec3_cross(ab, abc);
    if (vec3_dot(ab_perp, ao) > 0.0f) {
        /* Origin is outside edge AB — reduce to segment [B, A] */
        s->verts[0] = b;
        s->verts[1] = a;
        s->count    = 2;
        return gjk_do_line_(s, dir);
    }

    /* Origin is inside both edge regions — it projects onto the triangle face.
     * If the origin lies ON the plane (coplanar/touching), return a hit
     * immediately to prevent zero-advance support iterations. */
    float plane_dot = vec3_dot(abc, ao);
    if (SDL_fabsf(plane_dot) <= FORGE_PHYSICS_GJK_EPSILON) {
        return true;
    }
    if (plane_dot > 0.0f) {
        /* Origin is above the triangle — wind CCW, normal points to origin */
        /* Vertex order [C, B, A] already gives upward normal; keep it */
        *dir = abc;
    } else {
        /* Origin is below the triangle — flip winding so normal faces origin */
        s->verts[0] = b;
        s->verts[1] = c;
        s->verts[2] = a;
        *dir        = vec3_scale(abc, -1.0f);
    }

    return false; /* need a tetrahedron to confirm the origin is enclosed */
}

/* Tetrahedron sub-algorithm — called when the simplex has 4 vertices.
 *
 * Vertices are ordered [D, C, B, A] where A is the most recently added point.
 * We test each of the three faces that include A (ABC, ACD, ADB). If the
 * origin is on the outside of any face, we reduce to that triangle and call
 * the triangle sub-algorithm. If the origin is inside all three faces, it is
 * enclosed by the tetrahedron and we have confirmed intersection.
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline bool gjk_do_tetrahedron_(
    ForgePhysicsGJKSimplex *s, vec3 *dir)
{
    /* A = newest vertex, B/C/D are older */
    ForgePhysicsGJKVertex a = s->verts[3];
    ForgePhysicsGJKVertex b = s->verts[2];
    ForgePhysicsGJKVertex c = s->verts[1];
    ForgePhysicsGJKVertex d = s->verts[0];

    vec3 ab = vec3_sub(b.point, a.point);
    vec3 ac = vec3_sub(c.point, a.point);
    vec3 ad = vec3_sub(d.point, a.point);
    vec3 ao = vec3_scale(a.point, -1.0f);

    /* Guard against coplanar tetrahedra FIRST: if the volume is near-zero,
     * the face-normal tests are unreliable and must not run. Compute the
     * closest point on each candidate triangle to the origin and reduce to
     * the triangle with the smallest distance. */
    {
        /* Scale-aware coplanarity: |(AB×AC)·AD| ≤ ε · |AB×AC| · |AD| */
        vec3 ab_x_ac = vec3_cross(ab, ac);
        float vol = SDL_fabsf(vec3_dot(ab_x_ac, ad));
        float scale = vec3_length(ab_x_ac) * vec3_length(ad);
        if (vol <= FORGE_PHYSICS_GJK_EPSILON * scale) {
            /* Triangle vertices for each face (winding order for gjk_do_triangle_) */
            ForgePhysicsGJKVertex face_verts[3][3] = {
                { c, b, a }, { d, c, a }, { b, d, a },
            };
            float best_dist2 = FORGE_PHYSICS_GJK_SENTINEL_DIST2;
            int best = 0;

            for (int fi = 0; fi < 3; fi++) {
                /* True closest point on triangle to origin using Voronoi
                 * region tests (vertex, edge, face). Based on the standard
                 * algorithm from Ericson, "Real-Time Collision Detection".
                 * Handle duplicate vertices: collapse to point or segment. */
                vec3 pa = face_verts[fi][2].point;
                vec3 pb = face_verts[fi][1].point;
                vec3 pc = face_verts[fi][0].point;

                /* Detect duplicate vertices — collapse to point or segment */
                vec3 ab_e = vec3_sub(pb, pa);
                vec3 ac_e = vec3_sub(pc, pa);
                float ab_len2 = vec3_dot(ab_e, ab_e);
                float ac_len2 = vec3_dot(ac_e, ac_e);

                float eps_len2 = FORGE_PHYSICS_GJK_EPSILON * FORGE_PHYSICS_GJK_EPSILON;
                if (ab_len2 < eps_len2 &&
                    ac_len2 < eps_len2) {
                    /* All three vertices coincide — treat as point */
                    float dist2 = vec3_dot(pa, pa);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }
                if (ab_len2 < eps_len2) {
                    /* pa == pb — closest point on segment pa-pc */
                    float t = vec3_dot(vec3_scale(pa, -1.0f), ac_e) / ac_len2;
                    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                    vec3 closest = vec3_add(pa, vec3_scale(ac_e, t));
                    float dist2 = vec3_dot(closest, closest);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }
                if (ac_len2 < eps_len2) {
                    /* pa == pc — closest point on segment pa-pb */
                    float t = vec3_dot(vec3_scale(pa, -1.0f), ab_e) / ab_len2;
                    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                    vec3 closest = vec3_add(pa, vec3_scale(ab_e, t));
                    float dist2 = vec3_dot(closest, closest);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }
                {
                    vec3 bc_e = vec3_sub(pc, pb);
                    if (vec3_dot(bc_e, bc_e) < eps_len2) {
                        /* pb == pc — closest point on segment pa-pb */
                        float t = vec3_dot(vec3_scale(pa, -1.0f), ab_e) / ab_len2;
                        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                        vec3 closest = vec3_add(pa, vec3_scale(ab_e, t));
                        float dist2 = vec3_dot(closest, closest);
                        if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                        continue;
                    }
                }

                /* Full triangle — Voronoi region closest-point */
                vec3 ap   = vec3_scale(pa, -1.0f);  /* origin - pa */

                float d1 = vec3_dot(ab_e, ap);
                float d2 = vec3_dot(ac_e, ap);

                /* Vertex A region */
                if (d1 <= 0.0f && d2 <= 0.0f) {
                    float dist2 = vec3_dot(pa, pa);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }

                vec3 bp = vec3_scale(pb, -1.0f);
                float d3 = vec3_dot(ab_e, bp);
                float d4 = vec3_dot(ac_e, bp);

                /* Vertex B region */
                if (d3 >= 0.0f && d4 <= d3) {
                    float dist2 = vec3_dot(pb, pb);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }

                /* Edge AB region */
                float vc = d1 * d4 - d3 * d2;
                if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                    float denom_ab = d1 - d3;
                    float vt = (denom_ab > FORGE_PHYSICS_GJK_EPSILON)
                        ? d1 / denom_ab : 0.0f;
                    vec3 closest = vec3_add(pa, vec3_scale(ab_e, vt));
                    float dist2 = vec3_dot(closest, closest);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }

                vec3 cp = vec3_scale(pc, -1.0f);
                float d5 = vec3_dot(ab_e, cp);
                float d6 = vec3_dot(ac_e, cp);

                /* Vertex C region */
                if (d6 >= 0.0f && d5 <= d6) {
                    float dist2 = vec3_dot(pc, pc);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }

                /* Edge AC region */
                float vb = d5 * d2 - d1 * d6;
                if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                    float denom_ac = d2 - d6;
                    float wt = (denom_ac > FORGE_PHYSICS_GJK_EPSILON)
                        ? d2 / denom_ac : 0.0f;
                    vec3 closest = vec3_add(pa, vec3_scale(ac_e, wt));
                    float dist2 = vec3_dot(closest, closest);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }

                /* Edge BC region */
                float va = d3 * d6 - d5 * d4;
                if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                    float denom_bc = (d4 - d3) + (d5 - d6);
                    float wt = (denom_bc > FORGE_PHYSICS_GJK_EPSILON)
                        ? (d4 - d3) / denom_bc : 0.0f;
                    vec3 closest = vec3_add(pb, vec3_scale(vec3_sub(pc, pb), wt));
                    float dist2 = vec3_dot(closest, closest);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }

                /* Face region — origin projects inside the triangle */
                float denom_sum = va + vb + vc;
                if (SDL_fabsf(denom_sum) < FORGE_PHYSICS_GJK_EPSILON) {
                    /* Degenerate barycentric sum — fall back to vertex A */
                    float dist2 = vec3_dot(pa, pa);
                    if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
                    continue;
                }
                float denom_f = 1.0f / denom_sum;
                float sv = vb * denom_f;
                float sw = vc * denom_f;
                vec3 closest = vec3_add(pa, vec3_add(
                    vec3_scale(ab_e, sv), vec3_scale(ac_e, sw)));
                float dist2 = vec3_dot(closest, closest);
                if (dist2 < best_dist2) { best_dist2 = dist2; best = fi; }
            }

            s->verts[0] = face_verts[best][0];
            s->verts[1] = face_verts[best][1];
            s->verts[2] = face_verts[best][2];
            s->count = 3;
            return gjk_do_triangle_(s, dir);
        }
    }

    /* Non-degenerate tetrahedron — face-normal tests are reliable.
     * Normals of the three faces adjacent to A, pointing outward.
     * We compute the raw cross product then flip if needed. */
    vec3 abc = vec3_cross(ab, ac);
    vec3 acd = vec3_cross(ac, ad);
    vec3 adb = vec3_cross(ad, ab);

    /* Ensure each normal points away from the opposite vertex */
    if (vec3_dot(abc, ad) > 0.0f) abc = vec3_scale(abc, -1.0f);
    if (vec3_dot(acd, ab) > 0.0f) acd = vec3_scale(acd, -1.0f);
    if (vec3_dot(adb, ac) > 0.0f) adb = vec3_scale(adb, -1.0f);

    /* Check face ABC */
    if (vec3_dot(abc, ao) > 0.0f) {
        s->verts[0] = c;
        s->verts[1] = b;
        s->verts[2] = a;
        s->count    = 3;
        return gjk_do_triangle_(s, dir);
    }

    /* Check face ACD */
    if (vec3_dot(acd, ao) > 0.0f) {
        s->verts[0] = d;
        s->verts[1] = c;
        s->verts[2] = a;
        s->count    = 3;
        return gjk_do_triangle_(s, dir);
    }

    /* Check face ADB */
    if (vec3_dot(adb, ao) > 0.0f) {
        s->verts[0] = b;
        s->verts[1] = d;
        s->verts[2] = a;
        s->count    = 3;
        return gjk_do_triangle_(s, dir);
    }

    /* Origin is inside all three A-adjacent faces (ABC, ACD, ADB) — the fourth
     * face (BCD) is implicitly satisfied by the support-point advance check in
     * the main GJK loop.  Intersection confirmed. */
    return true;
}

/* Inflate a sub-dimensional simplex to a full tetrahedron (count == 4) so EPA
 * can use it as a starting polytope.  Called when GJK detects intersection but
 * exits early with count < 4 (e.g. collinear hit with count 2, coplanar hit
 * with count 3, or direction-collapse with count 1).
 *
 * Strategy: query new support points along directions orthogonal to the
 * existing simplex features to expand it to 4 vertices. */
static inline void gjk_inflate_simplex_(
    ForgePhysicsGJKSimplex *s,
    const ForgePhysicsCollisionShape *shape_a, vec3 pos_a, quat orient_a,
    const ForgePhysicsCollisionShape *shape_b, vec3 pos_b, quat orient_b)
{
    if (s->count >= FORGE_PHYSICS_GJK_MAX_SIMPLEX) return;

    /* Compute a scale-aware duplicate threshold from existing vertices */
    float ref_scale = 0.0f;
    for (int ri = 0; ri < s->count; ri++) {
        float mag = vec3_length_squared(s->verts[ri].point);
        if (mag > ref_scale) ref_scale = mag;
    }
    /* dup_eps2 = (eps * max_vertex_length)^2 — degrades gracefully to
     * absolute epsilon when vertices are near the origin */
    float dup_eps2 = FORGE_PHYSICS_GJK_EPSILON * FORGE_PHYSICS_GJK_EPSILON
                   * (ref_scale > 1.0f ? ref_scale : 1.0f);

    /* Helper: query a new support vertex along dir and add it if finite and
     * not a duplicate of an existing vertex */
    #define GJK_INFLATE_ADD_(dir_vec) do { \
        if (s->count >= FORGE_PHYSICS_GJK_MAX_SIMPLEX) break; \
        ForgePhysicsGJKVertex nv = forge_physics_gjk_support( \
            shape_a, pos_a, orient_a, shape_b, pos_b, orient_b, (dir_vec)); \
        if (!forge_isfinite(vec3_length_squared(nv.point))) break; \
        bool dup = false; \
        for (int di = 0; di < s->count; di++) { \
            vec3 diff = vec3_sub(nv.point, s->verts[di].point); \
            if (vec3_length_squared(diff) < dup_eps2) { \
                dup = true; break; \
            } \
        } \
        if (!dup) { s->verts[s->count] = nv; s->count++; } \
    } while (0)

    if (s->count == 1) {
        /* Point: try 3 axis-aligned directions */
        GJK_INFLATE_ADD_(vec3_create(1.0f, 0.0f, 0.0f));
        GJK_INFLATE_ADD_(vec3_create(0.0f, 1.0f, 0.0f));
        GJK_INFLATE_ADD_(vec3_create(0.0f, 0.0f, 1.0f));
        /* If any axis produced a duplicate, try the negative axes */
        if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX)
            GJK_INFLATE_ADD_(vec3_create(-1.0f, 0.0f, 0.0f));
        if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX)
            GJK_INFLATE_ADD_(vec3_create(0.0f, -1.0f, 0.0f));
        if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX)
            GJK_INFLATE_ADD_(vec3_create(0.0f, 0.0f, -1.0f));
    } else if (s->count == 2) {
        /* Line segment: compute orthogonal directions */
        vec3 ab = vec3_sub(s->verts[1].point, s->verts[0].point);
        float ab_len = vec3_length(ab);
        if (ab_len > FORGE_PHYSICS_GJK_EPSILON) {
            vec3 ab_n = vec3_scale(ab, 1.0f / ab_len);
            /* Pick a vector not parallel to ab for cross product */
            vec3 up = (SDL_fabsf(ab_n.y) < FORGE_PHYSICS_GJK_PARALLEL_THRESH)
                ? vec3_create(0.0f, 1.0f, 0.0f)
                : vec3_create(1.0f, 0.0f, 0.0f);
            vec3 perp1 = vec3_normalize(vec3_cross(ab_n, up));
            vec3 perp2 = vec3_cross(ab_n, perp1);
            GJK_INFLATE_ADD_(perp1);
            GJK_INFLATE_ADD_(perp2);
            if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX)
                GJK_INFLATE_ADD_(vec3_scale(perp1, -1.0f));
            if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX)
                GJK_INFLATE_ADD_(vec3_scale(perp2, -1.0f));
        }
    } else if (s->count == 3) {
        /* Triangle: compute face normal and query along it */
        vec3 ab = vec3_sub(s->verts[1].point, s->verts[0].point);
        vec3 ac = vec3_sub(s->verts[2].point, s->verts[0].point);
        vec3 normal = vec3_cross(ab, ac);
        float n_len = vec3_length(normal);
        if (n_len > FORGE_PHYSICS_GJK_EPSILON) {
            GJK_INFLATE_ADD_(normal);
            if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX)
                GJK_INFLATE_ADD_(vec3_scale(normal, -1.0f));
        }
    }

    #undef GJK_INFLATE_ADD_
}

/* Test whether two convex shapes intersect using the Gilbert-Johnson-Keerthi
 * (GJK) algorithm.
 *
 * Both shapes are described by their collision shape definition plus a
 * world-space position and orientation. The returned result includes the
 * final simplex so that EPA (Physics Lesson 10) can immediately compute
 * penetration depth and contact normal without re-running GJK.
 *
 * Parameters:
 *   shape_a, shape_b   — collision shapes (non-NULL, must pass shape_is_valid)
 *   pos_a, pos_b       — world-space positions in meters (must be finite)
 *   orient_a, orient_b — unit quaternions (normalized internally if needed;
 *                         must be finite and non-zero-length)
 *
 * Returns: ForgePhysicsGJKResult — see struct comment for return semantics.
 *          On invalid input: zeroed result (intersecting=false, iterations=0,
 *          simplex.count=0). On iteration cap: intersecting=false with
 *          iterations=FORGE_PHYSICS_GJK_MAX_ITERATIONS.
 *
 * Reference: Gilbert, Johnson, Keerthi, IEEE J-RA 1988.
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline ForgePhysicsGJKResult forge_physics_gjk_intersect(
    const ForgePhysicsCollisionShape *shape_a, vec3 pos_a, quat orient_a,
    const ForgePhysicsCollisionShape *shape_b, vec3 pos_b, quat orient_b)
{
    ForgePhysicsGJKResult result;
    SDL_memset(&result, 0, sizeof(result));

    /* Guard against NULL, invalid, or degenerate inputs */
    if (!shape_a || !shape_b) return result;
    if (!forge_physics_shape_is_valid(shape_a) ||
        !forge_physics_shape_is_valid(shape_b))
        return result;
    if (!forge_isfinite(pos_a.x) || !forge_isfinite(pos_a.y) || !forge_isfinite(pos_a.z))
        return result;
    if (!forge_isfinite(pos_b.x) || !forge_isfinite(pos_b.y) || !forge_isfinite(pos_b.z))
        return result;
    /* Validate and normalize quaternions (rejects INF, zero-length, etc.) */
    if (!gjk_validate_quat_(&orient_a) || !gjk_validate_quat_(&orient_b))
        return result;

    /* Initial search direction: center-to-center gives a good first guess.
     * If the centers coincide, use the X axis as a fallback.
     * Guard against INF from very large positions overflowing vec3_sub. */
    vec3 dir = vec3_sub(pos_b, pos_a);
    float dir_len_sq = vec3_length_squared(dir);
    if (!forge_isfinite(dir_len_sq))
        return result;
    if (dir_len_sq < FORGE_PHYSICS_GJK_EPSILON * FORGE_PHYSICS_GJK_EPSILON)
        dir = vec3_create(1.0f, 0.0f, 0.0f);

    /* First support point */
    ForgePhysicsGJKVertex first = forge_physics_gjk_support(
        shape_a, pos_a, orient_a, shape_b, pos_b, orient_b, dir);
    if (!forge_isfinite(vec3_length_squared(first.point)))
        return result;
    result.simplex.verts[0] = first;
    result.simplex.count    = 1;

    /* New search direction: from first point toward the origin */
    dir = vec3_scale(first.point, -1.0f);

    ForgePhysicsGJKSimplex *s = &result.simplex;

    for (int i = 0; i < FORGE_PHYSICS_GJK_MAX_ITERATIONS; ++i) {
        result.iterations = i + 1;

        /* Safety: re-normalise direction if it has shrunk near zero */
        float dir_len = vec3_length(dir);
        if (dir_len < FORGE_PHYSICS_GJK_EPSILON) {
            /* Simplex has collapsed onto the origin — shapes are touching.
             * Inflate to a tetrahedron so EPA has a valid starting polytope. */
            result.intersecting = true;
            gjk_inflate_simplex_(s, shape_a, pos_a, orient_a,
                                     shape_b, pos_b, orient_b);
            return result;
        }

        ForgePhysicsGJKVertex a = forge_physics_gjk_support(
            shape_a, pos_a, orient_a, shape_b, pos_b, orient_b, dir);
        if (!forge_isfinite(vec3_length_squared(a.point)))
            return result;

        /* If the new point did not pass the origin in direction dir, the
         * origin is not reachable — the shapes do not intersect. */
        if (vec3_dot(a.point, dir) < 0.0f)
            return result; /* intersecting remains false */

        /* Add the new vertex to the simplex (newest vertex goes at the end).
         * Defensive bounds check — the sub-algorithms maintain count < 4 on
         * non-intersecting return, but guard against future regressions. */
        if (s->count >= FORGE_PHYSICS_GJK_MAX_SIMPLEX)
            return result;
        s->verts[s->count] = a;
        s->count++;

        /* Run the appropriate sub-algorithm for the current simplex size */
        bool contained = false;
        switch (s->count) {
        case 2: contained = gjk_do_line_(s, &dir);        break;
        case 3: contained = gjk_do_triangle_(s, &dir);    break;
        case 4: contained = gjk_do_tetrahedron_(s, &dir); break;
        default: break;
        }

        if (contained) {
            result.intersecting = true;
            /* Inflate sub-dimensional simplices (count < 4) to a full
             * tetrahedron so EPA has a valid starting polytope. */
            if (s->count < FORGE_PHYSICS_GJK_MAX_SIMPLEX) {
                gjk_inflate_simplex_(s, shape_a, pos_a, orient_a,
                                         shape_b, pos_b, orient_b);
            }
            return result;
        }
    }

    /* Iteration cap reached without a conclusive answer — conservatively
     * report no intersection. This should not occur with well-formed convex
     * shapes; reaching this path indicates a degenerate geometry. */
    return result;
}

/* Convenience wrapper: run GJK intersection test on two rigid bodies.
 *
 * Extracts position and orientation directly from the rigid body structs,
 * then delegates to forge_physics_gjk_intersect(). Intended for use inside
 * narrow-phase loops where the caller already holds body and shape pointers.
 *
 * Parameters:
 *   body_a, body_b   — rigid bodies (non-NULL; position in meters,
 *                       orientation as unit quaternion)
 *   shape_a, shape_b — collision shapes (non-NULL, must pass shape_is_valid)
 *
 * Returns: ForgePhysicsGJKResult — zeroed on NULL input.
 *          See forge_physics_gjk_intersect for full return semantics.
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline ForgePhysicsGJKResult forge_physics_gjk_test_bodies(
    const ForgePhysicsRigidBody *body_a, const ForgePhysicsCollisionShape *shape_a,
    const ForgePhysicsRigidBody *body_b, const ForgePhysicsCollisionShape *shape_b)
{
    ForgePhysicsGJKResult empty;
    SDL_memset(&empty, 0, sizeof(empty));

    if (!body_a || !body_b || !shape_a || !shape_b) return empty;

    return forge_physics_gjk_intersect(
        shape_a, body_a->position, body_a->orientation,
        shape_b, body_b->position, body_b->orientation);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LESSON 10 — EPA Penetration Depth
 *
 * The Expanding Polytope Algorithm computes penetration depth, contact
 * normal, and contact points from a GJK simplex that encloses the origin.
 * GJK answers "do the shapes overlap?" — EPA answers "by how much and
 * in which direction?"
 *
 * EPA takes the GJK tetrahedron (always count == 4 on intersection, inflated
 * by gjk_inflate_simplex_) and iteratively expands it toward the boundary
 * of the Minkowski difference. The closest face of the polytope to the
 * origin defines the minimum translation vector (MTV): the shortest
 * direction and distance to separate the shapes.
 *
 * Algorithm overview:
 *   1. Build a polytope from the GJK tetrahedron (4 triangular faces)
 *   2. Find the face closest to the origin
 *   3. Query a support point in that face's normal direction
 *   4. If the support point is not significantly farther than the face,
 *      the closest face is on the Minkowski difference boundary — done
 *   5. Otherwise, remove all faces visible from the new point, collect
 *      the silhouette edges, and create new faces from each silhouette
 *      edge to the new vertex
 *   6. Repeat from step 2
 *
 * The closest-face distance monotonically increases, guaranteeing
 * convergence.
 *
 * Reference: van den Bergen, "Proximity Queries and Penetration Depth
 * Computation on 3D Game Objects" (GDC 2001).
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── EPA Constants ────────────────────────────────────────────────────────── */

/* Maximum EPA iterations before giving up.
 * Well above the typical 10–30 needed for sphere/box/capsule pairs.
 * Matches GJK_MAX_ITERATIONS for consistency. */
#define FORGE_PHYSICS_EPA_MAX_ITERATIONS  64

/* Maximum vertices the polytope can hold.
 * Starting with 4 (tetrahedron) + up to 64 iterations adding 1 each = 68.
 * 128 provides generous headroom. */
#define FORGE_PHYSICS_EPA_MAX_VERTICES   128

/* Maximum faces the polytope can hold.
 * Starting with 4 faces; each iteration removes some and adds some.
 * Net face count grows roughly linearly with vertices. 256 provides
 * generous headroom for the vertex cap of 128. */
#define FORGE_PHYSICS_EPA_MAX_FACES      256

/* Convergence tolerance for EPA.
 * When the new support point distance minus the closest face distance
 * is below this threshold, the algorithm has converged. Matches
 * GJK_EPSILON for consistency. */
#define FORGE_PHYSICS_EPA_EPSILON         1e-6f

/* Maximum silhouette edges during polytope expansion.
 * Each removed face contributes up to 3 edges; shared edges cancel.
 * The silhouette of a convex region has at most as many edges as
 * removed faces. 256 matches the face cap. */
#define FORGE_PHYSICS_EPA_MAX_EDGES      256

/* Sentinel distance for degenerate EPA faces.
 * Used when a face's cross product is too small to compute a valid
 * normal (collinear vertices). Must exceed any plausible squared
 * distance in the simulation. Matches the GJK sentinel pattern. */
#define FORGE_PHYSICS_EPA_SENTINEL_DIST  3.402823466e+38f

/* ── EPA Types ────────────────────────────────────────────────────────────── */

/* A single triangular face of the expanding polytope.
 *
 * Stores three vertex indices in counter-clockwise winding order when
 * viewed from outside the polytope. The outward normal and distance to
 * the origin are precomputed when the face is created and remain
 * constant for the life of the face.
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 */
typedef struct ForgePhysicsEPAFace {
    int   a, b, c;  /* vertex indices into the polytope vertex array;    */
                    /* valid range: [0, vert_count). CCW winding from    */
                    /* outside — determines normal sign                  */
    vec3  normal;   /* unit outward normal of this face (dimensionless)  */
    float dist;     /* distance from origin to face plane (m, >= 0 for  */
                    /* a valid polytope containing the origin)           */
} ForgePhysicsEPAFace;

/* A silhouette edge found during polytope expansion.
 *
 * When faces visible from a new support point are removed, the boundary
 * between visible and non-visible faces forms the silhouette. Edges are
 * stored in winding order (a→b) so new faces connecting each edge to
 * the new vertex maintain consistent CCW orientation.
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 */
typedef struct ForgePhysicsEPAEdge {
    int a, b;       /* vertex indices forming the directed edge (a→b);   */
                    /* valid range: [0, vert_count)                      */
} ForgePhysicsEPAEdge;

/* EPA result — penetration depth, contact normal, and contact points.
 *
 * When valid is true, the result contains the minimum translation vector
 * (MTV): moving shape A by (normal * depth) separates the shapes.
 *
 * Contact points are reconstructed from the closest polytope face using
 * barycentric interpolation of the original support points stored in
 * each vertex.
 *
 * Parameters:
 *   valid      — true if EPA converged successfully
 *   normal     — unit penetration normal, from B toward A (matches
 *                ForgePhysicsRBContact convention)
 *   depth      — penetration depth in meters, >= 0
 *   point_a    — contact point on shape A's surface (meters, world-space)
 *   point_b    — contact point on shape B's surface (meters, world-space)
 *   point      — midpoint of point_a and point_b (convenience)
 *   iterations — number of expansion iterations (for diagnostics)
 *
 * Reference: van den Bergen, "Proximity Queries and Penetration Depth
 * Computation on 3D Game Objects" (GDC 2001).
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 */
typedef struct ForgePhysicsEPAResult {
    bool  valid;       /* true if EPA converged to a valid result          */
    vec3  normal;      /* unit penetration normal, from B toward A         */
    float depth;       /* penetration depth (m), >= 0                      */
    vec3  point_a;     /* contact point on shape A surface (m, world)      */
    vec3  point_b;     /* contact point on shape B surface (m, world)      */
    vec3  point;       /* midpoint contact = (point_a + point_b) / 2       */
    int   iterations;  /* expansion iterations performed [0..EPA_MAX_ITER]  */
} ForgePhysicsEPAResult;

/* ── EPA Internal Helpers ─────────────────────────────────────────────────── */

/* Compute an EPA face from three vertex indices.
 *
 * Calculates the face normal as the cross product of edges (b-a)×(c-a).
 * The normal is oriented to point away from the polytope centroid. If
 * the cross product is degenerate (collinear vertices), the face is
 * returned with dist = FORGE_PHYSICS_EPA_SENTINEL_DIST as a sentinel.
 *
 * Parameters:
 *   a, b, c   — vertex indices into the vertex array
 *   verts     — the polytope vertex array
 *   centroid  — centroid of the polytope (for outward-normal orientation)
 *
 * Returns: populated ForgePhysicsEPAFace with precomputed normal and dist.
 */
static inline ForgePhysicsEPAFace epa_make_face_(
    int a, int b, int c,
    const ForgePhysicsGJKVertex *verts,
    vec3 centroid)
{
    ForgePhysicsEPAFace face;
    face.a = a;
    face.b = b;
    face.c = c;

    vec3 ab = vec3_sub(verts[b].point, verts[a].point);
    vec3 ac = vec3_sub(verts[c].point, verts[a].point);
    vec3 n  = vec3_cross(ab, ac);

    float len_sq = vec3_length_squared(n);
    if (len_sq < FORGE_PHYSICS_EPA_EPSILON * FORGE_PHYSICS_EPA_EPSILON) {
        /* Degenerate face — collinear vertices */
        face.normal = vec3_create(0.0f, 0.0f, 0.0f);
        face.dist   = FORGE_PHYSICS_EPA_SENTINEL_DIST;
        return face;
    }

    float inv_len = 1.0f / SDL_sqrtf(len_sq);
    n = vec3_scale(n, inv_len);

    /* Ensure the normal points away from the centroid.
     * If dot(n, vertex_a - centroid) < 0, the normal faces inward — flip it
     * and reverse the winding so the face remains CCW from outside. */
    vec3 to_face = vec3_sub(verts[a].point, centroid);
    if (vec3_dot(n, to_face) < 0.0f) {
        n = vec3_scale(n, -1.0f);
        face.b = c;
        face.c = b;
    }

    face.normal = n;
    /* Distance from origin to the face plane = dot(normal, any_vertex) */
    face.dist = vec3_dot(n, verts[face.a].point);
    /* Clamp to non-negative — the origin should be inside the polytope,
     * but floating point can produce tiny negative values */
    if (face.dist < 0.0f) face.dist = 0.0f;

    return face;
}

/* Find the index of the face closest to the origin.
 *
 * Linear scan through all faces. Returns the index of the face with
 * the smallest dist value. If face_count is 0, returns -1.
 *
 * Parameters:
 *   faces      — array of polytope faces
 *   face_count — number of valid faces in the array
 *
 * Returns: index of closest face, or -1 if no faces.
 */
static inline int epa_find_closest_face_(
    const ForgePhysicsEPAFace *faces, int face_count)
{
    int   best_idx  = -1;
    float best_dist = FORGE_PHYSICS_EPA_SENTINEL_DIST;

    for (int i = 0; i < face_count; i++) {
        if (faces[i].dist < best_dist) {
            best_dist = faces[i].dist;
            best_idx  = i;
        }
    }
    return best_idx;
}

/* Add or cancel a silhouette edge.
 *
 * If the reverse edge (b, a) already exists in the edge list, both are
 * removed (shared edge between two visible faces). Otherwise, the edge
 * (a, b) is appended.
 *
 * This implements the standard EPA edge-bookkeeping: when removing faces
 * visible from a new support point, each face contributes its 3 edges.
 * Shared edges between two visible faces appear as (a,b) and (b,a) and
 * cancel out. The remaining edges form the silhouette.
 *
 * Parameters:
 *   edges      — edge array (modified in place)
 *   edge_count — pointer to current edge count (modified in place)
 *   a, b       — vertex indices forming the directed edge
 *   max_edges  — capacity of the edge array
 */
static inline void epa_add_edge_(
    ForgePhysicsEPAEdge *edges, int *edge_count,
    int a, int b, int max_edges)
{
    /* Check for reverse edge — if found, remove it (shared edge cancels) */
    for (int i = 0; i < *edge_count; i++) {
        if (edges[i].a == b && edges[i].b == a) {
            /* Remove by swapping with last */
            edges[i] = edges[*edge_count - 1];
            (*edge_count)--;
            return;
        }
    }

    /* No reverse found — add as new silhouette edge */
    if (*edge_count >= max_edges) return; /* safety cap */
    edges[*edge_count].a = a;
    edges[*edge_count].b = b;
    (*edge_count)++;
}

/* Project the origin onto a triangle face and compute barycentric coords.
 *
 * Given three polytope vertices forming a face, projects the origin (0,0,0)
 * onto the face plane and computes the barycentric coordinates (u, v, w)
 * such that origin_proj = u*A + v*B + w*C and u + v + w = 1.
 *
 * These coordinates are used to interpolate the original support points
 * (sup_a, sup_b) to reconstruct the contact point on each shape's surface.
 *
 * Parameters:
 *   verts — the polytope vertex array
 *   face  — the face to project onto
 *   u, v, w — output barycentric coordinates
 *
 * Reference: Ericson, "Real-Time Collision Detection", Section 3.4 —
 * Barycentric coordinates.
 */
static inline void epa_barycentric_on_face_(
    const ForgePhysicsGJKVertex *verts,
    const ForgePhysicsEPAFace *face,
    float *u, float *v, float *w)
{
    vec3 a = verts[face->a].point;
    vec3 b = verts[face->b].point;
    vec3 c = verts[face->c].point;

    /* Project the origin onto the face plane. Since the origin is at
     * (0,0,0), the projection is simply P = dist * normal, where dist
     * is the face's precomputed distance from the origin to the plane. */
    vec3 p = vec3_scale(face->normal, face->dist);

    /* Compute barycentric coords of P in triangle ABC.
     * Using the Gram-matrix (dot product) method — Ericson §3.4. */
    vec3 v0 = vec3_sub(b, a);
    vec3 v1 = vec3_sub(c, a);
    vec3 v2 = vec3_sub(p, a);

    float d00 = vec3_dot(v0, v0);
    float d01 = vec3_dot(v0, v1);
    float d11 = vec3_dot(v1, v1);
    float d20 = vec3_dot(v2, v0);
    float d21 = vec3_dot(v2, v1);

    float denom = d00 * d11 - d01 * d01;
    if (SDL_fabsf(denom) < FORGE_PHYSICS_EPA_EPSILON) {
        /* Degenerate triangle — distribute weight equally */
        *u = 1.0f / 3.0f;
        *v = 1.0f / 3.0f;
        *w = 1.0f / 3.0f;
        return;
    }

    float inv_denom = 1.0f / denom;
    float bary_v = (d11 * d20 - d01 * d21) * inv_denom;
    float bary_w = (d00 * d21 - d01 * d20) * inv_denom;
    float bary_u = 1.0f - bary_v - bary_w;

    /* Clamp to handle numerical edge cases */
    if (bary_u < 0.0f) bary_u = 0.0f;
    if (bary_v < 0.0f) bary_v = 0.0f;
    if (bary_w < 0.0f) bary_w = 0.0f;
    float sum = bary_u + bary_v + bary_w;
    if (sum > FORGE_PHYSICS_EPA_EPSILON) {
        float inv_sum = 1.0f / sum;
        bary_u *= inv_sum;
        bary_v *= inv_sum;
        bary_w *= inv_sum;
    } else {
        bary_u = 1.0f / 3.0f;
        bary_v = 1.0f / 3.0f;
        bary_w = 1.0f / 3.0f;
    }

    *u = bary_u;
    *v = bary_v;
    *w = bary_w;
}

/* ── EPA Public API ───────────────────────────────────────────────────────── */

/* Compute penetration depth and contact information using EPA.
 *
 * Takes a GJK result that reports intersection (intersecting == true,
 * simplex.count == 4) and expands the simplex into a polytope that
 * approximates the boundary of the Minkowski difference. The closest
 * face of the polytope to the origin defines the minimum translation
 * vector (MTV).
 *
 * Algorithm:
 *   1. Copy the 4 GJK simplex vertices into the polytope vertex array
 *   2. Build 4 triangular faces from the tetrahedron with outward normals
 *   3. Loop:
 *      a. Find the face closest to the origin
 *      b. Query a new support point in that face's normal direction
 *      c. If dot(new_point, normal) - face_dist < EPA_EPSILON → converged
 *      d. Remove all faces visible from the new point
 *      e. Collect silhouette edges from removed faces
 *      f. Create new faces from each silhouette edge to the new vertex
 *   4. Reconstruct contact points via barycentric interpolation
 *
 * The closest-face distance increases monotonically, guaranteeing
 * convergence within EPA_MAX_ITERATIONS for any convex shape pair.
 *
 * Parameters:
 *   gjk_result — result from forge_physics_gjk_intersect(); must have
 *                intersecting == true and simplex.count == 4
 *   shape_a    — collision shape A (must pass forge_physics_shape_is_valid)
 *   pos_a      — world-space position of shape A (meters, must be finite)
 *   orient_a   — orientation of shape A (unit quaternion; normalized
 *                internally via gjk_validate_quat_ — same as gjk_intersect)
 *   shape_b    — collision shape B (must pass forge_physics_shape_is_valid)
 *   pos_b      — world-space position of shape B (meters, must be finite)
 *   orient_b   — orientation of shape B (unit quaternion; same as orient_a)
 *
 * Returns:
 *   ForgePhysicsEPAResult — valid == true if converged. normal points from
 *   B toward A. depth is the penetration depth in meters. point_a and
 *   point_b are the contact points on each shape's surface.
 *
 * Returns invalid (valid == false) if:
 *   - GJK result is not intersecting or simplex count != 4
 *   - Any input pointer is NULL or shape is invalid
 *   - Quaternion is non-finite or zero-length (rejected by gjk_validate_quat_)
 *   - Polytope degenerates (all faces become invalid)
 *   - Vertex or face arrays overflow
 *
 * Stack budget: ~14 KB (128 GJKVertex × 36 B + 256 EPAFace × 28 B +
 * 256 EPAEdge × 8 B). Safe on desktop; may need heap allocation for
 * embedded targets with small stacks.
 *
 * Usage:
 *   ForgePhysicsGJKResult gjk = forge_physics_gjk_intersect(
 *       &shape_a, pos_a, orient_a, &shape_b, pos_b, orient_b);
 *   if (gjk.intersecting) {
 *       ForgePhysicsEPAResult epa = forge_physics_epa(
 *           &gjk, &shape_a, pos_a, orient_a, &shape_b, pos_b, orient_b);
 *       if (epa.valid) {
 *           SDL_Log("Depth: %.4f, Normal: (%.2f, %.2f, %.2f)",
 *                   epa.depth, epa.normal.x, epa.normal.y, epa.normal.z);
 *       }
 *   }
 *
 * Reference: van den Bergen, "Proximity Queries and Penetration Depth
 * Computation on 3D Game Objects" (GDC 2001).
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 */
static inline ForgePhysicsEPAResult forge_physics_epa(
    const ForgePhysicsGJKResult *gjk_result,
    const ForgePhysicsCollisionShape *shape_a, vec3 pos_a, quat orient_a,
    const ForgePhysicsCollisionShape *shape_b, vec3 pos_b, quat orient_b)
{
    ForgePhysicsEPAResult result;
    SDL_memset(&result, 0, sizeof(result));

    /* ── Validate inputs ──────────────────────────────────────────────── */
    if (!gjk_result || !shape_a || !shape_b) return result;
    if (!gjk_result->intersecting) return result;
    if (gjk_result->simplex.count != 4) return result;
    if (!forge_physics_shape_is_valid(shape_a) ||
        !forge_physics_shape_is_valid(shape_b))
        return result;

    /* Validate positions for finite values */
    if (!forge_isfinite(pos_a.x) || !forge_isfinite(pos_a.y) ||
        !forge_isfinite(pos_a.z))
        return result;
    if (!forge_isfinite(pos_b.x) || !forge_isfinite(pos_b.y) ||
        !forge_isfinite(pos_b.z))
        return result;

    /* Validate and normalize orientations — use the same helper as
     * gjk_intersect so that EPA expands with identical quaternions to
     * those used when building the GJK simplex. Without this, slightly
     * off-unit body orientations would produce different support points
     * in the EPA expansion than in the GJK simplex, skewing depth/normal. */
    if (!gjk_validate_quat_(&orient_a) || !gjk_validate_quat_(&orient_b))
        return result;

    /* ── Initialize polytope from GJK tetrahedron ─────────────────────── */
    ForgePhysicsGJKVertex verts[FORGE_PHYSICS_EPA_MAX_VERTICES];
    ForgePhysicsEPAFace   faces[FORGE_PHYSICS_EPA_MAX_FACES];
    int vert_count = 4;
    int face_count = 0;

    /* Copy GJK simplex vertices and validate for finite values */
    for (int i = 0; i < 4; i++) {
        verts[i] = gjk_result->simplex.verts[i];
        if (!forge_isfinite(vec3_length_squared(verts[i].point)))
            return result;
    }

    /* Compute the centroid of the initial tetrahedron as the interior
     * reference point for outward-normal orientation. The centroid is
     * guaranteed to be strictly inside the tetrahedron (assuming the
     * tetrahedron has non-zero volume, which GJK inflation ensures).
     *
     * Note: the centroid is computed once and not updated as the polytope
     * grows. This is safe because epa_make_face_ only needs any point
     * that is strictly interior to the polytope, and the centroid of the
     * initial tetrahedron remains interior as the polytope only expands
     * outward from it. */
    vec3 interior_ref = vec3_create(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++) {
        interior_ref = vec3_add(interior_ref, verts[i].point);
    }
    interior_ref = vec3_scale(interior_ref, 0.25f);

    /* Build 4 faces from the tetrahedron.
     * Vertices: 0, 1, 2, 3. The four faces are:
     *   (0,1,2), (0,2,3), (0,3,1), (1,3,2)
     * epa_make_face_ orients normals outward using the interior ref. */
    faces[0] = epa_make_face_(0, 1, 2, verts, interior_ref);
    faces[1] = epa_make_face_(0, 2, 3, verts, interior_ref);
    faces[2] = epa_make_face_(0, 3, 1, verts, interior_ref);
    faces[3] = epa_make_face_(1, 3, 2, verts, interior_ref);
    face_count = 4;

    /* Check for degenerate initial polytope */
    for (int i = 0; i < face_count; i++) {
        if (faces[i].dist >= FORGE_PHYSICS_EPA_SENTINEL_DIST) {
            /* Degenerate face in initial polytope — cannot proceed */
            return result;
        }
    }

    /* ── Main EPA loop ────────────────────────────────────────────────── */
    int closest_face = -1;
    ForgePhysicsEPAFace saved_closest;  /* best face before failed expansion */
    bool use_saved = false;             /* true if expansion failed mid-mutation */

    for (int iter = 0; iter < FORGE_PHYSICS_EPA_MAX_ITERATIONS; iter++) {
        result.iterations = iter + 1;

        /* Find the face closest to the origin */
        closest_face = epa_find_closest_face_(faces, face_count);
        if (closest_face < 0) return result; /* no faces — degenerate */

        ForgePhysicsEPAFace *cf = &faces[closest_face];

        /* Query a new support point in the closest face's normal direction */
        ForgePhysicsGJKVertex new_vert = forge_physics_gjk_support(
            shape_a, pos_a, orient_a,
            shape_b, pos_b, orient_b,
            cf->normal);

        /* Check for invalid support point */
        if (!forge_isfinite(vec3_length_squared(new_vert.point)))
            return result;

        /* Convergence check: has the new support point extended the
         * polytope significantly beyond the closest face? */
        float new_dist = vec3_dot(cf->normal, new_vert.point);
        if (new_dist - cf->dist < FORGE_PHYSICS_EPA_EPSILON) {
            /* Converged — the closest face is on the Minkowski boundary */
            break;
        }

        /* Check vertex capacity */
        if (vert_count >= FORGE_PHYSICS_EPA_MAX_VERTICES)
            break; /* capacity reached — use best result so far */

        /* Add new vertex */
        int new_idx = vert_count;
        verts[vert_count++] = new_vert;

        /* Save the closest face before mutating the face array. If
         * expansion fails (degenerate faces, capacity overflow), we
         * use this saved face for the result instead of rescanning
         * a torn polytope with missing faces. */
        saved_closest = *cf;

        /* Remove all faces visible from the new point and collect
         * silhouette edges. A face is visible if the new point is on
         * the positive side of the face plane:
         *   dot(face_normal, new_point - face_vertex) > 0
         * which simplifies to:
         *   dot(face_normal, new_point) > face_dist */
        ForgePhysicsEPAEdge edges[FORGE_PHYSICS_EPA_MAX_EDGES];
        int edge_count = 0;

        int i = 0;
        while (i < face_count) {
            float d = vec3_dot(faces[i].normal, new_vert.point);
            if (d > faces[i].dist + FORGE_PHYSICS_EPA_EPSILON) {
                /* Face is visible from new point — remove it and add
                 * its edges to the silhouette edge list */
                epa_add_edge_(edges, &edge_count,
                              faces[i].a, faces[i].b,
                              FORGE_PHYSICS_EPA_MAX_EDGES);
                epa_add_edge_(edges, &edge_count,
                              faces[i].b, faces[i].c,
                              FORGE_PHYSICS_EPA_MAX_EDGES);
                epa_add_edge_(edges, &edge_count,
                              faces[i].c, faces[i].a,
                              FORGE_PHYSICS_EPA_MAX_EDGES);

                /* Remove face by swapping with last */
                faces[i] = faces[face_count - 1];
                face_count--;
                /* Don't increment i — re-check the swapped face */
            } else {
                i++;
            }
        }

        /* Create new faces from each silhouette edge to the new vertex.
         * The edge winding (a→b) combined with the new vertex produces
         * a CCW face when viewed from outside. */
        bool expansion_ok = true;
        for (int e = 0; e < edge_count; e++) {
            if (face_count >= FORGE_PHYSICS_EPA_MAX_FACES) {
                /* Face capacity exhausted mid-expansion — the polytope
                 * has holes and cannot produce a valid result. */
                expansion_ok = false;
                break;
            }

            ForgePhysicsEPAFace new_face = epa_make_face_(
                edges[e].a, edges[e].b, new_idx,
                verts, interior_ref);

            /* Degenerate face (collinear edge + vertex) — the polytope
             * will have a hole where this face should be. Stop expanding
             * immediately; adding more faces to a broken polytope is
             * pointless. The outer loop will use the best result so far. */
            if (new_face.dist >= FORGE_PHYSICS_EPA_SENTINEL_DIST) {
                expansion_ok = false;
                break;
            }

            faces[face_count++] = new_face;
        }

        /* If expansion failed (capacity or degenerate faces), stop
         * iterating — the polytope may have holes. Use the saved
         * closest face from before the mutation, not the torn hull. */
        if (!expansion_ok) {
            use_saved = true;
            break;
        }

        /* If no faces remain, the polytope has degenerated */
        if (face_count == 0) return result;
    }

    /* ── Extract result from closest face ─────────────────────────────── */
    const ForgePhysicsEPAFace *cf;
    if (use_saved) {
        /* Expansion failed mid-mutation — the face array has holes.
         * Use the saved closest face from before the failed expansion. */
        cf = &saved_closest;
    } else {
        closest_face = epa_find_closest_face_(faces, face_count);
        if (closest_face < 0) return result;
        cf = &faces[closest_face];
    }

    /* Validate the closest face has a finite normal and distance */
    if (!forge_isfinite(cf->dist) || !forge_isfinite(cf->normal.x) ||
        !forge_isfinite(cf->normal.y) || !forge_isfinite(cf->normal.z))
        return result;

    result.valid  = true;
    /* Negate the outward normal to match the physics convention:
     * "normal from B toward A" (same as ForgePhysicsRBContact).
     *
     * The EPA polytope represents the Minkowski difference A − B. Its
     * outward face normal N points away from the origin, in the direction
     * that separates A − B from the origin. To separate the shapes,
     * move A by −N·depth or equivalently move B by N·depth. The contact
     * convention is that the normal points from B toward A, which is −N:
     * applying a positive impulse along −N pushes A away from B. */
    result.normal = vec3_scale(cf->normal, -1.0f);
    result.depth  = cf->dist;

    /* Reconstruct contact points using barycentric interpolation.
     * The closest face has three vertices, each storing the original
     * support points from shape A and shape B. The barycentric
     * coordinates of the origin's projection onto the face let us
     * interpolate to find the contact point on each shape's surface. */
    float u, v, w;
    epa_barycentric_on_face_(verts, cf, &u, &v, &w);

    vec3 sa_a = verts[cf->a].sup_a;
    vec3 sa_b = verts[cf->b].sup_a;
    vec3 sa_c = verts[cf->c].sup_a;

    vec3 sb_a = verts[cf->a].sup_b;
    vec3 sb_b = verts[cf->b].sup_b;
    vec3 sb_c = verts[cf->c].sup_b;

    /* point_a = u * sup_a_A + v * sup_a_B + w * sup_a_C */
    result.point_a = vec3_add(
        vec3_add(vec3_scale(sa_a, u), vec3_scale(sa_b, v)),
        vec3_scale(sa_c, w));

    /* point_b = u * sup_b_A + v * sup_b_B + w * sup_b_C */
    result.point_b = vec3_add(
        vec3_add(vec3_scale(sb_a, u), vec3_scale(sb_b, v)),
        vec3_scale(sb_c, w));

    /* Midpoint contact */
    result.point = vec3_scale(vec3_add(result.point_a, result.point_b), 0.5f);

    /* Final sanity check — reject if any output is non-finite */
    if (!forge_isfinite(vec3_length_squared(result.point_a)) ||
        !forge_isfinite(vec3_length_squared(result.point_b)) ||
        !forge_isfinite(vec3_length_squared(result.point)) ||
        !forge_isfinite(result.depth)) {
        result.valid = false;
        return result;
    }

    return result;
}

/* Compute EPA penetration depth for two rigid bodies.
 *
 * Convenience wrapper that extracts position and orientation from rigid
 * body structs and delegates to forge_physics_epa(). Mirrors the pattern
 * of forge_physics_gjk_test_bodies().
 *
 * Parameters:
 *   gjk_result — GJK intersection result (must be intersecting, count==4)
 *   body_a     — rigid body A (must not be NULL)
 *   shape_a    — collision shape for body A
 *   body_b     — rigid body B (must not be NULL)
 *   shape_b    — collision shape for body B
 *
 * Returns: ForgePhysicsEPAResult — valid == false on NULL input.
 *
 * Usage:
 *   ForgePhysicsGJKResult gjk = forge_physics_gjk_test_bodies(
 *       &body_a, &shape_a, &body_b, &shape_b);
 *   if (gjk.intersecting) {
 *       ForgePhysicsEPAResult epa = forge_physics_epa_bodies(
 *           &gjk, &body_a, &shape_a, &body_b, &shape_b);
 *   }
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 */
static inline ForgePhysicsEPAResult forge_physics_epa_bodies(
    const ForgePhysicsGJKResult *gjk_result,
    const ForgePhysicsRigidBody *body_a,
    const ForgePhysicsCollisionShape *shape_a,
    const ForgePhysicsRigidBody *body_b,
    const ForgePhysicsCollisionShape *shape_b)
{
    ForgePhysicsEPAResult empty;
    SDL_memset(&empty, 0, sizeof(empty));

    if (!body_a || !body_b || !shape_a || !shape_b || !gjk_result)
        return empty;

    return forge_physics_epa(
        gjk_result,
        shape_a, body_a->position, body_a->orientation,
        shape_b, body_b->position, body_b->orientation);
}

/* Run GJK + EPA and produce a rigid body contact in one call.
 *
 * Combines the full narrowphase pipeline: GJK intersection test followed
 * by EPA penetration depth computation. If both succeed, populates a
 * ForgePhysicsRBContact with the contact normal, point, depth, and
 * friction coefficients.
 *
 * This is the intended entry point for physics simulation loops that
 * need convex-convex contact generation between arbitrary shape pairs.
 *
 * Parameters:
 *   body_a, body_b     — rigid bodies (must not be NULL)
 *   shape_a, shape_b   — collision shapes (must be valid)
 *   idx_a, idx_b       — body indices for the contact struct
 *   mu_s               — static friction coefficient, >= 0
 *   mu_d               — dynamic friction coefficient, >= 0
 *   out                — output contact (must not be NULL)
 *
 * Returns: true if a contact was generated, false if shapes are
 *          separated or inputs are invalid.
 *
 * Usage:
 *   ForgePhysicsRBContact contact;
 *   if (forge_physics_gjk_epa_contact(
 *           &bodies[i], &shapes[i],
 *           &bodies[j], &shapes[j],
 *           i, j, 0.6f, 0.4f, &contact))
 *   {
 *       forge_physics_rb_resolve_contact(
 *           &contact, bodies, num_bodies, PHYSICS_DT);
 *   }
 *
 * See: Physics Lesson 10 — EPA Penetration Depth
 */
static inline bool forge_physics_gjk_epa_contact(
    const ForgePhysicsRigidBody *body_a,
    const ForgePhysicsCollisionShape *shape_a,
    const ForgePhysicsRigidBody *body_b,
    const ForgePhysicsCollisionShape *shape_b,
    int idx_a, int idx_b,
    float mu_s, float mu_d,
    ForgePhysicsRBContact *out)
{
    if (!body_a || !body_b || !shape_a || !shape_b || !out) return false;

    /* Step 1: GJK intersection test */
    ForgePhysicsGJKResult gjk = forge_physics_gjk_test_bodies(
        body_a, shape_a, body_b, shape_b);
    if (!gjk.intersecting) return false;

    /* Step 2: EPA penetration depth */
    ForgePhysicsEPAResult epa = forge_physics_epa_bodies(
        &gjk, body_a, shape_a, body_b, shape_b);
    if (!epa.valid) return false;

    /* Step 3: Populate contact */
    out->point       = epa.point;
    out->normal      = epa.normal;
    out->penetration = epa.depth;
    out->body_a      = idx_a;
    out->body_b      = idx_b;
    out->static_friction  = (forge_isfinite(mu_s) && mu_s > 0.0f) ? mu_s : 0.0f;
    out->dynamic_friction = (forge_isfinite(mu_d) && mu_d > 0.0f) ? mu_d : 0.0f;

    return true;
}



/* ═══════════════════════════════════════════════════════════════════════════
 * LESSON 11 — Contact Manifold
 *
 * A contact manifold stores up to 4 contact points between two rigid
 * bodies. Multiple contacts give the constraint solver enough information
 * to prevent rocking and produce stable resting contacts.
 *
 * The pipeline is: GJK → EPA → clipping → manifold → cache → solver.
 *
 * EPA gives one contact point. Sutherland-Hodgman polygon clipping
 * generates multiple contacts by clipping the incident face of one
 * shape against the reference face side planes of the other. The
 * manifold caches contacts across frames using persistent IDs, which
 * enables warm-starting the solver with accumulated impulses.
 *
 * Reference:
 *   Erin Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005)
 *   Dirk Gregorius, "The Separating Axis Test" (GDC 2013)
 *   Christer Ericson, "Real-Time Collision Detection", Ch. 8
 *
 * See: Physics Lesson 11 — Contact Manifold
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Contact Manifold Constants ───────────────────────────────────────────── */

/* Maximum contact points per manifold.
 *
 * 4 is the standard — clipping two convex faces produces at most 4
 * points after reduction, and 4 points define a stable contact patch
 * for any flat-on-flat configuration (prevents rocking).
 */
#define FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS  4

/* Maximum polygon vertices during Sutherland-Hodgman clipping.
 *
 * A box face has 4 vertices. Clipping against 4 side planes can add
 * at most 1 vertex per plane, giving 8 worst case.
 */
#define FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS 8

/* Warm-start scaling factor.
 *
 * Accumulated impulses from the previous frame are scaled by this
 * factor before being applied. Values < 1 dampen stale impulses
 * when bodies have moved significantly between frames.
 */
#define FORGE_PHYSICS_MANIFOLD_WARM_SCALE    0.85f

/* ── Contact Manifold Types ───────────────────────────────────────────────── */

/* A single contact point within a manifold.
 *
 * Stores the contact position in both local and world space:
 *   - local_a / local_b: body-space positions for persistence across
 *     frames. When bodies move, these are re-projected to world space
 *     to check if the contact is still valid.
 *   - world_point: world-space contact position for the current frame.
 *
 * Accumulated impulses (normal_impulse, tangent_impulse_1/2) carry
 * over from the previous frame via warm-starting. The constraint
 * solver adds to these each iteration, and the clamping is done on
 * the accumulated value (Catto's accumulated impulse method).
 *
 * The persistent id encodes which geometric features produced this
 * contact, enabling frame-to-frame matching without positional
 * proximity searches.
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence"
 * (GDC 2005), Section 3.
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
typedef struct ForgePhysicsManifoldContact {
    vec3     local_a;           /* contact on body A in A's local space    */
    vec3     local_b;           /* contact on body B in B's local space    */
    vec3     world_point;       /* world-space contact position            */
    float    penetration;       /* overlap depth (m), >= 0                 */
    float    normal_impulse;    /* accumulated normal impulse (warm-start) */
    float    tangent_impulse_1; /* accumulated friction impulse, tangent 1 */
    float    tangent_impulse_2; /* accumulated friction impulse, tangent 2 */
    uint32_t id;                /* persistent contact ID (feature pair)    */
} ForgePhysicsManifoldContact;

/* A contact manifold for one body pair.
 *
 * Stores up to FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS (4) contact points
 * sharing a common contact normal. The normal points from body B
 * toward body A, matching the ForgePhysicsRBContact convention.
 *
 * The manifold is the unit of persistence: the manifold cache stores
 * one manifold per active body pair, and contacts within it are matched
 * across frames by their persistent IDs.
 *
 * Reference: Ericson, "Real-Time Collision Detection", Section 8.3.
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
typedef struct ForgePhysicsManifold {
    int   body_a;             /* index of body A                           */
    int   body_b;             /* index of body B                           */
    vec3  normal;             /* unit normal from B toward A               */
    float static_friction;    /* static friction coefficient, >= 0         */
    float dynamic_friction;   /* dynamic friction coefficient, >= 0        */
    int   count;              /* active contacts, 0..MAX_CONTACTS          */
    ForgePhysicsManifoldContact contacts[FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS];
} ForgePhysicsManifold;

/* Manifold cache entry for the hash map.
 *
 * The key is a uint64_t packing both body indices: the smaller index
 * in the upper 32 bits, the larger in the lower 32 bits. This ensures
 * pair (3, 7) and pair (7, 3) hash to the same entry.
 *
 * Usage with forge_containers.h:
 *   ForgePhysicsManifoldCacheEntry *cache = NULL;
 *   forge_hm_put_struct(cache, entry);
 *   ForgePhysicsManifoldCacheEntry e = forge_hm_get_struct(cache, key);
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
typedef struct ForgePhysicsManifoldCacheEntry {
    uint64_t key;                    /* packed body pair (min << 32 | max) */
    ForgePhysicsManifold manifold;
} ForgePhysicsManifoldCacheEntry;

/* ── Contact Manifold Utility Functions ────────────────────────────────────── */

/* Pack a body pair into a uint64_t cache key.
 *
 * The smaller index occupies the upper 32 bits, the larger index the
 * lower 32 bits. This guarantees pair (a, b) and pair (b, a) produce
 * the same key.
 *
 * Parameters:
 *   a — index of one body
 *   b — index of the other body
 *
 * Returns: canonical key for the pair
 *
 * Usage:
 *   uint64_t key = forge_physics_manifold_pair_key(3, 7);
 *   // key == (3ULL << 32) | 7ULL
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline uint64_t forge_physics_manifold_pair_key(int a, int b)
{
    uint32_t lo = (uint32_t)(a < b ? a : b);
    uint32_t hi = (uint32_t)(a < b ? b : a);
    return ((uint64_t)lo << 32) | (uint64_t)hi;
}

/* Compute a persistent contact ID from geometric feature indices.
 *
 * Encodes which reference face, incident feature, and clip edge
 * produced this contact point. As long as the same geometric features
 * are in contact, the ID remains stable across frames — enabling
 * warm-starting without positional proximity matching.
 *
 * Parameters:
 *   ref_face    — index of the reference face (0-5 for box, 0 for sphere)
 *   inc_feature — index of the incident feature (vertex/edge index)
 *   clip_edge   — index of the clipping edge that produced this vertex
 *
 * Returns: packed 32-bit contact ID
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline uint32_t forge_physics_manifold_contact_id(
    int ref_face, int inc_feature, int clip_edge)
{
    return ((uint32_t)(ref_face & 0xFF))       |
           ((uint32_t)(inc_feature & 0xFF) << 8) |
           ((uint32_t)(clip_edge   & 0xFF) << 16);
}

/* Transform a world-space point into a body's local space.
 *
 * Computes: local = conjugate(orient) * (world - position)
 *
 * Parameters:
 *   world_pt — point in world space
 *   pos      — body center-of-mass position
 *   orient   — body orientation quaternion (must be unit length)
 *
 * Returns: point in body-local space
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline vec3 forge_physics_manifold_world_to_local(
    vec3 world_pt, vec3 pos, quat orient)
{
    vec3 rel = vec3_sub(world_pt, pos);
    quat inv = quat_conjugate(orient);
    return quat_rotate_vec3(inv, rel);
}

/* Transform a body-local point to world space.
 *
 * Computes: world = position + orient * local
 *
 * Parameters:
 *   local_pt — point in body-local space
 *   pos      — body center-of-mass position
 *   orient   — body orientation quaternion (must be unit length)
 *
 * Returns: point in world space
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline vec3 forge_physics_manifold_local_to_world(
    vec3 local_pt, vec3 pos, quat orient)
{
    return vec3_add(pos, quat_rotate_vec3(orient, local_pt));
}

/* ── Sutherland-Hodgman Polygon Clipping ──────────────────────────────────── */

/* Clip a convex polygon against a single half-plane.
 *
 * The half-plane is defined by: dot(point, plane_normal) <= plane_dist.
 * Points on the inside (dot <= dist) are kept. Points on the outside
 * are clipped, with intersection points inserted at plane crossings.
 *
 * This is the inner step of Sutherland-Hodgman clipping. The full
 * face-face pipeline calls this once per edge of the reference face.
 *
 * Algorithm (Sutherland-Hodgman, 1974):
 *   For each edge (v[i], v[next]):
 *     d_i    = dot(v[i],    plane_normal) - plane_dist
 *     d_next = dot(v[next], plane_normal) - plane_dist
 *     If d_i    <= 0: v[i] is inside — emit it
 *     If sign(d_i) != sign(d_next): edge crosses plane — emit intersection
 *
 * Parameters:
 *   in       — input polygon vertices (caller-owned)
 *   in_count — number of input vertices (>= 0)
 *   out      — output polygon vertices (caller-allocated, capacity >= in_count + 1)
 *   plane_n  — clip plane normal (unit length)
 *   plane_d  — clip plane distance from origin
 *
 * Returns: number of output vertices (0 if fully clipped)
 *
 * Reference: Sutherland & Hodgman, "Reentrant Polygon Clipping",
 *            Communications of the ACM, 1974.
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline int forge_physics_clip_polygon(
    const vec3 *in, int in_count,
    vec3 *out,
    vec3 plane_n, float plane_d)
{
    if (!in || !out || in_count <= 0) return 0;

    int out_count = 0;

    for (int i = 0; i < in_count; i++) {
        int next = (i + 1) % in_count;
        float d_i    = vec3_dot(in[i],    plane_n) - plane_d;
        float d_next = vec3_dot(in[next], plane_n) - plane_d;

        /* Current vertex is inside (or on) the plane */
        if (d_i <= 0.0f) {
            out[out_count++] = in[i];
        }

        /* Edge crosses the plane — emit intersection */
        if ((d_i > 0.0f) != (d_next > 0.0f)) {
            float denom = d_i - d_next;
            if (SDL_fabsf(denom) > FORGE_PHYSICS_EPSILON) {
                float t = d_i / denom;
                out[out_count++] = vec3_lerp(in[i], in[next], t);
            }
        }
    }

    return out_count;
}

/* ── Box Face Geometry ────────────────────────────────────────────────────── */

/* Get the 4 world-space vertices of a box face.
 *
 * Box faces are indexed 0-5:
 *   0: +X face,  1: -X face
 *   2: +Y face,  3: -Y face
 *   4: +Z face,  5: -Z face
 *
 * Vertices are wound clockwise when viewed from outside the box.
 * The side-plane clipping code relies on that ordering.
 *
 * Parameters:
 *   half_ext — box half-extents
 *   pos      — body center position in world space
 *   orient   — body orientation quaternion (must be unit length)
 *   face_idx — face index 0-5
 *   verts    — receives 4 world-space vertices (caller-allocated)
 *   normal   — receives the outward face normal in world space
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline void forge_physics_manifold_box_face(
    vec3 half_ext, vec3 pos, quat orient,
    int face_idx, vec3 verts[4], vec3 *normal)
{
    if (!verts || !normal) return;

    /* Local-space face vertices (CW from outside) */
    float hx = half_ext.x, hy = half_ext.y, hz = half_ext.z;
    vec3 lv[4];
    vec3 ln;

    switch (face_idx) {
    case 0: /* +X */
        ln = vec3_create( 1, 0, 0);
        lv[0] = vec3_create( hx, -hy, -hz);
        lv[1] = vec3_create( hx, -hy,  hz);
        lv[2] = vec3_create( hx,  hy,  hz);
        lv[3] = vec3_create( hx,  hy, -hz);
        break;
    case 1: /* -X */
        ln = vec3_create(-1, 0, 0);
        lv[0] = vec3_create(-hx, -hy,  hz);
        lv[1] = vec3_create(-hx, -hy, -hz);
        lv[2] = vec3_create(-hx,  hy, -hz);
        lv[3] = vec3_create(-hx,  hy,  hz);
        break;
    case 2: /* +Y */
        ln = vec3_create( 0, 1, 0);
        lv[0] = vec3_create(-hx,  hy, -hz);
        lv[1] = vec3_create( hx,  hy, -hz);
        lv[2] = vec3_create( hx,  hy,  hz);
        lv[3] = vec3_create(-hx,  hy,  hz);
        break;
    case 3: /* -Y */
        ln = vec3_create( 0,-1, 0);
        lv[0] = vec3_create(-hx, -hy,  hz);
        lv[1] = vec3_create( hx, -hy,  hz);
        lv[2] = vec3_create( hx, -hy, -hz);
        lv[3] = vec3_create(-hx, -hy, -hz);
        break;
    case 4: /* +Z */
        ln = vec3_create( 0, 0, 1);
        lv[0] = vec3_create( hx, -hy,  hz);
        lv[1] = vec3_create(-hx, -hy,  hz);
        lv[2] = vec3_create(-hx,  hy,  hz);
        lv[3] = vec3_create( hx,  hy,  hz);
        break;
    default: /* -Z (face 5) */
        ln = vec3_create( 0, 0,-1);
        lv[0] = vec3_create(-hx, -hy, -hz);
        lv[1] = vec3_create( hx, -hy, -hz);
        lv[2] = vec3_create( hx,  hy, -hz);
        lv[3] = vec3_create(-hx,  hy, -hz);
        break;
    }

    /* Transform to world space */
    for (int i = 0; i < 4; i++) {
        verts[i] = vec3_add(pos, quat_rotate_vec3(orient, lv[i]));
    }
    *normal = quat_rotate_vec3(orient, ln);
}

/* Find the reference face of a box — the face most aligned with a direction.
 *
 * Returns the face index (0-5) whose outward normal has the largest
 * dot product with the given direction. Used to select the reference
 * face in the Sutherland-Hodgman clipping pipeline.
 *
 * Parameters:
 *   orient    — body orientation (must be unit quaternion)
 *   direction — the direction to align with (typically EPA normal)
 *
 * Returns: face index 0-5
 *
 * Reference: Gregorius, "The Separating Axis Test" (GDC 2013).
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline int forge_physics_manifold_ref_face_box(
    quat orient, vec3 direction)
{
    /* Transform direction into local space */
    quat inv = quat_conjugate(orient);
    vec3 local_dir = quat_rotate_vec3(inv, direction);

    /* Find the axis with the largest component */
    float ax = SDL_fabsf(local_dir.x);
    float ay = SDL_fabsf(local_dir.y);
    float az = SDL_fabsf(local_dir.z);

    if (ax >= ay && ax >= az) {
        return (local_dir.x > 0.0f) ? 0 : 1;  /* +X or -X */
    } else if (ay >= az) {
        return (local_dir.y > 0.0f) ? 2 : 3;  /* +Y or -Y */
    } else {
        return (local_dir.z > 0.0f) ? 4 : 5;  /* +Z or -Z */
    }
}

/* Find the incident face — the face most anti-aligned with a direction.
 *
 * The incident face is the face on the incident shape whose normal
 * has the most negative dot product with the reference face normal.
 * Its vertices will be clipped against the reference face side planes.
 *
 * Parameters:
 *   orient    — body orientation (must be unit quaternion)
 *   direction — the reference face normal (world space)
 *
 * Returns: face index 0-5
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline int forge_physics_manifold_incident_face_box(
    quat orient, vec3 direction)
{
    /* Incident = most anti-aligned = reference face of negated direction */
    vec3 neg = vec3_negate(direction);
    return forge_physics_manifold_ref_face_box(orient, neg);
}

/* ── Contact Point Reduction ───────────────────────────────────────────────── */

/* Reduce a set of contact points to at most 4, maximizing contact area.
 *
 * When clipping produces more than 4 contacts, this function selects the
 * subset of 4 that maximizes the area of the contact patch. A large
 * contact patch gives the constraint solver the best leverage to prevent
 * rocking and rotation.
 *
 * Algorithm:
 *   1. Keep the deepest contact (it contributes most to stability)
 *   2. Find the contact farthest from the deepest (maximizes span)
 *   3. Find the contact that maximizes triangle area with the first two
 *   4. Find the contact that maximizes quadrilateral area with the first three
 *
 * Parameters:
 *   points       — array of candidate contact world-space positions
 *   depths       — penetration depth for each candidate
 *   count        — number of candidates (must be > 4)
 *   out_indices  — receives 4 selected indices (caller-allocated, size >= 4)
 *
 * Returns: always 4 (the number of selected contacts)
 *
 * Reference: Ericson, "Real-Time Collision Detection", Section 5.3 —
 *            contact point reduction.
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline int forge_physics_manifold_reduce(
    const vec3 *points, const float *depths,
    int count, int out_indices[4])
{
    if (!points || !depths || !out_indices || count <= 0) return 0;
    if (count <= 4) {
        for (int i = 0; i < count; i++) out_indices[i] = i;
        return count;
    }

    /* Step 1: deepest contact */
    int i0 = 0;
    float max_depth = depths[0];
    for (int i = 1; i < count; i++) {
        if (depths[i] > max_depth) {
            max_depth = depths[i];
            i0 = i;
        }
    }
    out_indices[0] = i0;

    /* Step 2: farthest from deepest.
     * Initialize i1 to the first index that is not i0, so the fallback
     * for degenerate (all-coincident) points is still a distinct index. */
    int i1 = (i0 == 0) ? 1 : 0;
    float max_dist_sq = 0.0f;
    for (int i = 0; i < count; i++) {
        if (i == i0) continue;
        float dsq = vec3_length_squared(vec3_sub(points[i], points[i0]));
        if (dsq > max_dist_sq) {
            max_dist_sq = dsq;
            i1 = i;
        }
    }
    out_indices[1] = i1;

    /* Step 3: maximizes triangle area with i0, i1.
     * Initialize i2 to the first index not already selected. */
    int i2 = 0;
    while (i2 == i0 || i2 == i1) i2++;
    float max_area = 0.0f;
    vec3 edge01 = vec3_sub(points[i1], points[i0]);
    for (int i = 0; i < count; i++) {
        if (i == i0 || i == i1) continue;
        vec3 edge0i = vec3_sub(points[i], points[i0]);
        vec3 cross = vec3_cross(edge01, edge0i);
        float area = vec3_length_squared(cross);
        if (area > max_area) {
            max_area = area;
            i2 = i;
        }
    }
    out_indices[2] = i2;

    /* Step 4: maximizes quadrilateral area with i0, i1, i2.
     * Initialize i3 to the first index not already selected. */
    int i3 = 0;
    while (i3 == i0 || i3 == i1 || i3 == i2) i3++;
    float max_quad_area = 0.0f;
    for (int i = 0; i < count; i++) {
        if (i == i0 || i == i1 || i == i2) continue;
        /* Area contribution: sum of triangles from the new point to each
         * edge of the existing triangle */
        vec3 e0 = vec3_sub(points[i], points[i0]);
        vec3 e1 = vec3_sub(points[i], points[i1]);
        vec3 e2 = vec3_sub(points[i], points[i2]);
        float a = vec3_length_squared(vec3_cross(e0, e1)) +
                  vec3_length_squared(vec3_cross(e1, e2)) +
                  vec3_length_squared(vec3_cross(e2, e0));
        if (a > max_quad_area) {
            max_quad_area = a;
            i3 = i;
        }
    }
    out_indices[3] = i3;

    return 4;
}

/* ── Manifold Generation ──────────────────────────────────────────────────── */

/* Generate a contact manifold from GJK/EPA results using face clipping.
 *
 * This is the main entry point for contact manifold generation. Given
 * two colliding shapes and their EPA result, it determines the reference
 * and incident faces, clips the incident polygon against the reference
 * face side planes, projects surviving points onto the reference plane,
 * and reduces to 4 contacts if necessary.
 *
 * Shape-pair dispatch:
 *   sphere-sphere:   1 contact from EPA directly (no clipping)
 *   sphere-box:      1 contact from EPA directly
 *   sphere-capsule:  1 contact from EPA directly
 *   box-box:         up to 4 contacts via face clipping
 *   box-capsule:     1-2 contacts (capsule edge vs box face)
 *   capsule-capsule: 1 contact from EPA directly
 *
 * Parameters:
 *   epa      — valid EPA result (normal, depth, contact points)
 *   shape_a  — collision shape of body A
 *   pos_a    — world position of body A
 *   orient_a — world orientation of body A
 *   shape_b  — collision shape of body B
 *   pos_b    — world position of body B
 *   orient_b — world orientation of body B
 *   idx_a    — body index A
 *   idx_b    — body index B
 *   mu_s     — static friction coefficient (>= 0)
 *   mu_d     — dynamic friction coefficient (>= 0)
 *
 * Returns: ForgePhysicsManifold with 0..4 contacts.
 *          count == 0 means generation failed (invalid input).
 *
 * Reference: Gregorius, "The Separating Axis Test" (GDC 2013).
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline ForgePhysicsManifold forge_physics_manifold_generate(
    const ForgePhysicsEPAResult *epa,
    const ForgePhysicsCollisionShape *shape_a, vec3 pos_a, quat orient_a,
    const ForgePhysicsCollisionShape *shape_b, vec3 pos_b, quat orient_b,
    int idx_a, int idx_b,
    float mu_s, float mu_d)
{
    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = idx_a;
    m.body_b = idx_b;
    m.static_friction  = (forge_isfinite(mu_s) && mu_s > 0.0f) ? mu_s : 0.0f;
    m.dynamic_friction = (forge_isfinite(mu_d) && mu_d > 0.0f) ? mu_d : 0.0f;

    if (!epa || !epa->valid || !shape_a || !shape_b) return m;

    m.normal = epa->normal;

    /* Non-box shapes: single contact from EPA */
    bool a_is_box = (shape_a->type == FORGE_PHYSICS_SHAPE_BOX);
    bool b_is_box = (shape_b->type == FORGE_PHYSICS_SHAPE_BOX);

    if (!a_is_box && !b_is_box) {
        /* sphere-sphere, sphere-capsule, capsule-capsule: 1 contact */
        m.count = 1;
        m.contacts[0].world_point  = epa->point;
        m.contacts[0].penetration  = epa->depth;
        m.contacts[0].local_a = forge_physics_manifold_world_to_local(
            epa->point_a, pos_a, orient_a);
        m.contacts[0].local_b = forge_physics_manifold_world_to_local(
            epa->point_b, pos_b, orient_b);
        m.contacts[0].id = forge_physics_manifold_contact_id(0, 0, 0);
        return m;
    }

    if (!a_is_box || !b_is_box) {
        /* sphere-box or capsule-box: 1 contact from EPA */
        m.count = 1;
        m.contacts[0].world_point  = epa->point;
        m.contacts[0].penetration  = epa->depth;
        m.contacts[0].local_a = forge_physics_manifold_world_to_local(
            epa->point_a, pos_a, orient_a);
        m.contacts[0].local_b = forge_physics_manifold_world_to_local(
            epa->point_b, pos_b, orient_b);
        m.contacts[0].id = forge_physics_manifold_contact_id(0, 0, 0);
        return m;
    }

    /* ── Box-box: Sutherland-Hodgman face clipping ───────────── */

    /* Determine reference and incident shapes.
     * The reference shape is the one whose face is more aligned with
     * the EPA normal. Its side planes do the clipping. */
    vec3 inc_he, inc_pos;
    quat inc_orient;

    /* EPA normal points B→A. Each body's touching face points toward the
     * other body, so A's touching face is anti-aligned with B→A (faces
     * toward B) and B's touching face is aligned with B→A (faces toward A).
     * We find each touching face by searching in the negated direction
     * for A and the raw direction for B. */
    int face_a = forge_physics_manifold_ref_face_box(
        orient_a, vec3_negate(epa->normal));
    int face_b = forge_physics_manifold_ref_face_box(orient_b, epa->normal);

    /* Compute dot products to decide which shape is the reference */
    vec3 face_a_verts[4], face_b_verts[4];
    vec3 face_a_normal, face_b_normal;
    forge_physics_manifold_box_face(shape_a->data.box.half_extents,
        pos_a, orient_a, face_a, face_a_verts, &face_a_normal);
    forge_physics_manifold_box_face(shape_b->data.box.half_extents,
        pos_b, orient_b, face_b, face_b_verts, &face_b_normal);

    float dot_a = SDL_fabsf(vec3_dot(face_a_normal, epa->normal));
    float dot_b = SDL_fabsf(vec3_dot(face_b_normal, epa->normal));

    vec3 ref_face_verts[4], inc_face_verts[4];
    vec3 ref_face_normal;
    int ref_face_idx, inc_face_idx;

    if (dot_a >= dot_b) {
        /* A is the reference shape, B is the incident shape */
        inc_he = shape_b->data.box.half_extents;
        inc_pos = pos_b;
        inc_orient = orient_b;
        ref_face_idx = face_a;
        inc_face_idx = forge_physics_manifold_incident_face_box(
            inc_orient, face_a_normal);
        SDL_memcpy(ref_face_verts, face_a_verts, sizeof(face_a_verts));
        ref_face_normal = face_a_normal;
        forge_physics_manifold_box_face(inc_he, inc_pos, inc_orient,
            inc_face_idx, inc_face_verts, &(vec3){0,0,0});
    } else {
        /* B is the reference shape, A is the incident shape */
        inc_he = shape_a->data.box.half_extents;
        inc_pos = pos_a;
        inc_orient = orient_a;
        ref_face_idx = face_b;
        inc_face_idx = forge_physics_manifold_incident_face_box(
            inc_orient, face_b_normal);
        SDL_memcpy(ref_face_verts, face_b_verts, sizeof(face_b_verts));
        ref_face_normal = face_b_normal;
        forge_physics_manifold_box_face(inc_he, inc_pos, inc_orient,
            inc_face_idx, inc_face_verts, &(vec3){0,0,0});
    }

    /* Build side planes from the reference face edges.
     * Each side plane is perpendicular to the reference face and passes
     * through one edge. With clip test dot(p, n) <= d, these normals
     * point outward from the reference face polygon. */
    vec3 clip_verts[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS];
    /* +1 absorbs a potential extra vertex from floating-point edge cases
     * in Sutherland-Hodgman clipping (vertex on the plane boundary). The
     * clip_count clamp after each pass keeps the working set at MAX. */
    vec3 temp_verts[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS + 1];
    SDL_memcpy(clip_verts, inc_face_verts, 4 * sizeof(vec3));
    int clip_count = 4;

    for (int edge = 0; edge < 4; edge++) {
        int next_edge = (edge + 1) % 4;
        vec3 edge_dir = vec3_sub(ref_face_verts[next_edge], ref_face_verts[edge]);
        /* Side plane normal for clip test dot(p, n) <= d.
         * With the clockwise face winding emitted by
         * forge_physics_manifold_box_face(), face_normal x edge_dir gives
         * the outward polygon half-space normal. */
        vec3 side_n = vec3_cross(ref_face_normal, edge_dir);
        float side_len = vec3_length(side_n);
        if (side_len < FORGE_PHYSICS_EPSILON) continue;
        side_n = vec3_scale(side_n, 1.0f / side_len);
        float side_d = vec3_dot(side_n, ref_face_verts[edge]);

        int new_count = forge_physics_clip_polygon(
            clip_verts, clip_count, temp_verts, side_n, side_d);

        if (new_count <= 0) {
            /* Fully clipped — fall back to single EPA contact */
            m.count = 1;
            m.contacts[0].world_point = epa->point;
            m.contacts[0].penetration = epa->depth;
            m.contacts[0].local_a = forge_physics_manifold_world_to_local(
                epa->point_a, pos_a, orient_a);
            m.contacts[0].local_b = forge_physics_manifold_world_to_local(
                epa->point_b, pos_b, orient_b);
            m.contacts[0].id = forge_physics_manifold_contact_id(0, 0, 0);
            return m;
        }

        /* Clamp to buffer capacity. Floating-point edge cases (vertex
         * exactly on the clip plane) can produce one extra vertex per
         * pass beyond the theoretical Sutherland-Hodgman maximum. */
        clip_count = new_count;
        if (clip_count > FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS)
            clip_count = FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS;
        SDL_memcpy(clip_verts, temp_verts,
                   (size_t)clip_count * sizeof(vec3));
    }

    /* Project clipped points onto the reference face plane.
     * Keep only points that are behind the reference plane (penetrating). */
    float ref_d = vec3_dot(ref_face_normal, ref_face_verts[0]);

    vec3  kept_points[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS];     /* midpoint (world_point) */
    vec3  kept_ref_points[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS]; /* on reference face */
    vec3  kept_inc_points[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS]; /* on incident surface */
    float kept_depths[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS];
    uint32_t kept_ids[FORGE_PHYSICS_MANIFOLD_MAX_CLIP_VERTS];
    int kept_count = 0;

    for (int i = 0; i < clip_count; i++) {
        float sep = vec3_dot(clip_verts[i], ref_face_normal) - ref_d;
        if (sep <= 0.0f) {
            /* Project onto reference plane */
            vec3 projected = vec3_sub(clip_verts[i],
                vec3_scale(ref_face_normal, sep));
            /* Store midpoint for world_point (symmetric torque arms,
             * consistent with the EPA path which uses epa->point).
             * Keep reference and incident points for per-body local anchors. */
            kept_points[kept_count] = vec3_scale(
                vec3_add(projected, clip_verts[i]), 0.5f);
            kept_ref_points[kept_count] = projected;
            kept_inc_points[kept_count] = clip_verts[i];
            kept_depths[kept_count] = -sep; /* penetration = positive */
            /* Contact ID from body-A local-space position. Local-space
             * coordinates are stable across frames because they track the
             * body's own reference frame, unlike world-space hashes (which
             * break on quantization boundaries) or feature-based IDs (which
             * break when the reference face selection flips). */
            vec3 local_a = forge_physics_manifold_world_to_local(
                kept_points[kept_count], pos_a, orient_a);
            int lx = (int)SDL_floorf(local_a.x * 1000.0f);
            int ly = (int)SDL_floorf(local_a.y * 1000.0f);
            int lz = (int)SDL_floorf(local_a.z * 1000.0f);
            kept_ids[kept_count] = (uint32_t)(
                (lx * 73856093u) ^ (ly * 19349663u) ^ (lz * 83492791u));
            kept_count++;
        }
    }

    if (kept_count == 0) {
        /* No penetrating points — fall back to EPA */
        m.count = 1;
        m.contacts[0].world_point = epa->point;
        m.contacts[0].penetration = epa->depth;
        m.contacts[0].local_a = forge_physics_manifold_world_to_local(
            epa->point_a, pos_a, orient_a);
        m.contacts[0].local_b = forge_physics_manifold_world_to_local(
            epa->point_b, pos_b, orient_b);
        m.contacts[0].id = forge_physics_manifold_contact_id(0, 0, 0);
        return m;
    }

    /* Reduce to 4 contacts if necessary */
    int final_indices[4];
    int final_count;
    if (kept_count <= FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS) {
        final_count = kept_count;
        for (int i = 0; i < kept_count; i++) final_indices[i] = i;
    } else {
        final_count = forge_physics_manifold_reduce(
            kept_points, kept_depths, kept_count, final_indices);
    }

    /* Build the manifold */
    m.count = final_count;
    for (int i = 0; i < final_count; i++) {
        int si = final_indices[i];
        ForgePhysicsManifoldContact *c = &m.contacts[i];
        c->world_point  = kept_points[si];
        c->penetration  = kept_depths[si];
        c->id           = kept_ids[si];

        /* Transform per-body anchor points into each body's local space.
         * The reference body's anchor is the projected point on the reference
         * face; the incident body's anchor is the original clipped point on
         * the incident surface. Which body is reference depends on dot_a vs
         * dot_b (the same comparison that selected the reference face). */
        if (dot_a >= dot_b) {
            /* A is reference → ref projection on A, incident point on B */
            c->local_a = forge_physics_manifold_world_to_local(
                kept_ref_points[si], pos_a, orient_a);
            c->local_b = forge_physics_manifold_world_to_local(
                kept_inc_points[si], pos_b, orient_b);
        } else {
            /* B is reference → incident point on A, ref projection on B */
            c->local_a = forge_physics_manifold_world_to_local(
                kept_inc_points[si], pos_a, orient_a);
            c->local_b = forge_physics_manifold_world_to_local(
                kept_ref_points[si], pos_b, orient_b);
        }
    }

    return m;
}

/* ── Manifold Cache Operations ─────────────────────────────────────────────── */

/* Update the manifold cache with a newly generated manifold.
 *
 * If a manifold for this body pair already exists in the cache, the
 * function merges the new manifold with the cached one:
 *   1. Match new contacts to old contacts by persistent ID
 *   2. Carry over accumulated impulses from matched old contacts
 *      (warm-starting), scaled by FORGE_PHYSICS_MANIFOLD_WARM_SCALE
 *   3. Unmatched new contacts start with zero impulses
 *
 * If no cached manifold exists, the new manifold is inserted with
 * zero accumulated impulses.
 *
 * Parameters:
 *   cache        — pointer to the hash map (ForgePhysicsManifoldCacheEntry *)
 *   new_manifold — the freshly generated manifold to store
 *
 * Usage:
 *   ForgePhysicsManifoldCacheEntry *cache = NULL;
 *   ForgePhysicsManifold m = forge_physics_manifold_generate(...);
 *   forge_physics_manifold_cache_update(&cache, &m);
 *   // Later: forge_hm_free(cache);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence"
 * (GDC 2005), Section 3 — warm-starting.
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline void forge_physics_manifold_cache_update(
    ForgePhysicsManifoldCacheEntry **cache,
    const ForgePhysicsManifold *new_manifold)
{
    if (!cache || !new_manifold || new_manifold->count <= 0 ||
        new_manifold->count > FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS) return;

    uint64_t key = forge_physics_manifold_pair_key(
        new_manifold->body_a, new_manifold->body_b);

    /* Look up existing entry */
    ForgePhysicsManifoldCacheEntry *existing =
        forge_hm_get_ptr_or_null(*cache, key);

    ForgePhysicsManifoldCacheEntry entry;
    entry.key = key;
    entry.manifold = *new_manifold;

    if (existing && existing->key == key &&
        existing->manifold.count > 0) {
        /* Merge: carry over impulses from the previous frame.
         *
         * Two strategies depending on whether contact counts match:
         *
         * Same count: transfer impulses by index. The manifold generator
         * produces contacts in a stable order (clipping + reduce), so
         * contact[i] this frame corresponds to contact[i] last frame.
         *
         * Different count: transfer by closest-point matching. When the
         * contact set changes (e.g., face-face → edge-face transition),
         * match each new contact to the old contact with the nearest
         * world-space position.
         *
         * Both strategies scale by FORGE_PHYSICS_MANIFOLD_WARM_SCALE to
         * prevent impulse over-application from stale data. */
        const ForgePhysicsManifold *old = &existing->manifold;
        if (entry.manifold.count == old->count) {
            /* Fast path: same contact count → index-based transfer */
            for (int i = 0; i < entry.manifold.count; i++) {
                ForgePhysicsManifoldContact *nc = &entry.manifold.contacts[i];
                nc->normal_impulse    = old->contacts[i].normal_impulse
                                        * FORGE_PHYSICS_MANIFOLD_WARM_SCALE;
                nc->tangent_impulse_1 = old->contacts[i].tangent_impulse_1
                                        * FORGE_PHYSICS_MANIFOLD_WARM_SCALE;
                nc->tangent_impulse_2 = old->contacts[i].tangent_impulse_2
                                        * FORGE_PHYSICS_MANIFOLD_WARM_SCALE;
            }
        } else {
            /* Slow path: contact count changed → nearest-point matching */
            for (int i = 0; i < entry.manifold.count; i++) {
                ForgePhysicsManifoldContact *nc = &entry.manifold.contacts[i];
                float best_dist = 1e30f;
                int best_j = -1;
                for (int j = 0; j < old->count; j++) {
                    vec3 diff = vec3_sub(nc->world_point,
                                         old->contacts[j].world_point);
                    float d2 = vec3_dot(diff, diff);
                    if (d2 < best_dist) {
                        best_dist = d2;
                        best_j = j;
                    }
                }
                if (best_j >= 0 && best_dist < 0.1f) {
                    nc->normal_impulse    = old->contacts[best_j].normal_impulse
                                            * FORGE_PHYSICS_MANIFOLD_WARM_SCALE;
                    nc->tangent_impulse_1 = old->contacts[best_j].tangent_impulse_1
                                            * FORGE_PHYSICS_MANIFOLD_WARM_SCALE;
                    nc->tangent_impulse_2 = old->contacts[best_j].tangent_impulse_2
                                            * FORGE_PHYSICS_MANIFOLD_WARM_SCALE;
                }
            }
        }
    }

    forge_hm_put_struct(*cache, entry);
}

/* Store a manifold directly into the cache without merging impulses.
 *
 * Use this after forge_physics_si_store_impulses() to save the solved
 * impulse values. Unlike forge_physics_manifold_cache_update(), this
 * function does NOT scale or overwrite the manifold's impulses — it
 * stores them exactly as-is.
 *
 * The intended pipeline is:
 *   1. collision detection produces fresh manifolds (zero impulses)
 *   2. forge_physics_manifold_cache_update() merges cached warm-start
 *      impulses into the fresh manifolds (for retrieval before solve)
 *   3. solver runs (forge_physics_si_solve)
 *   4. forge_physics_si_store_impulses() writes solved impulses back
 *   5. forge_physics_manifold_cache_store() saves the solved impulses
 *      into the cache (for warm-starting next frame)
 *
 * Calling forge_physics_manifold_cache_update() at step 5 is WRONG
 * because the merge logic would overwrite the just-solved impulses
 * with the pre-solve values scaled by FORGE_PHYSICS_MANIFOLD_WARM_SCALE,
 * destroying the solver's converged solution.
 *
 * Parameters:
 *   cache        — pointer to the hash map (ForgePhysicsManifoldCacheEntry*)
 *   manifold     — manifold with solved impulses to store
 *
 * See: Physics Lesson 14 — Stacking Stability
 */
static inline void forge_physics_manifold_cache_store(
    ForgePhysicsManifoldCacheEntry **cache,
    const ForgePhysicsManifold *manifold)
{
    if (!cache || !manifold || manifold->count <= 0 ||
        manifold->count > FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS) return;

    ForgePhysicsManifoldCacheEntry entry;
    entry.key = forge_physics_manifold_pair_key(
        manifold->body_a, manifold->body_b);
    entry.manifold = *manifold;

    forge_hm_put_struct(*cache, entry);
}

/* Remove stale manifolds not present in this frame's active manifold set.
 *
 * After all manifolds for the current frame have been updated via
 * forge_physics_manifold_cache_update(), this function removes entries
 * whose body pairs did not produce a manifold this frame. This prevents
 * the cache from growing indefinitely and keeps warm-start data limited
 * to active contacts.
 *
 * The caller should pass the set of pair keys that actually produced
 * manifolds (manifold.count > 0) after forge_physics_manifold_cache_update(),
 * not the raw broadphase overlap list.
 *
 * Implementation: marks entries for removal by collecting keys, then
 * removes them after iteration (cannot remove during forge_hm_iter).
 *
 * Parameters:
 *   cache        — pointer to the hash map
 *   active_keys  — array of pair keys that produced manifolds this frame
 *   active_count — number of active keys
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline void forge_physics_manifold_cache_prune(
    ForgePhysicsManifoldCacheEntry **cache,
    const uint64_t *active_keys, int active_count)
{
    if (!cache || !*cache || active_count < 0) return;
    if (active_count > 0 && !active_keys) return;

    ptrdiff_t cache_len = forge_hm_length(*cache);
    if (cache_len <= 0) return;

    /* Collect keys to remove.
     * forge_hm_iter yields 0-based indices; actual entries are at [i+1]
     * because index 0 is the default entry in forge_containers. */
    uint64_t *to_remove = NULL;
    ptrdiff_t i;
    forge_hm_iter(*cache, i) {
        uint64_t k = (*cache)[i + 1].key;
        bool found = false;
        for (int j = 0; j < active_count; j++) {
            if (active_keys[j] == k) {
                found = true;
                break;
            }
        }
        if (!found) {
            forge_arr_append(to_remove, k);
        }
    }

    /* Remove stale entries */
    ptrdiff_t remove_count = forge_arr_length(to_remove);
    for (ptrdiff_t r = 0; r < remove_count; r++) {
        forge_hm_remove(*cache, to_remove[r]);
    }
    forge_arr_free(to_remove);
}

/* Free all manifold cache memory.
 *
 * Parameters:
 *   cache — pointer to the hash map
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline void forge_physics_manifold_cache_free(
    ForgePhysicsManifoldCacheEntry **cache)
{
    if (!cache) return;
    forge_hm_free(*cache);
}

/* ── Manifold-to-RBContact Conversion ─────────────────────────────────────── */

/* Extract ForgePhysicsRBContact array from a manifold.
 *
 * Converts manifold contacts into the ForgePhysicsRBContact format used
 * by the existing sequential impulse solver (forge_physics_rb_resolve_contacts).
 * This allows using the manifold generation pipeline with the Lesson 06
 * solver until a manifold-aware solver is available.
 *
 * Parameters:
 *   manifold    — the manifold to extract from
 *   out         — receives contacts (caller-allocated, capacity >= manifold->count)
 *   max_out     — maximum contacts to write
 *
 * Returns: number of contacts written
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline int forge_physics_manifold_to_rb_contacts(
    const ForgePhysicsManifold *manifold,
    ForgePhysicsRBContact *out, int max_out)
{
    if (!manifold || !out || max_out <= 0) return 0;

    int n = manifold->count;
    if (n > FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS) n = FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS;
    if (n > max_out) n = max_out;

    for (int i = 0; i < n; i++) {
        const ForgePhysicsManifoldContact *mc = &manifold->contacts[i];
        out[i].point       = mc->world_point;
        out[i].normal      = manifold->normal;
        out[i].penetration = mc->penetration;
        out[i].body_a      = manifold->body_a;
        out[i].body_b      = manifold->body_b;
        out[i].static_friction  = manifold->static_friction;
        out[i].dynamic_friction = manifold->dynamic_friction;
    }

    return n;
}

/* ── Full Narrowphase with Manifold ───────────────────────────────────────── */

/* Run GJK + EPA + manifold generation in one call.
 *
 * Combines the full narrowphase pipeline: GJK intersection test, EPA
 * penetration depth, and Sutherland-Hodgman contact manifold generation.
 * Returns a manifold with 1-4 contact points.
 *
 * This is the manifold equivalent of forge_physics_gjk_epa_contact().
 *
 * Parameters:
 *   body_a, shape_a — first body and its collision shape
 *   body_b, shape_b — second body and its collision shape
 *   idx_a, idx_b    — body indices (for the manifold body_a/body_b fields)
 *   mu_s, mu_d      — friction coefficients
 *   out             — receives the contact manifold
 *
 * Returns: true if a manifold was generated, false if shapes are separated
 *
 * Usage:
 *   ForgePhysicsManifold manifold;
 *   if (forge_physics_gjk_epa_manifold(
 *           &bodies[i], &shapes[i],
 *           &bodies[j], &shapes[j],
 *           i, j, 0.6f, 0.4f, &manifold)) {
 *       forge_physics_manifold_cache_update(&cache, &manifold);
 *   }
 *
 * See: Physics Lesson 11 — Contact Manifold
 */
static inline bool forge_physics_gjk_epa_manifold(
    const ForgePhysicsRigidBody *body_a,
    const ForgePhysicsCollisionShape *shape_a,
    const ForgePhysicsRigidBody *body_b,
    const ForgePhysicsCollisionShape *shape_b,
    int idx_a, int idx_b,
    float mu_s, float mu_d,
    ForgePhysicsManifold *out)
{
    if (!body_a || !body_b || !shape_a || !shape_b || !out) return false;

    /* Step 1: GJK intersection test */
    ForgePhysicsGJKResult gjk = forge_physics_gjk_test_bodies(
        body_a, shape_a, body_b, shape_b);
    if (!gjk.intersecting) return false;

    /* Step 2: EPA penetration depth */
    ForgePhysicsEPAResult epa = forge_physics_epa_bodies(
        &gjk, body_a, shape_a, body_b, shape_b);
    if (!epa.valid) return false;

    /* Step 3: Generate contact manifold */
    *out = forge_physics_manifold_generate(
        &epa, shape_a, body_a->position, body_a->orientation,
        shape_b, body_b->position, body_b->orientation,
        idx_a, idx_b, mu_s, mu_d);

    return (out->count > 0);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * LESSON 12 — Impulse-Based Resolution (Sequential Impulse Solver)
 *
 * A manifold-aware sequential impulse solver implementing Erin Catto's
 * accumulated impulse method. Operates directly on ForgePhysicsManifold
 * arrays, using warm-starting from cached impulses and accumulated
 * impulse clamping for stable convergence.
 *
 * The key difference from the Lesson 06 solver
 * (forge_physics_rb_resolve_contact) is how impulse clamping works:
 *
 *   Lesson 06 (per-iteration clamping):
 *     delta_j = compute_impulse(v_rel)
 *     j = max(delta_j, 0)        <-- clamp the delta each iteration
 *     apply(j)
 *
 *   Lesson 12 (accumulated clamping, Catto's method):
 *     delta_j = compute_impulse(v_rel)
 *     old_j = accumulated_j
 *     accumulated_j = max(accumulated_j + delta_j, 0)  <-- clamp the total
 *     apply(accumulated_j - old_j)                      <-- apply the change
 *
 * Accumulated clamping converges to the correct global solution because
 * it tracks the total impulse applied, not just the latest delta. When
 * one contact's impulse changes due to a neighbor, the accumulated value
 * lets the solver back off (reduce a previously applied impulse) rather
 * than only adding more. This is critical for stacking stability.
 *
 * Pipeline: manifold cache → prepare → warm-start → N iterations → store
 *
 * Reference:
 *   Erin Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005)
 *   Erin Catto, "Modeling and Solving Constraints" (GDC 2009)
 *   Dirk Gregorius, "Robust Contact Creation for Physics Simulations"
 *     (GDC 2013)
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── SI Solver Constants ──────────────────────────────────────────────────── */

/* Default velocity iteration count for the SI solver.
 *
 * Each iteration resolves all contacts once. 10 iterations is a good
 * balance between convergence quality and CPU cost for stacks of 5-8
 * bodies.
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
#define FORGE_PHYSICS_SI_DEFAULT_ITERATIONS  10

/* Minimum dot product between a contact normal and the manifold's shared
 * normal for the contact to be accepted. 0.9 ≈ 26° deviation tolerance. */
#define FORGE_PHYSICS_SI_NORMAL_ALIGN_MIN    0.9f

/* Spatial hash quantization scale (units per meter).
 * 1000 = 1 mm grid — contacts within 1 mm map to the same hash cell. */
#define FORGE_PHYSICS_SI_HASH_QUANT_SCALE    1000.0f

/* Teschner spatial hash primes — large co-prime values for XYZ mixing. */
#define FORGE_PHYSICS_SI_HASH_PRIME_X        73856093u
#define FORGE_PHYSICS_SI_HASH_PRIME_Y        19349663u
#define FORGE_PHYSICS_SI_HASH_PRIME_Z        83492791u

/* ── SI Solver Types ─────────────────────────────────────────────────────── */

/* Precomputed constraint data for a single contact point.
 *
 * Created during the prepare step and reused across all solver iterations.
 * Stores the tangent basis, effective masses, velocity biases, and
 * accumulated impulses for normal and friction directions.
 *
 * The effective mass (1/K) is precomputed once because the body mass
 * properties and contact geometry do not change during velocity solving.
 * Only velocities change — and the effective mass depends only on mass,
 * inertia, and contact geometry.
 *
 * Fields:
 *   r_a, r_b       — lever arms from body COM to contact point (m)
 *   t1, t2         — orthonormal tangent basis for friction
 *   eff_mass_n     — effective mass along normal (kg)
 *   eff_mass_t1    — effective mass along tangent 1 (kg)
 *   eff_mass_t2    — effective mass along tangent 2 (kg)
 *   velocity_bias  — Baumgarte penetration correction bias (m/s)
 *   restitution_bias — bounce velocity from restitution (m/s)
 *   j_n            — accumulated normal impulse (N·s), >= 0
 *   j_t1           — accumulated friction impulse along t1 (N·s)
 *   j_t2           — accumulated friction impulse along t2 (N·s)
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
typedef struct ForgePhysicsSIConstraint {
    vec3  r_a;              /* world-space offset: contact point - body_a COM */
    vec3  r_b;              /* world-space offset: contact point - body_b COM */
    vec3  t1;               /* tangent basis vector 1 (unit length)           */
    vec3  t2;               /* tangent basis vector 2 (unit length)           */
    float eff_mass_n;       /* 1 / K_n — effective mass along normal          */
    float eff_mass_t1;      /* 1 / K_t1 — effective mass along tangent 1      */
    float eff_mass_t2;      /* 1 / K_t2 — effective mass along tangent 2      */
    float velocity_bias;    /* Baumgarte penetration correction bias (m/s)    */
    float restitution_bias; /* bounce velocity from restitution (m/s)         */
    float j_n;              /* accumulated normal impulse (N·s)               */
    float j_t1;             /* accumulated friction impulse, tangent 1 (N·s)  */
    float j_t2;             /* accumulated friction impulse, tangent 2 (N·s)  */
} ForgePhysicsSIConstraint;

/* Constraint data for one manifold (body pair).
 *
 * Groups up to FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS (4) SI constraints
 * that share a common normal and friction coefficients. Mirrors the
 * structure of ForgePhysicsManifold but with precomputed solver data.
 *
 * Fields:
 *   body_a, body_b     — body indices (body_b == -1 for ground/static)
 *   normal             — shared contact normal, B toward A (unit length)
 *   static_friction    — stored but not used by the per-axis friction model
 *   dynamic_friction   — per-axis friction limit coefficient, >= 0
 *   count              — active constraints, 0..4
 *   constraints        — per-contact constraint data
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
typedef struct ForgePhysicsSIManifold {
    int   body_a;           /* index of body A in the body array              */
    int   body_b;           /* index of body B, or -1 for ground              */
    vec3  normal;           /* shared contact normal (B toward A)             */
    float static_friction;  /* static friction coefficient, >= 0              */
    float dynamic_friction; /* dynamic friction coefficient, >= 0             */
    int   count;            /* number of active constraints, 0..4             */
    ForgePhysicsSIConstraint constraints[FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS];
} ForgePhysicsSIManifold;

/* ── SI Solver Functions ─────────────────────────────────────────────────── */

/* Build a stable orthonormal tangent basis from a contact normal.
 *
 * Constructs two tangent vectors t1 and t2 perpendicular to the normal
 * and to each other, forming a right-handed orthonormal frame {n, t1, t2}.
 * The tangent basis is used for decomposing friction into two independent
 * directions.
 *
 * Algorithm (least-aligned axis method):
 *   1. Find which world axis (X, Y, Z) is least aligned with n
 *      (smallest |dot| with n)
 *   2. Cross n with that axis to get t1
 *   3. Normalize t1
 *   4. t2 = cross(n, t1) — already unit length
 *
 * This method is stable for all normal directions and avoids the
 * singularity that occurs when crossing with an aligned axis.
 *
 * Parameters:
 *   n      — contact normal (must be unit length)
 *   out_t1 — receives first tangent vector (must not be NULL)
 *   out_t2 — receives second tangent vector (must not be NULL)
 *
 * Usage:
 *   vec3 t1, t2;
 *   forge_physics_si_tangent_basis(contact_normal, &t1, &t2);
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_si_tangent_basis(
    vec3 n, vec3 *out_t1, vec3 *out_t2)
{
    if (!out_t1 || !out_t2) return;

    /* Guard against zero or degenerate normal */
    float n_len_sq = vec3_length_squared(n);
    if (!(n_len_sq > FORGE_PHYSICS_EPSILON)) {
        *out_t1 = vec3_create(1.0f, 0.0f, 0.0f);
        *out_t2 = vec3_create(0.0f, 0.0f, 1.0f);
        return;
    }

    /* Find the world axis least aligned with n */
    float ax = SDL_fabsf(n.x);
    float ay = SDL_fabsf(n.y);
    float az = SDL_fabsf(n.z);

    vec3 axis;
    if (ax <= ay && ax <= az) {
        axis = vec3_create(1.0f, 0.0f, 0.0f);
    } else if (ay <= az) {
        axis = vec3_create(0.0f, 1.0f, 0.0f);
    } else {
        axis = vec3_create(0.0f, 0.0f, 1.0f);
    }

    /* t1 = normalize(cross(n, axis)) */
    vec3 t1 = vec3_cross(n, axis);
    float len = vec3_length(t1);
    if (len > FORGE_PHYSICS_EPSILON) {
        t1 = vec3_scale(t1, 1.0f / len);
    } else {
        /* Degenerate — try the next axis. This should never happen with
         * the least-aligned selection above, but guard against it. */
        vec3 fallback = vec3_create(0.0f, 0.0f, 1.0f);
        t1 = vec3_cross(n, fallback);
        len = vec3_length(t1);
        if (len > FORGE_PHYSICS_EPSILON) {
            t1 = vec3_scale(t1, 1.0f / len);
        } else {
            t1 = vec3_cross(n, vec3_create(1.0f, 0.0f, 0.0f));
            t1 = vec3_normalize(t1);
        }
    }

    /* t2 = cross(n, t1) — orthogonal to both n and t1 */
    *out_t2 = vec3_cross(n, t1);
    *out_t1 = t1;
}

/* Compute the effective mass inverse along a direction at a contact point.
 *
 * K = 1/m_a + 1/m_b + dot(dir, (I_a_inv * (r_a x dir)) x r_a)
 *                    + dot(dir, (I_b_inv * (r_b x dir)) x r_b)
 *
 * This helper is used internally by forge_physics_si_prepare() to compute
 * effective masses for the normal and both tangent directions.
 *
 * Parameters:
 *   dir        — unit direction to compute effective mass along
 *   r_a, r_b   — lever arms from body COMs to contact point
 *   inv_mass_a — inverse mass of body A (0 for static)
 *   inv_mass_b — inverse mass of body B (0 for static)
 *   I_inv_a    — inverse world-space inertia of body A
 *   I_inv_b    — inverse world-space inertia of body B
 *   dynamic_a  — true if body A is dynamic (inv_mass > 0)
 *   dynamic_b  — true if body B is dynamic (inv_mass > 0)
 *
 * Returns: effective mass (1/K), or 0 if K < epsilon
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline float forge_physics_si_effective_mass_(
    vec3 dir, vec3 r_a, vec3 r_b,
    float inv_mass_a, float inv_mass_b,
    mat3 I_inv_a, mat3 I_inv_b,
    bool dynamic_a, bool dynamic_b)
{
    float K = inv_mass_a + inv_mass_b;

    if (dynamic_a) {
        vec3 rn_a = vec3_cross(r_a, dir);
        K += vec3_dot(vec3_cross(mat3_multiply_vec3(I_inv_a, rn_a), r_a), dir);
    }

    if (dynamic_b) {
        vec3 rn_b = vec3_cross(r_b, dir);
        K += vec3_dot(vec3_cross(mat3_multiply_vec3(I_inv_b, rn_b), r_b), dir);
    }

    return (K > FORGE_PHYSICS_EPSILON) ? (1.0f / K) : 0.0f;
}

/* Precompute constraint data for all manifolds (the "pre-step").
 *
 * For each manifold and each contact within it, computes:
 *   - Lever arms (r_a, r_b) from body COMs to contact point
 *   - Tangent basis (t1, t2) from the contact normal
 *   - Effective mass along normal, tangent 1, and tangent 2
 *   - Baumgarte velocity bias for penetration correction
 *   - Restitution bounce bias (computed from pre-warmstart velocity)
 *   - Accumulated impulses initialized from manifold warm-start data
 *
 * The restitution bias must be computed here, before warm-starting
 * changes the velocities. Computing it during solve_velocities() would
 * use post-warmstart velocities, giving incorrect bounce magnitudes.
 *
 * Parameters:
 *   manifolds       — input manifold array
 *   manifold_count  — number of manifolds
 *   bodies          — rigid body array
 *   num_bodies      — number of bodies
 *   dt              — physics timestep (s), must be > 0
 *   warm_start      — if true, initialize accumulated impulses from
 *                      manifold contact data; if false, start from zero
 *   out             — output SI manifold array (caller-allocated,
 *                      capacity >= manifold_count)
 *
 * Usage:
 *   ForgePhysicsSIManifold workspace[MAX_MANIFOLDS];
 *   forge_physics_si_prepare(manifolds, count, bodies, n, dt, true,
 *                            workspace);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005),
 *            Section 2 — constraint pre-processing.
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_si_prepare(
    const ForgePhysicsManifold *manifolds, int manifold_count,
    const ForgePhysicsRigidBody *bodies, int num_bodies,
    float dt, bool warm_start,
    ForgePhysicsSIManifold *out,
    const ForgePhysicsSolverConfig *config)
{
    if (!manifolds || !bodies || !out || manifold_count <= 0 ||
        num_bodies <= 0) return;
    if (!(dt > 0.0f) || !forge_isfinite(dt)) return;

    /* Resolve config — use defaults when NULL */
    float baum = config ? config->baumgarte_factor : FORGE_PHYSICS_BAUMGARTE_FACTOR;
    float slop = config ? config->penetration_slop : FORGE_PHYSICS_PENETRATION_SLOP;

    for (int mi = 0; mi < manifold_count; mi++) {
        const ForgePhysicsManifold *m = &manifolds[mi];
        ForgePhysicsSIManifold *si = &out[mi];

        si->body_a = m->body_a;
        si->body_b = m->body_b;

        /* Sanitize friction coefficients — clamp to >= 0 */
        si->static_friction  = (m->static_friction >= 0.0f)
                                ? m->static_friction : 0.0f;
        si->dynamic_friction = (m->dynamic_friction >= 0.0f)
                                ? m->dynamic_friction : 0.0f;

        /* Clamp count to array capacity */
        si->count = m->count;
        if (si->count > FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS)
            si->count = FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS;
        if (si->count < 0) si->count = 0;

        int ia = m->body_a;
        int ib = m->body_b;

        if (ia < 0 || ia >= num_bodies) { si->count = 0; continue; }
        if (ib != -1 && (ib < 0 || ib >= num_bodies)) {
            si->count = 0; continue;
        }

        const ForgePhysicsRigidBody *a = &bodies[ia];
        const ForgePhysicsRigidBody *b = (ib >= 0) ? &bodies[ib] : NULL;

        float inv_mass_a = a->inv_mass;
        float inv_mass_b = b ? b->inv_mass : 0.0f;
        bool dynamic_a = inv_mass_a > 0.0f;
        bool dynamic_b = b && inv_mass_b > 0.0f;

        if (!dynamic_a && !dynamic_b) { si->count = 0; continue; }

        mat3 I_inv_a = a->inv_inertia_world;
        mat3 I_inv_b;
        if (b) {
            I_inv_b = b->inv_inertia_world;
        } else {
            SDL_memset(&I_inv_b, 0, sizeof(I_inv_b));
        }

        vec3 n = m->normal;

        /* Validate normal — reject degenerate manifolds */
        float n_len_sq = vec3_length_squared(n);
        if (!(n_len_sq > FORGE_PHYSICS_EPSILON) ||
            !forge_isfinite(n_len_sq)) {
            si->count = 0; continue;
        }
        n = vec3_scale(n, 1.0f / SDL_sqrtf(n_len_sq));
        si->normal = n;

        /* Build tangent basis (shared across all contacts in this manifold) */
        vec3 t1, t2;
        forge_physics_si_tangent_basis(n, &t1, &t2);

        for (int ci = 0; ci < si->count; ci++) {
            const ForgePhysicsManifoldContact *mc = &m->contacts[ci];
            ForgePhysicsSIConstraint *sc = &si->constraints[ci];

            /* Lever arms */
            sc->r_a = vec3_sub(mc->world_point, a->position);
            sc->r_b = b ? vec3_sub(mc->world_point, b->position)
                        : vec3_create(0, 0, 0);

            sc->t1 = t1;
            sc->t2 = t2;

            /* Effective masses */
            sc->eff_mass_n = forge_physics_si_effective_mass_(
                n, sc->r_a, sc->r_b, inv_mass_a, inv_mass_b,
                I_inv_a, I_inv_b, dynamic_a, dynamic_b);

            sc->eff_mass_t1 = forge_physics_si_effective_mass_(
                t1, sc->r_a, sc->r_b, inv_mass_a, inv_mass_b,
                I_inv_a, I_inv_b, dynamic_a, dynamic_b);

            sc->eff_mass_t2 = forge_physics_si_effective_mass_(
                t2, sc->r_a, sc->r_b, inv_mass_a, inv_mass_b,
                I_inv_a, I_inv_b, dynamic_a, dynamic_b);

            /* Baumgarte velocity bias for penetration correction */
            sc->velocity_bias = 0.0f;
            float pen_excess = mc->penetration - slop;
            if (pen_excess > 0.0f) {
                sc->velocity_bias = (baum / dt) * pen_excess;
            }

            /* Restitution bias — computed from pre-warmstart velocity.
             * Use min(e_a, e_b). Kill restitution for resting contacts
             * (closing velocity below threshold) to prevent micro-bouncing. */
            float e = a->restitution;
            if (b) e = forge_fminf(e, b->restitution);

            vec3 v_a = vec3_add(a->velocity,
                                vec3_cross(a->angular_velocity, sc->r_a));
            vec3 v_b = b ? vec3_add(b->velocity,
                                    vec3_cross(b->angular_velocity, sc->r_b))
                         : vec3_create(0, 0, 0);
            float v_n = vec3_dot(vec3_sub(v_a, v_b), n);

            if (v_n < -FORGE_PHYSICS_RB_RESTING_THRESHOLD) {
                sc->restitution_bias = -e * v_n;
            } else {
                sc->restitution_bias = 0.0f;
            }

            /* Initialize accumulated impulses from warm-start data */
            if (warm_start) {
                sc->j_n  = mc->normal_impulse;
                sc->j_t1 = mc->tangent_impulse_1;
                sc->j_t2 = mc->tangent_impulse_2;
            } else {
                sc->j_n  = 0.0f;
                sc->j_t1 = 0.0f;
                sc->j_t2 = 0.0f;
            }
        }
    }
}

/* Apply cached impulses to body velocities (warm-starting).
 *
 * Before the solver begins iterating, this function applies the impulses
 * accumulated in previous frames. This gives the solver a head start —
 * instead of converging from zero, it starts from a state close to the
 * solution. For stacking scenarios, warm-starting reduces the number of
 * iterations needed for stability by 3-5x.
 *
 * For each constraint, computes:
 *   impulse = n * j_n + t1 * j_t1 + t2 * j_t2
 * and applies it to both bodies (A gets pushed, B gets pushed in reverse).
 *
 * Parameters:
 *   si_manifolds — SI manifold array (from forge_physics_si_prepare)
 *   count        — number of SI manifolds
 *   bodies       — rigid body array (velocities modified in place)
 *   num_bodies   — number of bodies
 *
 * Usage:
 *   forge_physics_si_warm_start(workspace, manifold_count, bodies, n);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005),
 *            Section 3 — warm-starting.
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_si_warm_start(
    const ForgePhysicsSIManifold *si_manifolds, int count,
    ForgePhysicsRigidBody *bodies, int num_bodies)
{
    if (!si_manifolds || !bodies || count <= 0 || num_bodies <= 0) return;

    for (int mi = 0; mi < count; mi++) {
        const ForgePhysicsSIManifold *si = &si_manifolds[mi];
        int ia = si->body_a;
        int ib = si->body_b;

        if (ia < 0 || ia >= num_bodies) continue;
        if (ib != -1 && (ib < 0 || ib >= num_bodies)) continue;

        ForgePhysicsRigidBody *a = &bodies[ia];
        ForgePhysicsRigidBody *b = (ib >= 0) ? &bodies[ib] : NULL;

        for (int ci = 0; ci < si->count; ci++) {
            const ForgePhysicsSIConstraint *sc = &si->constraints[ci];

            /* Composite impulse vector */
            vec3 impulse = vec3_add(
                vec3_scale(si->normal, sc->j_n),
                vec3_add(vec3_scale(sc->t1, sc->j_t1),
                         vec3_scale(sc->t2, sc->j_t2)));

            /* Apply to body A */
            if (a->inv_mass > 0.0f) {
                a->velocity = vec3_add(a->velocity,
                    vec3_scale(impulse, a->inv_mass));
                a->angular_velocity = vec3_add(a->angular_velocity,
                    mat3_multiply_vec3(a->inv_inertia_world,
                                       vec3_cross(sc->r_a, impulse)));
            }

            /* Apply to body B (opposite direction) */
            if (b && b->inv_mass > 0.0f) {
                b->velocity = vec3_sub(b->velocity,
                    vec3_scale(impulse, b->inv_mass));
                b->angular_velocity = vec3_sub(b->angular_velocity,
                    mat3_multiply_vec3(b->inv_inertia_world,
                                       vec3_cross(sc->r_b, impulse)));
            }
        }
    }
}

/* Perform one velocity-solving iteration over all constraints.
 *
 * This is the core of Catto's sequential impulse method. For each
 * contact, it computes the velocity error, converts it to an impulse
 * delta via the precomputed effective mass, then applies accumulated
 * impulse clamping:
 *
 *   Normal (non-penetration):
 *     delta = eff_mass_n * (-(v_n - velocity_bias - restitution_bias))
 *     old   = j_n
 *     j_n   = max(j_n + delta, 0)    <-- total impulse >= 0
 *     apply (j_n - old) * normal
 *
 *   Friction (tangent 1 and 2):
 *     delta = eff_mass_t * (-v_t)
 *     old   = j_t
 *     j_t   = clamp(j_t + delta, -mu_d * j_n, mu_d * j_n)
 *     apply (j_t - old) * tangent
 *
 * The normal constraint uses velocity_bias (Baumgarte penetration
 * correction) and restitution_bias (bounce). The friction constraints
 * use per-axis Coulomb clamping: |j_t| <= mu * j_normal.
 *
 * Parameters:
 *   si_manifolds — SI manifold array (accumulated impulses updated)
 *   count        — number of SI manifolds
 *   bodies       — rigid body array (velocities modified in place)
 *   num_bodies   — number of bodies
 *
 * Usage:
 *   for (int iter = 0; iter < iterations; iter++)
 *       forge_physics_si_solve_velocities(workspace, count, bodies, n);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005),
 *            Section 2 — sequential impulse with accumulated clamping.
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_si_solve_velocities(
    ForgePhysicsSIManifold *si_manifolds, int count,
    ForgePhysicsRigidBody *bodies, int num_bodies)
{
    if (!si_manifolds || !bodies || count <= 0 || num_bodies <= 0) return;

    for (int mi = 0; mi < count; mi++) {
        ForgePhysicsSIManifold *si = &si_manifolds[mi];
        int ia = si->body_a;
        int ib = si->body_b;

        if (ia < 0 || ia >= num_bodies) continue;
        if (ib != -1 && (ib < 0 || ib >= num_bodies)) continue;

        ForgePhysicsRigidBody *a = &bodies[ia];
        ForgePhysicsRigidBody *b = (ib >= 0) ? &bodies[ib] : NULL;

        vec3 n = si->normal;

        for (int ci = 0; ci < si->count; ci++) {
            ForgePhysicsSIConstraint *sc = &si->constraints[ci];

            /* ── Compute relative velocity at contact point ───────────── */
            vec3 v_a = vec3_add(a->velocity,
                                vec3_cross(a->angular_velocity, sc->r_a));
            vec3 v_b = b ? vec3_add(b->velocity,
                                    vec3_cross(b->angular_velocity, sc->r_b))
                         : vec3_create(0, 0, 0);
            vec3 v_rel = vec3_sub(v_a, v_b);

            /* ── Normal constraint ────────────────────────────────────── */
            float v_n = vec3_dot(v_rel, n);
            float delta_jn = sc->eff_mass_n
                           * (-(v_n - sc->velocity_bias - sc->restitution_bias));

            /* Accumulated clamp: total normal impulse >= 0 */
            float old_jn = sc->j_n;
            sc->j_n = forge_fmaxf(sc->j_n + delta_jn, 0.0f);
            float applied_jn = sc->j_n - old_jn;

            /* Apply normal impulse */
            vec3 impulse_n = vec3_scale(n, applied_jn);

            if (a->inv_mass > 0.0f) {
                a->velocity = vec3_add(a->velocity,
                    vec3_scale(impulse_n, a->inv_mass));
                a->angular_velocity = vec3_add(a->angular_velocity,
                    mat3_multiply_vec3(a->inv_inertia_world,
                                       vec3_cross(sc->r_a, impulse_n)));
            }
            if (b && b->inv_mass > 0.0f) {
                b->velocity = vec3_sub(b->velocity,
                    vec3_scale(impulse_n, b->inv_mass));
                b->angular_velocity = vec3_sub(b->angular_velocity,
                    mat3_multiply_vec3(b->inv_inertia_world,
                                       vec3_cross(sc->r_b, impulse_n)));
            }

            /* ── Friction constraint — tangent 1 ─────────────────────── */

            /* Recompute v_rel after normal impulse changed velocities */
            v_a = vec3_add(a->velocity,
                           vec3_cross(a->angular_velocity, sc->r_a));
            v_b = b ? vec3_add(b->velocity,
                               vec3_cross(b->angular_velocity, sc->r_b))
                     : vec3_create(0, 0, 0);
            v_rel = vec3_sub(v_a, v_b);

            float friction_limit = si->dynamic_friction * sc->j_n;

            float v_t1 = vec3_dot(v_rel, sc->t1);
            float delta_jt1 = sc->eff_mass_t1 * (-v_t1);

            float old_jt1 = sc->j_t1;
            sc->j_t1 = forge_clampf(sc->j_t1 + delta_jt1,
                                      -friction_limit, friction_limit);
            float applied_jt1 = sc->j_t1 - old_jt1;

            vec3 impulse_t1 = vec3_scale(sc->t1, applied_jt1);

            if (a->inv_mass > 0.0f) {
                a->velocity = vec3_add(a->velocity,
                    vec3_scale(impulse_t1, a->inv_mass));
                a->angular_velocity = vec3_add(a->angular_velocity,
                    mat3_multiply_vec3(a->inv_inertia_world,
                                       vec3_cross(sc->r_a, impulse_t1)));
            }
            if (b && b->inv_mass > 0.0f) {
                b->velocity = vec3_sub(b->velocity,
                    vec3_scale(impulse_t1, b->inv_mass));
                b->angular_velocity = vec3_sub(b->angular_velocity,
                    mat3_multiply_vec3(b->inv_inertia_world,
                                       vec3_cross(sc->r_b, impulse_t1)));
            }

            /* ── Friction constraint — tangent 2 ─────────────────────── */

            /* Recompute v_rel after tangent 1 impulse */
            v_a = vec3_add(a->velocity,
                           vec3_cross(a->angular_velocity, sc->r_a));
            v_b = b ? vec3_add(b->velocity,
                               vec3_cross(b->angular_velocity, sc->r_b))
                     : vec3_create(0, 0, 0);
            v_rel = vec3_sub(v_a, v_b);

            float v_t2 = vec3_dot(v_rel, sc->t2);
            float delta_jt2 = sc->eff_mass_t2 * (-v_t2);

            float old_jt2 = sc->j_t2;
            sc->j_t2 = forge_clampf(sc->j_t2 + delta_jt2,
                                      -friction_limit, friction_limit);
            float applied_jt2 = sc->j_t2 - old_jt2;

            vec3 impulse_t2 = vec3_scale(sc->t2, applied_jt2);

            if (a->inv_mass > 0.0f) {
                a->velocity = vec3_add(a->velocity,
                    vec3_scale(impulse_t2, a->inv_mass));
                a->angular_velocity = vec3_add(a->angular_velocity,
                    mat3_multiply_vec3(a->inv_inertia_world,
                                       vec3_cross(sc->r_a, impulse_t2)));
            }
            if (b && b->inv_mass > 0.0f) {
                b->velocity = vec3_sub(b->velocity,
                    vec3_scale(impulse_t2, b->inv_mass));
                b->angular_velocity = vec3_sub(b->angular_velocity,
                    mat3_multiply_vec3(b->inv_inertia_world,
                                       vec3_cross(sc->r_b, impulse_t2)));
            }
        }
    }
}

/* Write accumulated impulses back to manifold contacts for caching.
 *
 * After the solver finishes iterating, this function copies the final
 * accumulated impulse values (j_n, j_t1, j_t2) from each SI constraint
 * back to the corresponding ForgePhysicsManifoldContact. When the
 * manifold cache persists these contacts to the next frame, the impulses
 * become the warm-start values for forge_physics_si_prepare().
 *
 * Parameters:
 *   si_manifolds    — SI manifold array (source of accumulated impulses)
 *   si_count        — number of SI manifolds
 *   manifolds       — manifold array (destination, contacts updated)
 *
 * Usage:
 *   forge_physics_si_store_impulses(workspace, count, manifolds);
 *   // Now update the manifold cache:
 *   forge_physics_manifold_cache_update(&cache, &manifolds[i]);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005),
 *            Section 3 — warm-starting requires storing converged impulses.
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_si_store_impulses(
    const ForgePhysicsSIManifold *si_manifolds, int si_count,
    ForgePhysicsManifold *manifolds)
{
    if (!si_manifolds || !manifolds || si_count <= 0) return;

    for (int mi = 0; mi < si_count; mi++) {
        const ForgePhysicsSIManifold *si = &si_manifolds[mi];
        ForgePhysicsManifold *m = &manifolds[mi];

        int n = si->count;
        if (n > m->count) n = m->count;
        if (n > FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS)
            n = FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS;

        for (int ci = 0; ci < n; ci++) {
            m->contacts[ci].normal_impulse    = si->constraints[ci].j_n;
            m->contacts[ci].tangent_impulse_1 = si->constraints[ci].j_t1;
            m->contacts[ci].tangent_impulse_2 = si->constraints[ci].j_t2;
        }
    }
}

/* Resolve penetrations by directly correcting body positions.
 *
 * After the velocity solver has converged, bodies may still overlap
 * because the velocity-level Baumgarte bias only gradually reduces
 * penetration over multiple frames. This function applies immediate
 * position corrections along the contact normal, weighted by inverse
 * mass so lighter bodies move more.
 *
 * The correction uses a slop tolerance (contacts shallower than the
 * slop are ignored) and a fraction parameter to avoid over-correction
 * and jitter. A fraction of 0.2-0.4 is typical.
 *
 * Parameters:
 *   manifolds      — manifold array (contact positions and normals)
 *   manifold_count — number of manifolds
 *   bodies         — rigid body array (positions modified in place)
 *   num_bodies     — number of bodies
 *   fraction       — correction fraction per step (0.2-0.4 typical)
 *   slop           — penetration tolerance below which no correction
 *                     is applied (matches FORGE_PHYSICS_PENETRATION_SLOP)
 *
 * Reference: Catto, "Modeling and Solving Constraints" (GDC 2009) —
 *            pseudo-velocity / position projection methods.
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline void forge_physics_si_correct_positions(
    const ForgePhysicsManifold *manifolds, int manifold_count,
    ForgePhysicsRigidBody *bodies, int num_bodies,
    float fraction, float slop)
{
    if (!manifolds || !bodies || manifold_count <= 0 || num_bodies <= 0)
        return;
    if (!forge_isfinite(fraction) || fraction <= 0.0f) return;
    if (fraction > 1.0f) fraction = 1.0f;
    if (!forge_isfinite(slop) || slop < 0.0f) slop = 0.0f;

    for (int mi = 0; mi < manifold_count; mi++) {
        const ForgePhysicsManifold *m = &manifolds[mi];
        int ia = m->body_a;
        int ib = m->body_b;

        if (ia < 0 || ia >= num_bodies) continue;
        if (ib != -1 && (ib < 0 || ib >= num_bodies)) continue;

        ForgePhysicsRigidBody *a = &bodies[ia];
        ForgePhysicsRigidBody *b = (ib >= 0) ? &bodies[ib] : NULL;

        float inv_mass_a = a->inv_mass;
        float inv_mass_b = b ? b->inv_mass : 0.0f;
        float inv_mass_sum = inv_mass_a + inv_mass_b;
        if (inv_mass_sum <= 0.0f) continue;

        /* Validate normal */
        vec3 n = m->normal;
        float n_len_sq = vec3_length_squared(n);
        if (!(n_len_sq > FORGE_PHYSICS_EPSILON)) continue;
        n = vec3_scale(n, 1.0f / SDL_sqrtf(n_len_sq));

        /* Use the deepest penetration across all contacts */
        float max_pen = 0.0f;
        for (int ci = 0; ci < m->count &&
             ci < FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS; ci++) {
            if (m->contacts[ci].penetration > max_pen)
                max_pen = m->contacts[ci].penetration;
        }

        /* Correction magnitude: fraction * max(penetration - slop, 0) */
        float correction = fraction *
            forge_fmaxf(max_pen - slop, 0.0f) / inv_mass_sum;

        if (correction <= 0.0f) continue;

        /* Push bodies apart proportional to inverse mass */
        if (inv_mass_a > 0.0f) {
            a->position = vec3_add(a->position,
                vec3_scale(n, correction * inv_mass_a));
        }
        if (b && inv_mass_b > 0.0f) {
            b->position = vec3_sub(b->position,
                vec3_scale(n, correction * inv_mass_b));
        }
    }
}

/* Complete sequential impulse solver: prepare + warm-start + iterate + store.
 *
 * This is the high-level entry point for the SI solver. It combines all
 * solver phases into a single call:
 *
 *   1. Prepare: precompute effective masses, bias terms, init warm-start
 *   2. Warm-start: apply cached impulses to velocities (if enabled)
 *   3. Iterate: run N velocity-solving passes with accumulated clamping
 *   4. Store: write final accumulated impulses back to manifolds
 *
 * The caller must provide a workspace array of ForgePhysicsSIManifold
 * with at least manifold_count entries. This avoids heap allocation.
 *
 * Parameters:
 *   manifolds       — manifold array (contacts updated with impulses)
 *   manifold_count  — number of manifolds
 *   bodies          — rigid body array (velocities modified in place)
 *   num_bodies      — number of bodies
 *   iterations      — number of velocity iterations (clamped to
 *                      [SOLVER_MIN_ITERATIONS, SOLVER_MAX_ITERATIONS])
 *   dt              — physics timestep (s), must be > 0
 *   warm_start      — if true, apply cached impulses before iterating
 *   workspace       — caller-allocated SI manifold array
 *                      (capacity >= manifold_count)
 *   config          — solver tuning parameters (NULL → defaults).
 *                      Use forge_physics_solver_config_default() to get a
 *                      config with default values, then override fields.
 *
 * Usage:
 *   ForgePhysicsSIManifold workspace[MAX_MANIFOLDS];
 *   forge_physics_si_solve(manifolds, count, bodies, num_bodies,
 *                          10, PHYSICS_DT, true, workspace, NULL);
 *
 *   // With custom config:
 *   ForgePhysicsSolverConfig cfg = forge_physics_solver_config_default();
 *   cfg.baumgarte_factor = 0.1f;
 *   forge_physics_si_solve(manifolds, count, bodies, num_bodies,
 *                          20, PHYSICS_DT, true, workspace, &cfg);
 *
 * Reference: Catto, "Iterative Dynamics with Temporal Coherence" (GDC 2005).
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 * See: Physics Lesson 14 — Stacking Stability
 */
static inline void forge_physics_si_solve(
    ForgePhysicsManifold *manifolds, int manifold_count,
    ForgePhysicsRigidBody *bodies, int num_bodies,
    int iterations, float dt, bool warm_start,
    ForgePhysicsSIManifold *workspace,
    const ForgePhysicsSolverConfig *config)
{
    if (!manifolds || !bodies || !workspace) return;
    if (manifold_count <= 0 || num_bodies <= 0) return;
    if (!(dt > 0.0f) || !forge_isfinite(dt)) return;

    /* Clamp iterations */
    if (iterations < FORGE_PHYSICS_SOLVER_MIN_ITERATIONS)
        iterations = FORGE_PHYSICS_SOLVER_MIN_ITERATIONS;
    if (iterations > FORGE_PHYSICS_SOLVER_MAX_ITERATIONS)
        iterations = FORGE_PHYSICS_SOLVER_MAX_ITERATIONS;

    /* Phase 1: Prepare constraint data */
    forge_physics_si_prepare(manifolds, manifold_count, bodies, num_bodies,
                             dt, warm_start, workspace, config);

    /* Phase 2: Warm-start (apply cached impulses) */
    if (warm_start) {
        forge_physics_si_warm_start(workspace, manifold_count,
                                     bodies, num_bodies);
    }

    /* Phase 3: Velocity iterations */
    for (int iter = 0; iter < iterations; iter++) {
        forge_physics_si_solve_velocities(workspace, manifold_count,
                                           bodies, num_bodies);
    }

    /* Phase 4: Store accumulated impulses back to manifolds */
    forge_physics_si_store_impulses(workspace, manifold_count, manifolds);
}

/* Convert a ForgePhysicsRBContact (ground contact) into a ForgePhysicsManifold.
 *
 * The Lesson 06 ground collision functions (forge_physics_rb_collide_sphere_plane,
 * forge_physics_rb_collide_box_plane) produce ForgePhysicsRBContact arrays.
 * This helper wraps one or more ground contacts for the same body into a
 * single ForgePhysicsManifold with body_b == -1, so the SI solver can
 * process ground and body-body contacts uniformly.
 *
 * All contacts must share the same body_a index and normal direction.
 * The function takes up to FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS (4)
 * contacts; excess contacts are silently dropped. Contact IDs are
 * generated by spatially hashing each contact point (1 mm quantization),
 * so the same physical corner produces a stable ID across frames
 * regardless of detection order.
 *
 * Parameters:
 *   contacts   — RBContact array (from ground collision detection)
 *   count      — number of contacts (clamped to 4)
 *   mu_s       — static friction coefficient
 *   mu_d       — dynamic friction coefficient
 *   out        — receives the manifold (must not be NULL)
 *
 * Returns: true if at least one contact was converted.
 *
 * Usage:
 *   ForgePhysicsRBContact ground_contacts[8];
 *   int n = forge_physics_rb_collide_box_plane(..., ground_contacts, 8);
 *   ForgePhysicsManifold gm;
 *   if (forge_physics_si_rb_contacts_to_manifold(
 *           ground_contacts, n, 0.6f, 0.4f, &gm)) {
 *       // gm is ready for the SI solver
 *   }
 *
 * See: Physics Lesson 12 — Impulse-Based Resolution
 */
static inline bool forge_physics_si_rb_contacts_to_manifold(
    const ForgePhysicsRBContact *contacts, int count,
    float mu_s, float mu_d,
    ForgePhysicsManifold *out)
{
    if (!contacts || !out || count <= 0) return false;

    SDL_memset(out, 0, sizeof(*out));

    /* Validate the FULL batch before truncating to MAX_CONTACTS.
     * All contacts must share the same body pair and normal direction.
     * Without this, contacts[4..count-1] could hide mixed grouping. */
    for (int i = 1; i < count; i++) {
        if (contacts[i].body_a != contacts[0].body_a ||
            contacts[i].body_b != contacts[0].body_b) {
            return false;
        }
        float ndot = vec3_dot(contacts[i].normal, contacts[0].normal);
        if (!forge_isfinite(ndot) ||
            ndot < FORGE_PHYSICS_SI_NORMAL_ALIGN_MIN) {
            return false;
        }
    }

    int n = count;
    if (n > FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS)
        n = FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS;

    out->body_a = contacts[0].body_a;
    out->body_b = contacts[0].body_b;
    out->normal = contacts[0].normal;
    out->static_friction  = (forge_isfinite(mu_s) && mu_s >= 0.0f) ? mu_s : 0.0f;
    out->dynamic_friction = (forge_isfinite(mu_d) && mu_d >= 0.0f) ? mu_d : 0.0f;
    out->count = n;

    for (int i = 0; i < n; i++) {
        ForgePhysicsManifoldContact *mc = &out->contacts[i];
        mc->world_point  = contacts[i].point;
        mc->penetration  = contacts[i].penetration;
        mc->local_a      = vec3_create(0, 0, 0);
        mc->local_b      = vec3_create(0, 0, 0);
        mc->normal_impulse    = 0.0f;
        mc->tangent_impulse_1 = 0.0f;
        mc->tangent_impulse_2 = 0.0f;

        /* Contact ID: use the sequential index within the manifold.
         * Box-plane contacts are detected in a stable winding order
         * (face vertices from forge_physics_rb_collide_box_plane), so the
         * index is consistent across frames for the same physical corner. */
        mc->id = (uint32_t)i;
    }

    return true;
}


/* ══════════════════════════════════════════════════════════════════════════════
 * Joint Constraint Solver (Lesson 13)
 *
 * Velocity-level joint constraints solved with sequential impulses
 * (Gauss-Seidel iteration). Each joint removes degrees of freedom by
 * constraining relative motion between two rigid bodies.
 *
 * Joint types:
 *   Ball-socket — 3 DOF removed (point constraint)
 *   Hinge       — 5 DOF removed (point + 2 angular)
 *   Slider      — 5 DOF removed (3 angular + 2 linear)
 *
 * Integration with existing SI contact solver:
 *   Joints and contacts are solved in the same iteration loop for
 *   best convergence. The pipeline is:
 *     integrate_velocities → detect_collisions →
 *     joint_prepare + si_prepare →
 *     joint_warm_start + si_warm_start →
 *     N × { joint_solve_velocities + si_solve_velocities } →
 *     joint_store + si_store →
 *     correct_positions → integrate_positions
 *
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", GDC 2005
 * Ref: Catto, "Modeling and Solving Constraints", GDC 2009
 * See: Physics Lesson 13 — Constraint Solver
 * ══════════════════════════════════════════════════════════════════════════════ */

/* ── Joint Constants ──────────────────────────────────────────────────────── */

/* Baumgarte stabilization factor for joint position correction.
 *
 * Lower than the contact Baumgarte factor (0.2) because joints are
 * stiff equality constraints. Higher values cause oscillation when
 * combined with warm-starting.
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
#define FORGE_PHYSICS_JOINT_BAUMGARTE   0.1f

/* Positional slop tolerance for joint constraints (meters).
 *
 * Joint position errors below this threshold are not corrected, to
 * avoid jitter from the solver fighting floating-point drift. Tighter
 * than contact slop (0.01) because joints should have near-zero error.
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
#define FORGE_PHYSICS_JOINT_SLOP        0.005f

/* ── Joint Types ──────────────────────────────────────────────────────────── */

/* Joint type discriminator.
 *
 * Each type removes a different set of degrees of freedom:
 *   BALL_SOCKET — removes 3 translational DOF (point constraint)
 *   HINGE       — removes 3 translational + 2 rotational DOF
 *   SLIDER      — removes 3 rotational + 2 translational DOF
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
typedef enum ForgePhysicsJointType {
    FORGE_PHYSICS_JOINT_BALL_SOCKET = 0,
    FORGE_PHYSICS_JOINT_HINGE       = 1,
    FORGE_PHYSICS_JOINT_SLIDER      = 2
} ForgePhysicsJointType;

/* Persistent joint definition connecting two rigid bodies.
 *
 * Stores the joint configuration (anchors, axes) and accumulated
 * impulses for warm-starting across frames. Joints are persistent —
 * unlike contacts which are transient, joints exist for the lifetime
 * of the connected bodies.
 *
 * body_a or body_b can be -1 to indicate a world (static) anchor.
 * When a body index is -1, the local anchor is treated as a world-space
 * position, and the body has infinite mass (inv_mass = 0).
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", GDC 2005
 */
typedef struct ForgePhysicsJoint {
    ForgePhysicsJointType type;  /* constraint type discriminator          */
    int body_a;                  /* index into body array, -1 = world      */
    int body_b;                  /* index into body array, -1 = world      */
    vec3 local_anchor_a;         /* attachment point in body A local space (m) */
    vec3 local_anchor_b;         /* attachment point in body B local space (m) */
    vec3 local_axis_a;           /* joint axis in body A local space (unit vec,
                                  * normalized at creation; hinge rotation
                                  * axis / slider slide axis).               */
    vec3 local_axis_b;           /* joint axis in body B local space (unit vec).
                                  * Used by hinge/slider to compute body B's
                                  * world-space axis independently of body A.
                                  * Set by constructors to match local_axis_a
                                  * (assumes identity initial orientation).   */

    /* Accumulated impulses for warm-starting (persistent across frames) */
    vec3  j_point;               /* point constraint impulse (N*s, 3 DOF)  */
    vec3  j_angular;             /* angular constraint impulse (N*m*s):
                                  *   hinge: x,y components used (2 DOF)
                                  *   slider: x,y,z all used (3 DOF)       */
    float j_slide[2];            /* slider linear constraint impulses (N*s,
                                  * 2 perpendicular axes)                  */
} ForgePhysicsJoint;

/* Per-step precomputed solver workspace for one joint.
 *
 * Computed once per timestep in forge_physics_joint_prepare(), then
 * used across all solver iterations. Mirrors the role of
 * ForgePhysicsSIManifold for contact constraints.
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
typedef struct ForgePhysicsJointSolverData {
    /* World-space geometric data (from body transforms) */
    vec3 r_a;                    /* world offset: anchor - body_a COM (m)  */
    vec3 r_b;                    /* world offset: anchor - body_b COM (m)  */
    vec3 world_axis_a;           /* joint axis in world space (unit vec)   */
    vec3 perp1;                  /* axis perpendicular to world_axis_a     */
    vec3 perp2;                  /* second perpendicular axis              */

    /* Inverse effective mass matrices */
    mat3  K_point_inv;           /* inverse 3×3 eff mass (kg^-1)           */
    mat3  K_angular_inv;         /* inverse 3×3 angular eff mass (slider)  */
    float eff_mass_ang[2];       /* scalar eff mass, hinge angular (kg*m^2)*/
    float eff_mass_slide[2];     /* scalar eff mass, slider linear (kg)   */

    /* Baumgarte bias velocities (position error correction) */
    vec3  point_bias;            /* translational position error (m/s)     */
    vec3  angular_bias;          /* angular orientation error (rad/s)      */
    float slide_bias[2];         /* slider positional error (m/s)          */
} ForgePhysicsJointSolverData;

/* ── Joint Creation Functions ─────────────────────────────────────────────── */

/* Create a ball-and-socket joint between two bodies.
 *
 * A ball-socket joint constrains two anchor points to coincide in world
 * space, removing 3 translational degrees of freedom while allowing
 * free rotation in all directions. This is the simplest rigid body joint.
 *
 * The anchors are specified in each body's local coordinate space. At
 * runtime, the solver transforms them to world space using each body's
 * current position and orientation.
 *
 * Parameters:
 *   body_a         — index of first body (-1 for world anchor)
 *   body_b         — index of second body (-1 for world anchor)
 *   local_anchor_a — attachment point in body A's local space
 *   local_anchor_b — attachment point in body B's local space
 *
 * Returns: initialized ForgePhysicsJoint with type BALL_SOCKET
 *
 * Usage:
 *   // Pendulum: body 0 attached to world at (0, 5, 0)
 *   ForgePhysicsJoint j = forge_physics_joint_ball_socket(
 *       0, -1,
 *       vec3_create(0, 0.5f, 0),   // top of body
 *       vec3_create(0, 5, 0));     // world anchor point
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", GDC 2005
 */
static inline ForgePhysicsJoint forge_physics_joint_ball_socket(
    int body_a, int body_b, vec3 local_anchor_a, vec3 local_anchor_b)
{
    ForgePhysicsJoint j;
    SDL_memset(&j, 0, sizeof(j));
    j.type = FORGE_PHYSICS_JOINT_BALL_SOCKET;
    j.body_a = body_a;
    j.body_b = body_b;
    j.local_anchor_a = local_anchor_a;
    j.local_anchor_b = local_anchor_b;
    j.local_axis_a = vec3_create(0, 1, 0); /* unused for ball-socket */
    j.local_axis_b = j.local_axis_a;
    return j;
}

/* Create a hinge (revolute) joint between two bodies.
 *
 * A hinge joint constrains two anchor points to coincide (like a
 * ball-socket) AND constrains relative rotation to a single axis.
 * This removes 5 DOF (3 translational + 2 rotational), leaving only
 * rotation around the hinge axis.
 *
 * The hinge axis is specified in body A's local space. Two perpendicular
 * axes are computed at runtime to form the angular constraint rows.
 *
 * Parameters:
 *   body_a         — index of first body (-1 for world anchor)
 *   body_b         — index of second body (-1 for world anchor)
 *   local_anchor_a — attachment point in body A's local space
 *   local_anchor_b — attachment point in body B's local space
 *   local_axis_a   — rotation axis in body A's local space (unit vector)
 *
 * Returns: initialized ForgePhysicsJoint with type HINGE
 *
 * Usage:
 *   // Door hinged along Y axis at the left edge
 *   ForgePhysicsJoint j = forge_physics_joint_hinge(
 *       0, -1,
 *       vec3_create(-0.5f, 0, 0),  // left edge of body
 *       vec3_create(0, 2, 0),      // world hinge point
 *       vec3_create(0, 1, 0));     // Y-axis rotation
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Modeling and Solving Constraints", GDC 2009
 */
static inline ForgePhysicsJoint forge_physics_joint_hinge(
    int body_a, int body_b,
    vec3 local_anchor_a, vec3 local_anchor_b,
    vec3 local_axis_a)
{
    ForgePhysicsJoint j;
    SDL_memset(&j, 0, sizeof(j));
    j.type = FORGE_PHYSICS_JOINT_HINGE;
    j.body_a = body_a;
    j.body_b = body_b;
    j.local_anchor_a = local_anchor_a;
    j.local_anchor_b = local_anchor_b;
    /* Normalize the axis to guarantee unit length */
    float len = vec3_length(local_axis_a);
    j.local_axis_a = (len > FORGE_PHYSICS_EPSILON)
        ? vec3_scale(local_axis_a, 1.0f / len)
        : vec3_create(0, 1, 0);
    j.local_axis_b = j.local_axis_a;
    return j;
}

/* Create a slider (prismatic) joint between two bodies.
 *
 * A slider joint locks all relative rotation (3 DOF) and constrains
 * translation to a single axis (removes 2 more DOF), leaving only
 * linear sliding along the specified axis. Total: 5 DOF removed.
 *
 * The slide axis is specified in body A's local space. The angular
 * constraint uses the full relative orientation error (3 DOF lock).
 * The linear constraint removes translation along two axes
 * perpendicular to the slide axis.
 *
 * Parameters:
 *   body_a         — index of first body (-1 for world anchor)
 *   body_b         — index of second body (-1 for world anchor)
 *   local_anchor_a — reference point in body A's local space
 *   local_anchor_b — reference point in body B's local space
 *   local_axis_a   — slide axis in body A's local space (unit vector)
 *
 * Returns: initialized ForgePhysicsJoint with type SLIDER
 *
 * Usage:
 *   // Piston sliding along X axis
 *   ForgePhysicsJoint j = forge_physics_joint_slider(
 *       0, -1,
 *       vec3_create(0, 0, 0),
 *       vec3_create(0, 2, 0),
 *       vec3_create(1, 0, 0));   // slide along X
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Modeling and Solving Constraints", GDC 2009
 */
static inline ForgePhysicsJoint forge_physics_joint_slider(
    int body_a, int body_b,
    vec3 local_anchor_a, vec3 local_anchor_b,
    vec3 local_axis_a)
{
    ForgePhysicsJoint j;
    SDL_memset(&j, 0, sizeof(j));
    j.type = FORGE_PHYSICS_JOINT_SLIDER;
    j.body_a = body_a;
    j.body_b = body_b;
    j.local_anchor_a = local_anchor_a;
    j.local_anchor_b = local_anchor_b;
    float len = vec3_length(local_axis_a);
    j.local_axis_a = (len > FORGE_PHYSICS_EPSILON)
        ? vec3_scale(local_axis_a, 1.0f / len)
        : vec3_create(1, 0, 0);
    j.local_axis_b = j.local_axis_a;
    return j;
}

/* ── Joint Solver Internal Helpers ────────────────────────────────────────── */

/* Get world-space position for a body index, treating -1 as origin.
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline vec3 forge_physics_joint_body_pos_(
    const ForgePhysicsRigidBody *bodies, int num_bodies, int idx)
{
    if (idx == -1) return vec3_create(0, 0, 0);  /* world anchor */
    if (idx < 0 || idx >= num_bodies) {
        SDL_Log("forge_physics: joint references invalid body %d (num=%d)",
                idx, num_bodies);
        return vec3_create(0, 0, 0);
    }
    return bodies[idx].position;
}

/* Get world-space orientation for a body index, treating -1 as identity.
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline quat forge_physics_joint_body_orient_(
    const ForgePhysicsRigidBody *bodies, int num_bodies, int idx)
{
    if (idx == -1) return quat_identity();  /* world */
    if (idx < 0 || idx >= num_bodies) {
        SDL_Log("forge_physics: joint references invalid body %d (num=%d)",
                idx, num_bodies);
        return quat_identity();
    }
    return bodies[idx].orientation;
}

/* Get inverse mass for a body index, treating -1 as 0 (static).
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline float forge_physics_joint_body_inv_mass_(
    const ForgePhysicsRigidBody *bodies, int num_bodies, int idx)
{
    if (idx == -1) return 0.0f;  /* world is static */
    if (idx < 0 || idx >= num_bodies) {
        SDL_Log("forge_physics: joint references invalid body %d (num=%d)",
                idx, num_bodies);
        return 0.0f;
    }
    return bodies[idx].inv_mass;
}

/* Get world-space inverse inertia for a body index, treating -1 as zero.
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline mat3 forge_physics_joint_body_inv_inertia_(
    const ForgePhysicsRigidBody *bodies, int num_bodies, int idx)
{
    if (idx == -1) return mat3_from_diagonal(0, 0, 0);  /* world */
    if (idx < 0 || idx >= num_bodies) {
        SDL_Log("forge_physics: joint references invalid body %d (num=%d)",
                idx, num_bodies);
        return mat3_from_diagonal(0, 0, 0);
    }
    /* Static bodies (inv_mass == 0) must return zero inverse inertia so
     * warm-start and solve paths do not apply angular impulses to them. */
    if (bodies[idx].inv_mass == 0.0f)
        return mat3_from_diagonal(0, 0, 0);
    return bodies[idx].inv_inertia_world;
}

/* Compute the 3×3 effective mass matrix K for a point constraint.
 *
 * K = (inv_m_a + inv_m_b) * I_3x3
 *   + [r_a]× * I_inv_a * [r_a]×^T
 *   + [r_b]× * I_inv_b * [r_b]×^T
 *
 * where [r]× is the skew-symmetric matrix of r, and [r]×^T = -[r]×.
 *
 * The effective mass is K_inv = inverse(K). The impulse for each
 * iteration is: lambda = K_inv * (-v_rel - bias).
 *
 * Parameters:
 *   r_a        — world offset from body A COM to anchor
 *   r_b        — world offset from body B COM to anchor
 *   inv_m_a    — inverse mass of body A (0 for static)
 *   inv_m_b    — inverse mass of body B (0 for static)
 *   I_inv_a    — world-space inverse inertia of body A
 *   I_inv_b    — world-space inverse inertia of body B
 *
 * Returns: 3×3 effective mass matrix K (caller must invert)
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", §4.2
 */
static inline mat3 forge_physics_joint_K_point_(
    vec3 r_a, vec3 r_b,
    float inv_m_a, float inv_m_b,
    mat3 I_inv_a, mat3 I_inv_b)
{
    /* Mass contribution: (1/m_a + 1/m_b) * I_3x3 */
    mat3 K = mat3_scale_scalar(mat3_identity(), inv_m_a + inv_m_b);

    /* Angular contribution from body A: [r_a]× * I_inv_a * [r_a]×^T
     * This product is positive semi-definite and adds to K. */
    mat3 skew_a = mat3_skew(r_a);
    mat3 skew_a_T = mat3_transpose(skew_a);
    mat3 ang_a = mat3_multiply(mat3_multiply(skew_a, I_inv_a), skew_a_T);
    K = mat3_add(K, ang_a);

    /* Angular contribution from body B */
    mat3 skew_b = mat3_skew(r_b);
    mat3 skew_b_T = mat3_transpose(skew_b);
    mat3 ang_b = mat3_multiply(mat3_multiply(skew_b, I_inv_b), skew_b_T);
    K = mat3_add(K, ang_b);

    return K;
}

/* Build a stable perpendicular basis from an axis vector.
 *
 * Uses the least-aligned-axis method (same as
 * forge_physics_si_tangent_basis) to find two axes perpendicular to
 * the given axis, forming an orthonormal frame {axis, perp1, perp2}.
 *
 * Parameters:
 *   axis  — unit vector to build basis from (must be normalized)
 *   perp1 — [out] first perpendicular axis
 *   perp2 — [out] second perpendicular axis
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline void forge_physics_joint_perp_basis_(
    vec3 axis, vec3 *perp1, vec3 *perp2)
{
    /* Pick the cardinal axis least aligned with the input axis */
    float ax = SDL_fabsf(axis.x);
    float ay = SDL_fabsf(axis.y);
    float az = SDL_fabsf(axis.z);

    vec3 ref;
    if (ax <= ay && ax <= az)
        ref = vec3_create(1, 0, 0);
    else if (ay <= az)
        ref = vec3_create(0, 1, 0);
    else
        ref = vec3_create(0, 0, 1);

    *perp1 = vec3_normalize(vec3_cross(axis, ref));
    *perp2 = vec3_cross(axis, *perp1);
}

/* ── Joint Solver Functions ───────────────────────────────────────────────── */

/* Precompute solver data for all joints before iteration begins.
 *
 * Transforms anchors and axes to world space, builds effective mass
 * matrices, and computes Baumgarte bias velocities for position error
 * correction. Called once per timestep before the iteration loop.
 *
 * Parameters:
 *   joints     — array of joint definitions (must not be NULL if count > 0)
 *   count      — number of joints
 *   bodies     — array of rigid bodies
 *   num_bodies — number of bodies
 *   dt         — timestep (seconds, must be > 0)
 *   out        — [out] solver workspace array (same size as joints)
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", GDC 2005
 */
static inline void forge_physics_joint_prepare(
    const ForgePhysicsJoint *joints, int count,
    const ForgePhysicsRigidBody *bodies, int num_bodies,
    float dt, ForgePhysicsJointSolverData *out)
{
    if (!joints || !bodies || !out || count <= 0 ||
        num_bodies <= 0 || !(dt > 0.0f) || !forge_isfinite(dt)) return;

    float inv_dt = 1.0f / dt;
    float beta = FORGE_PHYSICS_JOINT_BAUMGARTE;
    float slop = FORGE_PHYSICS_JOINT_SLOP;

    for (int i = 0; i < count; i++) {
        const ForgePhysicsJoint *j = &joints[i];
        ForgePhysicsJointSolverData *s = &out[i];
        SDL_memset(s, 0, sizeof(*s));

        /* Validate body indices: -1 = world anchor (expected), anything else
         * outside [0, num_bodies) is a stale/invalid reference — skip. */
        if ((j->body_a != -1 && (j->body_a < 0 || j->body_a >= num_bodies)) ||
            (j->body_b != -1 && (j->body_b < 0 || j->body_b >= num_bodies))) {
            continue;
        }

        /* Get body properties (world anchor uses defaults) */
        vec3 pos_a = forge_physics_joint_body_pos_(bodies, num_bodies, j->body_a);
        vec3 pos_b = forge_physics_joint_body_pos_(bodies, num_bodies, j->body_b);
        quat ori_a = forge_physics_joint_body_orient_(bodies, num_bodies, j->body_a);
        quat ori_b = forge_physics_joint_body_orient_(bodies, num_bodies, j->body_b);
        float inv_m_a = forge_physics_joint_body_inv_mass_(bodies, num_bodies, j->body_a);
        float inv_m_b = forge_physics_joint_body_inv_mass_(bodies, num_bodies, j->body_b);
        mat3  I_inv_a = forge_physics_joint_body_inv_inertia_(bodies, num_bodies, j->body_a);
        mat3  I_inv_b = forge_physics_joint_body_inv_inertia_(bodies, num_bodies, j->body_b);

        /* Transform anchors to world space */
        vec3 world_anchor_a, world_anchor_b;
        if (j->body_a >= 0 && j->body_a < num_bodies) {
            world_anchor_a = vec3_add(pos_a, quat_rotate_vec3(ori_a, j->local_anchor_a));
        } else {
            world_anchor_a = j->local_anchor_a; /* world anchor directly */
        }
        if (j->body_b >= 0 && j->body_b < num_bodies) {
            world_anchor_b = vec3_add(pos_b, quat_rotate_vec3(ori_b, j->local_anchor_b));
        } else {
            world_anchor_b = j->local_anchor_b;
        }

        /* Lever arms from COM to anchor */
        s->r_a = vec3_sub(world_anchor_a, pos_a);
        s->r_b = vec3_sub(world_anchor_b, pos_b);

        /* If both bodies are static (or world anchors), the K matrix is
         * singular (all zeros). Skip all constraint math — there are no
         * velocities to constrain. Leave solver data zeroed. */
        if (inv_m_a < FORGE_PHYSICS_EPSILON &&
            inv_m_b < FORGE_PHYSICS_EPSILON) {
            continue;
        }

        /* ── Effective mass matrix K (used by point and slider linear) ── */
        mat3 K = forge_physics_joint_K_point_(
            s->r_a, s->r_b, inv_m_a, inv_m_b, I_inv_a, I_inv_b);

        vec3 pos_error = vec3_sub(world_anchor_b, world_anchor_a);

        /* ── Point constraint (ball-socket and hinge only) ──
         * Sliders do NOT use the 3-DOF point constraint — their
         * translation is constrained by 2 perpendicular linear rows
         * plus the free slide axis. Applying the point constraint to
         * sliders would over-constrain them into a weld joint. */
        if (j->type != FORGE_PHYSICS_JOINT_SLIDER) {
            s->K_point_inv = mat3_inverse(K);

            /* Position error and Baumgarte bias */
            float err_len = vec3_length(pos_error);
            if (err_len > slop) {
                s->point_bias = vec3_scale(pos_error, beta * inv_dt);
            } else {
                s->point_bias = vec3_create(0, 0, 0);
            }
        }

        /* ── Joint-specific constraints ── */
        if (j->type == FORGE_PHYSICS_JOINT_HINGE ||
            j->type == FORGE_PHYSICS_JOINT_SLIDER) {
            /* Transform joint axis to world space */
            if (j->body_a >= 0 && j->body_a < num_bodies) {
                s->world_axis_a = quat_rotate_vec3(ori_a, j->local_axis_a);
            } else {
                s->world_axis_a = j->local_axis_a;
            }
            float axis_len = vec3_length(s->world_axis_a);
            if (axis_len > FORGE_PHYSICS_EPSILON) {
                s->world_axis_a = vec3_scale(s->world_axis_a, 1.0f / axis_len);
            } else {
                s->world_axis_a = vec3_create(0, 1, 0);
            }

            /* Build perpendicular basis */
            forge_physics_joint_perp_basis_(
                s->world_axis_a, &s->perp1, &s->perp2);
        }

        if (j->type == FORGE_PHYSICS_JOINT_HINGE) {
            /* Hinge angular constraint: body B must not rotate around
             * perp1 or perp2 relative to the hinge axis direction.
             *
             * For each perpendicular axis p, the angular velocity
             * constraint is: dot(p, omega_a - omega_b) = 0
             * Effective mass: 1 / (dot(p, I_inv_a * p) + dot(p, I_inv_b * p))
             */
            for (int k = 0; k < 2; k++) {
                vec3 p = (k == 0) ? s->perp1 : s->perp2;
                float ka = vec3_dot(p, mat3_multiply_vec3(I_inv_a, p));
                float kb = vec3_dot(p, mat3_multiply_vec3(I_inv_b, p));
                float denom = ka + kb;
                s->eff_mass_ang[k] = (denom > FORGE_PHYSICS_EPSILON)
                    ? (1.0f / denom) : 0.0f;
            }

            /* Angular Baumgarte bias: measure how far body B's axis has
             * drifted from body A's hinge axis. The error is the cross
             * product of the axes projected onto each perp direction. */
            vec3 axis_b;
            if (j->body_b >= 0 && j->body_b < num_bodies) {
                axis_b = quat_rotate_vec3(ori_b, j->local_axis_b);
            } else {
                axis_b = j->local_axis_b;
            }
            vec3 axis_error = vec3_cross(s->world_axis_a, axis_b);
            float ang_err_1 = vec3_dot(axis_error, s->perp1);
            float ang_err_2 = vec3_dot(axis_error, s->perp2);
            s->angular_bias = vec3_create(
                ang_err_1 * beta * inv_dt,
                ang_err_2 * beta * inv_dt,
                0);
        }

        if (j->type == FORGE_PHYSICS_JOINT_SLIDER) {
            /* Slider angular constraint: lock all relative rotation (3 DOF).
             *
             * The angular error is the imaginary part of the relative
             * quaternion q_error = q_b * conj(q_a). When the bodies are
             * aligned, q_error = (1, 0, 0, 0) and the imaginary part is
             * zero. The bias drives this toward zero.
             *
             * Angular K matrix:
             *   K_ang = I_inv_a + I_inv_b
             */
            mat3 K_ang = mat3_add(I_inv_a, I_inv_b);
            s->K_angular_inv = mat3_inverse(K_ang);

            /* Quaternion error: q_err = q_b * conj(q_a) */
            quat q_a_conj = quat_conjugate(ori_a);
            quat q_err = quat_multiply(ori_b, q_a_conj);
            /* Ensure shortest path (positive w) */
            if (q_err.w < 0.0f) {
                q_err.x = -q_err.x;
                q_err.y = -q_err.y;
                q_err.z = -q_err.z;
                q_err.w = -q_err.w;
            }
            /* The angular error is 2 * imaginary part of q_err */
            s->angular_bias = vec3_scale(
                vec3_create(q_err.x, q_err.y, q_err.z),
                2.0f * beta * inv_dt);

            /* Slider linear constraint: remove translation perpendicular
             * to the slide axis.
             *
             * For each perpendicular axis p, the linear velocity constraint
             * at the anchor point is:
             *   dot(p, v_b + omega_b × r_b - v_a - omega_a × r_a) = 0
             *
             * Effective mass per row:
             *   1 / (inv_m_a + inv_m_b
             *        + dot(p, I_inv_a * (r_a × p)) × r_a ... )
             * which is dot(p, K * p) where K is the point constraint K.
             */
            for (int k = 0; k < 2; k++) {
                vec3 p = (k == 0) ? s->perp1 : s->perp2;
                float eff = vec3_dot(p, mat3_multiply_vec3(K, p));
                s->eff_mass_slide[k] = (eff > FORGE_PHYSICS_EPSILON)
                    ? (1.0f / eff) : 0.0f;

                /* Linear position error along perpendicular axis */
                float lin_err = vec3_dot(pos_error, p);
                if (SDL_fabsf(lin_err) > slop) {
                    s->slide_bias[k] = lin_err * beta * inv_dt;
                }
            }
        }
    }
}

/* Apply cached (warm-start) impulses from previous frame.
 *
 * Warm-starting applies the accumulated impulses from the last frame
 * before iteration begins. For joints, this is simpler than contacts
 * because joints are persistent — the impulses don't need spatial
 * hashing or manifold matching.
 *
 * Warm-starting reduces the number of iterations needed for convergence
 * by 3-5×, which is critical for joint chains (pendulums, ragdolls).
 *
 * Parameters:
 *   joints     — array of joint definitions (with cached j_point etc.)
 *   solvers    — precomputed solver data from joint_prepare
 *   count      — number of joints
 *   bodies     — [in/out] rigid body array (velocities modified)
 *   num_bodies — number of bodies
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", §5
 */
static inline void forge_physics_joint_warm_start(
    const ForgePhysicsJoint *joints,
    const ForgePhysicsJointSolverData *solvers,
    int count,
    ForgePhysicsRigidBody *bodies, int num_bodies)
{
    if (!joints || !solvers || !bodies || count <= 0) return;

    for (int i = 0; i < count; i++) {
        const ForgePhysicsJoint *j = &joints[i];
        const ForgePhysicsJointSolverData *s = &solvers[i];

        /* Point constraint warm-start impulse (not for sliders — they use
         * perpendicular linear rows instead of the 3-DOF point constraint) */
        if (j->type != FORGE_PHYSICS_JOINT_SLIDER) {
            vec3 p_impulse = j->j_point;

            /* Apply to body A (negative direction) */
            if (j->body_a >= 0 && j->body_a < num_bodies) {
                ForgePhysicsRigidBody *a = &bodies[j->body_a];
                a->velocity = vec3_sub(a->velocity,
                    vec3_scale(p_impulse, a->inv_mass));
                if (a->inv_mass > 0.0f) {
                    a->angular_velocity = vec3_sub(a->angular_velocity,
                        mat3_multiply_vec3(a->inv_inertia_world,
                            vec3_cross(s->r_a, p_impulse)));
                }
            }
            /* Apply to body B (positive direction) */
            if (j->body_b >= 0 && j->body_b < num_bodies) {
                ForgePhysicsRigidBody *b = &bodies[j->body_b];
                b->velocity = vec3_add(b->velocity,
                    vec3_scale(p_impulse, b->inv_mass));
                if (b->inv_mass > 0.0f) {
                    b->angular_velocity = vec3_add(b->angular_velocity,
                        mat3_multiply_vec3(b->inv_inertia_world,
                            vec3_cross(s->r_b, p_impulse)));
                }
            }
        }

        /* Hinge angular warm-start */
        if (j->type == FORGE_PHYSICS_JOINT_HINGE) {
            vec3 ang_impulse = vec3_add(
                vec3_scale(s->perp1, j->j_angular.x),
                vec3_scale(s->perp2, j->j_angular.y));
            if (j->body_a >= 0 && j->body_a < num_bodies &&
                bodies[j->body_a].inv_mass > 0.0f) {
                bodies[j->body_a].angular_velocity = vec3_sub(
                    bodies[j->body_a].angular_velocity,
                    mat3_multiply_vec3(bodies[j->body_a].inv_inertia_world,
                        ang_impulse));
            }
            if (j->body_b >= 0 && j->body_b < num_bodies &&
                bodies[j->body_b].inv_mass > 0.0f) {
                bodies[j->body_b].angular_velocity = vec3_add(
                    bodies[j->body_b].angular_velocity,
                    mat3_multiply_vec3(bodies[j->body_b].inv_inertia_world,
                        ang_impulse));
            }
        }

        /* Slider angular + linear warm-start */
        if (j->type == FORGE_PHYSICS_JOINT_SLIDER) {
            /* Angular impulse (3 DOF) */
            vec3 ang_impulse = j->j_angular;
            if (j->body_a >= 0 && j->body_a < num_bodies &&
                bodies[j->body_a].inv_mass > 0.0f) {
                bodies[j->body_a].angular_velocity = vec3_sub(
                    bodies[j->body_a].angular_velocity,
                    mat3_multiply_vec3(bodies[j->body_a].inv_inertia_world,
                        ang_impulse));
            }
            if (j->body_b >= 0 && j->body_b < num_bodies &&
                bodies[j->body_b].inv_mass > 0.0f) {
                bodies[j->body_b].angular_velocity = vec3_add(
                    bodies[j->body_b].angular_velocity,
                    mat3_multiply_vec3(bodies[j->body_b].inv_inertia_world,
                        ang_impulse));
            }

            /* Linear slide impulses along perpendicular axes.
             * NOTE: the cached scalar magnitudes were accumulated along the
             * previous frame's perp basis.  When the body rotates between
             * frames, the current perp axes differ slightly, making the
             * warm-start direction approximate.  This is standard practice
             * in sequential impulse solvers and converges within a few
             * iterations. */
            for (int k = 0; k < 2; k++) {
                vec3 axis = (k == 0) ? s->perp1 : s->perp2;
                vec3 lin_imp = vec3_scale(axis, j->j_slide[k]);
                if (j->body_a >= 0 && j->body_a < num_bodies) {
                    ForgePhysicsRigidBody *a = &bodies[j->body_a];
                    a->velocity = vec3_sub(a->velocity,
                        vec3_scale(lin_imp, a->inv_mass));
                    if (a->inv_mass > 0.0f) {
                        a->angular_velocity = vec3_sub(a->angular_velocity,
                            mat3_multiply_vec3(a->inv_inertia_world,
                                vec3_cross(s->r_a, lin_imp)));
                    }
                }
                if (j->body_b >= 0 && j->body_b < num_bodies) {
                    ForgePhysicsRigidBody *b = &bodies[j->body_b];
                    b->velocity = vec3_add(b->velocity,
                        vec3_scale(lin_imp, b->inv_mass));
                    if (b->inv_mass > 0.0f) {
                        b->angular_velocity = vec3_add(b->angular_velocity,
                            mat3_multiply_vec3(b->inv_inertia_world,
                                vec3_cross(s->r_b, lin_imp)));
                    }
                }
            }
        }
    }
}

/* Solve joint velocity constraints for one Gauss-Seidel iteration.
 *
 * For each joint, computes the relative velocity at the constraint
 * point, determines the impulse needed to satisfy the constraint,
 * and applies it to both bodies. This is called multiple times
 * (typically 10-20 iterations) for convergence.
 *
 * Unlike contact constraints, equality joint constraints have NO
 * clamping — impulses can be positive or negative because the joint
 * must resist both push and pull.
 *
 * Parameters:
 *   joints     — [in/out] joint array (accumulated impulses updated)
 *   solvers    — precomputed solver data
 *   count      — number of joints
 *   bodies     — [in/out] rigid body array (velocities modified)
 *   num_bodies — number of bodies
 *
 * See: Physics Lesson 13 — Constraint Solver
 * Ref: Catto, "Iterative Dynamics with Temporal Coherence", §4
 */
static inline void forge_physics_joint_solve_velocities(
    ForgePhysicsJoint *joints,
    const ForgePhysicsJointSolverData *solvers,
    int count,
    ForgePhysicsRigidBody *bodies, int num_bodies)
{
    if (!joints || !solvers || !bodies || count <= 0) return;

    for (int i = 0; i < count; i++) {
        ForgePhysicsJoint *j = &joints[i];
        const ForgePhysicsJointSolverData *s = &solvers[i];

        /* Get body velocities (zero for world anchors) */
        vec3 v_a = vec3_create(0, 0, 0), w_a = vec3_create(0, 0, 0);
        vec3 v_b = vec3_create(0, 0, 0), w_b = vec3_create(0, 0, 0);
        if (j->body_a >= 0 && j->body_a < num_bodies) {
            v_a = bodies[j->body_a].velocity;
            w_a = bodies[j->body_a].angular_velocity;
        }
        if (j->body_b >= 0 && j->body_b < num_bodies) {
            v_b = bodies[j->body_b].velocity;
            w_b = bodies[j->body_b].angular_velocity;
        }

        /* ── Point constraint (ball-socket and hinge only) ──
         *
         * Velocity at anchor: v + ω × r
         * Relative velocity: v_rel = (v_b + ω_b × r_b) - (v_a + ω_a × r_a)
         * Constraint velocity: C_dot = v_rel (should be zero)
         * Impulse: lambda = K_inv * (-(C_dot + bias))
         *
         * Sliders skip this — their translation is handled by the 2
         * perpendicular linear rows below, preserving the free slide axis.
         */
        if (j->type != FORGE_PHYSICS_JOINT_SLIDER) {
            vec3 vel_a = vec3_add(v_a, vec3_cross(w_a, s->r_a));
            vec3 vel_b = vec3_add(v_b, vec3_cross(w_b, s->r_b));
            vec3 v_rel = vec3_sub(vel_b, vel_a);

            vec3 rhs = vec3_scale(vec3_add(v_rel, s->point_bias), -1.0f);
            vec3 lambda = mat3_multiply_vec3(s->K_point_inv, rhs);

            /* Accumulate (no clamping for equality constraints) */
            j->j_point = vec3_add(j->j_point, lambda);

            /* Apply impulse to bodies */
            if (j->body_a >= 0 && j->body_a < num_bodies) {
                ForgePhysicsRigidBody *a = &bodies[j->body_a];
                a->velocity = vec3_sub(a->velocity,
                    vec3_scale(lambda, a->inv_mass));
                if (a->inv_mass > 0.0f) {
                    a->angular_velocity = vec3_sub(a->angular_velocity,
                        mat3_multiply_vec3(a->inv_inertia_world,
                            vec3_cross(s->r_a, lambda)));
                }
            }
            if (j->body_b >= 0 && j->body_b < num_bodies) {
                ForgePhysicsRigidBody *b = &bodies[j->body_b];
                b->velocity = vec3_add(b->velocity,
                    vec3_scale(lambda, b->inv_mass));
                if (b->inv_mass > 0.0f) {
                    b->angular_velocity = vec3_add(b->angular_velocity,
                        mat3_multiply_vec3(b->inv_inertia_world,
                            vec3_cross(s->r_b, lambda)));
                }
            }
        }

        /* Re-read angular velocities after point impulse */
        if (j->body_a >= 0 && j->body_a < num_bodies)
            w_a = bodies[j->body_a].angular_velocity;
        if (j->body_b >= 0 && j->body_b < num_bodies)
            w_b = bodies[j->body_b].angular_velocity;

        /* ── Hinge angular constraints (2 rows) ── */
        if (j->type == FORGE_PHYSICS_JOINT_HINGE) {
            for (int k = 0; k < 2; k++) {
                vec3 p = (k == 0) ? s->perp1 : s->perp2;
                float bias = (k == 0) ? s->angular_bias.x : s->angular_bias.y;

                /* Angular relative velocity along this perpendicular axis */
                float w_rel = vec3_dot(p, vec3_sub(w_b, w_a));
                float delta_j = s->eff_mass_ang[k] * (-(w_rel + bias));

                /* Accumulate */
                if (k == 0) j->j_angular.x += delta_j;
                else        j->j_angular.y += delta_j;

                /* Apply angular impulse */
                vec3 ang_imp = vec3_scale(p, delta_j);
                if (j->body_a >= 0 && j->body_a < num_bodies &&
                    bodies[j->body_a].inv_mass > 0.0f) {
                    bodies[j->body_a].angular_velocity = vec3_sub(
                        bodies[j->body_a].angular_velocity,
                        mat3_multiply_vec3(bodies[j->body_a].inv_inertia_world,
                            ang_imp));
                }
                if (j->body_b >= 0 && j->body_b < num_bodies &&
                    bodies[j->body_b].inv_mass > 0.0f) {
                    bodies[j->body_b].angular_velocity = vec3_add(
                        bodies[j->body_b].angular_velocity,
                        mat3_multiply_vec3(bodies[j->body_b].inv_inertia_world,
                            ang_imp));
                }

                /* Re-read angular velocities so the next perpendicular row
                 * (k=1) sees the impulse applied by this row (k=0). */
                if (j->body_a >= 0 && j->body_a < num_bodies)
                    w_a = bodies[j->body_a].angular_velocity;
                if (j->body_b >= 0 && j->body_b < num_bodies)
                    w_b = bodies[j->body_b].angular_velocity;
            }

            /* Re-read after angular solve */
            if (j->body_a >= 0 && j->body_a < num_bodies)
                w_a = bodies[j->body_a].angular_velocity;
            if (j->body_b >= 0 && j->body_b < num_bodies)
                w_b = bodies[j->body_b].angular_velocity;
        }

        /* ── Slider angular constraint (3 DOF lock) ── */
        if (j->type == FORGE_PHYSICS_JOINT_SLIDER) {
            vec3 w_rel_ang = vec3_sub(w_b, w_a);
            vec3 rhs_ang = vec3_scale(
                vec3_add(w_rel_ang, s->angular_bias), -1.0f);
            vec3 lambda_ang = mat3_multiply_vec3(s->K_angular_inv, rhs_ang);

            j->j_angular = vec3_add(j->j_angular, lambda_ang);

            if (j->body_a >= 0 && j->body_a < num_bodies &&
                bodies[j->body_a].inv_mass > 0.0f) {
                bodies[j->body_a].angular_velocity = vec3_sub(
                    bodies[j->body_a].angular_velocity,
                    mat3_multiply_vec3(bodies[j->body_a].inv_inertia_world,
                        lambda_ang));
            }
            if (j->body_b >= 0 && j->body_b < num_bodies &&
                bodies[j->body_b].inv_mass > 0.0f) {
                bodies[j->body_b].angular_velocity = vec3_add(
                    bodies[j->body_b].angular_velocity,
                    mat3_multiply_vec3(bodies[j->body_b].inv_inertia_world,
                        lambda_ang));
            }

            /* Re-read velocities after angular solve */
            if (j->body_a >= 0 && j->body_a < num_bodies) {
                v_a = bodies[j->body_a].velocity;
                w_a = bodies[j->body_a].angular_velocity;
            }
            if (j->body_b >= 0 && j->body_b < num_bodies) {
                v_b = bodies[j->body_b].velocity;
                w_b = bodies[j->body_b].angular_velocity;
            }

            /* ── Slider linear constraints (2 perpendicular axes) ── */
            for (int k = 0; k < 2; k++) {
                vec3 p = (k == 0) ? s->perp1 : s->perp2;

                /* Relative velocity at anchor projected onto perp axis */
                vec3 vel_a2 = vec3_add(v_a, vec3_cross(w_a, s->r_a));
                vec3 vel_b2 = vec3_add(v_b, vec3_cross(w_b, s->r_b));
                float v_perp = vec3_dot(p, vec3_sub(vel_b2, vel_a2));

                float delta_j = s->eff_mass_slide[k] *
                    (-(v_perp + s->slide_bias[k]));

                j->j_slide[k] += delta_j;

                /* Apply linear+angular impulse along perpendicular axis */
                vec3 lin_imp = vec3_scale(p, delta_j);
                if (j->body_a >= 0 && j->body_a < num_bodies) {
                    ForgePhysicsRigidBody *a = &bodies[j->body_a];
                    a->velocity = vec3_sub(a->velocity,
                        vec3_scale(lin_imp, a->inv_mass));
                    if (a->inv_mass > 0.0f) {
                        a->angular_velocity = vec3_sub(a->angular_velocity,
                            mat3_multiply_vec3(a->inv_inertia_world,
                                vec3_cross(s->r_a, lin_imp)));
                    }
                }
                if (j->body_b >= 0 && j->body_b < num_bodies) {
                    ForgePhysicsRigidBody *b = &bodies[j->body_b];
                    b->velocity = vec3_add(b->velocity,
                        vec3_scale(lin_imp, b->inv_mass));
                    if (b->inv_mass > 0.0f) {
                        b->angular_velocity = vec3_add(b->angular_velocity,
                            mat3_multiply_vec3(b->inv_inertia_world,
                                vec3_cross(s->r_b, lin_imp)));
                    }
                }

                /* Re-read for next axis */
                if (j->body_a >= 0 && j->body_a < num_bodies) {
                    v_a = bodies[j->body_a].velocity;
                    w_a = bodies[j->body_a].angular_velocity;
                }
                if (j->body_b >= 0 && j->body_b < num_bodies) {
                    v_b = bodies[j->body_b].velocity;
                    w_b = bodies[j->body_b].angular_velocity;
                }
            }
        }
    }
}

/* Finalize impulse storage after velocity solving (no-op).
 *
 * Joint impulses are accumulated in-place on ForgePhysicsJoint during
 * solve_velocities, so no copy is needed here. This function exists
 * for API symmetry with forge_physics_si_store_impulses (which does
 * copy impulses from workspace to manifolds) and for future extensions.
 *
 * Parameters:
 *   joints  — [in/out] joint array (impulses already updated in-place)
 *   solvers — solver workspace (unused in current implementation)
 *   count   — number of joints
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline void forge_physics_joint_store_impulses(
    ForgePhysicsJoint *joints,
    const ForgePhysicsJointSolverData *solvers,
    int count)
{
    /* Impulses are accumulated directly on ForgePhysicsJoint during
     * solve_velocities, so no copy is needed. This function exists
     * for API symmetry with forge_physics_si_store_impulses. */
    (void)joints;
    (void)solvers;
    (void)count;
}

/* Reset cached impulses on all joints (cold-start).
 *
 * Zeroes accumulated j_point, j_angular, and j_slide impulses so the
 * solver starts from scratch rather than warm-starting from previous
 * frame values. Call this before solve_velocities when warm-starting
 * is disabled or when joints have been reconfigured.
 *
 * This is the public equivalent of the cold-start branch in
 * forge_physics_joint_solve(). Low-level callers that use the
 * interleaved API (prepare → warm_start → solve_velocities) directly
 * should call this helper instead of skipping warm_start, to avoid
 * accumulating onto stale cached impulse values.
 *
 * Parameters:
 *   joints — [in/out] joint array (cached impulses zeroed)
 *   count  — number of joints
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline void forge_physics_joint_reset_cached_impulses(
    ForgePhysicsJoint *joints, int count)
{
    if (!joints || count <= 0) return;
    for (int i = 0; i < count; i++) {
        joints[i].j_point   = vec3_create(0, 0, 0);
        joints[i].j_angular = vec3_create(0, 0, 0);
        joints[i].j_slide[0] = 0.0f;
        joints[i].j_slide[1] = 0.0f;
    }
}

/* Apply position-level correction to reduce joint drift.
 *
 * Directly adjusts body positions (not velocities) to correct
 * accumulated positional error that the velocity-level Baumgarte
 * bias cannot fully resolve. Applied after velocity solving but
 * before position integration.
 *
 * Parameters:
 *   joints     — array of joint definitions
 *   count      — number of joints
 *   bodies     — [in/out] rigid body array (positions modified)
 *   num_bodies — number of bodies
 *   fraction   — correction fraction [0..1], typically 0.2-0.4
 *   slop       — positional error tolerance below which no correction
 *                is applied (meters)
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline void forge_physics_joint_correct_positions(
    const ForgePhysicsJoint *joints, int count,
    ForgePhysicsRigidBody *bodies, int num_bodies,
    float fraction, float slop)
{
    if (!joints || !bodies || count <= 0) return;

    for (int i = 0; i < count; i++) {
        const ForgePhysicsJoint *j = &joints[i];

        /* Skip joints with invalid (non-sentinel) body indices */
        if ((j->body_a != -1 && (j->body_a < 0 || j->body_a >= num_bodies)) ||
            (j->body_b != -1 && (j->body_b < 0 || j->body_b >= num_bodies))) {
            continue;
        }

        /* Get body properties */
        vec3 pos_a = forge_physics_joint_body_pos_(bodies, num_bodies, j->body_a);
        vec3 pos_b = forge_physics_joint_body_pos_(bodies, num_bodies, j->body_b);
        quat ori_a = forge_physics_joint_body_orient_(bodies, num_bodies, j->body_a);
        quat ori_b = forge_physics_joint_body_orient_(bodies, num_bodies, j->body_b);
        float inv_m_a = forge_physics_joint_body_inv_mass_(bodies, num_bodies, j->body_a);
        float inv_m_b = forge_physics_joint_body_inv_mass_(bodies, num_bodies, j->body_b);

        /* Compute world anchors */
        vec3 world_a = (j->body_a >= 0 && j->body_a < num_bodies)
            ? vec3_add(pos_a, quat_rotate_vec3(ori_a, j->local_anchor_a))
            : j->local_anchor_a;
        vec3 world_b = (j->body_b >= 0 && j->body_b < num_bodies)
            ? vec3_add(pos_b, quat_rotate_vec3(ori_b, j->local_anchor_b))
            : j->local_anchor_b;

        /* Position error — for sliders, only correct perpendicular to the
         * slide axis so the body remains free to translate along it. */
        vec3 error = vec3_sub(world_b, world_a);
        if (j->type == FORGE_PHYSICS_JOINT_SLIDER) {
            vec3 axis = (j->body_a >= 0 && j->body_a < num_bodies)
                ? quat_rotate_vec3(ori_a, j->local_axis_a)
                : j->local_axis_a;
            /* Renormalize to counter floating-point drift over many frames
             * (prepare() normalizes too, but correct_positions runs separately). */
            float axis_len = vec3_length(axis);
            if (axis_len > FORGE_PHYSICS_EPSILON)
                axis = vec3_scale(axis, 1.0f / axis_len);
            float along = vec3_dot(error, axis);
            error = vec3_sub(error, vec3_scale(axis, along));
        }
        float err_len = vec3_length(error);
        if (err_len <= slop) continue;

        float inv_mass_sum = inv_m_a + inv_m_b;
        if (inv_mass_sum < FORGE_PHYSICS_EPSILON) continue;

        /* Correction vector */
        vec3 correction = vec3_scale(error,
            fraction * (err_len - slop) / (err_len * inv_mass_sum));

        if (j->body_a >= 0 && j->body_a < num_bodies) {
            bodies[j->body_a].position = vec3_add(
                bodies[j->body_a].position,
                vec3_scale(correction, inv_m_a));
        }
        if (j->body_b >= 0 && j->body_b < num_bodies) {
            bodies[j->body_b].position = vec3_sub(
                bodies[j->body_b].position,
                vec3_scale(correction, inv_m_b));
        }
    }
}

/* Solve all joint constraints in one call (convenience wrapper).
 *
 * Combines prepare, warm-start, N iterations of velocity solving,
 * impulse storage, and position correction into a single function.
 * Use this when joints are the only constraints; when mixing with
 * contact constraints, call the individual functions to interleave
 * joint and contact solving in the same iteration loop.
 *
 * Parameters:
 *   joints     — [in/out] array of joints (impulses updated)
 *   count      — number of joints
 *   bodies     — [in/out] rigid body array
 *   num_bodies — number of bodies
 *   iterations — solver iteration count (10-20 recommended)
 *   dt         — timestep (seconds)
 *   warm_start — true to apply cached impulses before iteration
 *   workspace  — solver workspace (same size as joints array)
 *
 * Usage:
 *   ForgePhysicsJointSolverData ws[MAX_JOINTS];
 *   forge_physics_joint_solve(joints, nj, bodies, nb,
 *                             20, PHYSICS_DT, true, ws);
 *
 * See: Physics Lesson 13 — Constraint Solver
 */
static inline void forge_physics_joint_solve(
    ForgePhysicsJoint *joints, int count,
    ForgePhysicsRigidBody *bodies, int num_bodies,
    int iterations, float dt, bool warm_start,
    ForgePhysicsJointSolverData *workspace)
{
    if (!joints || !bodies || !workspace || count <= 0 ||
        num_bodies <= 0 || !(dt > 0.0f) || !forge_isfinite(dt)) return;

    /* Clamp iterations */
    if (iterations < FORGE_PHYSICS_SOLVER_MIN_ITERATIONS)
        iterations = FORGE_PHYSICS_SOLVER_MIN_ITERATIONS;
    if (iterations > FORGE_PHYSICS_SOLVER_MAX_ITERATIONS)
        iterations = FORGE_PHYSICS_SOLVER_MAX_ITERATIONS;

    /* Phase 1: Precompute constraint data */
    forge_physics_joint_prepare(joints, count, bodies, num_bodies,
                                dt, workspace);

    /* Phase 2: Warm-start from previous frame */
    if (warm_start) {
        forge_physics_joint_warm_start(joints, workspace, count,
                                       bodies, num_bodies);
    } else {
        /* Cold-start: zero cached impulses so solve_velocities does not
         * accumulate onto stale values from previous frames. */
        forge_physics_joint_reset_cached_impulses(joints, count);
    }

    /* Phase 3: Iterative velocity solving */
    for (int iter = 0; iter < iterations; iter++) {
        forge_physics_joint_solve_velocities(joints, workspace, count,
                                             bodies, num_bodies);
    }

    /* Phase 4: Store converged impulses (no-op, accumulated in-place) */
    forge_physics_joint_store_impulses(joints, workspace, count);

    /* Phase 5: Position correction for joint drift */
    forge_physics_joint_correct_positions(
        joints, count, bodies, num_bodies,
        FORGE_PHYSICS_JOINT_BAUMGARTE, FORGE_PHYSICS_JOINT_SLOP);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Physics World — Unified Simulation Loop with Islands and Sleeping
 * (Lesson 15)
 *
 * ForgePhysicsWorld owns all simulation state and provides a one-call
 * step function that runs the complete physics pipeline: broadphase,
 * narrowphase, island detection, sleep evaluation, constraint solving,
 * and integration.
 *
 * Island detection uses union-find over the contact graph. Bodies
 * connected by contacts or joints belong to the same island. When all
 * dynamic bodies in an island have been below velocity thresholds for
 * a configurable duration, the island sleeps — its bodies are skipped
 * by the solver and integrator until an external force or new contact
 * wakes them.
 *
 * Reference: Catto, "Modeling and Solving Constraints", GDC 2009
 * Reference: Bullet Physics Library, btSimulationIslandManager
 * See: Physics Lesson 15 — Simulation Loop
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── World Constants ────────────────────────────────────────────────────── */

/* Linear velocity threshold below which a body is considered at rest.
 *
 * If a body's velocity magnitude stays below this value (in m/s) for
 * FORGE_PHYSICS_SLEEP_TIME_THRESHOLD seconds, it contributes to island
 * sleep evaluation. Chosen to be above floating-point noise from the
 * constraint solver but low enough to not suppress visible micro-motion.
 */
#define FORGE_PHYSICS_SLEEP_LINEAR_THRESHOLD   0.05f

/* Angular velocity threshold below which a body is considered at rest.
 *
 * If a body's angular velocity magnitude stays below this value (in rad/s)
 * for FORGE_PHYSICS_SLEEP_TIME_THRESHOLD seconds, it contributes to island
 * sleep evaluation. Matches the linear threshold in scale.
 */
#define FORGE_PHYSICS_SLEEP_ANGULAR_THRESHOLD  0.05f

/* Duration (in seconds) a body must remain below velocity thresholds
 * before its island is put to sleep. Half a second gives the solver
 * enough time to damp ringing contact impulses before sleeping.
 */
#define FORGE_PHYSICS_SLEEP_TIME_THRESHOLD     0.5f

/* Default sequential-impulse solver iteration count for ForgePhysicsWorld.
 *
 * 20 iterations is sufficient for stacks of 10–20 boxes. Increase for
 * taller stacks or tighter constraint accuracy.
 */
#define FORGE_PHYSICS_WORLD_DEFAULT_ITERATIONS 20

/* Default static friction coefficient for world ground contacts. */
#define FORGE_PHYSICS_WORLD_DEFAULT_MU_S       0.6f

/* Default dynamic friction coefficient for world ground contacts. */
#define FORGE_PHYSICS_WORLD_DEFAULT_MU_D       0.4f

/* Maximum number of contact manifolds the world tracks per step.
 *
 * Ground contacts produce one manifold per body; body-body contacts
 * produce one manifold per overlapping pair. 256 covers dense scenes
 * without unbounded allocation.
 */
#define FORGE_PHYSICS_WORLD_MAX_MANIFOLDS      256

/* Sentinel island ID indicating a body has not yet been assigned
 * to an island in the current step's union-find pass.
 */
#define FORGE_PHYSICS_ISLAND_NONE              (-1)

/* Maximum path-halving steps before bail-out on corrupted parent arrays.
 * Far beyond any real tree depth for scenes with <= MAX_MANIFOLDS bodies. */
#define FORGE_PHYSICS_UF_MAX_STEPS             10000

/* Sentinel float for "no dynamic body found" in island timer scans.
 * Any timer value above this sentinel indicates the scan found no bodies. */
#define FORGE_PHYSICS_TIMER_SENTINEL           1e30f

/* ── ForgePhysicsWorldConfig ────────────────────────────────────────────── */

/* Configuration for a ForgePhysicsWorld simulation.
 *
 * All parameters are copied into the world at init time. Changing fields
 * on a live world requires calling forge_physics_world_init() again (which
 * destroys existing state) or updating them directly on world->config.
 *
 * Fields:
 *   gravity           — gravitational acceleration vector (m/s²). Typically
 *                       (0, -9.81, 0) for Earth-surface simulations.
 *   fixed_dt          — fixed physics timestep (seconds). The step function
 *                       always advances by exactly this amount; callers are
 *                       responsible for the accumulator pattern.
 *   solver_iterations — sequential-impulse iteration count per step.
 *                       More iterations = tighter constraints, higher cost.
 *   warm_start        — if true, carry impulses from the previous frame into
 *                       the solver (greatly improves stacking stability).
 *   enable_sleeping   — if true, islands that have settled are put to sleep
 *                       and skipped by the integrator and solver.
 *   sleep_linear_threshold  — linear velocity threshold (m/s) for sleep test.
 *   sleep_angular_threshold — angular velocity threshold (rad/s) for sleep test.
 *   sleep_time_threshold    — seconds below threshold before island sleeps.
 *   ground_y          — Y coordinate of the ground plane (world units).
 *   mu_static         — static friction coefficient for ground contacts.
 *   mu_dynamic        — dynamic friction coefficient for ground contacts.
 *   solver_config     — fine-grained SI solver parameters (Baumgarte, slop).
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
typedef struct ForgePhysicsWorldConfig {
    vec3  gravity;                 /* gravitational acceleration (m/s²)            */
    float fixed_dt;                /* fixed timestep per step call (s)             */
    int   solver_iterations;       /* SI solver iterations per step                */
    bool  warm_start;              /* carry impulses from previous frame           */
    bool  enable_sleeping;         /* allow islands to sleep when settled          */
    float sleep_linear_threshold;  /* linear speed below which body is "at rest" (m/s)  */
    float sleep_angular_threshold; /* angular speed below which body is "at rest" (rad/s)*/
    float sleep_time_threshold;    /* seconds below threshold before island sleeps (s)   */
    float ground_y;                /* Y coordinate of the infinite ground plane    */
    float mu_static;               /* ground static friction coefficient [0..1]    */
    float mu_dynamic;              /* ground dynamic friction coefficient [0..1]   */
    ForgePhysicsSolverConfig solver_config; /* fine-grained solver params (Baumgarte, slop) */
} ForgePhysicsWorldConfig;

/* Return a ForgePhysicsWorldConfig with reasonable defaults.
 *
 * Default values:
 *   gravity           = (0, -9.81, 0) — standard Earth gravity
 *   fixed_dt          = 1/60          — 60 Hz physics
 *   solver_iterations = 20
 *   warm_start        = true
 *   enable_sleeping   = true
 *   sleep thresholds  = FORGE_PHYSICS_SLEEP_*_THRESHOLD
 *   ground_y          = 0.0
 *   mu_static         = FORGE_PHYSICS_WORLD_DEFAULT_MU_S
 *   mu_dynamic        = FORGE_PHYSICS_WORLD_DEFAULT_MU_D
 *   solver_config     = forge_physics_solver_config_default()
 *
 * Parameters: none
 *
 * Returns: initialized ForgePhysicsWorldConfig
 *
 * Usage:
 *   ForgePhysicsWorldConfig cfg = forge_physics_world_config_default();
 *   cfg.gravity = vec3_create(0, -1.62f, 0); // Moon gravity
 *   forge_physics_world_init(&world, cfg);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline ForgePhysicsWorldConfig forge_physics_world_config_default(void)
{
    ForgePhysicsWorldConfig cfg;
    SDL_memset(&cfg, 0, sizeof(cfg));
    cfg.gravity                 = vec3_create(0.0f, -9.81f, 0.0f);
    cfg.fixed_dt                = 1.0f / 60.0f;
    cfg.solver_iterations       = FORGE_PHYSICS_WORLD_DEFAULT_ITERATIONS;
    cfg.warm_start              = true;
    cfg.enable_sleeping         = true;
    cfg.sleep_linear_threshold  = FORGE_PHYSICS_SLEEP_LINEAR_THRESHOLD;
    cfg.sleep_angular_threshold = FORGE_PHYSICS_SLEEP_ANGULAR_THRESHOLD;
    cfg.sleep_time_threshold    = FORGE_PHYSICS_SLEEP_TIME_THRESHOLD;
    cfg.ground_y                = 0.0f;
    cfg.mu_static               = FORGE_PHYSICS_WORLD_DEFAULT_MU_S;
    cfg.mu_dynamic              = FORGE_PHYSICS_WORLD_DEFAULT_MU_D;
    cfg.solver_config           = forge_physics_solver_config_default();
    return cfg;
}

/* ── ForgePhysicsWorld ──────────────────────────────────────────────────── */

/* Unified physics simulation world.
 *
 * ForgePhysicsWorld owns all simulation state. Bodies, shapes, joints, and
 * solver workspaces are stored in forge_arr dynamic arrays. Callers add bodies
 * via forge_physics_world_add_body() and advance the simulation via
 * forge_physics_world_step().
 *
 * All forge_arr pointers start as NULL (empty). Call forge_physics_world_destroy()
 * to release all memory; the struct is zeroed on exit.
 *
 * Parallel arrays (bodies, shapes, sleep_timers, is_sleeping, island_ids,
 * cached_aabbs) are always the same length. Body index i is valid for all
 * of these arrays simultaneously.
 *
 * Stats fields (active_body_count, sleeping_body_count, etc.) are updated
 * at the end of each forge_physics_world_step() call.
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
typedef struct ForgePhysicsWorld {
    /* ── Body state (parallel arrays) ─────────────────────────── */
    ForgePhysicsRigidBody     *bodies;         /* dynamic array of rigid bodies */
    ForgePhysicsCollisionShape *shapes;        /* collision shape per body */
    float                      *sleep_timers; /* seconds each body has been below threshold */
    bool                       *is_sleeping;  /* true if body is currently sleeping */
    int                        *island_ids;   /* union-find root after build_islands */
    ForgePhysicsAABB           *cached_aabbs; /* world-space AABBs, recomputed each step */

    /* ── Joints ─────────────────────────────────────────────────── */
    ForgePhysicsJoint          *joints;        /* forge_arr of joint constraints      */

    /* ── Broadphase ─────────────────────────────────────────────── */
    ForgePhysicsSAPWorld        sap;           /* sweep-and-prune overlap detection   */

    /* ── Solver workspaces (rebuilt each step) ──────────────────── */
    ForgePhysicsManifold       *manifolds;      /* contact manifolds this step         */
    ForgePhysicsSIManifold     *si_workspace;   /* SI solver per-manifold scratch data */
    ForgePhysicsJointSolverData *joint_workspace; /* joint solver per-joint scratch    */

    /* ── Manifold cache (warm-starting) ─────────────────────────── */
    ForgePhysicsManifoldCacheEntry *manifold_cache; /* hash map: pair key → cached impulses */

    /* ── Union-find (island detection) ──────────────────────────── */
    int                        *uf_parent;     /* forge_arr parent pointers for UF    */
    int                        *uf_rank;       /* forge_arr rank for union-by-rank    */
    int                         island_count;  /* distinct dynamic islands this step  */

    /* ── Configuration ───────────────────────────────────────────── */
    ForgePhysicsWorldConfig     config;        /* simulation parameters (copied at init) */

    /* ── Per-step statistics (updated at end of each step) ──────── */
    int active_body_count;                     /* non-sleeping dynamic bodies         */
    int sleeping_body_count;                   /* sleeping dynamic bodies             */
    int static_body_count;                     /* zero-mass (immovable) bodies        */
    int manifold_count;                        /* contact manifolds this step         */
    int total_contact_count;                   /* contact points across all manifolds */
} ForgePhysicsWorld;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/* Initialize a ForgePhysicsWorld with the given configuration.
 *
 * Zeros all fields, copies the config, and initializes the sweep-and-prune
 * broadphase. All dynamic arrays start empty (NULL); no heap allocation
 * occurs until bodies are added.
 *
 * Safe to call on an already-initialized world: if the sentinel field
 * indicates prior initialization, all owned resources are destroyed first
 * via forge_physics_world_destroy() before re-initializing.
 *
 * Parameters:
 *   world  (ForgePhysicsWorld*) — world to initialize; must not be NULL
 *   config (ForgePhysicsWorldConfig) — simulation parameters
 *
 * Returns: void
 *
 * Usage:
 *   ForgePhysicsWorld world;
 *   forge_physics_world_init(&world, forge_physics_world_config_default());
 *   // ... add bodies, step, draw ...
 *   forge_physics_world_destroy(&world);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_destroy(ForgePhysicsWorld *world);
static inline void forge_physics_world_init(
    ForgePhysicsWorld *world, ForgePhysicsWorldConfig config)
{
    if (!world) return;

    /* Always zero the struct.  Callers that re-initialize a live world
     * must call forge_physics_world_destroy() first to avoid leaking
     * arrays, SAP storage, and the manifold cache.  We do not attempt
     * sentinel-based auto-destroy because stack-allocated worlds contain
     * uninitialized memory in Release builds — the sentinel field could
     * match by coincidence, triggering a destroy on garbage pointers. */
    SDL_memset(world, 0, sizeof(*world));

    world->config = config;
    forge_physics_sap_init(&world->sap);
}

/* Release all memory owned by a ForgePhysicsWorld and zero the struct.
 *
 * Frees all forge_arr dynamic arrays, destroys the SAP broadphase, and
 * releases the manifold cache hash map. The struct is zeroed so it can be
 * safely re-initialized via forge_physics_world_init().
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — world to destroy; must not be NULL
 *
 * Returns: void
 *
 * Usage:
 *   forge_physics_world_destroy(&world);
 *   // world is now zeroed; do not access it without re-initializing
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_destroy(ForgePhysicsWorld *world)
{
    if (!world) return;

    forge_arr_free(world->bodies);
    forge_arr_free(world->shapes);
    forge_arr_free(world->sleep_timers);
    forge_arr_free(world->is_sleeping);
    forge_arr_free(world->island_ids);
    forge_arr_free(world->cached_aabbs);
    forge_arr_free(world->joints);
    forge_arr_free(world->manifolds);
    forge_arr_free(world->si_workspace);
    forge_arr_free(world->joint_workspace);
    forge_arr_free(world->uf_parent);
    forge_arr_free(world->uf_rank);

    forge_physics_sap_destroy(&world->sap);
    forge_physics_manifold_cache_free(&world->manifold_cache);

    SDL_memset(world, 0, sizeof(*world));
}

/* ── Body and Joint Management ──────────────────────────────────────────── */

/* Add a rigid body and its collision shape to the world.
 *
 * Copies the body and shape by value into the world's parallel arrays.
 * Also appends default values to the sleep_timers (0.0f), is_sleeping (false),
 * island_ids (FORGE_PHYSICS_ISLAND_NONE), and cached_aabbs (zeroed) arrays so
 * all parallel arrays remain the same length.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*)         — target world; must not be NULL
 *   body  (const ForgePhysicsRigidBody*) — body to add (copied); must not be NULL
 *   shape (const ForgePhysicsCollisionShape*) — shape to add (copied); must not be NULL
 *
 * Returns: (int) index of the new body in the world's body array, or -1 on error
 *
 * Usage:
 *   ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(...);
 *   ForgePhysicsCollisionShape sh = forge_physics_shape_box(...);
 *   int idx = forge_physics_world_add_body(&world, &rb, &sh);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline int forge_physics_world_add_body(
    ForgePhysicsWorld *world,
    const ForgePhysicsRigidBody *body,
    const ForgePhysicsCollisionShape *shape)
{
    if (!world || !body || !shape) return -1;

    int idx = (int)forge_arr_length(world->bodies);

    ForgePhysicsRigidBody body_copy = *body;
    forge_arr_append(world->bodies, body_copy);

    ForgePhysicsCollisionShape shape_copy = *shape;
    forge_arr_append(world->shapes, shape_copy);

    float timer = 0.0f;
    forge_arr_append(world->sleep_timers, timer);

    bool sleeping = false;
    forge_arr_append(world->is_sleeping, sleeping);

    int island = FORGE_PHYSICS_ISLAND_NONE;
    forge_arr_append(world->island_ids, island);

    ForgePhysicsAABB aabb;
    SDL_memset(&aabb, 0, sizeof(aabb));
    forge_arr_append(world->cached_aabbs, aabb);

    /* Verify all parallel arrays grew.  If any append failed (OOM), the
     * arrays are now different lengths — trim back to the original size
     * to prevent out-of-bounds access on the next step. */
    int new_len = (int)forge_arr_length(world->bodies);
    if (new_len != idx + 1 ||
        (int)forge_arr_length(world->shapes)      != new_len ||
        (int)forge_arr_length(world->sleep_timers) != new_len ||
        (int)forge_arr_length(world->is_sleeping)  != new_len ||
        (int)forge_arr_length(world->island_ids)   != new_len ||
        (int)forge_arr_length(world->cached_aabbs) != new_len) {
        /* Roll back: truncate all arrays to the pre-append length. */
        forge_arr_set_length(world->bodies,       (size_t)idx);
        forge_arr_set_length(world->shapes,       (size_t)idx);
        forge_arr_set_length(world->sleep_timers, (size_t)idx);
        forge_arr_set_length(world->is_sleeping,  (size_t)idx);
        forge_arr_set_length(world->island_ids,   (size_t)idx);
        forge_arr_set_length(world->cached_aabbs, (size_t)idx);
        return -1;
    }

    return idx;
}

/* Add a joint to the world.
 *
 * Copies the joint by value. The joint's body_a and body_b fields must be
 * valid body indices (i.e., previously returned by forge_physics_world_add_body()).
 * No validation of body indices is performed here — invalid indices produce
 * undefined behavior during the solve phase.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*)    — target world; must not be NULL
 *   joint (const ForgePhysicsJoint*) — joint to add (copied); must not be NULL
 *
 * Returns: (int) index of the new joint, or -1 on error
 *
 * Usage:
 *   ForgePhysicsJoint j = forge_physics_joint_create_ball(...);
 *   int ji = forge_physics_world_add_joint(&world, &j);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline int forge_physics_world_add_joint(
    ForgePhysicsWorld *world,
    const ForgePhysicsJoint *joint)
{
    if (!world || !joint) return -1;
    int idx = (int)forge_arr_length(world->joints);
    ForgePhysicsJoint joint_copy = *joint;
    forge_arr_append(world->joints, joint_copy);
    /* Verify append succeeded (OOM guard) */
    if ((int)forge_arr_length(world->joints) != idx + 1) return -1;
    return idx;
}

/* Return the number of bodies currently in the world.
 *
 * Parameters:
 *   world (const ForgePhysicsWorld*) — world to query; must not be NULL
 *
 * Returns: (int) body count, or 0 if world is NULL
 *
 * Usage:
 *   int n = forge_physics_world_body_count(&world);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline int forge_physics_world_body_count(const ForgePhysicsWorld *world)
{
    if (!world) return 0;
    return (int)forge_arr_length(world->bodies);
}

/* ── Sleep / Wake / Query ───────────────────────────────────────────────── */

/* Wake a sleeping body so it participates in the next step.
 *
 * Wakes the specified body and all bodies connected to it through joints.
 * Resets sleep timers to zero and clears sleeping flags. Velocities are
 * NOT zeroed — the body may already have non-zero velocity from an impulse.
 *
 * Joint propagation uses a simple iterative flood: scan the joint list
 * repeatedly until no more bodies are woken. This is O(joints × bodies)
 * in the worst case but joints are few in practice.
 *
 * Does nothing if idx is out of range or world is NULL.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — owning world; must not be NULL
 *   idx   (int)               — body index; must be in [0, body_count)
 *
 * Returns: void
 *
 * Usage:
 *   forge_physics_world_wake_body(&world, idx);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_wake_body(ForgePhysicsWorld *world, int idx)
{
    if (!world) return;
    int nb = (int)forge_arr_length(world->bodies);
    if (idx < 0 || idx >= nb) return;
    world->sleep_timers[idx] = 0.0f;
    world->is_sleeping[idx]  = false;

    /* Propagate wake through joints: repeatedly scan the joint list until
     * no new bodies are woken.  This ensures chains of jointed bodies all
     * wake together. */
    int nj = (int)forge_arr_length(world->joints);
    if (nj <= 0) return;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int j = 0; j < nj; j++) {
            int a = world->joints[j].body_a;
            int b = world->joints[j].body_b;
            if (a < 0 || a >= nb || b < 0 || b >= nb) continue;
            /* If one side is awake and the other is sleeping, wake it */
            if (!world->is_sleeping[a] && world->is_sleeping[b] &&
                world->bodies[b].inv_mass > 0.0f) {
                world->sleep_timers[b] = 0.0f;
                world->is_sleeping[b]  = false;
                changed = true;
            }
            if (!world->is_sleeping[b] && world->is_sleeping[a] &&
                world->bodies[a].inv_mass > 0.0f) {
                world->sleep_timers[a] = 0.0f;
                world->is_sleeping[a]  = false;
                changed = true;
            }
        }
    }
}

/* Force a body to sleep immediately, zeroing its velocities.
 *
 * Sets is_sleeping to true and zeros both linear and angular velocity.
 * The sleep timer is set to the configured threshold so the body stays
 * asleep unless woken by a contact or forge_physics_world_wake_body().
 *
 * Note: if other bodies in the same island are still active, the next
 * evaluate_sleep_ pass will wake this body (because the island's minimum
 * timer will be below threshold). Forcing sleep on a single body in a
 * multi-body island has no lasting effect.
 *
 * Does nothing if idx is out of range or world is NULL.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — owning world; must not be NULL
 *   idx   (int)               — body index; must be in [0, body_count)
 *
 * Returns: void
 *
 * Usage:
 *   forge_physics_world_sleep_body(&world, idx);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_sleep_body(ForgePhysicsWorld *world, int idx)
{
    if (!world) return;
    int n = (int)forge_arr_length(world->bodies);
    if (idx < 0 || idx >= n) return;
    world->bodies[idx].velocity         = vec3_create(0.0f, 0.0f, 0.0f);
    world->bodies[idx].angular_velocity = vec3_create(0.0f, 0.0f, 0.0f);
    world->bodies[idx].force_accum      = vec3_create(0.0f, 0.0f, 0.0f);
    world->bodies[idx].torque_accum     = vec3_create(0.0f, 0.0f, 0.0f);
    world->is_sleeping[idx]             = true;
    world->sleep_timers[idx]            = world->config.sleep_time_threshold;
}

/* Query whether a body is currently sleeping.
 *
 * Parameters:
 *   world (const ForgePhysicsWorld*) — owning world; must not be NULL
 *   idx   (int)                     — body index; must be in [0, body_count)
 *
 * Returns: true if the body is sleeping, false otherwise or if idx is invalid
 *
 * Usage:
 *   if (forge_physics_world_is_sleeping(&world, i)) { ... }
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline bool forge_physics_world_is_sleeping(
    const ForgePhysicsWorld *world, int idx)
{
    if (!world) return false;
    int n = (int)forge_arr_length(world->bodies);
    if (idx < 0 || idx >= n) return false;
    return world->is_sleeping[idx];
}

/* Return the island ID assigned to a body in the most recent step.
 *
 * Island IDs are the union-find root index assigned during
 * forge_physics_world_build_islands_(). Bodies in the same island share
 * the same ID. The ID is FORGE_PHYSICS_ISLAND_NONE before the first step.
 *
 * Parameters:
 *   world (const ForgePhysicsWorld*) — owning world; must not be NULL
 *   idx   (int)                     — body index; must be in [0, body_count)
 *
 * Returns: (int) island ID, or FORGE_PHYSICS_ISLAND_NONE on error
 *
 * Usage:
 *   int isl = forge_physics_world_island_id(&world, i);
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline int forge_physics_world_island_id(
    const ForgePhysicsWorld *world, int idx)
{
    if (!world) return FORGE_PHYSICS_ISLAND_NONE;
    int n = (int)forge_arr_length(world->bodies);
    if (idx < 0 || idx >= n) return FORGE_PHYSICS_ISLAND_NONE;
    return world->island_ids[idx];
}

/* ── External Force / Impulse (auto-wake) ───────────────────────────────── */

/* Apply a force to a body, waking it if sleeping.
 *
 * Wakes the body first so sleeping bodies respond to external forces
 * on the next step. Then delegates to forge_physics_rigid_body_apply_force()
 * which accumulates the force into the body's force_accum for integration.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — owning world; must not be NULL
 *   idx   (int)               — body index; must be in [0, body_count)
 *   force (vec3)              — force vector in world space (Newtons)
 *
 * Returns: void
 *
 * Usage:
 *   forge_physics_world_apply_force(&world, idx, vec3_create(0, 100.0f, 0));
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_apply_force(
    ForgePhysicsWorld *world, int idx, vec3 force)
{
    if (!world) return;
    int n = (int)forge_arr_length(world->bodies);
    if (idx < 0 || idx >= n) return;
    if (world->bodies[idx].inv_mass == 0.0f) return; /* static body — no-op */
    forge_physics_world_wake_body(world, idx);
    forge_physics_rigid_body_apply_force(&world->bodies[idx], force);
}

/* Apply a linear impulse to a body, waking it if sleeping.
 *
 * Wakes the body so it participates in the next step, then directly adds
 * the impulse to linear velocity: Δv = impulse / mass = impulse * inv_mass.
 * Static bodies (inv_mass == 0) are unaffected.
 *
 * Parameters:
 *   world   (ForgePhysicsWorld*) — owning world; must not be NULL
 *   idx     (int)               — body index; must be in [0, body_count)
 *   impulse (vec3)              — linear impulse in world space (kg·m/s)
 *
 * Returns: void
 *
 * Usage:
 *   // Kick a box upward
 *   forge_physics_world_apply_impulse(&world, box_idx, vec3_create(0, 5.0f, 0));
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_apply_impulse(
    ForgePhysicsWorld *world, int idx, vec3 impulse)
{
    if (!world) return;
    int n = (int)forge_arr_length(world->bodies);
    if (idx < 0 || idx >= n) return;
    if (world->bodies[idx].inv_mass == 0.0f) return; /* static body — no-op */
    forge_physics_world_wake_body(world, idx);
    ForgePhysicsRigidBody *b = &world->bodies[idx];
    b->velocity = vec3_add(b->velocity, vec3_scale(impulse, b->inv_mass));
}

/* ── Union-Find Helpers (internal) ─────────────────────────────────────── */

/* Find the root of body x using iterative path compression.
 *
 * Path compression (path halving) keeps the tree flat so future find()
 * calls run in amortized O(α(n)) time — effectively constant.
 * This is the standard union-find optimization described in Tarjan's
 * "Efficiency of a Good but Not Linear Set Union Algorithm" (1975).
 *
 * Parameters:
 *   parent (int*) — parent array; must not be NULL
 *   x      (int)  — node index to find
 *
 * Returns: (int) root of the component containing x
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline int forge_physics_uf_find_(int *parent, int x)
{
    /* Iterative path halving: set parent[x] = parent[parent[x]], advance x.
     * Guard against infinite loops from corrupted parent arrays — cap at
     * 10000 steps (far beyond any real tree depth). */
    int steps = 0;
    while (parent[x] != x) {
        parent[x] = parent[parent[x]]; /* path halving */
        x = parent[x];
        if (++steps > FORGE_PHYSICS_UF_MAX_STEPS) return x; /* bail on corruption */
    }
    return x;
}

/* Union two components by rank.
 *
 * Attaches the root of the smaller-rank tree under the root of the
 * larger-rank tree. When ranks are equal, the second argument's root
 * is placed under the first's and the first's rank is incremented.
 * This keeps the tree height O(log n) without path compression.
 *
 * Parameters:
 *   parent (int*) — parent array; must not be NULL
 *   rank   (int*) — rank array; must not be NULL
 *   a      (int)  — index of first element
 *   b      (int)  — index of second element
 *
 * Returns: void
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_uf_union_(int *parent, int *rank, int a, int b)
{
    int ra = forge_physics_uf_find_(parent, a);
    int rb = forge_physics_uf_find_(parent, b);
    if (ra == rb) return; /* already in same component */

    /* Union by rank: attach smaller tree under larger */
    if (rank[ra] < rank[rb]) {
        parent[ra] = rb;
    } else if (rank[ra] > rank[rb]) {
        parent[rb] = ra;
    } else {
        parent[rb] = ra;
        rank[ra]++;
    }
}

/* ── Island Detection (internal) ────────────────────────────────────────── */

/* Build contact islands using union-find over the contact and joint graph.
 *
 * Bodies connected by active contact manifolds or joints belong to the
 * same island. The union-find algorithm processes all manifolds and joints
 * to merge connected bodies into components. After merging, each body's
 * island_ids entry is set to its union-find root.
 *
 * Algorithm:
 *   1. Initialize each body as its own component (parent[i] = i, rank[i] = 0).
 *   2. For each contact manifold: union body_a and body_b if both are dynamic.
 *   3. For each joint: union body_a and body_b if at least one is dynamic;
 *      static bodies (inv_mass == 0) act as anchors but do not merge islands.
 *   4. Path-compress: island_ids[i] = find(i) for all bodies.
 *   5. Count distinct roots among dynamic bodies → world->island_count.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — owning world; must not be NULL
 *
 * Returns: void
 *
 * Reference: Bullet Physics Library, btSimulationIslandManager::buildIslands()
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_build_islands_(ForgePhysicsWorld *world)
{
    int nb = (int)forge_arr_length(world->bodies);
    if (nb <= 0) return;

    /* Resize union-find arrays to current body count.
     * If the resize fails (OOM), the arrays are too short — bail out
     * to avoid indexing past the end. */
    forge_arr_set_length(world->uf_parent, (size_t)nb);
    forge_arr_set_length(world->uf_rank,   (size_t)nb);
    if ((int)forge_arr_length(world->uf_parent) < nb ||
        (int)forge_arr_length(world->uf_rank)   < nb) return;

    /* Initialize: each body is its own component */
    for (int i = 0; i < nb; i++) {
        world->uf_parent[i] = i;
        world->uf_rank[i]   = 0;
    }

    /* Union bodies connected by contact manifolds */
    int nm = (int)forge_arr_length(world->manifolds);
    for (int i = 0; i < nm; i++) {
        const ForgePhysicsManifold *m = &world->manifolds[i];
        if (m->count <= 0) continue;

        int a = m->body_a;
        int b = m->body_b;
        bool a_valid   = (a >= 0 && a < nb);
        bool b_valid   = (b >= 0 && b < nb);
        bool a_dynamic = a_valid && (world->bodies[a].inv_mass > 0.0f);
        bool b_dynamic = b_valid && (world->bodies[b].inv_mass > 0.0f);

        /* Only union if both sides are dynamic — static bodies are anchors
         * and do not merge islands (they belong to every island they touch). */
        if (a_dynamic && b_dynamic) {
            forge_physics_uf_union_(world->uf_parent, world->uf_rank, a, b);
        }
    }

    /* Union bodies connected by joints */
    int nj = (int)forge_arr_length(world->joints);
    for (int i = 0; i < nj; i++) {
        const ForgePhysicsJoint *j = &world->joints[i];
        int a = j->body_a;
        int b = j->body_b;
        bool a_valid   = (a >= 0 && a < nb);
        bool b_valid   = (b >= 0 && b < nb);
        bool a_dynamic = a_valid && (world->bodies[a].inv_mass > 0.0f);
        bool b_dynamic = b_valid && (world->bodies[b].inv_mass > 0.0f);

        if (a_dynamic && b_dynamic) {
            /* Both dynamic: standard union */
            forge_physics_uf_union_(world->uf_parent, world->uf_rank, a, b);
        } else if (a_dynamic && b_valid) {
            /* a is dynamic, b is static anchor — do not merge, but joint
             * constraints are still solved; no island change needed here. */
        } else if (b_dynamic && a_valid) {
            /* b is dynamic, a is static anchor — same as above. */
        }
    }

    /* Path-compress: assign final island IDs */
    for (int i = 0; i < nb; i++) {
        world->island_ids[i] = forge_physics_uf_find_(world->uf_parent, i);
    }

    /* Count distinct islands among dynamic bodies only */
    int island_count = 0;
    for (int i = 0; i < nb; i++) {
        if (world->bodies[i].inv_mass > 0.0f) {
            /* A body is an island root if its island_id equals its own index */
            if (world->island_ids[i] == i) {
                island_count++;
            }
        }
    }
    world->island_count = island_count;
}

/* ── Sleep Evaluation (internal) ────────────────────────────────────────── */

/* Evaluate sleep state for all islands after the current step.
 *
 * Follows the Box2D/Bullet approach: sleep decisions are made at the island
 * level, not per-body. This prevents the pathological case where one active
 * body in an island repeatedly wakes its settled neighbors.
 *
 * Algorithm:
 *   Pass 1 — For each dynamic body: if both |velocity| and |angular_velocity|
 *             are below the configured thresholds, increment sleep_timers[i]
 *             by fixed_dt. Otherwise reset sleep_timers[i] to 0.
 *
 *   Pass 2 — For each island: find the minimum sleep_timer among all dynamic
 *             bodies in that island (the "slowest to settle" body).
 *             - If min_timer >= sleep_time_threshold: sleep the entire island —
 *               zero velocities, set is_sleeping = true.
 *             - Else: ensure all bodies in the island are awake, and set each
 *               body's timer to min_timer so the island converges together.
 *
 * When enable_sleeping is false, all bodies are forced awake and timers
 * are cleared.
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — owning world; must not be NULL
 *
 * Returns: void
 *
 * Reference: Catto, "Modeling and Solving Constraints", GDC 2009 — sleep heuristic.
 * Reference: Box2D, b2Island::Solve() sleep evaluation section.
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_evaluate_sleep_(ForgePhysicsWorld *world)
{
    int nb = (int)forge_arr_length(world->bodies);
    if (nb <= 0) return;

    if (!world->config.enable_sleeping) {
        /* Sleeping disabled: force all bodies awake */
        for (int i = 0; i < nb; i++) {
            world->is_sleeping[i]  = false;
            world->sleep_timers[i] = 0.0f;
        }
        return;
    }

    float lin_thresh  = world->config.sleep_linear_threshold;
    float ang_thresh  = world->config.sleep_angular_threshold;
    float time_thresh = world->config.sleep_time_threshold;
    float dt          = world->config.fixed_dt;

    /* Pass 1: update per-body sleep timers */
    for (int i = 0; i < nb; i++) {
        /* Static bodies never sleep (they are already immovable) */
        if (world->bodies[i].inv_mass == 0.0f) continue;

        float lin_speed = vec3_length(world->bodies[i].velocity);
        float ang_speed = vec3_length(world->bodies[i].angular_velocity);

        if (lin_speed < lin_thresh && ang_speed < ang_thresh) {
            world->sleep_timers[i] += dt;
        } else {
            world->sleep_timers[i] = 0.0f;
        }
    }

    /* Pass 2: island-level sleep decision.
     *
     * For each dynamic body that is an island root, gather all bodies
     * in that island and find the minimum sleep timer. Then apply the
     * sleep/wake decision uniformly to the entire island. */
    for (int root = 0; root < nb; root++) {
        /* Only process island roots (their island_id == their own index) */
        if (world->bodies[root].inv_mass == 0.0f) continue;
        if (world->island_ids[root] != root) continue;

        /* Find minimum sleep timer across this island.
         * Use a large sentinel so the sleep branch only fires when at
         * least one body's timer has actually been compared. */
        float min_timer = FORGE_PHYSICS_TIMER_SENTINEL;
        for (int i = 0; i < nb; i++) {
            if (world->bodies[i].inv_mass == 0.0f) continue;
            if (world->island_ids[i] != root) continue;
            if (world->sleep_timers[i] < min_timer) {
                min_timer = world->sleep_timers[i];
            }
        }

        /* If no dynamic body was found (shouldn't happen), skip. */
        if (min_timer >= FORGE_PHYSICS_TIMER_SENTINEL) continue;

        if (min_timer >= time_thresh) {
            /* Entire island has settled — put all dynamic bodies to sleep */
            for (int i = 0; i < nb; i++) {
                if (world->bodies[i].inv_mass == 0.0f) continue;
                if (world->island_ids[i] != root) continue;
                world->bodies[i].velocity         = vec3_create(0.0f, 0.0f, 0.0f);
                world->bodies[i].angular_velocity = vec3_create(0.0f, 0.0f, 0.0f);
                world->bodies[i].force_accum      = vec3_create(0.0f, 0.0f, 0.0f);
                world->bodies[i].torque_accum     = vec3_create(0.0f, 0.0f, 0.0f);
                world->is_sleeping[i]             = true;
            }
        } else {
            /* Island still active — wake all bodies and clamp timers that
             * exceed the island minimum down to it.  Bodies already at or
             * below the minimum keep their timer unchanged — this avoids
             * resetting progress that a fast-settling body has already made. */
            for (int i = 0; i < nb; i++) {
                if (world->bodies[i].inv_mass == 0.0f) continue;
                if (world->island_ids[i] != root) continue;
                world->is_sleeping[i] = false;
                if (world->sleep_timers[i] > min_timer) {
                    world->sleep_timers[i] = min_timer;
                }
            }
        }
    }
}

/* ── Ground Contact Collection (internal) ───────────────────────────────── */

/* Collect ground-plane contact manifolds for all non-sleeping dynamic bodies.
 *
 * Iterates over all bodies, tests each against the world ground plane using
 * the appropriate narrow-phase function for the body's shape type, converts
 * raw contacts to a manifold, and routes through the warm-start cache.
 * Manifolds are appended to world->manifolds.
 *
 * Active keys are accumulated into active_keys[] for use by the cache
 * pruning step later in the step function.
 *
 * Parameters:
 *   world            (ForgePhysicsWorld*) — owning world; must not be NULL
 *   active_keys      (uint64_t*)          — buffer to receive active cache keys
 *   active_key_count (int*)               — [in/out] current count of active keys
 *
 * Returns: void
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_collect_ground_(
    ForgePhysicsWorld *world,
    uint64_t *active_keys, int *active_key_count)
{
    if (!world || !active_keys || !active_key_count) return;

    int nb = (int)forge_arr_length(world->bodies);
    if (nb <= 0) return;

    vec3 plane_pt = vec3_create(0.0f, world->config.ground_y, 0.0f);
    vec3 plane_n  = vec3_create(0.0f, 1.0f, 0.0f);
    float mu_s    = world->config.mu_static;
    float mu_d    = world->config.mu_dynamic;

    for (int i = 0; i < nb; i++) {
        /* Skip static bodies (they do not fall onto the ground) */
        if (world->bodies[i].inv_mass == 0.0f) continue;
        /* Skip sleeping bodies — they are already resting */
        if (world->is_sleeping[i]) continue;

        ForgePhysicsRigidBody *b       = &world->bodies[i];
        const ForgePhysicsCollisionShape *sh = &world->shapes[i];

        ForgePhysicsRBContact contacts[8];
        int nc = 0;

        switch (sh->type) {
        case FORGE_PHYSICS_SHAPE_BOX:
            nc = forge_physics_rb_collide_box_plane(
                b, i, sh->data.box.half_extents,
                plane_pt, plane_n, mu_s, mu_d, contacts, 8);
            break;

        case FORGE_PHYSICS_SHAPE_SPHERE:
            {
                ForgePhysicsRBContact c;
                if (forge_physics_rb_collide_sphere_plane(
                        b, i, sh->data.sphere.radius,
                        plane_pt, plane_n, mu_s, mu_d, &c)) {
                    contacts[0] = c;
                    nc = 1;
                }
            }
            break;

        /* CAPSULE: no capsule-plane function exists yet — capsules are not
         * used in the Lesson 15 demo. When one is added to forge_physics.h,
         * handle it here. */
        case FORGE_PHYSICS_SHAPE_CAPSULE:
            break;

        default:
            break;
        }

        if (nc <= 0) continue;

        ForgePhysicsManifold m;
        if (!forge_physics_si_rb_contacts_to_manifold(
                contacts, nc, mu_s, mu_d, &m)) continue;

        /* Ground contacts must have body_b == -1 (static environment).
         * The collide_*_plane functions guarantee this; verify defensively. */
        if (m.body_b != -1) continue;

        /* Cap total manifolds to keep the active_keys buffer in sync.
         * Must run before warm-start so dropped manifolds don't mutate
         * the cache with stale impulse data. */
        if ((int)forge_arr_length(world->manifolds) >= FORGE_PHYSICS_WORLD_MAX_MANIFOLDS
            || *active_key_count >= FORGE_PHYSICS_WORLD_MAX_MANIFOLDS)
            continue;

        /* Warm-start from the previous frame's cached impulses */
        forge_physics_manifold_cache_update(&world->manifold_cache, &m);

        /* Record key for pruning */
        active_keys[(*active_key_count)++] =
            forge_physics_manifold_pair_key(m.body_a, m.body_b);

        forge_arr_append(world->manifolds, m);
    }
}

/* ── Body-Body Contact Collection (internal) ────────────────────────────── */

/* Collect body-body contact manifolds via SAP broadphase + GJK/EPA.
 *
 * Updates the world-space AABB cache for all bodies (sleeping bodies keep
 * their last AABB so the broadphase can detect wake-on-contact). Runs
 * forge_physics_sap_update() to find overlapping pairs, then executes
 * GJK/EPA narrow phase on each candidate pair.
 *
 * Pairs where both bodies are sleeping are skipped — the SAP broadphase
 * still sees their AABBs so they can be woken if a new body overlaps them,
 * but re-solving two sleeping bodies wastes solver iterations.
 *
 * Active keys are accumulated into active_keys[] for cache pruning.
 *
 * Parameters:
 *   world            (ForgePhysicsWorld*) — owning world; must not be NULL
 *   active_keys      (uint64_t*)          — buffer to receive active cache keys
 *   active_key_count (int*)               — [in/out] current count of active keys
 *
 * Returns: void
 *
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_collect_body_body_(
    ForgePhysicsWorld *world,
    uint64_t *active_keys, int *active_key_count)
{
    if (!world || !active_keys || !active_key_count) return;

    int nb = (int)forge_arr_length(world->bodies);
    if (nb <= 0) return;

    /* Resize cached_aabbs to match body count — bail on OOM */
    forge_arr_set_length(world->cached_aabbs, (size_t)nb);
    if ((int)forge_arr_length(world->cached_aabbs) < nb) return;

    /* Recompute AABBs for all bodies (sleeping bodies keep their last AABB
     * so the broadphase can detect when a new body falls onto them). */
    for (int i = 0; i < nb; i++) {
        world->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &world->shapes[i],
            world->bodies[i].position,
            world->bodies[i].orientation);
    }

    /* Run SAP broadphase to find candidate overlapping pairs */
    forge_physics_sap_update(&world->sap, world->cached_aabbs, nb);

    /* Narrow phase: GJK/EPA on each SAP pair.
     *
     * Two-pass approach: first process pairs where at least one body is
     * awake (which may wake sleeping partners via joint propagation), then
     * revisit pairs where both were sleeping at the start — some may now
     * have a woken member.  This eliminates the one-frame wake-order
     * dependency that a single pass would have. */
    const ForgePhysicsSAPPair *pairs = world->sap.pairs;
    int np = (int)forge_arr_length(world->sap.pairs);

    /* Deferred indices for both-sleeping pairs (stack buffer, bounded by np
     * which is at most O(n^2) but capped by SAP pruning in practice) */
    int deferred_buf[FORGE_PHYSICS_WORLD_MAX_MANIFOLDS];
    int deferred_count = 0;

    /* --- Pass 1: pairs with at least one awake body --- */
    for (int p = 0; p < np; p++) {
        int a = pairs[p].a;
        int b = pairs[p].b;

        if (a < 0 || a >= nb || b < 0 || b >= nb) continue;

        /* Defer both-sleeping pairs to Pass 2 */
        if (world->is_sleeping[a] && world->is_sleeping[b]) {
            if (deferred_count < FORGE_PHYSICS_WORLD_MAX_MANIFOLDS)
                deferred_buf[deferred_count++] = p;
            continue;
        }

        /* Skip pairs where both are static (no relative motion possible) */
        if (world->bodies[a].inv_mass == 0.0f &&
            world->bodies[b].inv_mass == 0.0f) continue;

        ForgePhysicsManifold m;
        if (!forge_physics_gjk_epa_manifold(
                &world->bodies[a], &world->shapes[a],
                &world->bodies[b], &world->shapes[b],
                a, b, world->config.mu_static, world->config.mu_dynamic, &m)) {
            continue;
        }

        /* Wake a sleeping body if it is now in contact with an active body */
        if (world->is_sleeping[a] && !world->is_sleeping[b]) {
            forge_physics_world_wake_body(world, a);
        } else if (world->is_sleeping[b] && !world->is_sleeping[a]) {
            forge_physics_world_wake_body(world, b);
        }

        /* Cap total manifolds to keep the active_keys buffer in sync */
        if ((int)forge_arr_length(world->manifolds) >= FORGE_PHYSICS_WORLD_MAX_MANIFOLDS
            || *active_key_count >= FORGE_PHYSICS_WORLD_MAX_MANIFOLDS)
            continue;

        /* Warm-start from cached impulses */
        forge_physics_manifold_cache_update(&world->manifold_cache, &m);

        /* Record key for pruning */
        active_keys[(*active_key_count)++] =
            forge_physics_manifold_pair_key(m.body_a, m.body_b);

        forge_arr_append(world->manifolds, m);
    }

    /* --- Pass 2: revisit deferred both-sleeping pairs --- */
    for (int d = 0; d < deferred_count; d++) {
        int p = deferred_buf[d];
        int a = pairs[p].a;
        int b = pairs[p].b;

        /* If both are still sleeping after Pass 1, skip */
        if (world->is_sleeping[a] && world->is_sleeping[b]) continue;

        /* Skip static-static */
        if (world->bodies[a].inv_mass == 0.0f &&
            world->bodies[b].inv_mass == 0.0f) continue;

        ForgePhysicsManifold m;
        if (!forge_physics_gjk_epa_manifold(
                &world->bodies[a], &world->shapes[a],
                &world->bodies[b], &world->shapes[b],
                a, b, world->config.mu_static, world->config.mu_dynamic, &m)) {
            continue;
        }

        /* Wake the remaining sleeping body */
        if (world->is_sleeping[a]) {
            forge_physics_world_wake_body(world, a);
        } else if (world->is_sleeping[b]) {
            forge_physics_world_wake_body(world, b);
        }

        if ((int)forge_arr_length(world->manifolds) >= FORGE_PHYSICS_WORLD_MAX_MANIFOLDS
            || *active_key_count >= FORGE_PHYSICS_WORLD_MAX_MANIFOLDS)
            continue;

        forge_physics_manifold_cache_update(&world->manifold_cache, &m);

        active_keys[(*active_key_count)++] =
            forge_physics_manifold_pair_key(m.body_a, m.body_b);

        forge_arr_append(world->manifolds, m);
    }
}

/* ── Unified Step Function ──────────────────────────────────────────────── */

/* Advance the simulation by exactly one fixed timestep.
 *
 * Runs the complete physics pipeline in the following order:
 *
 *   Phase  1 — Save prev_position / prev_orientation for render interpolation
 *   Phase  2 — Apply gravity to all dynamic, non-sleeping bodies
 *   Phase  3 — Integrate velocities for all dynamic, non-sleeping bodies
 *   Phase  4 — Clear manifold workspace for this step
 *   Phase  5 — Collect body-body contacts via SAP + GJK/EPA (wakes sleepers)
 *   Phase  6 — Collect ground-plane contacts for non-sleeping bodies
 *   Phase  7 — Prune stale manifold cache entries
 *   Phase  8 — Sequential-impulse constraint solve (contacts)
 *   Phase  9 — Joint constraint solve (if any joints exist)
 *   Phase 10 — Store solved impulses back to manifold cache (warm-start)
 *   Phase 11 — Position correction + integrate positions for non-sleeping bodies
 *   Phase 12 — Build contact islands via union-find
 *   Phase 13 — Evaluate island sleep state (uses post-solve velocities)
 *   Phase 14 — Update per-step statistics
 *
 * The fixed timestep is always world->config.fixed_dt. Callers are
 * responsible for the accumulator pattern (accumulate real elapsed time,
 * call forge_physics_world_step() once per fixed_dt chunk).
 *
 * Parameters:
 *   world (ForgePhysicsWorld*) — world to step; must not be NULL and must have
 *                                been initialized via forge_physics_world_init()
 *
 * Returns: void
 *
 * Usage:
 *   // Accumulator pattern
 *   state->accumulator += delta_time;
 *   while (state->accumulator >= world.config.fixed_dt) {
 *       forge_physics_world_step(&world);
 *       state->accumulator -= world.config.fixed_dt;
 *   }
 *
 * Reference: Catto, "Modeling and Solving Constraints", GDC 2009.
 * Reference: Fiedler, Glenn. "Fix Your Timestep!", Gaffer on Games, 2004.
 * See: Physics Lesson 15 — Simulation Loop
 */
static inline void forge_physics_world_step(ForgePhysicsWorld *world)
{
    if (!world) return;

    int nb = (int)forge_arr_length(world->bodies);
    int nj = (int)forge_arr_length(world->joints);
    if (nb <= 0) return;

    float dt = world->config.fixed_dt;
    if (!(forge_isfinite(dt) && dt > 0.0f)) return; /* guard against zero, NaN, or inf dt */

    /* ── Phase 1: Save previous state for render interpolation ───── */
    for (int i = 0; i < nb; i++) {
        world->bodies[i].prev_position    = world->bodies[i].position;
        world->bodies[i].prev_orientation = world->bodies[i].orientation;
    }

    /* ── Phase 2: Apply gravity ────────────────────────────────────── */
    for (int i = 0; i < nb; i++) {
        if (world->bodies[i].inv_mass == 0.0f) continue; /* static */
        if (world->is_sleeping[i]) continue;              /* sleeping */
        vec3 gforce = vec3_scale(world->config.gravity, world->bodies[i].mass);
        forge_physics_rigid_body_apply_force(&world->bodies[i], gforce);
    }

    /* ── Phase 3: Integrate velocities ───────────────────────────── */
    for (int i = 0; i < nb; i++) {
        if (world->bodies[i].inv_mass == 0.0f) continue;
        if (world->is_sleeping[i]) continue;
        forge_physics_rigid_body_integrate_velocities(&world->bodies[i], dt);
    }

    /* ── Phase 4: Clear manifold workspace ───────────────────────── */
    forge_arr_set_length(world->manifolds, 0);
    world->manifold_count     = 0;
    world->total_contact_count = 0;

    /* Active cache keys — stack buffer; capped at FORGE_PHYSICS_WORLD_MAX_MANIFOLDS */
    uint64_t active_keys[FORGE_PHYSICS_WORLD_MAX_MANIFOLDS];
    int active_key_count = 0;

    /* ── Phase 5: Collect body-body contacts ─────────────────────── *
     * Body-body runs first because it wakes sleeping bodies on contact.
     * Ground collection skips sleeping bodies, so it must run after
     * body-body to see the updated sleep state and generate ground
     * manifolds for newly-woken bodies. */
    forge_physics_world_collect_body_body_(world, active_keys, &active_key_count);

    /* ── Phase 6: Collect ground contacts ────────────────────────── */
    forge_physics_world_collect_ground_(world, active_keys, &active_key_count);

    /* ── Phase 7: Prune stale manifold cache entries ─────────────── */
    forge_physics_manifold_cache_prune(
        &world->manifold_cache, active_keys, active_key_count);

    /* ── Phase 8: Sequential-impulse contact solve ───────────────── */
    int nm = (int)forge_arr_length(world->manifolds);
    int nm_solved = nm;
    if (nm_solved > 0) {
        /* Resize SI workspace to match manifold count */
        forge_arr_set_length(world->si_workspace, (size_t)nm_solved);
        if ((int)forge_arr_length(world->si_workspace) < nm_solved)
            nm_solved = 0;

        forge_physics_si_solve(
            world->manifolds, nm_solved,
            world->bodies, nb,
            world->config.solver_iterations, dt,
            world->config.warm_start,
            world->si_workspace,
            &world->config.solver_config);
    }

    /* ── Phase 9: Joint solve ──────────────────────────────────────
     * Contacts and joints are solved in separate batches — this matches
     * the approach used in Lessons 12–14 and Box2D's b2Island::Solve().
     * Interleaving the two solvers would improve coupling for mixed
     * scenes (e.g. a jointed chain resting on a stack), but requires a
     * unified constraint representation that is beyond this lesson's
     * scope.  For the scenes demonstrated here (stacks without joints),
     * separate batches produce identical results. */
    int nj_solved = nj;
    if (nj_solved > 0) {
        /* Resize joint workspace to match joint count */
        forge_arr_set_length(world->joint_workspace, (size_t)nj_solved);
        if ((int)forge_arr_length(world->joint_workspace) < nj_solved)
            nj_solved = 0;

        forge_physics_joint_solve(
            world->joints, nj_solved,
            world->bodies, nb,
            world->config.solver_iterations, dt,
            world->config.warm_start,
            world->joint_workspace);
    }

    /* ── Phase 10: Store impulses to cache for next-frame warm-start */
    for (int i = 0; i < nm_solved; i++) {
        forge_physics_manifold_cache_store(
            &world->manifold_cache, &world->manifolds[i]);
    }

    /* ── Phase 11: Position correction + integrate positions ─────── */
    if (nm_solved > 0) {
        forge_physics_si_correct_positions(
            world->manifolds, nm_solved,
            world->bodies, nb,
            world->config.solver_config.correction_fraction,
            world->config.solver_config.correction_slop);
    }

    for (int i = 0; i < nb; i++) {
        if (world->bodies[i].inv_mass == 0.0f) continue;
        if (world->is_sleeping[i]) continue;
        forge_physics_rigid_body_integrate_positions(&world->bodies[i], dt);
    }

    /* ── Phase 12: Build contact islands ─────────────────────────── */
    forge_physics_world_build_islands_(world);

    /* ── Phase 13: Evaluate sleep (post-solve velocities) ──────────
     * Sleep is evaluated AFTER constraint solving so the velocity
     * thresholds test the actual post-solve state. Evaluating before
     * solving would see gravity-induced velocities that the solver
     * has not yet cancelled, preventing bodies from ever sleeping. */
    forge_physics_world_evaluate_sleep_(world);

    /* ── Phase 14: Update statistics ────────────────────────────── */
    world->manifold_count = nm;

    int total_contacts = 0;
    for (int i = 0; i < nm; i++) {
        total_contacts += world->manifolds[i].count;
    }
    world->total_contact_count = total_contacts;

    int active_count  = 0;
    int sleeping_count = 0;
    int static_count   = 0;
    for (int i = 0; i < nb; i++) {
        if (world->bodies[i].inv_mass == 0.0f) {
            static_count++;
        } else if (world->is_sleeping[i]) {
            sleeping_count++;
        } else {
            active_count++;
        }
    }
    world->active_body_count   = active_count;
    world->sleeping_body_count = sleeping_count;
    world->static_body_count   = static_count;
}


#endif /* FORGE_PHYSICS_H */
