/*
 * particle_sim.comp.hlsl — Combined emit + simulate compute kernel
 *
 * Each thread processes one particle from the fixed-size pool:
 *   - Dead particles (lifetime <= 0) attempt to respawn by atomically
 *     decrementing a spawn counter.  If the counter is positive, the
 *     particle is reinitialized with random position/velocity based
 *     on the active emitter type.
 *   - Alive particles integrate velocity (gravity + drag), advance
 *     position, decay lifetime, and update color based on age.
 *
 * The spawn counter is reset each frame by the CPU via a 4-byte copy
 * pass before the compute dispatch.  This budget-based approach avoids
 * GPU readback and lets the CPU control spawn rate precisely.
 *
 * Register layout (SDL GPU compute conventions):
 *   u0, space1 — RWStructuredBuffer<Particle>  (particle pool)
 *   u1, space1 — RWStructuredBuffer<int>        (spawn counter, 1 element)
 *   b0, space2 — cbuffer SimUniforms            (per-frame parameters)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Must match C-side MAX_PARTICLES — used for bounds checking */
#define MAX_PARTICLES 4096

/* ── Particle data layout (64 bytes, float4-aligned) ──────────────────── */

struct Particle {
    float4 pos_lifetime;   /* xyz = world position, w = lifetime remaining (s) */
    float4 vel_size;       /* xyz = velocity (m/s),  w = billboard size        */
    float4 color;          /* rgba                                             */
    float4 max_life_type;  /* x = max lifetime (for age ratio), y = type       */
                           /* z = original size (for smoke growth), w = unused */
};

/* ── GPU resources ────────────────────────────────────────────────────── */

RWStructuredBuffer<Particle> particles     : register(u0, space1);
RWStructuredBuffer<int>      spawn_counter : register(u1, space1);

cbuffer SimUniforms : register(b0, space2) {
    float  dt;               /* frame delta time (seconds)                    */
    float  gravity;          /* gravity acceleration (negative = down)        */
    float  drag;             /* drag coefficient (velocity damping per second)*/
    uint   frame_counter;    /* monotonic frame index for PRNG seeding        */
    float4 emitter_pos;      /* xyz = emitter world position, w = unused      */
    float4 emitter_params;   /* x = type (0=fountain,1=fire,2=smoke)          */
                             /* y = initial speed, z = size_min, w = size_max */
    float4 extra_params;     /* per-emitter tunables (meaning varies by type) */
                             /* fire:  x = spread multiplier (1.0 = default)  */
                             /* smoke: x = rise speed mult, y = spread mult,  */
                             /*        z = base opacity (0–1)                 */
};

/* ── Emitter type constants ───────────────────────────────────────────── */

#define EMITTER_FOUNTAIN  0
#define EMITTER_FIRE      1
#define EMITTER_SMOKE     2

/* ── Hash-based pseudo-random number generator ────────────────────────── */
/* Wang hash — fast, decorrelated, no state needed.  Each particle uses
 * its index combined with the frame counter as a unique seed. */

uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

/* Returns a float in [0, 1) from a hash state.  Advances the state. */
float rand_float(inout uint state) {
    state = wang_hash(state);
    return float(state) / 4294967296.0;
}

/* Returns a float in [lo, hi) */
float rand_range(inout uint state, float lo, float hi) {
    return lo + rand_float(state) * (hi - lo);
}

/* ── Emitter spawn logic ──────────────────────────────────────────────── */

