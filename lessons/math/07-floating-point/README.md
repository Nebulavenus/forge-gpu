# Math Lesson 07 — Floating Point

How computers represent real numbers, and why it matters for graphics.

## What you'll learn

- Why fixed-point arithmetic motivated floating-point
- IEEE 754 representation: sign, exponent, mantissa
- How precision varies across the number line (more near zero, less far away)
- Epsilon and testing for equality (absolute vs relative tolerance)
- Depth buffer precision: z-fighting and why it's non-linear
- 32-bit float vs 64-bit double: trade-offs and when each is appropriate

## Result

The demo walks through seven sections, showing bit patterns, precision
measurements, and practical graphics pitfalls with concrete numbers.

**Example output (abbreviated):**

```text
=============================================================
  Math Lesson 07 — Floating Point
  How computers represent real numbers (and where they fail)
=============================================================

-- 1. Fixed-point as motivation --------------------------------

  8.8 fixed-point (scale = 256):
    pi    = 3.14159 -> stored as   804 -> back = 3.14062 (error = 0.00097)
    small = 0.01    -> stored as     2 -> back = 0.00781 (error = 0.00219)
    big   = 100.5   -> stored as 25728 -> back = 100.50000 (error = 0.00000)

-- 2. IEEE 754 representation ----------------------------------

  1.0 = 1
    bits: 0 01111111 00000000000000000000000
    sign=0  exponent=0 (biased 127)  mantissa=0x000000
  0.1 = 0.1
    bits: 0 01111011 10011001100110011001101
    sign=0  exponent=-4 (biased 123)  mantissa=0x4CCCCD

-- 3. How precision varies across the number line --------------

       value      |   spacing (eps at value)   | digits of precision
  ----------------|---------------------------|--------------------
             1.0  |          0.000000119209290  |  ~6.9
          1000.0  |          0.000061035156250  |  ~7.2
      16777216.0  |          2.000000000000000  |  ~6.9

  Key insight: 16,777,216 + 1 = ?
    16777216.0f + 1.0f = 16777216.0
    They're equal! At this magnitude, 1.0 is below the
    spacing between consecutive floats.

-- 4. Epsilon and equality testing ------------------------------

  sqrt(2) * sqrt(2) == 2?
    sqrtf(2) * sqrtf(2) = 1.99999988079071044922
    2.0f                = 2.00000000000000000000
    Equal (==): NO
    Difference: -0.00000011920928955078

-- 5. Depth buffer precision (z-fighting) ----------------------

   view-space z  |  NDC z     |  depth buffer bits used
    z =    -0.1  |  0.000000  |  0.0% of depth range (near plane)
    z =    -1.0  |  0.900901  |  10.0% of depth range
    z =  -100.0  |  1.000000  |  0.1% of depth range

-- 6. float vs double (32-bit vs 64-bit) -----------------------

  Adding 0.1 ten million times:
    float  result:  1087937.000000  (error: 87937.000000)
    double result:   999999.999839  (error: -0.000161)
```

## Key concepts

- **IEEE 754** — The standard representation: sign bit, 8-bit exponent,
  23-bit mantissa. Value = (-1)^S \* 2^(E-127) \* (1 + M/2^23)
- **Relative precision** — Floats give ~7 decimal digits at any magnitude,
  but the absolute spacing between consecutive values depends on how large
  the number is
- **Machine epsilon** — The smallest value e where (1.0 + e) != 1.0.
  For 32-bit float: ~1.19e-7 (`FORGE_EPSILON`)
- **Absolute tolerance** — Compare by checking |a - b| < epsilon. Good
  near zero, breaks for large values
- **Relative tolerance** — Compare by checking |a - b| < epsilon *
  max(|a|, |b|). Good at any magnitude, breaks near zero
- **Z-fighting** — When two surfaces at slightly different depths get the
  same depth buffer value, causing flickering

## The math

### Fixed-point: the problem

Before floating-point, computers used **fixed-point** numbers: integers where
some bits represent the fractional part. An 8.8 fixed-point number uses 8 bits
for the integer part and 8 for the fraction (multiply by 256 to store, divide
by 256 to read back).

The problem: **constant precision everywhere.** You get the same number of
fractional bits whether you're representing 0.001 or 100,000. If your
fractional bits can only resolve 1/256 = 0.0039, you can't store 0.001 at all.

### IEEE 754: the solution

Floating-point lets the decimal point "float" to where you need it. A 32-bit
float has three fields:

```text
[S] [EEEEEEEE] [MMMMMMMMMMMMMMMMMMMMMMM]
 1      8               23 bits
sign  exponent        mantissa (fraction)
```

The value is: **(-1)^S * 2^(E-127) * (1 + M/2^23)**

The "1 +" is the **hidden bit** — you get 24 bits of precision from only 23
stored mantissa bits. The exponent shifts the "window" of precision to the
right scale:

| Value | Exponent (E-127) | What it means |
|-------|-------------------|---------------|
| 0.5 | -1 | Window centered around 0.5-1.0 |
| 1.0 | 0 | Window centered around 1.0-2.0 |
| 2.0 | 1 | Window centered around 2.0-4.0 |
| 1000.0 | 9 | Window centered around 512-1024 |

### Precision is relative

The key insight: **floats have ~7 decimal digits of precision at any
magnitude**, but the absolute spacing changes:

