/*
 * Math Lesson 12 — Hash Functions & White Noise
 *
 * Demonstrates:
 *   1. Deterministic hashing vs rand() — why GPUs need reproducible randomness
 *   2. Wang hash — multiply-xor-shift mixing
 *   3. PCG hash — permuted congruential generator output permutation
 *   4. xxHash32 finalizer — xor-multiply-shift avalanche
 *   5. Avalanche effect — one-bit input change flips ~16 output bits
 *   6. Key constants — where hash constants come from
 *   7. Hash-to-float — mapping uint32 to uniform [0, 1)
 *   8. Distribution quality — bucket uniformity test
 *   9. Multi-dimensional seeding — combining position coordinates
 *  10. White noise visualization — ASCII density map
 *
 * This is a console program — no window needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

/* Count the number of set bits (population count) in a 32-bit integer.
 * Used to measure the avalanche effect — how many bits changed between
 * two hash outputs. A good hash flips ~16 of 32 bits per input change. */
static int popcount32(uint32_t x)
{
    int count = 0;
    while (x) {
        count += (int)(x & 1u);
        x >>= 1;
    }
    return count;
}

/* Print a 32-bit integer as binary with spaces every 8 bits.
 * Makes it easy to see which bits flipped between two values. */
static void print_binary(uint32_t x)
{
    for (int i = 31; i >= 0; i--) {
        printf("%c", (x >> i) & 1 ? '1' : '0');
        if (i > 0 && i % 8 == 0) printf(" ");
    }
}

/* ── 1. Why Hashing, Not rand()? ─────────────────────────────────────── */

static void demo_why_hashing(void)
{
    print_header("1. WHY HASHING, NOT rand()?");

    printf("\n  GPU shaders execute THOUSANDS of fragments in parallel.\n");
    printf("  C's rand() is:\n");
    printf("    - Sequential: relies on shared mutable state\n");
    printf("    - Non-deterministic across threads: race conditions\n");
    printf("    - Unavailable in GPU shaders (no global state)\n\n");

    printf("  Hash functions are:\n");
    printf("    - Parallel-safe: each pixel computes independently\n");
    printf("    - Deterministic: same input always gives same output\n");
    printf("    - Stateless: no shared memory needed\n\n");

    printf("  Demo — hashing is deterministic:\n\n");

    for (int run = 0; run < 3; run++) {
        uint32_t h = forge_hash_wang(42u);
        printf("    Run %d: forge_hash_wang(42) = %10u (0x%08x)\n",
               run + 1, h, h);
    }

    printf("\n  Same input, same output, every time. This is exactly what\n");
    printf("  a GPU shader needs — reproducible randomness per pixel.\n");
}

/* ── 2. Wang Hash ────────────────────────────────────────────────────── */

static void demo_wang_hash(void)
{
    print_header("2. WANG HASH: Multiply-Xor-Shift Mixing");

    printf("\n  Thomas Wang's integer hash (2007) uses a sequence of\n");
    printf("  xor-shift and multiply operations to mix all input bits\n");
    printf("  thoroughly into the output:\n\n");

    printf("    key = (key ^ 61) ^ (key >> 16)\n");
    printf("    key *= 9\n");
    printf("    key ^= key >> 4\n");
    printf("    key *= 0x27d4eb2d\n");
    printf("    key ^= key >> 15\n\n");

    printf("  %-12s -> %-12s %-12s\n", "Input", "Output (dec)", "Output (hex)");
    printf("  %-12s    %-12s %-12s\n", "-----", "-----------", "-----------");

    uint32_t inputs[] = {0, 1, 2, 3, 100, 1000, 0xDEADBEEFu};
    int count = sizeof(inputs) / sizeof(inputs[0]);

    for (int i = 0; i < count; i++) {
        uint32_t h = forge_hash_wang(inputs[i]);
        printf("  %-12u -> %-12u 0x%08x\n", inputs[i], h, h);
    }

    printf("\n  Consecutive inputs (0, 1, 2, 3) produce wildly different\n");
    printf("  outputs. That is the hallmark of a good hash function.\n");
}

