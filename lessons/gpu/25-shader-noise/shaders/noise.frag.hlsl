/*
 * noise.frag.hlsl — GPU noise functions for procedural patterns
 *
 * Implements several noise functions entirely on the GPU:
 *   Mode 0 — White noise (hash-based random per pixel)
 *   Mode 1 — Value noise (smoothly interpolated hash values)
 *   Mode 2 — Gradient noise (Perlin 2D with quintic fade curve)
 *   Mode 3 — fBm (fractal Brownian motion — octave-stacked Perlin)
 *   Mode 4 — Domain warping (fBm composed with itself for organic shapes)
 *   Mode 5 — Procedural terrain (fBm height mapped to biome colors)
 *
 * Hash functions:
 *   GPU shaders cannot use rand() — they have no mutable state between
 *   invocations.  Instead, we use deterministic integer hash functions
 *   that map coordinates to pseudo-random values.  The same input always
 *   produces the same output, making the noise repeatable and stable.
 *
 *   This shader ports Thomas Wang's 32-bit integer hash from the math
 *   library (forge_math.h -> forge_hash_wang).  The hash is applied to
 *   integer grid coordinates, and the output is mapped to [0, 1).
 *
 * Gradient noise:
 *   Perlin noise assigns random gradient vectors at integer lattice
 *   points, computes dot products with distance vectors, and smoothly
 *   interpolates using a quintic curve (6t^5 - 15t^4 + 10t^3) that has
 *   zero first and second derivatives at t=0 and t=1.
 *
 * fBm (fractal Brownian motion):
 *   Layers multiple octaves of noise at increasing frequency and
 *   decreasing amplitude.  Each octave doubles the frequency (lacunarity)
 *   and halves the amplitude (persistence).  The sum produces self-similar
 *   detail at multiple scales — clouds, terrain, organic textures.
 *
 * Domain warping:
 *   Uses fBm output to distort the input coordinates of another fBm
 *   evaluation.  This creates organic, flowing patterns that resemble
 *   marble, smoke, or fluid dynamics.  Described by Inigo Quilez (2002).
 *
 * Dithering:
 *   Interleaved Gradient Noise (Jimenez 2014) adds sub-pixel noise to
 *   break up color banding.  The pattern has blue-noise-like spectral
 *   properties — visually uniform distribution without clumping.
 *
 * See also:
 *   - Math Lesson 12 — Hash Functions & White Noise
 *   - Math Lesson 13 — Gradient Noise (Perlin & Simplex)
 *   - Math Lesson 14 — Blue Noise & Low-Discrepancy Sequences
 *
 * SPDX-License-Identifier: Zlib
 */

/* Fragment uniforms — pushed each frame from C code.
 * register(b0, space3) is the SDL GPU convention for fragment uniforms. */
cbuffer NoiseParams : register(b0, space3)
{
    float  time;             /* elapsed time in seconds                      */
    int    mode;             /* noise type: 0-5 (see mode descriptions)      */
    int    dither_enabled;   /* 1 = apply IGN dithering, 0 = off             */
    float  scale;            /* spatial frequency (higher = more detail)      */
    float2 resolution;       /* window size in pixels                         */
    float2 _pad;             /* padding to align cbuffer to 16-byte boundary  */
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};


/* ═══════════════════════════════════════════════════════════════════════════
 * Hash functions
 *
 * These are the GPU-side equivalents of forge_hash_wang, forge_hash_combine,
 * and forge_hash2d from forge_math.h (see Math Lesson 12).
 *
 * GPU shaders lack rand() or any persistent mutable state.  Deterministic
 * hash functions fill this role: given the same integer coordinates they
 * always return the same pseudo-random value, which makes noise patterns
 * stable and repeatable.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Thomas Wang's 32-bit integer hash (2007).
 * Each line applies a different mixing operation (XOR-shift, multiply)
 * to spread entropy across all 32 bits.  The specific constants were
 * chosen for good avalanche properties — changing one input bit
 * flips approximately half the output bits. */
uint wang_hash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

/* Map a 32-bit hash to a float in [0, 1).
 * Dividing by 2^32 maps the full uint range uniformly. */
float hash_to_float(uint h)
{
    return float(h) / 4294967296.0;
}

