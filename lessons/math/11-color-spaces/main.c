/*
 * Math Lesson 11 — Color Spaces
 *
 * Demonstrates:
 *   1. Gamma correction — sRGB transfer function vs simple power curve
 *   2. Why linear space matters — midpoint blending comparison
 *   3. Luminance — perceptual brightness of different colors
 *   4. RGB <-> HSL — hue/saturation/lightness decomposition
 *   5. RGB <-> HSV — hue/saturation/value decomposition
 *   6. RGB <-> CIE XYZ — device-independent color
 *   7. CIE xyY chromaticity — separating color from brightness
 *   8. Gamut boundaries — when XYZ->RGB produces out-of-range values
 *   9. Tone mapping — Reinhard and ACES curves for HDR
 *  10. Exposure — photographic stops (EV)
 *
 * This is a console program — no window needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

static void print_vec3(const char *label, vec3 v)
{
    printf("  %-30s (%7.4f, %7.4f, %7.4f)\n", label, v.x, v.y, v.z);
}

/* ── 1. Gamma Correction ─────────────────────────────────────────────── */

/* Show that the sRGB transfer function is NOT a simple pow(x, 2.2) —
 * it has a linear segment near black. Compare the two side by side. */
static void demo_gamma_correction(void)
{
    print_header("1. GAMMA CORRECTION: sRGB Transfer Function");

    printf("\n  The sRGB standard uses a PIECEWISE transfer function:\n");
    printf("    Near black (<=0.04045): linear segment (s / 12.92)\n");
    printf("    Rest:                   power curve ((s+0.055)/1.055)^2.4\n\n");

    printf("  %-12s %-12s %-12s %-12s\n",
           "sRGB value", "sRGB->linear", "pow(x,2.2)", "Difference");
    printf("  %-12s %-12s %-12s %-12s\n",
           "----------", "-----------", "---------", "----------");

    float test_values[] = {0.0f, 0.01f, 0.04045f, 0.1f, 0.2f,
                           0.5f, 0.735f, 0.9f, 1.0f};
    int count = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < count; i++) {
        float s = test_values[i];
        float correct = color_srgb_to_linear(s);
        float approx  = powf(s, 2.2f);
        float diff    = correct - approx;
        printf("  %-12.4f %-12.4f %-12.4f %-+12.4f\n",
               s, correct, approx, diff);
    }

    printf("\n  Note: The difference is small but measurable. The linear\n");
    printf("  segment prevents a division-by-zero slope at the origin.\n");
}

/* ── 2. Why Linear Space Matters ─────────────────────────────────────── */

/* Blending two colors: if you average in sRGB, you get the wrong result.
 * The linear midpoint between black and white is 0.5 (50% light intensity),
 * which encodes to sRGB ~0.735.  sRGB 0.5 decodes to only ~0.214 linear
 * (about 21% of white light).  Note: 18% reflectance ("middle gray" in
 * photography) is a separate perceptual concept, not the linear midpoint. */
static void demo_linear_space_matters(void)
{
    print_header("2. WHY LINEAR SPACE MATTERS: Blending Comparison");

    printf("\n  Averaging black (0.0) and white (1.0):\n\n");

    /* The wrong way: average sRGB values directly */
    float srgb_mid = 0.5f;
    float srgb_mid_linear = color_srgb_to_linear(srgb_mid);

    /* The right way: average in linear, then encode */
    float linear_mid = 0.5f;
    float linear_mid_srgb = color_linear_to_srgb(linear_mid);

    printf("  WRONG (average in sRGB):\n");
    printf("    sRGB midpoint      = 0.5000\n");
    printf("    Actual light level  = %.4f (only ~21%% of white!)\n",
           srgb_mid_linear);

    printf("\n  CORRECT (average in linear, then encode):\n");
    printf("    Linear midpoint    = 0.5000 (50%% of white light)\n");
    printf("    sRGB for display   = %.4f\n", linear_mid_srgb);

    printf("\n  The correct midpoint looks BRIGHTER than sRGB 0.5 because\n");
    printf("  the human eye is more sensitive to dark-to-mid transitions.\n");
    printf("  sRGB encodes more steps in the dark range where we need them.\n");

    /* Show color blending example */
    printf("\n  Blending red (1,0,0) and green (0,1,0) at 50%%:\n");
    vec3 red = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 green = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 linear_blend = vec3_lerp(red, green, 0.5f);
    vec3 linear_blend_srgb = color_linear_to_srgb_rgb(linear_blend);

    print_vec3("Linear blend:", linear_blend);
    print_vec3("Encoded for display:", linear_blend_srgb);
    printf("    Luminance = %.4f (perceived brightness)\n",
           color_luminance(linear_blend));
}