/* ── 3. PCG Hash ─────────────────────────────────────────────────────── */

static void demo_pcg_hash(void)
{
    print_header("3. PCG HASH: Permuted Congruential Generator");

    printf("\n  Based on Melissa O'Neill's PCG (2014). Uses a linear\n");
    printf("  congruential step followed by a data-dependent permutation.\n");
    printf("  The high bits control how the low bits are shuffled:\n\n");

    printf("    state = input * 747796405 + 2891336453\n");
    printf("    word  = ((state >> ((state >> 28) + 4)) ^ state)\n");
    printf("            * 277803737\n");
    printf("    hash  = (word >> 22) ^ word\n\n");

    printf("  %-12s -> %-12s %-12s\n", "Input", "Output (dec)", "Output (hex)");
    printf("  %-12s    %-12s %-12s\n", "-----", "-----------", "-----------");

    uint32_t inputs[] = {0, 1, 2, 3, 100, 1000, 0xDEADBEEFu};
    int count = sizeof(inputs) / sizeof(inputs[0]);

    for (int i = 0; i < count; i++) {
        uint32_t h = forge_hash_pcg(inputs[i]);
        printf("  %-12u -> %-12u 0x%08x\n", inputs[i], h, h);
    }

    printf("\n  The data-dependent shift is a key insight from PCG: it\n");
    printf("  provides stronger mixing than fixed-shift alternatives.\n");
}

/* ── 4. xxHash32 Finalizer ───────────────────────────────────────────── */

static void demo_xxhash32(void)
{
    print_header("4. XXHASH32 FINALIZER: Xor-Multiply-Shift Avalanche");

    printf("\n  Yann Collet's xxHash (2012) uses this finalizer to ensure\n");
    printf("  full avalanche — every input bit affects every output bit.\n");
    printf("  The pattern is xor-shift followed by multiply, repeated:\n\n");

    printf("    h ^= h >> 15\n");
    printf("    h *= 0x85ebca77   (2,246,822,519 — a large prime)\n");
    printf("    h ^= h >> 13\n");
    printf("    h *= 0xc2b2ae3d   (3,266,489,917 — a large prime)\n");
    printf("    h ^= h >> 16\n\n");

    printf("  %-12s -> %-12s %-12s\n", "Input", "Output (dec)", "Output (hex)");
    printf("  %-12s    %-12s %-12s\n", "-----", "-----------", "-----------");

    uint32_t inputs[] = {0, 1, 2, 3, 100, 1000, 0xDEADBEEFu};
    int count = sizeof(inputs) / sizeof(inputs[0]);

    for (int i = 0; i < count; i++) {
        uint32_t h = forge_hash_xxhash32(inputs[i]);
        printf("  %-12u -> %-12u 0x%08x\n", inputs[i], h, h);
    }

    printf("\n  This xor-shift-multiply pattern is also used in MurmurHash3\n");
    printf("  and many other modern hash functions. The primes are chosen\n");
    printf("  by automated search to maximize bit mixing.\n");
}

/* ── 5. Avalanche Effect ─────────────────────────────────────────────── */