/* Cascade-combine two hash values (Boost hash_combine pattern).
 * The golden ratio constant 0x9e3779b9 provides good bit mixing when
 * combining multiple dimensions into a single seed. */
uint hash_combine(uint seed, uint value)
{
    return seed ^ (value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u));
}

/* Hash 2D integer coordinates to a uint.
 * Matches forge_hash2d: wang(x) -> combine with y -> wang again.
 * The double application of wang_hash ensures good distribution
 * even when x and y are correlated (e.g. adjacent cells). */
uint hash2d_uint(int2 p)
{
    uint h = wang_hash(uint(p.x));
    h = hash_combine(h, uint(p.y));
    return wang_hash(h);
}

/* Hash 2D integer coordinates to a float in [0, 1). */
float hash2d(int2 p)
{
    return hash_to_float(hash2d_uint(p));
}


/* ═══════════════════════════════════════════════════════════════════════════
 * White noise
 *
 * The simplest noise: hash integer coordinates for a random value per cell.
 * Animated by incorporating time into the hash seed — each frame produces
 * a completely different pattern (TV static effect).
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Multiplier that maps time to integer seed changes.
 * At 60 FPS, each frame gets a unique hash seed for TV-static effect. */
#define WHITE_NOISE_TIME_SCALE 60.0

float white_noise(float2 p, float t)
{
    int2 ip = int2(floor(p));
    /* Mix time into the seed so the pattern changes every frame.
     * Multiplying by 60 makes the pattern refresh roughly per-frame
     * at 60 FPS. */
    uint seed = hash_combine(hash2d_uint(ip), uint(t * WHITE_NOISE_TIME_SCALE));
    return hash_to_float(seed);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Value noise
 *
 * Smoothly interpolates random values at lattice points.  Uses Hermite
 * smoothstep (3t^2 - 2t^3) for interpolation, which has zero first
 * derivative at t=0 and t=1 — eliminating visible seams at cell
 * boundaries.  (See Math Lesson 03 — Bilinear Interpolation.)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Hermite smoothstep: 3t^2 - 2t^3.  Cheaper than quintic, sufficient
 * for value noise since there is no gradient continuity requirement. */
float2 hermite_smooth(float2 t)
{
    return t * t * (3.0 - 2.0 * t);
}

/* Bilinearly interpolate random values at the four surrounding lattice
 * points.  The smoothstep weighting removes the grid pattern that would
 * result from linear interpolation. */
float value_noise(float2 p)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);
    float2 u = hermite_smooth(f);

    /* Hash values at the four corners of the cell. */
    float a = hash2d(i);
    float b = hash2d(i + int2(1, 0));
    float c = hash2d(i + int2(0, 1));
    float d = hash2d(i + int2(1, 1));

    /* Bilinear interpolation with smoothstep weighting. */
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Gradient noise (Perlin 2D)
 *
 * At each lattice point, a pseudo-random gradient is assigned via hashing.
 * The noise value at a point is the smooth interpolation of gradient dot
 * products at the four surrounding corners.
 *
 * This produces smoother, more "organic" results than value noise because
 * the noise function itself is continuous and its derivative is continuous
 * (C2 with quintic interpolation).
 *
 * See Math Lesson 13 — Gradient Noise (Perlin & Simplex).
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Compute the dot product of a pseudo-random gradient with a distance
 * vector.  The gradient is selected from four directions based on the
 * low bits of the hash: (+1,+1), (-1,+1), (+1,-1), (-1,-1).
 *
 * This is the core operation of Perlin noise — each lattice point
 * "pushes" nearby values in its gradient direction, creating smooth
 * directional variation.  Matches forge_noise_grad2d from forge_math.h. */
float grad2d(uint hash, float2 d)
{
    uint h = hash & 3u;
    /* Bit 0 controls the sign of d.x, bit 1 controls the sign of d.y. */
    float u = ((h & 1u) != 0u) ? -d.x : d.x;
    float v = ((h & 2u) != 0u) ? -d.y : d.y;
    return u + v;
}

/* Quintic interpolation curve: 6t^5 - 15t^4 + 10t^3.
 * Unlike Hermite smoothstep, this curve has zero SECOND derivative at
 * t=0 and t=1, which eliminates visible creases where cells meet.
 * Ken Perlin introduced this in "Improving Noise" (2002). */