Particle spawn_particle(uint idx, float emitter_type) {
    /* Seed PRNG with particle index and frame counter for unique values */
    uint rng = wang_hash(idx * 1103515245u + frame_counter * 12345u);

    Particle p;
    float3 pos  = emitter_pos.xyz;
    float3 vel  = float3(0, 0, 0);
    float4 col  = float4(1, 1, 1, 1);
    float  size = 0.15;
    float  life = 2.0;

    if (emitter_type < 0.5) {
        /* ── Fountain: upward spray with XZ spread ────────── */
        vel.x = rand_range(rng, -2.0, 2.0);
        vel.y = rand_range(rng, 5.0, 10.0);
        vel.z = rand_range(rng, -2.0, 2.0);
        size  = rand_range(rng, 0.08, 0.18);
        life  = rand_range(rng, 2.0, 4.0);
        col   = float4(0.3, 0.8, 1.0, 1.0);  /* cyan */
    } else if (emitter_type < 1.5) {
        /* ── Fire: cohesive upward flame ─────────────────── */
        /* extra_params.x = spread multiplier (1.0 = default torch width) */
        float fire_spread = extra_params.x;
        pos.x += rand_range(rng, -0.1 * fire_spread, 0.1 * fire_spread);
        pos.z += rand_range(rng, -0.1 * fire_spread, 0.1 * fire_spread);
        vel.x = rand_range(rng, -0.2 * fire_spread, 0.2 * fire_spread);
        vel.y = rand_range(rng, 1.5, 2.5);
        vel.z = rand_range(rng, -0.2 * fire_spread, 0.2 * fire_spread);
        size  = rand_range(rng, 0.25, 0.45);
        life  = rand_range(rng, 0.6, 1.2);
        col   = float4(1.0, 0.9, 0.3, 1.0);  /* bright yellow */
    } else {
        /* ── Smoke: steady constant-speed rise, billows, fades ──── */
        /* extra_params: x = rise speed mult, y = spread mult, z = opacity */
        float smoke_rise   = extra_params.x;
        float smoke_spread = extra_params.y;
        pos.x += rand_range(rng, -0.3 * smoke_spread, 0.3 * smoke_spread);
        pos.y += 0.2;
        pos.z += rand_range(rng, -0.3 * smoke_spread, 0.3 * smoke_spread);
        vel.x = rand_range(rng, -0.15 * smoke_spread, 0.15 * smoke_spread);
        vel.y = rand_range(rng, 1.2 * smoke_rise, 1.6 * smoke_rise);
        vel.z = rand_range(rng, -0.15 * smoke_spread, 0.15 * smoke_spread);
        size  = rand_range(rng, 0.3, 0.5);
        life  = rand_range(rng, 4.0, 6.0);
        col   = float4(0.55, 0.53, 0.5, 0.5);
    }

    p.pos_lifetime = float4(pos, life);
    p.vel_size     = float4(vel, size);
    p.color        = col;
    /* z = original size (for smoke growth over lifetime) */
    p.max_life_type = float4(life, emitter_type, size, 0);

    return p;
}

/* ── Color ramp based on particle age ─────────────────────────────────── */

float4 compute_color(float4 start_color, float age_ratio, float emitter_type) {
    /* age_ratio: 0 = just born, 1 = about to die */

    if (emitter_type < 0.5) {
        /* Fountain: cyan → white → fade out */
        float3 c = lerp(start_color.xyz, float3(1, 1, 1), age_ratio * 0.5);
        float  a = 1.0 - smoothstep(0.6, 1.0, age_ratio);
        return float4(c, a);
    } else if (emitter_type < 1.5) {
        /* Fire: yellow → orange → red → black with intensity fade */
        float3 c;
        if (age_ratio < 0.3) {
            c = lerp(float3(1.0, 0.9, 0.3), float3(1.0, 0.5, 0.05), age_ratio / 0.3);
        } else if (age_ratio < 0.7) {
            float t = (age_ratio - 0.3) / 0.4;
            c = lerp(float3(1.0, 0.5, 0.05), float3(0.6, 0.1, 0.0), t);
        } else {
            float t = (age_ratio - 0.7) / 0.3;
            c = lerp(float3(0.6, 0.1, 0.0), float3(0.05, 0.02, 0.01), t);
        }
        float a = 1.0 - smoothstep(0.7, 1.0, age_ratio);
        return float4(c, a);
    } else {
        /* Smoke: gray, alpha fades out gradually.
         * Use fixed base alpha rather than start_color.a to avoid
         * cumulative fade — color is recomputed from scratch each frame.
         * extra_params.z controls base opacity (0–1). */
        float base_alpha = extra_params.z;
        float a = base_alpha * (1.0 - smoothstep(0.3, 1.0, age_ratio));
        return float4(0.5, 0.5, 0.5, a);
    }
}