static void demo_avalanche(void)
{
    print_header("5. AVALANCHE EFFECT: One Bit In, Many Bits Out");

    printf("\n  A good hash function has the 'avalanche' property:\n");
    printf("  flipping ONE input bit flips roughly HALF (~16) of the\n");
    printf("  32 output bits. This ensures small input changes spread\n");
    printf("  across the entire output.\n\n");

    printf("  Flipping each bit of input 0 through Wang hash:\n\n");

    uint32_t base = 0u;
    uint32_t base_hash = forge_hash_wang(base);

    printf("  Base:  hash(0) = 0x%08x\n\n", base_hash);

    printf("  %-12s %-14s %-14s %-12s\n",
           "Bit flipped", "Input", "Hash (hex)", "Bits changed");
    printf("  %-12s %-14s %-14s %-12s\n",
           "-----------", "-----", "----------", "------------");

    int total_flipped = 0;
    for (int bit = 0; bit < 32; bit++) {
        uint32_t modified = base ^ (1u << bit);
        uint32_t modified_hash = forge_hash_wang(modified);
        int flipped = popcount32(base_hash ^ modified_hash);
        total_flipped += flipped;

        /* Show selected bits to keep output manageable */
        if (bit < 6 || bit == 15 || bit == 23 || bit == 31) {
            printf("  bit %-7d %-14u 0x%08x   %d / 32\n",
                   bit, modified, modified_hash, flipped);
        } else if (bit == 6) {
            printf("  ...         (bits 6-14 omitted for brevity)\n");
        }
    }

    float avg = (float)total_flipped / 32.0f;
    printf("\n  Average bits changed: %.1f / 32 (ideal: 16.0)\n", avg);

    /* Visual binary comparison */
    printf("\n  Binary comparison (input 1000 vs 1001):\n\n");

    uint32_t a = forge_hash_wang(1000u);
    uint32_t b = forge_hash_wang(1001u);
    printf("    hash(1000) = ");
    print_binary(a);
    printf("\n    hash(1001) = ");
    print_binary(b);
    printf("\n    XOR diff   = ");
    print_binary(a ^ b);
    printf("  (%d bits differ)\n", popcount32(a ^ b));
}

/* ── 6. Key Constants ────────────────────────────────────────────────── */

static void demo_key_constants(void)
{
    print_header("6. KEY CONSTANTS: Where Hash Constants Come From");

    printf("\n  Hash function constants are not arbitrary. Each type\n");
    printf("  of constant serves a specific mathematical purpose.\n");

    /* Large odd primes */
    printf("\n  LARGE ODD PRIMES (e.g., 0x27d4eb2d = 668,265,261)\n");
    printf("    Multiplication by a prime ensures every input bit can\n");
    printf("    affect every output bit. Even multipliers always clear\n");
    printf("    the lowest bit. Composite numbers create periodic\n");
    printf("    patterns. Large primes spread bits across the full\n");
    printf("    32-bit range.\n");

    /* Golden ratio */
    printf("\n  GOLDEN RATIO: 0x9e3779b9 = floor(2^32 / phi)\n");
    double phi = (1.0 + sqrt(5.0)) / 2.0;
    printf("    phi = (1 + sqrt(5)) / 2 = %.10f\n", phi);
    printf("    2^32 / phi = %.1f -> 0x%08x\n",
           4294967296.0 / phi, (uint32_t)(4294967296.0 / phi));
    printf("    The golden ratio is the 'most irrational' number.\n");
    printf("    Its continued fraction converges slower than any other\n");
    printf("    irrational, making it spread additive sequences as\n");
    printf("    evenly as possible around the integer ring.\n");

    /* Xor-shift amounts */
    printf("\n  XOR-SHIFT AMOUNTS (>> 15, >> 13, >> 16, etc.)\n");
    printf("    Right-shifting and XORing folds the upper bits into\n");
    printf("    the lower bits. After several rounds, every output\n");
    printf("    bit depends on every input bit. The specific amounts\n");
    printf("    are chosen by testing all combinations and measuring\n");
    printf("    the avalanche quality.\n");

    /* Computer search */
    printf("\n  COMPUTER SEARCH\n");
    printf("    Modern hash constants are found by brute-force search:\n");
    printf("    test millions of candidate multipliers, measure bit\n");
    printf("    bias with chi-squared tests and avalanche matrices,\n");
    printf("    and keep the constants with the lowest bias. The PCG\n");
    printf("    and xxHash constants were refined this way.\n");
}

/* ── 7. Hash to Float ────────────────────────────────────────────────── */