float2 quintic(float2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

/* 2D Perlin gradient noise.
 * Returns values in approximately [-1, 1]. */
float perlin2d(float2 p)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);
    float2 u = quintic(f);

    /* Gradient dot products at the four cell corners.
     * Each corner's gradient is determined by its hash, and the dot
     * product with the distance vector creates directional variation. */
    float a = grad2d(hash2d_uint(i),              f);
    float b = grad2d(hash2d_uint(i + int2(1, 0)), f - float2(1.0, 0.0));
    float c = grad2d(hash2d_uint(i + int2(0, 1)), f - float2(0.0, 1.0));
    float d = grad2d(hash2d_uint(i + int2(1, 1)), f - float2(1.0, 1.0));

    /* Bilinear interpolation with quintic weighting. */
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * fBm (fractal Brownian motion)
 *
 * Stacks multiple octaves of Perlin noise at increasing frequency and
 * decreasing amplitude.  Each octave adds finer detail:
 *   - Lacunarity (2.0): frequency multiplier per octave
 *   - Persistence (0.5): amplitude multiplier per octave
 *
 * Six octaves provide a good balance between detail and cost.
 * The result approximates natural fractal patterns — clouds, terrain,
 * organic textures.
 *
 * See Math Lesson 13 — Gradient Noise.
 * ═══════════════════════════════════════════════════════════════════════════ */


#define FBM_OCTAVES     6
#define FBM_LACUNARITY  2.0
#define FBM_PERSISTENCE 0.5

float fbm(float2 p)
{
    float value     = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int oct = 0; oct < FBM_OCTAVES; oct++)
    {
        value     += amplitude * perlin2d(p * frequency);
        frequency *= FBM_LACUNARITY;
        amplitude *= FBM_PERSISTENCE;
    }

    return value;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Domain warping
 *
 * Uses fBm output to distort the input coordinates of another fBm
 * evaluation.  This creates organic, flowing patterns resembling marble,
 * smoke, or fluid dynamics.
 *
 * The technique applies two levels of warping:
 *   1. q = (fBm(p + offset_a), fBm(p + offset_b))
 *   2. r = (fBm(p + 4*q + offset_c), fBm(p + 4*q + offset_d))
 *   3. result = fBm(p + 4*r)
 *
 * The constant offsets (5.2, 1.3, etc.) prevent symmetry — without them,
 * the x and y warps would be identical and produce axially symmetric
 * output.  These specific values follow Inigo Quilez's 2002 article on
 * domain warping.
 *
 * The factor 4.0 controls warp intensity — how far coordinates are
 * displaced.  Larger values create more distortion.
 * ═══════════════════════════════════════════════════════════════════════════ */