/* ── Main kernel ──────────────────────────────────────────────────────── */

[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint idx = tid.x;

    /* Structured buffer bounds check — threads beyond the pool are no-ops */
    /* MAX_PARTICLES is passed implicitly by dispatch group count * 256.
     * We rely on the C side dispatching exactly ceil(MAX_PARTICLES/256)
     * groups, so any excess threads index beyond the buffer.  The
     * RWStructuredBuffer access is bounds-checked by the runtime. */

    /* Bounds check — prevent out-of-bounds access when MAX_PARTICLES
     * is not a multiple of WORKGROUP_SIZE (256). */
    if (idx >= MAX_PARTICLES) return;

    Particle p = particles[idx];

    /* ── First frame: pre-fill the pool to skip cold start ────────── */
    /* Spawn every particle with a randomized age so the effect appears
     * fully formed immediately — no visible ramp-up. */
    if (frame_counter == 0) {
        p = spawn_particle(idx, emitter_params.x);
        /* Randomize remaining lifetime to spread particles across ages */
        uint rng_age = wang_hash(idx * 7919u + 42u);
        float age_frac = rand_float(rng_age);
        float max_life = p.max_life_type.x;
        p.pos_lifetime.w = max_life * age_frac;
        /* Advance position along velocity to match the age */
        float elapsed = max_life * (1.0 - age_frac);
        p.pos_lifetime.xyz += p.vel_size.xyz * elapsed;
        particles[idx] = p;
        return;
    }

    if (p.pos_lifetime.w <= 0.0) {
        /* ── Dead particle: attempt to claim a spawn slot ─────────── */
        int prev;
        InterlockedAdd(spawn_counter[0], -1, prev);

        if (prev > 0) {
            /* Spawn budget available — reinitialize this particle */
            p = spawn_particle(idx, emitter_params.x);
        }
        /* If prev <= 0, no budget remains; particle stays dead.
         * The counter may go negative, but that's fine — the CPU
         * resets it each frame. */
    } else {
        /* ── Alive particle: simulate physics ─────────────────────── */
        float3 vel = p.vel_size.xyz;
        float3 pos = p.pos_lifetime.xyz;

        /* Apply forces based on emitter type. */
        float emitter_type = p.max_life_type.y;
        if (emitter_type >= 1.5) {
            /* Smoke: no gravity, no drag — constant-speed rise.
             * Particles drift upward at their spawn velocity until they
             * die, producing an even column with no gap or clumping. */
        } else if (emitter_type >= 0.5) {
            /* Fire: minimal gravity (hot air rises), moderate drag to
             * keep particles in a tight column that tapers upward. */
            vel.y += gravity * 0.05 * dt;
            float drag_factor = max(1.0 - 0.8 * dt, 0.0);
            vel *= drag_factor;
        } else {
            /* Fountain: full gravity and user-controlled drag. */
            vel.y += gravity * dt;
            float drag_factor = max(1.0 - drag * dt, 0.0);
            vel *= drag_factor;
        }

        /* Integrate position (semi-implicit Euler) */
        pos += vel * dt;

        /* Decay lifetime */
        float life = p.pos_lifetime.w - dt;

        /* Compute age ratio for color/size animation */
        float max_life = p.max_life_type.x;
        float age_ratio = clamp(1.0 - (life / max(max_life, 0.001)), 0.0, 1.0);

        /* Update color based on age and emitter type */
        p.color = compute_color(p.color, age_ratio, emitter_type);

        /* Smoke billows — grows to 2.5x original size over lifetime */
        if (emitter_type >= 1.5) {
            float base_size = p.max_life_type.z;
            p.vel_size.w = base_size * (1.0 + age_ratio * 1.5);
        }

        p.pos_lifetime = float4(pos, life);
        p.vel_size.xyz = vel;
    }

    particles[idx] = p;
}