static void demo_hash_to_float(void)
{
    print_header("7. HASH TO FLOAT: Mapping Integers to [0, 1)");

    printf("\n  Converting a 32-bit hash to a uniform float in [0, 1):\n\n");
    printf("    float f = (h >> 8) * (1.0f / 16777216.0f)\n\n");

    printf("  Why shift right by 8?\n");
    printf("    A 32-bit float has 23 mantissa bits + 1 implicit leading\n");
    printf("    bit = 24 bits of integer precision. Floats can represent\n");
    printf("    every integer up to 2^24 = 16,777,216 exactly, but not\n");
    printf("    beyond. By shifting right 8 bits (keeping the top 24),\n");
    printf("    every hash value maps to a distinct float. Dividing by\n");
    printf("    2^24 produces 16,777,216 uniformly-spaced values in [0, 1).\n\n");

    printf("  %-12s %-12s %-12s %-14s\n",
           "Input", "Hash (hex)", "Float [0,1)", "Signed [-1,1)");
    printf("  %-12s %-12s %-12s %-14s\n",
           "-----", "----------", "-----------", "--------------");

    uint32_t inputs[] = {0, 1, 42, 100, 255, 1000, 99999};
    int count = sizeof(inputs) / sizeof(inputs[0]);

    for (int i = 0; i < count; i++) {
        uint32_t h = forge_hash_wang(inputs[i]);
        float f = forge_hash_to_float(h);
        float sf = forge_hash_to_sfloat(h);
        printf("  %-12u 0x%08x  %10.7f  %+11.7f\n",
               inputs[i], h, f, sf);
    }

    printf("\n  The signed variant simply maps [0, 1) to [-1, 1):\n");
    printf("    sfloat = float * 2.0 - 1.0\n");
}

/* ── 8. Distribution Quality ─────────────────────────────────────────── */

static void demo_distribution(void)
{
    print_header("8. DISTRIBUTION: Bucket Uniformity Test");

    printf("\n  Hash 100,000 sequential integers and count how many fall\n");
    printf("  into each of 10 equal buckets. A uniform distribution\n");
    printf("  gives 10,000 per bucket.\n\n");

    int buckets_wang[10] = {0};
    int buckets_pcg[10] = {0};
    int buckets_xx[10] = {0};
    int n = 100000;

    for (int i = 0; i < n; i++) {
        uint32_t input = (uint32_t)i;

        float fw = forge_hash_to_float(forge_hash_wang(input));
        float fp = forge_hash_to_float(forge_hash_pcg(input));
        float fx = forge_hash_to_float(forge_hash_xxhash32(input));

        int bw = (int)(fw * 10.0f);
        int bp = (int)(fp * 10.0f);
        int bx = (int)(fx * 10.0f);

        /* Clamp to bucket 9: forge_hash_to_float returns [0, 1), but
         * floating-point rounding can produce exactly 1.0f, which would
         * give bucket index 10 — one past the end of our 10-bucket array. */
        if (bw > 9) bw = 9;
        if (bp > 9) bp = 9;
        if (bx > 9) bx = 9;

        buckets_wang[bw]++;
        buckets_pcg[bp]++;
        buckets_xx[bx]++;
    }

    printf("  %-12s %-10s %-10s %-10s %-10s\n",
           "Bucket", "Expected", "Wang", "PCG", "xxHash32");
    printf("  %-12s %-10s %-10s %-10s %-10s\n",
           "------", "--------", "----", "---", "--------");

    for (int i = 0; i < 10; i++) {
        printf("  [%.1f, %.1f)  %-10d %-10d %-10d %-10d\n",
               i * 0.1f, (i + 1) * 0.1f, n / 10,
               buckets_wang[i], buckets_pcg[i], buckets_xx[i]);
    }

    printf("\n  All three produce near-uniform distributions. Small\n");
    printf("  deviations from 10,000 are expected statistical noise.\n");
}

/* ── 9. Multi-Dimensional Seeding ────────────────────────────────────── */

