/*
 * Engine Lesson 09 — HLSL Shared Headers
 *
 * Demonstrates how shared headers work — in both C and HLSL:
 *   - A shared header (shared_params.h) defines constants once
 *   - Two .c files (main.c and sky_pass.c) include it
 *   - This mirrors two HLSL shaders including the same .hlsli file
 *   - Include guards prevent redefinition errors in both languages
 *
 * The HLSL equivalent is atmosphere_params.hlsli from GPU Lesson 26,
 * which is included by both sky.frag.hlsl and multiscatter_lut.comp.hlsl.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "shared_params.h"  /* HORIZON_FADE_SCALE, HORIZON_FADE_BIAS */
#include "sky_pass.h"       /* sky_pass_horizon_fade, sky_pass_print_params */

/* ── Helpers ────────────────────────────────────────────────────────────── */

static void print_divider(const char *title)
{
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("  %s", title);
    SDL_Log("------------------------------------------------------------");
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* SDL_Init(0) initializes core SDL state without enabling any subsystem
     * (video, audio, etc.).  We get SDL_Log and SDL_GetError — everything
     * this console program needs. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 09: HLSL Shared Headers ===");

    /* ── Section 1: The problem — duplicated constants ────────────────── */

    print_divider("1. The Problem: Duplicated Constants");

    SDL_Log("Imagine two shaders that both need the same constants:");
    SDL_Log(" ");
    SDL_Log("  // sky.frag.hlsl");
    SDL_Log("  static const float HORIZON_FADE_SCALE = 10.0;");
    SDL_Log("  static const float HORIZON_FADE_BIAS  = 0.1;");
    SDL_Log(" ");
    SDL_Log("  // multiscatter_lut.comp.hlsl");
    SDL_Log("  static const float HORIZON_FADE_SCALE = 10.0;  // copy-pasted!");
    SDL_Log("  static const float HORIZON_FADE_BIAS  = 0.1;   // copy-pasted!");
    SDL_Log(" ");
    SDL_Log("If you change BIAS in one file but forget the other, the sky");
    SDL_Log("pass and LUT compute disagree — a subtle, hard-to-find bug.");

    /* ── Section 2: The solution — a shared header ────────────────────── */

    print_divider("2. The Solution: A Shared Header");

    SDL_Log("Define the constants once in a shared header:");
    SDL_Log(" ");
    SDL_Log("  // atmosphere_params.hlsli");
    SDL_Log("  #ifndef ATMOSPHERE_PARAMS_HLSLI");
    SDL_Log("  #define ATMOSPHERE_PARAMS_HLSLI");
    SDL_Log("  static const float HORIZON_FADE_SCALE = 10.0;");
    SDL_Log("  static const float HORIZON_FADE_BIAS  = 0.1;");
    SDL_Log("  #endif");
    SDL_Log(" ");
    SDL_Log("Then each shader just includes it:");
    SDL_Log("  #include \"atmosphere_params.hlsli\"");
    SDL_Log(" ");
    SDL_Log("This is the .hlsli pattern — HLSL's equivalent of a .h file.");

    /* ── Section 3: Both files share the same constants ───────────────── */

    print_divider("3. Both Files Share the Same Constants");

    SDL_Log("This C program mirrors that pattern.  main.c and sky_pass.c");
    SDL_Log("both include shared_params.h:");
    SDL_Log(" ");
    SDL_Log("  main.c includes shared_params.h:");
    SDL_Log("  main.c     sees HORIZON_FADE_SCALE = %.1f", HORIZON_FADE_SCALE);
    SDL_Log("  main.c     sees HORIZON_FADE_BIAS  = %.1f", HORIZON_FADE_BIAS);
    SDL_Log(" ");
    sky_pass_print_params();
    SDL_Log(" ");
    SDL_Log("Both files see the same values — defined once, used everywhere.");

    /* Demonstrate the horizon fade function from sky_pass.c. */
    SDL_Log(" ");
    SDL_Log("Using the shared constants (horizon fade formula):");
    SDL_Log("  cos_zenith =  1.0 -> fade = %.2f  (looking up, fully lit)",
            sky_pass_horizon_fade(1.0f));
    SDL_Log("  cos_zenith =  0.0 -> fade = %.2f  (horizon)",
            sky_pass_horizon_fade(0.0f));
    SDL_Log("  cos_zenith = -0.5 -> fade = %.2f  (below horizon, shadowed)",
            sky_pass_horizon_fade(-0.5f));

    /* ── Section 4: How this maps to HLSL ─────────────────────────────── */

    print_divider("4. How This Maps to HLSL");

    SDL_Log("The C and HLSL patterns are nearly identical:");
    SDL_Log(" ");
    SDL_Log("  C header file:     shared_params.h");
    SDL_Log("  HLSL header file:  atmosphere_params.hlsli");
    SDL_Log(" ");
    SDL_Log("  C include:         #include \"shared_params.h\"");
    SDL_Log("  HLSL include:      #include \"atmosphere_params.hlsli\"");
    SDL_Log(" ");
    SDL_Log("  C include guard:   #ifndef SHARED_PARAMS_H");
    SDL_Log("  HLSL include guard: #ifndef ATMOSPHERE_PARAMS_HLSLI");
    SDL_Log(" ");
    SDL_Log("  C search path:     -I directory  (gcc/clang/MSVC)");
    SDL_Log("  HLSL search path:  -I directory  (dxc)");
    SDL_Log(" ");
    SDL_Log("The preprocessor directive #include works the same way in both");
    SDL_Log("languages.  The -I flag tells the compiler where to search for");
    SDL_Log("included files.  compile_shaders.py passes -I to dxc:");
    SDL_Log("  dxc -spirv -I shaders/ -T ps_6_0 -E main sky.frag.hlsl");

    /* ── Section 5: Key differences from C ────────────────────────────── */

    print_divider("5. How HLSL Differs from C");

    SDL_Log("HLSL has a simpler compilation model than C:");
    SDL_Log(" ");
    SDL_Log("  1. No linker step");
    SDL_Log("     C:    main.c -> main.o -+-> executable (linker combines)");
    SDL_Log("           sky_pass.c -> sky_pass.o -+");
    SDL_Log("     HLSL: sky.frag.hlsl -> sky_frag.spv  (standalone)");
    SDL_Log("           lut.comp.hlsl -> lut_comp.spv  (standalone)");
    SDL_Log("     Each shader compiles independently to its own bytecode.");
    SDL_Log(" ");
    SDL_Log("  2. No ODR (one-definition rule) concerns with 'static const'");
    SDL_Log("     The ODR says each symbol can have only one definition");
    SDL_Log("     across all .o files the linker combines.");
    SDL_Log("     In C, a non-static global in a header causes 'multiple");
    SDL_Log("     definition' linker errors (see Engine Lesson 05).");
    SDL_Log("     In HLSL, there is no linker, so 'static const' in a");
    SDL_Log("     .hlsli just works — each shader gets its own copy.");
    SDL_Log(" ");
    SDL_Log("  3. Utility functions do not need 'static inline'");
    SDL_Log("     In C headers, functions must be 'static inline' to avoid");
    SDL_Log("     ODR violations.  In HLSL headers, plain functions work");
    SDL_Log("     fine because each shader is compiled alone.");

    /* ── Summary ──────────────────────────────────────────────────────── */

    print_divider("Summary");

    SDL_Log("HLSL shared header checklist:");
    SDL_Log("  [1] File extension:  .hlsli (convention, not enforced)");
    SDL_Log("  [2] Include guard:   #ifndef NAME_HLSLI / #define / #endif");
    SDL_Log("  [3] Include:         #include \"name.hlsli\"");
    SDL_Log("  [4] Search path:     dxc -I shader_directory/");
    SDL_Log("  [5] Contents:        constants, structs, utility functions");
    SDL_Log(" ");
    SDL_Log("The pattern is the same one you learned in Engine Lesson 05");
    SDL_Log("for C header-only libraries.  The only difference is that HLSL");
    SDL_Log("has no linker, so the rules are simpler — no ODR to worry about.");

    SDL_Log(" ");
    SDL_Log("=== All sections complete ===");

    SDL_Quit();
    return 0;
}
