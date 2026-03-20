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
 * If dir is zero or near-zero, returns the shape center (pos).
 *
 * Parameters:
 *   shape  — collision shape
 *   pos    — world-space position of the body
 *   orient — orientation quaternion of the body
 *   dir    — world-space direction to query (need not be normalized)
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
    /* Safe fallback: use pos if finite, otherwise origin */
    vec3 fallback = vec3_create(0.0f, 0.0f, 0.0f);
    if (forge_isfinite(pos.x) && forge_isfinite(pos.y) && forge_isfinite(pos.z))
        fallback = pos;

    if (!shape || !forge_physics_shape_is_valid(shape)) return fallback;

    /* Guard against non-finite inputs to prevent NaN propagation */
    if (!forge_isfinite(pos.x) || !forge_isfinite(pos.y) || !forge_isfinite(pos.z))
        return fallback;
    if (!forge_isfinite(dir.x) || !forge_isfinite(dir.y) || !forge_isfinite(dir.z))
        return fallback;
    if (!forge_isfinite(orient.w) || !forge_isfinite(orient.x) ||
        !forge_isfinite(orient.y) || !forge_isfinite(orient.z))
        return fallback;

    /* Normalize quaternion — conjugate of a non-unit quat is not its inverse,
     * so box/capsule support would be incorrect without this. */
    {
        float qlen_sq = quat_length_sq(orient);
        if (!forge_isfinite(qlen_sq) || !(qlen_sq > FORGE_PHYSICS_EPSILON))
            return fallback;
        if (SDL_fabsf(qlen_sq - 1.0f) > FORGE_PHYSICS_EPSILON)
            orient = quat_normalize(orient);
    }

    float dir_len = vec3_length(dir);
    if (!forge_isfinite(dir_len) || dir_len < FORGE_PHYSICS_EPSILON) return fallback;

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
 * Parameters:
 *   shape_a, shape_b  — collision shapes (non-NULL, must pass shape_is_valid)
 *   pos_a, pos_b      — world-space positions in meters (must be finite)
 *   orient_a, orient_b — unit quaternions (normalized internally if needed;
 *                        must be finite and non-zero-length)
 *   dir               — search direction, unitless (need not be normalized
 *                        but must be finite and non-zero)
 *
 * Returns: fully populated ForgePhysicsGJKVertex, or NaN-sentinel vertex
 *          (all fields NaN) on invalid input — callers detect via
 *          !forge_isfinite(vec3_length_squared(v.point)).
 *
 * See: Physics Lesson 09 — GJK Intersection Testing
 */
static inline ForgePhysicsGJKVertex forge_physics_gjk_support(
    const ForgePhysicsCollisionShape *shape_a, vec3 pos_a, quat orient_a,
    const ForgePhysicsCollisionShape *shape_b, vec3 pos_b, quat orient_b,
    vec3 dir)
{
    /* On failure, return a NaN-sentinel vertex so callers can distinguish
     * "invalid input" from a legitimate zero-point (coincident shapes).
     * The existing finiteness checks in gjk_intersect catch NaN naturally. */
    const float nan_val = SDL_sqrtf(-1.0f); /* portable NaN */
    vec3 nan_vec = vec3_create(nan_val, nan_val, nan_val);
    ForgePhysicsGJKVertex fail = { nan_vec, nan_vec, nan_vec };

    /* Validate shapes */
    if (!shape_a || !shape_b ||
        !forge_physics_shape_is_valid(shape_a) ||
        !forge_physics_shape_is_valid(shape_b))
        return fail;

    /* Validate positions */
    if (!forge_isfinite(pos_a.x) || !forge_isfinite(pos_a.y) ||
        !forge_isfinite(pos_a.z) ||
        !forge_isfinite(pos_b.x) || !forge_isfinite(pos_b.y) ||
        !forge_isfinite(pos_b.z))
        return fail;

    /* Validate direction — zero-length or overflowed direction has no
     * meaningful support. Component Inf is caught first; squared-length
     * overflow (finite components whose squares sum to Inf) is caught second. */
    if (!forge_isfinite(dir.x) || !forge_isfinite(dir.y) ||
        !forge_isfinite(dir.z))
        return fail;
    {
        float dir_len2 = vec3_length_squared(dir);
        if (!forge_isfinite(dir_len2) ||
            dir_len2 <= FORGE_PHYSICS_GJK_EPSILON * FORGE_PHYSICS_GJK_EPSILON)
            return fail;
    }

    /* Validate and normalize quaternions */
    if (!gjk_validate_quat_(&orient_a) || !gjk_validate_quat_(&orient_b))
        return fail;

    ForgePhysicsGJKVertex v;
    vec3 neg_dir = vec3_scale(dir, -1.0f);
    v.sup_a = forge_physics_shape_support(shape_a, pos_a, orient_a, dir);
    v.sup_b = forge_physics_shape_support(shape_b, pos_b, orient_b, neg_dir);
    v.point = vec3_sub(v.sup_a, v.sup_b);

    /* Guard against arithmetic overflow producing INF in the Minkowski
     * difference point — this can happen when positions are very large */
    if (!forge_isfinite(v.point.x) || !forge_isfinite(v.point.y) ||
        !forge_isfinite(v.point.z))
        return fail;
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

#endif /* FORGE_PHYSICS_H */