static void demo_seeding(void)
{
    print_header("9. MULTI-DIMENSIONAL SEEDING: Position, Time, Frame");

    printf("\n  In a shader, the hash seed is typically derived from:\n");
    printf("    - Pixel position (x, y) for spatial noise\n");
    printf("    - Frame index for temporal variation\n");
    printf("    - A combination of both for animated noise\n\n");

    /* 2D position hash */
    printf("  2D position hash — forge_hash2d(x, y) -> float:\n\n");

    printf("  y\\x ");
    for (int x = 0; x < 8; x++) printf("   %d    ", x);
    printf("\n  --- ");
    for (int x = 0; x < 8; x++) printf("-------- ");
    printf("\n");

    for (int y = 0; y < 4; y++) {
        printf("   %d  ", y);
        for (int x = 0; x < 8; x++) {
            float f = forge_hash_to_float(
                forge_hash2d((uint32_t)x, (uint32_t)y));
            printf(" %6.4f ", f);
        }
        printf("\n");
    }

    /* 3D hash with time/frame dimension */
    printf("\n  3D hash — adding time variation: forge_hash3d(5, 3, frame)\n\n");

    printf("  %-8s %-14s %-12s\n", "Frame", "Hash (hex)", "Float");
    printf("  %-8s %-14s %-12s\n", "-----", "----------", "-----");

    for (uint32_t frame = 0; frame < 8; frame++) {
        uint32_t h = forge_hash3d(5u, 3u, frame);
        float f = forge_hash_to_float(h);
        printf("  %-8u 0x%08x     %.6f\n", frame, h, f);
    }

    printf("\n  Each frame produces a different value at position (5,3),\n");
    printf("  but frame 0 always gives the same result. Fully\n");
    printf("  deterministic and reproducible.\n");
}

/* ── 10. White Noise Visualization ───────────────────────────────────── */

static void demo_white_noise(void)
{
    print_header("10. WHITE NOISE: Hashing Every Pixel");

    printf("\n  White noise means every sample is independent and\n");
    printf("  uniformly distributed — no correlation between neighbors.\n");
    printf("  The name comes from an analogy with white light, which\n");
    printf("  contains all frequencies in equal amounts.\n\n");

    printf("  64x20 white noise (Wang hash, position-seeded):\n\n");

    /* ASCII density ramp — 9 levels from empty to full */
    const char *ramp = " .:-=+*#@";
    int ramp_len = 9;

    for (int y = 0; y < 20; y++) {
        printf("  ");
        for (int x = 0; x < 64; x++) {
            float f = forge_hash_to_float(
                forge_hash2d((uint32_t)x, (uint32_t)y));
            int idx = (int)(f * (float)ramp_len);
            if (idx >= ramp_len) idx = ramp_len - 1;
            printf("%c", ramp[idx]);
        }
        printf("\n");
    }

    printf("\n  Each pixel is independent — this 'static' pattern has\n");
    printf("  equal energy at all spatial frequencies. It is the\n");
    printf("  building block for more structured noise:\n");
    printf("    - Perlin/simplex noise (smooth with interpolation)\n");
    printf("    - Blue noise (suppress low-frequency clumps)\n");
    printf("    - Dithering (break up color banding)\n");
    printf("    - Dissolve effects (threshold against noise)\n");
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("=============================================================\n");
    printf("  Math Lesson 12 -- Hash Functions & White Noise\n");
    printf("=============================================================\n");

    demo_why_hashing();
    demo_wang_hash();
    demo_pcg_hash();
    demo_xxhash32();
    demo_avalanche();
    demo_key_constants();
    demo_hash_to_float();
    demo_distribution();
    demo_seeding();
    demo_white_noise();

    printf("\n=============================================================\n");
    printf("  See README.md for diagrams and detailed explanations.\n");
    printf("  See common/math/forge_math.h for the implementations.\n");
    printf("=============================================================\n\n");

    SDL_Quit();
    return 0;
}