float domain_warp(float2 p)
{
    /* First warp: sample fBm at offset positions. */
    float2 q = float2(
        fbm(p + float2(0.0, 0.0)),
        fbm(p + float2(5.2, 1.3))
    );

    /* Second warp: feed q-warped coordinates back through fBm. */
    float2 r = float2(
        fbm(p + 4.0 * q + float2(1.7, 9.2)),
        fbm(p + 4.0 * q + float2(8.3, 2.8))
    );

    /* Final evaluation at double-warped coordinates. */
    return fbm(p + 4.0 * r);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Terrain coloring
 *
 * Maps an fBm height value to terrain colors using smooth gradient
 * transitions.  This demonstrates a practical application of noise:
 * procedural terrain with realistic biome coloring.
 * ═══════════════════════════════════════════════════════════════════════════ */

float3 terrain_color(float height)
{
    /* Remap from [-0.5, 0.5] range (typical fBm output) to [0, 1]. */
    float h = saturate(height + 0.5);

    /* Color palette — each biome is a linear RGB color. */
    float3 deep_water    = float3(0.05, 0.10, 0.35);
    float3 shallow_water = float3(0.10, 0.30, 0.50);
    float3 sand          = float3(0.76, 0.69, 0.50);
    float3 grass         = float3(0.20, 0.50, 0.15);
    float3 forest        = float3(0.10, 0.35, 0.10);
    float3 rock          = float3(0.45, 0.40, 0.35);
    float3 snow          = float3(0.90, 0.92, 0.95);

    /* Smoothstep transitions between biomes.
     * Each threshold marks where one biome blends into the next. */
    float3 color = deep_water;
    color = lerp(color, shallow_water, smoothstep(0.30, 0.38, h));
    color = lerp(color, sand,          smoothstep(0.38, 0.42, h));
    color = lerp(color, grass,         smoothstep(0.42, 0.50, h));
    color = lerp(color, forest,        smoothstep(0.50, 0.62, h));
    color = lerp(color, rock,          smoothstep(0.62, 0.75, h));
    color = lerp(color, snow,          smoothstep(0.75, 0.85, h));

    return color;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Interleaved Gradient Noise (dithering)
 *
 * Jimenez 2014 — a fast, GPU-friendly dithering pattern with
 * blue-noise-like spectral properties.  The three constants were
 * optimized to minimize low-frequency energy in the output, producing
 * visually uniform distribution without clumping.
 *
 * Applied after final color computation, IGN breaks up color banding
 * in 8-bit output.  The noise magnitude is 1/255 (one quantization
 * step), so it is invisible in smooth areas but eliminates the
 * staircase pattern of 8-bit quantization.
 *
 * See Math Lesson 14 — Blue Noise & Low-Discrepancy Sequences.
 * ═══════════════════════════════════════════════════════════════════════════ */

float ign(float2 screen_pos)
{
    float3 ign_coeffs = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(ign_coeffs.z * frac(dot(screen_pos, ign_coeffs.xy)));
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Main fragment shader
 * ═══════════════════════════════════════════════════════════════════════════ */

float4 main(PSInput input) : SV_Target
{
    /* Scale UV coordinates to noise space.
     * Maintain aspect ratio so noise cells are square, not stretched. */
    float aspect = resolution.x / resolution.y;
    float2 p = input.uv * scale;
    p.x *= aspect;

    float3 color = float3(0.0, 0.0, 0.0);

    if (mode == 0)
    {
        /* White noise — random per cell, animated with time.
         * Changing the hash seed each frame produces TV-static. */
        float n = white_noise(p, time);
        color = float3(n, n, n);
    }
    else if (mode == 1)
    {
        /* Value noise — smooth interpolation of random lattice values.
         * Slowly scrolls over time to show the smooth variation. */
        float n = value_noise(p + float2(time * 0.3, time * 0.2));
        color = float3(n, n, n);
    }
    else if (mode == 2)
    {
        /* Gradient noise (Perlin) — smooth, directional variation.
         * Remap from [-1, 1] to [0, 1] for display. */
        float n = perlin2d(p + float2(time * 0.3, time * 0.2));
        n = n * 0.5 + 0.5;
        color = float3(n, n, n);
    }
    else if (mode == 3)
    {
        /* fBm — multi-octave Perlin for natural fractal detail.
         * Slowly drifts to reveal the self-similar structure. */
        float n = fbm(p + float2(time * 0.1, time * 0.08));
        n = n * 0.5 + 0.5;
        color = float3(n, n, n);
    }
    else if (mode == 4)
    {
        /* Domain warping — fBm composed with itself.
         * Time is added to the input for flowing animation. */
        float2 animated_p = p + float2(time * 0.05, time * 0.03);
        float n = domain_warp(animated_p);
        n = n * 0.5 + 0.5;

        /* Apply a color gradient to highlight the warp structure.
         * Squaring the red channel darkens shadows, and sqrt on
         * blue brightens highlights, creating a warm-to-cool map. */
        color = float3(
            n * n,
            n,
            sqrt(saturate(n))
        );
    }
    else if (mode == 5)
    {
        /* Procedural terrain — fBm height mapped to biome colors.
         * Demonstrates a practical application of noise in games:
         * generating terrain coloring without any texture assets. */
        float2 terrain_p = p * 0.5 + float2(time * 0.02, time * 0.01);
        float height = fbm(terrain_p);
        color = terrain_color(height);
    }

    /* Optional dithering — add Interleaved Gradient Noise to break
     * up color banding in 8-bit output.  The noise magnitude is
     * 1/255 (one quantization step), so it is invisible in smooth
     * areas but eliminates visible banding in gradients. */
    if (dither_enabled != 0)
    {
        float dither_noise = ign(input.clip_pos.xy) - 0.5;
        color += dither_noise / 255.0;
    }

    return float4(saturate(color), 1.0);
}
