/*
 * Engine Lesson 05 — Header-Only Libraries
 *
 * Demonstrates how header-only libraries work in C:
 *   - Include guards prevent double inclusion within one translation unit
 *   - static inline functions avoid one-definition rule (ODR) violations
 *   - Multiple .c files can safely include the same header-only library
 *   - forge_math.h uses these exact patterns
 *
 * This program and physics.c both include my_vec.h — a tiny header-only
 * 2D vector library that follows the same patterns as forge_math.h.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>

/* Include my_vec.h — our header-only library.
 *
 * We deliberately include it TWICE here.  This is not a mistake — it
 * demonstrates that include guards work.  The second #include is silently
 * ignored because MY_VEC_H is already defined after the first one. */
#include "my_vec.h"
#include "my_vec.h"  /* Second include — ignored by the include guard */

/* physics.h also includes my_vec.h internally.  That's a THIRD include
 * of the same file in this translation unit, and it's still fine —
 * the include guard skips it every time after the first. */
#include "physics.h"

/* forge_math.h uses the same include guard pattern (FORGE_MATH_H).
 * We include it here to show that the real math library follows
 * the exact same conventions as our teaching example. */
#include "math/forge_math.h"

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

    SDL_Log("=== Engine Lesson 05: Header-Only Libraries ===");

    /* ── Section 1: Using a header-only library ───────────────────────── */

    print_divider("1. Using a Header-Only Library");

    Vec2 position = Vec2_create(0.0f, 10.0f);
    Vec2 velocity = Vec2_create(3.0f, 0.0f);

    SDL_Log("Starting position: (%.1f, %.1f)", position.x, position.y);
    SDL_Log("Starting velocity: (%.1f, %.1f)", velocity.x, velocity.y);
    SDL_Log(" ");
    SDL_Log("These functions come from my_vec.h -- a header-only library.");
    SDL_Log("No .c file for the library, no separate compilation step.");
    SDL_Log("Just #include \"my_vec.h\" and every function is available.");

    /* ── Section 2: Include guards in action ──────────────────────────── */

    print_divider("2. Include Guards in Action");

    SDL_Log("This file includes my_vec.h THREE times:");
    SDL_Log("  1. #include \"my_vec.h\"      (directly)");
    SDL_Log("  2. #include \"my_vec.h\"      (directly, on purpose)");
    SDL_Log("  3. #include \"physics.h\"     (which includes my_vec.h internally)");
    SDL_Log(" ");
    SDL_Log("All three compiles down to just ONE copy of the declarations.");
    SDL_Log("The include guard (#ifndef MY_VEC_H / #define MY_VEC_H / #endif)");
    SDL_Log("ensures that only the first #include is processed.  The second");
    SDL_Log("and third see that MY_VEC_H is already defined and skip the file.");
    SDL_Log(" ");
    SDL_Log("Without include guards, the compiler would see the Vec2 typedef");
    SDL_Log("three times and report: \"error: redefinition of 'Vec2'\"");

    /* ── Section 3: Multiple translation units (the ODR) ──────────────── */

    print_divider("3. Multiple Translation Units");

    SDL_Log("This program is built from two .c files:");
    SDL_Log("  main.c    -- includes my_vec.h, calls Vec2_create, Vec2_length");
    SDL_Log("  physics.c -- includes my_vec.h, calls Vec2_add, Vec2_scale");
    SDL_Log(" ");
    SDL_Log("Both files include my_vec.h, so both contain Vec2_create,");
    SDL_Log("Vec2_add, Vec2_scale, etc.  This works because the functions");
    SDL_Log("are declared 'static inline':");
    SDL_Log(" ");
    SDL_Log("  static  -> each .c file gets a PRIVATE copy (not exported)");
    SDL_Log("  inline  -> compiler substitutes body at call site (no call)");
    SDL_Log(" ");
    SDL_Log("Without 'static', the linker would see two PUBLIC definitions");
    SDL_Log("of Vec2_create (one from main.o, one from physics.o) and");
    SDL_Log("report: \"multiple definition of 'Vec2_create'\"");
    SDL_Log("That error is the one-definition rule (ODR) being enforced.");

    /* Demonstrate that physics.c's functions work — they use the same
     * Vec2_add and Vec2_scale from my_vec.h, compiled separately. */
    SDL_Log(" ");
    SDL_Log("Calling physics functions (compiled separately in physics.c):");

    float dt = 0.016f;  /* ~60 FPS time step */
    SDL_Log("  Time step: %.3f seconds", dt);

    velocity = physics_apply_gravity(velocity, dt);
    SDL_Log("  After gravity:  velocity = (%.3f, %.3f)", velocity.x, velocity.y);

    position = physics_update_position(position, velocity, dt);
    SDL_Log("  After movement: position = (%.3f, %.3f)", position.x, position.y);

    SDL_Log(" ");
    SDL_Log("Both main.c and physics.c use the same header-only library,");
    SDL_Log("compiled independently, linked together without conflict.");

    /* ── Section 4: How forge_math.h uses these patterns ──────────────── */

    print_divider("4. How forge_math.h Uses These Patterns");

    SDL_Log("forge_math.h is a real header-only library with ~2000 lines.");
    SDL_Log("It uses the same three patterns:");
    SDL_Log(" ");
    SDL_Log("  1. Include guard:  #ifndef FORGE_MATH_H / #define FORGE_MATH_H");
    SDL_Log("  2. Type defs:      typedef struct vec3 { float x, y, z; } vec3;");
    SDL_Log("  3. static inline:  static inline vec3 vec3_add(vec3 a, vec3 b)");
    SDL_Log(" ");

    /* Actually use forge_math.h to prove it works alongside my_vec.h. */
    vec3 a = vec3_create(1.0f, 2.0f, 3.0f);
    vec3 b = vec3_create(4.0f, 5.0f, 6.0f);
    vec3 sum = vec3_add(a, b);

    SDL_Log("Using forge_math.h right now:");
    SDL_Log("  vec3_add((1, 2, 3), (4, 5, 6)) = (%.0f, %.0f, %.0f)",
            sum.x, sum.y, sum.z);
    SDL_Log(" ");
    SDL_Log("forge_math.h and my_vec.h coexist in the same file because");
    SDL_Log("their include guards use different names (FORGE_MATH_H vs");
    SDL_Log("MY_VEC_H) and their types have different names (vec3 vs Vec2).");

    /* ── Section 5: What would go wrong ───────────────────────────────── */

    print_divider("5. What Goes Wrong Without These Patterns");

    SDL_Log("Error 1: Missing include guard");
    SDL_Log("  If my_vec.h had no #ifndef/#define/#endif and you included");
    SDL_Log("  it twice (or indirectly via another header), the compiler");
    SDL_Log("  would see 'typedef struct Vec2' twice and report:");
    SDL_Log("    error: redefinition of 'Vec2'");
    SDL_Log(" ");
    SDL_Log("Error 2: Missing 'static' on functions");
    SDL_Log("  If Vec2_create was declared as just 'inline Vec2 Vec2_create'");
    SDL_Log("  (without 'static'), both main.o and physics.o would export");
    SDL_Log("  a public symbol 'Vec2_create'.  The linker would report:");
    SDL_Log("    multiple definition of 'Vec2_create'");
    SDL_Log("    first defined in main.o");
    SDL_Log("  This is the one-definition rule: each external (non-static)");
    SDL_Log("  function can have exactly one definition across all .o files.");
    SDL_Log(" ");
    SDL_Log("Error 3: Plain global variable in a header");
    SDL_Log("  If you put 'int counter = 0;' in a header (not static),");
    SDL_Log("  every .c file that includes it gets its own 'counter', and");
    SDL_Log("  the linker sees multiple definitions of the same symbol.");
    SDL_Log("  Use 'static' for per-file variables or 'extern' with one");
    SDL_Log("  definition in a .c file for truly shared globals.");

    /* ── Summary ──────────────────────────────────────────────────────── */

    print_divider("Summary");

    SDL_Log("Header-only library checklist:");
    SDL_Log("  [1] Include guard:  #ifndef NAME_H / #define NAME_H / #endif");
    SDL_Log("  [2] All functions:  static inline return_type name(...)");
    SDL_Log("  [3] Types:          typedef struct (safe, no storage)");
    SDL_Log("  [4] Constants:      #define (preprocessor, no symbols)");
    SDL_Log(" ");
    SDL_Log("This is how forge_math.h, forge_obj.h, and forge_gltf.h all");
    SDL_Log("work.  Every GPU lesson includes them without linker conflicts");
    SDL_Log("because they follow these four rules.");

    SDL_Log(" ");
    SDL_Log("=== All sections complete ===");

    SDL_Quit();
    return 0;
}