- At 1.0: spacing = ~1.19e-7 (very precise)
- At 1000.0: spacing = ~6.1e-5 (still 7 digits, but coarser)
- At 16,777,216 (2^24): spacing = 2.0 (can't even represent +1!)

This means `16777216.0f + 1.0f == 16777216.0f` — the 1.0 is lost entirely.

### Equality testing

Because of rounding errors, **never compare floats with `==`**. For example,
`sqrtf(2.0f) * sqrtf(2.0f)` gives 1.9999998..., not 2.0. Instead:

**Absolute tolerance** — good near zero:

```c
/* |a - b| < tolerance */
forge_approx_equalf(result, expected, 1e-6f);
```

**Relative tolerance** — good at any magnitude:

```c
/* |a - b| < tolerance * max(|a|, |b|) */
forge_rel_equalf(result, expected, 1e-5f);
```

**Best practice** — combine both:

```c
if (forge_approx_equalf(a, b, 1e-6f) || forge_rel_equalf(a, b, 1e-5f)) {
    /* close enough */
}
```

### Depth buffer precision

The perspective projection matrix maps view-space z to NDC z with a
**hyperbolic** function. This concentrates almost all the precision near the
near plane:

```text
View-space z  |  NDC z   |  Depth range used
-0.1 (near)   |  0.000   |  Near plane
-0.2          |  0.500   |  50% of range for first 0.1 units!
-1.0          |  0.901   |  90% gone after 1 unit
-50.0         |  0.999   |  Only 0.1% left for half the scene
-100.0 (far)  |  1.000   |  Far plane
```

When two surfaces are close together far from the camera, they may map to the
same depth buffer value. The GPU can't tell which is in front, so they flicker
back and forth each frame. This is **z-fighting**.

**Mitigations:**

1. Push the near plane as far as possible (0.1, not 0.001)
2. Reduce the far/near ratio (1000:1 is much better than 100000:1)
3. Use **reversed-Z** (near maps to 1.0, far to 0.0) — this redistributes
   float precision to counteract the hyperbolic mapping
4. Use a 32-bit depth buffer (`D32_FLOAT`) instead of 24-bit

### float vs double

| | float (32-bit) | double (64-bit) |
|---|---|---|
| Mantissa bits | 23 | 52 |
| Decimal digits | ~7 | ~15 |
| Epsilon | 1.19e-7 | 2.22e-16 |
| Max value | 3.4e+38 | 1.8e+308 |

**Why GPUs use 32-bit float:**

- 2x throughput (two 32-bit operations per 64-bit ALU lane)
- 2x memory bandwidth savings
- A 4K display is ~4000 pixels wide — needs only 4 digits of precision
- Colors are 8 bits per channel — 3 digits is plenty

**When you need double (on the CPU):**

- Large-world coordinates (open-world games spanning kilometers)
- Physics accumulation over many frames
- Intermediate calculations that get cast back to float for the GPU

## Where it's used

Understanding floating-point behavior is essential for:

- **Depth buffer setup** — Choosing near/far planes and depth format
- **Camera systems** — Large worlds need double-precision positions
- **Physics** — Accumulation errors compound over thousands of frames
- **Shader math** — Knowing when 7 digits is enough (almost always)
- **Comparison logic** — Collision detection, intersection tests

**In forge-gpu lessons:**

- [GPU Lesson 06 — Depth Buffer & 3D Transforms](../../gpu/06-depth-and-3d/)
  uses the depth buffer whose non-linear precision this lesson explains
- [Math Lesson 06 — Projections](../06-projections/) derives the perspective
  matrix that creates the non-linear depth mapping

## New math library additions

This lesson adds to `common/math/forge_math.h`:

| Function | Purpose |
|---|---|
| `FORGE_EPSILON` | Machine epsilon for 32-bit float (~1.19e-7) |
| `forge_approx_equalf(a, b, tolerance)` | Absolute tolerance comparison |
| `forge_rel_equalf(a, b, tolerance)` | Relative tolerance comparison |

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\math\07-floating-point\Debug\07-floating-point.exe

# Linux / macOS
./build/lessons/math/07-floating-point/07-floating-point
```

The demo prints all sections to the console — no window required.

## Exercises

1. **Bit explorer** — Modify the program to print the bit pattern of any float
   entered on the command line. Try values like 0.333333, -42.0, and 1e30.

2. **Kahan summation** — The accumulation test shows huge error when adding
   0.1 a million times. Implement
   [Kahan summation](https://en.wikipedia.org/wiki/Kahan_summation_algorithm)
   and compare the result. How much does the error improve?

3. **Reversed-Z** — Modify the depth buffer section to use reversed-Z
   (swap near=1 and far=0 in the depth mapping). Show how the precision
   distribution changes — does the far-plane z-fighting improve?

4. **ULP comparison** — Instead of absolute/relative tolerance, implement
   comparison by counting ULPs (Units in the Last Place) between two floats.
   Hint: interpret the float bits as integers and subtract.

## Further reading

- [Math Lesson 06 — Projections](../06-projections/) — The projection matrix
  that creates non-linear depth
- [What Every Computer Scientist Should Know About Floating-Point Arithmetic](https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html)
  — The classic deep-dive reference
- [Depth Precision Visualized](https://developer.nvidia.com/content/depth-precision-visualized)
  — NVIDIA's article on reversed-Z and depth buffer precision