/* ── 3. Human Perception and Luminance ───────────────────────────────── */

static void demo_luminance(void)
{
    print_header("3. LUMINANCE: How Bright Does Each Color Look?");

    printf("\n  BT.709/sRGB luminance coefficients:\n");
    printf("    Y = 0.2126*R + 0.7152*G + 0.0722*B\n\n");
    printf("  Green dominates because human vision evolved to be most\n");
    printf("  sensitive to green light (peak of solar spectrum).\n\n");

    struct { const char *name; vec3 color; } colors[] = {
        {"Pure red   (1,0,0)",   {1.0f, 0.0f, 0.0f}},
        {"Pure green (0,1,0)",   {0.0f, 1.0f, 0.0f}},
        {"Pure blue  (0,0,1)",   {0.0f, 0.0f, 1.0f}},
        {"Yellow     (1,1,0)",   {1.0f, 1.0f, 0.0f}},
        {"Cyan       (0,1,1)",   {0.0f, 1.0f, 1.0f}},
        {"Magenta    (1,0,1)",   {1.0f, 0.0f, 1.0f}},
        {"White      (1,1,1)",   {1.0f, 1.0f, 1.0f}},
        {"50% gray   (0.5,..)",  {0.5f, 0.5f, 0.5f}},
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    printf("  %-25s %-10s %-10s\n", "Color", "Luminance", "Relative");
    printf("  %-25s %-10s %-10s\n", "-----", "---------", "--------");

    for (int i = 0; i < count; i++) {
        float lum = color_luminance(colors[i].color);
        printf("  %-25s %-10.4f %-10.1f%%\n",
               colors[i].name, lum, lum * 100.0f);
    }

    printf("\n  Green alone is brighter than red and blue COMBINED:\n");
    printf("    Red + Blue = %.4f,  Green = %.4f\n",
           0.2126f + 0.0722f, 0.7152f);
}

/* ── 4. RGB <-> HSL ──────────────────────────────────────────────────── */

static void demo_rgb_hsl(void)
{
    print_header("4. RGB <-> HSL: Hue, Saturation, Lightness");

    printf("\n  HSL separates color into three intuitive axes:\n");
    printf("    H (hue):        0-360 degrees on the color wheel\n");
    printf("    S (saturation): 0=gray, 1=vivid\n");
    printf("    L (lightness):  0=black, 0.5=pure color, 1=white\n\n");

    struct { const char *name; vec3 rgb; } colors[] = {
        {"Red",         {1.0f, 0.0f, 0.0f}},
        {"Green",       {0.0f, 1.0f, 0.0f}},
        {"Blue",        {0.0f, 0.0f, 1.0f}},
        {"Yellow",      {1.0f, 1.0f, 0.0f}},
        {"Orange",      {1.0f, 0.5f, 0.0f}},
        {"Gray 50%",    {0.5f, 0.5f, 0.5f}},
        {"Dark cyan",   {0.0f, 0.3f, 0.3f}},
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    printf("  %-12s %-22s -> %-22s\n", "Color", "RGB", "HSL (H, S, L)");
    printf("  %-12s %-22s    %-22s\n",  "-----", "---", "-------------");

    for (int i = 0; i < count; i++) {
        vec3 hsl = color_rgb_to_hsl(colors[i].rgb);
        printf("  %-12s (%5.2f, %5.2f, %5.2f) -> (%6.1f, %5.3f, %5.3f)\n",
               colors[i].name,
               colors[i].rgb.x, colors[i].rgb.y, colors[i].rgb.z,
               hsl.x, hsl.y, hsl.z);
    }

    /* Round-trip test */
    printf("\n  Round-trip test (RGB -> HSL -> RGB):\n");
    vec3 test = vec3_create(0.8f, 0.3f, 0.5f);
    vec3 hsl = color_rgb_to_hsl(test);
    vec3 back = color_hsl_to_rgb(hsl);
    print_vec3("Original RGB:", test);
    print_vec3("HSL:",          hsl);
    print_vec3("Back to RGB:",  back);
    printf("  Max error: %.8f\n",
           fmaxf(fmaxf(fabsf(test.x - back.x),
                        fabsf(test.y - back.y)),
                 fabsf(test.z - back.z)));
}

/* ── 5. RGB <-> HSV ──────────────────────────────────────────────────── */

static void demo_rgb_hsv(void)
{
    print_header("5. RGB <-> HSV: Hue, Saturation, Value");

    printf("\n  HSV vs HSL — how brightness differs:\n");
    printf("    HSV value = max(R,G,B) — peak channel intensity\n");
    printf("    HSL lightness = (max+min)/2 — midpoint of range\n\n");

    struct { const char *name; vec3 rgb; } colors[] = {
        {"Red",         {1.0f, 0.0f, 0.0f}},
        {"Dark red",    {0.5f, 0.0f, 0.0f}},
        {"Orange",      {1.0f, 0.5f, 0.0f}},
        {"White",       {1.0f, 1.0f, 1.0f}},
        {"Gray 50%",    {0.5f, 0.5f, 0.5f}},
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    printf("  %-12s %-22s -> %-15s  vs  %-15s\n",
           "Color", "RGB", "HSV (H,S,V)", "HSL (H,S,L)");
    printf("  %-12s %-22s    %-15s      %-15s\n",
           "-----", "---", "----------", "----------");

    for (int i = 0; i < count; i++) {
        vec3 hsv = color_rgb_to_hsv(colors[i].rgb);
        vec3 hsl = color_rgb_to_hsl(colors[i].rgb);
        printf("  %-12s (%5.2f,%5.2f,%5.2f) -> "
               "(%3.0f,%5.3f,%5.3f)  vs  (%3.0f,%5.3f,%5.3f)\n",
               colors[i].name,
               colors[i].rgb.x, colors[i].rgb.y, colors[i].rgb.z,
               hsv.x, hsv.y, hsv.z,
               hsl.x, hsl.y, hsl.z);
    }

    /* Round-trip test */
    printf("\n  Round-trip test (RGB -> HSV -> RGB):\n");
    vec3 test = vec3_create(0.3f, 0.7f, 0.2f);
    vec3 hsv = color_rgb_to_hsv(test);
    vec3 back = color_hsv_to_rgb(hsv);
    print_vec3("Original RGB:", test);
    print_vec3("HSV:",          hsv);
    print_vec3("Back to RGB:",  back);
    printf("  Max error: %.8f\n",
           fmaxf(fmaxf(fabsf(test.x - back.x),
                        fabsf(test.y - back.y)),
                 fabsf(test.z - back.z)));
}

/* ── 6. RGB <-> CIE XYZ ─────────────────────────────────────────────── */

static void demo_rgb_xyz(void)
{
    print_header("6. RGB <-> CIE XYZ: Device-Independent Color");

    printf("\n  CIE XYZ (1931) is the reference space for all color science.\n");
    printf("  The Y component equals luminance (perceptual brightness).\n");
    printf("  The matrix is derived from sRGB's primary chromaticities\n");
    printf("  and the D65 white point (6504K daylight).\n\n");

    struct { const char *name; vec3 rgb; } colors[] = {
        {"Red",     {1.0f, 0.0f, 0.0f}},
        {"Green",   {0.0f, 1.0f, 0.0f}},
        {"Blue",    {0.0f, 0.0f, 1.0f}},
        {"White",   {1.0f, 1.0f, 1.0f}},
        {"D65 gray",{0.5f, 0.5f, 0.5f}},
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    printf("  %-12s %-22s -> %-22s\n", "Color", "Linear RGB", "CIE XYZ");
    printf("  %-12s %-22s    %-22s\n", "-----", "----------", "-------");

    for (int i = 0; i < count; i++) {
        vec3 xyz = color_linear_rgb_to_xyz(colors[i].rgb);
        printf("  %-12s (%5.3f, %5.3f, %5.3f) -> (%6.4f, %6.4f, %6.4f)\n",
               colors[i].name,
               colors[i].rgb.x, colors[i].rgb.y, colors[i].rgb.z,
               xyz.x, xyz.y, xyz.z);
    }

    /* Show that Y matches luminance */
    printf("\n  Notice: the Y column of XYZ matches the luminance values\n");
    printf("  from Section 3. This is by design — Y IS luminance.\n");

    /* Round-trip test */
    printf("\n  Round-trip test (RGB -> XYZ -> RGB):\n");
    vec3 test = vec3_create(0.6f, 0.3f, 0.8f);
    vec3 xyz = color_linear_rgb_to_xyz(test);
    vec3 back = color_xyz_to_linear_rgb(xyz);
    print_vec3("Original RGB:", test);
    print_vec3("XYZ:",          xyz);
    print_vec3("Back to RGB:",  back);
    printf("  Max error: %.8f\n",
           fmaxf(fmaxf(fabsf(test.x - back.x),
                        fabsf(test.y - back.y)),
                 fabsf(test.z - back.z)));
}

/* ── 7. CIE xyY Chromaticity ────────────────────────────────────────── */

static void demo_chromaticity(void)
{
    print_header("7. CIE xyY: Chromaticity Coordinates");

    printf("\n  Chromaticity separates color from brightness by\n");
    printf("  projecting XYZ onto the x+y+z=1 plane:\n");
    printf("    x = X/(X+Y+Z),  y = Y/(X+Y+Z)\n");
    printf("  The Y (luminance) is carried along as a third coordinate.\n\n");

    /* Show sRGB primary chromaticities */
    printf("  sRGB primaries on the chromaticity diagram:\n\n");

    struct { const char *name; vec3 rgb; float ex; float ey; } primaries[] = {
        {"Red primary",   {1, 0, 0}, 0.6400f, 0.3300f},
        {"Green primary", {0, 1, 0}, 0.3000f, 0.6000f},
        {"Blue primary",  {0, 0, 1}, 0.1500f, 0.0600f},
        {"D65 white",     {1, 1, 1}, 0.3127f, 0.3290f},
    };
    int count = sizeof(primaries) / sizeof(primaries[0]);

    printf("  %-15s  %-14s  %-14s  %-10s\n",
           "Color", "Computed (x,y)", "Expected (x,y)", "Match?");
    printf("  %-15s  %-14s  %-14s  %-10s\n",
           "-----", "--------------", "--------------", "------");

    for (int i = 0; i < count; i++) {
        vec3 xyz = color_linear_rgb_to_xyz(primaries[i].rgb);
        vec3 xyY = color_xyz_to_xyY(xyz);
        float dx = fabsf(xyY.x - primaries[i].ex);
        float dy = fabsf(xyY.y - primaries[i].ey);
        const char *ok = (dx < 0.002f && dy < 0.002f) ? "[OK]" : "[!]";
        printf("  %-15s  (%6.4f,%6.4f)  (%6.4f,%6.4f)  %s\n",
               primaries[i].name,
               xyY.x, xyY.y,
               primaries[i].ex, primaries[i].ey,
               ok);
    }

    printf("\n  The sRGB gamut is a TRIANGLE connecting these three points\n");
    printf("  on the CIE xy diagram. Any color inside the triangle can be\n");
    printf("  displayed on an sRGB monitor. Colors outside it are out of gamut.\n");

    /* Show xyY round-trip */
    printf("\n  Round-trip test (XYZ -> xyY -> XYZ):\n");
    vec3 test_xyz = color_linear_rgb_to_xyz(vec3_create(0.4f, 0.7f, 0.2f));
    vec3 xyY = color_xyz_to_xyY(test_xyz);
    vec3 back_xyz = color_xyY_to_xyz(xyY);
    print_vec3("Original XYZ:", test_xyz);
    print_vec3("xyY:",          xyY);
    print_vec3("Back to XYZ:",  back_xyz);
    printf("  Max error: %.8f\n",
           fmaxf(fmaxf(fabsf(test_xyz.x - back_xyz.x),
                        fabsf(test_xyz.y - back_xyz.y)),
                 fabsf(test_xyz.z - back_xyz.z)));
}

/* ── 8. Gamut Boundaries ─────────────────────────────────────────────── */

static void demo_gamut(void)
{
    print_header("8. GAMUT: When Colors Cannot Be Displayed");

    printf("\n  A gamut is the set of colors a device can produce. When\n");
    printf("  converting from XYZ to sRGB, some colors fall OUTSIDE the\n");
    printf("  sRGB triangle — they produce negative or >1.0 RGB values.\n\n");

    /* Create an XYZ color that's outside sRGB gamut:
     * a very saturated spectral green */
    printf("  Example: a saturated spectral green (xy = 0.17, 0.80):\n\n");

    vec3 xyY_spectral = vec3_create(0.17f, 0.80f, 0.5f);
    vec3 xyz = color_xyY_to_xyz(xyY_spectral);
    vec3 rgb = color_xyz_to_linear_rgb(xyz);

    print_vec3("xyY:", xyY_spectral);
    print_vec3("XYZ:", xyz);
    print_vec3("Linear RGB:", rgb);

    printf("\n  Negative R (%.4f) means this green is MORE saturated than\n",
           rgb.x);
    printf("  the sRGB red primary can compensate for. This color is\n");
    printf("  outside the sRGB gamut and cannot be displayed exactly.\n");

    printf("\n  Wide-gamut displays (DCI-P3, Rec.2020) have LARGER triangles\n");
    printf("  on the chromaticity diagram, covering more visible colors.\n");

    /* Show DCI-P3 primaries for comparison */
    printf("\n  Gamut comparison (chromaticity coordinates):\n\n");
    printf("  %-10s  %-18s  %-18s  %-18s\n",
           "Gamut", "Red (x, y)", "Green (x, y)", "Blue (x, y)");
    printf("  %-10s  %-18s  %-18s  %-18s\n",
           "-----", "----------", "-----------", "----------");
    printf("  %-10s  (0.6400, 0.3300)  (0.3000, 0.6000)  (0.1500, 0.0600)\n",
           "sRGB");
    printf("  %-10s  (0.6800, 0.3200)  (0.2650, 0.6900)  (0.1500, 0.0600)\n",
           "DCI-P3");
    printf("  %-10s  (0.7080, 0.2920)  (0.1700, 0.7970)  (0.1310, 0.0460)\n",
           "Rec.2020");
}

/* ── 9. Tone Mapping ─────────────────────────────────────────────────── */

static void demo_tone_mapping(void)
{
    print_header("9. TONE MAPPING: HDR to Display Range");

    printf("\n  Real-world light spans a huge dynamic range:\n");
    printf("    Starlight:   ~0.001 cd/m2\n");
    printf("    Office:      ~500 cd/m2\n");
    printf("    Direct sun:  ~100,000 cd/m2\n");
    printf("  But an SDR display shows only 0 to ~300 cd/m2.\n");
    printf("  Tone mapping compresses HDR values into displayable range.\n\n");

    float intensities[] = {0.1f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
    int count = sizeof(intensities) / sizeof(intensities[0]);

    printf("  %-10s  %-12s  %-12s  %-10s\n",
           "Input", "Reinhard", "ACES", "Linear clamp");
    printf("  %-10s  %-12s  %-12s  %-10s\n",
           "-----", "--------", "----", "------------");

    for (int i = 0; i < count; i++) {
        float x = intensities[i];
        vec3 hdr = vec3_create(x, x, x);
        vec3 reinhard = color_tonemap_reinhard(hdr);
        vec3 aces = color_tonemap_aces(hdr);
        float clamped = fminf(x, 1.0f);
        printf("  %-10.1f  %-12.4f  %-12.4f  %-10.4f\n",
               x, reinhard.x, aces.x, clamped);
    }

    printf("\n  Key differences:\n");
    printf("    Linear clamp: loses all detail above 1.0\n");
    printf("    Reinhard:     preserves detail but washes out highlights\n");
    printf("    ACES:         filmic curve with natural highlight rolloff\n");

    /* Show ACES on a colored HDR value */
    printf("\n  ACES on a colored HDR value (sunlit gold: 4.0, 3.0, 0.5):\n");
    vec3 gold = vec3_create(4.0f, 3.0f, 0.5f);
    vec3 aces = color_tonemap_aces(gold);
    vec3 display = color_linear_to_srgb_rgb(aces);
    print_vec3("HDR input:",    gold);
    print_vec3("After ACES:",   aces);
    print_vec3("sRGB display:", display);
}

/* ── 10. Exposure ────────────────────────────────────────────────────── */

static void demo_exposure(void)
{
    print_header("10. EXPOSURE: Photographic Stops (EV)");

    printf("\n  Exposure adjusts brightness in photographic stops (EV).\n");
    printf("  Each stop doubles (+1 EV) or halves (-1 EV) the light.\n");
    printf("  Formula: output = input * 2^EV\n\n");

    vec3 base = vec3_create(0.5f, 0.3f, 0.2f);
    print_vec3("Base color:", base);
    printf("\n");

    float evs[] = {-3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f};
    int count = sizeof(evs) / sizeof(evs[0]);

    printf("  %-8s  %-10s  %-28s\n", "EV", "Multiplier", "Result (R, G, B)");
    printf("  %-8s  %-10s  %-28s\n", "--", "----------", "---------------");

    for (int i = 0; i < count; i++) {
        vec3 exposed = color_apply_exposure(base, evs[i]);
        float mult = powf(2.0f, evs[i]);
        printf("  %-+8.1f  %-10.3f  (%7.4f, %7.4f, %7.4f)\n",
               evs[i], mult, exposed.x, exposed.y, exposed.z);
    }

    printf("\n  In a real pipeline: expose -> tone map -> gamma encode\n");
    vec3 hdr = vec3_create(3.0f, 2.0f, 1.0f);
    vec3 exposed = color_apply_exposure(hdr, -1.0f);
    vec3 mapped = color_tonemap_aces(exposed);
    vec3 display = color_linear_to_srgb_rgb(mapped);
    printf("\n  Full pipeline example:\n");
    print_vec3("HDR scene color:", hdr);
    print_vec3("After EV -1.0:",   exposed);
    print_vec3("After ACES:",      mapped);
    print_vec3("sRGB for display:", display);
}

/* ── 11. Gamma Perception: Why sRGB Allocates More Bits to Darks ──── */

static void demo_gamma_perception(void)
{
    print_header("11. GAMMA PERCEPTION: Why Darks Need More Precision");

    printf("\n  The human visual system responds roughly logarithmically\n");
    printf("  to light intensity. We are much more sensitive to changes\n");
    printf("  in dark values than in bright ones.\n\n");

    printf("  If we encoded light levels linearly in 8 bits, we would waste\n");
    printf("  steps on bright values we can barely distinguish and starve\n");
    printf("  the dark values where we see every step (banding).\n\n");

    printf("  sRGB gamma encoding solves this by spacing values perceptually:\n\n");

    printf("  %-14s %-14s %-14s %-14s\n",
           "Linear light", "sRGB encoded", "8-bit level", "Step size");
    printf("  %-14s %-14s %-14s %-14s\n",
           "------------", "-----------", "-----------", "---------");

    float levels[] = {0.0f, 0.01f, 0.02f, 0.05f, 0.1f, 0.2f,
                      0.4f, 0.6f, 0.8f, 1.0f};
    int count = sizeof(levels) / sizeof(levels[0]);

    float prev_srgb = 0.0f;
    for (int i = 0; i < count; i++) {
        float s = color_linear_to_srgb(levels[i]);
        float step = (i > 0) ? s - prev_srgb : 0.0f;
        printf("  %-14.4f %-14.4f %-14.0f %-14.4f\n",
               levels[i], s, s * 255.0f, step);
        prev_srgb = s;
    }

    printf("\n  Observe: the first 10%% of light intensity uses ~35%% of the\n");
    printf("  8-bit range. This matches human perception — we need those\n");
    printf("  extra steps in the darks to avoid visible banding.\n");
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
    printf("  Math Lesson 11 -- Color Spaces\n");
    printf("=============================================================\n");

    demo_gamma_correction();
    demo_linear_space_matters();
    demo_luminance();
    demo_rgb_hsl();
    demo_rgb_hsv();
    demo_rgb_xyz();
    demo_chromaticity();
    demo_gamut();
    demo_tone_mapping();
    demo_exposure();
    demo_gamma_perception();

    printf("\n=============================================================\n");
    printf("  See README.md for diagrams and detailed explanations.\n");
    printf("  See common/math/forge_math.h for the implementations.\n");
    printf("=============================================================\n\n");

    SDL_Quit();
    return 0;
}
