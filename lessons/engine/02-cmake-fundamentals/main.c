/*
 * Engine Lesson 02 — CMake Fundamentals
 *
 * Demonstrates: Targets, properties, add_executable, target_link_libraries,
 *               target_include_directories, generator expressions, and what
 *               happens when the build configuration is wrong.
 *
 * This program is intentionally simple — the interesting part is the
 * CMakeLists.txt that builds it and the README that explains the concepts.
 * The code here exercises the build system features so you can see that
 * everything links and runs correctly.
 *
 * What this program proves when it runs:
 *   1. add_executable compiled both main.c and greeting.c (multi-source)
 *   2. target_link_libraries linked SDL3 (we can call SDL functions)
 *   3. target_include_directories found the common/ headers
 *   4. The generator expression copied SDL3.dll on Windows (no crash)
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>

/* greeting.h is in the same directory as main.c.  The compiler finds it
 * because CMake's add_executable implicitly adds the source directory to
 * the include search path.  However, if we needed headers from another
 * directory (like common/), we would need target_include_directories. */
#include "greeting.h"

/* forge_math.h lives in common/math/, NOT in our source directory.
 * The compiler can only find it because our CMakeLists.txt has:
 *
 *   target_include_directories(02-cmake-fundamentals PRIVATE ${FORGE_COMMON_DIR})
 *
 * ${FORGE_COMMON_DIR} expands to the absolute path of the common/ directory.
 * PRIVATE means only this target uses that include path — other targets
 * that might link against us would not inherit it.
 *
 * If you remove that target_include_directories line and rebuild, you will
 * see an error like:
 *
 *   fatal error: 'math/forge_math.h' file not found
 *
 * That error means the compiler searched every directory on its include
 * path and could not find the header.  The fix: tell CMake where to look. */
#include "math/forge_math.h"

/* ── SDL version encoding ────────────────────────────────────────────────
 * SDL_GetVersion() returns a single integer encoding major.minor.patch as:
 *   version = major * 1000000 + minor * 1000 + patch
 * These constants decode it back into its three components. */
#define SDL_VERSION_MAJOR_DIV  1000000
#define SDL_VERSION_MINOR_DIV  1000
#define SDL_VERSION_PART_MOD   1000

/* ── 4x4 matrix diagonal indices (column-major) ─────────────────────────
 * A mat4 stores 16 floats in column-major order.  The diagonal elements
 * (row == column) sit at indices 0, 5, 10, 15. */
#define MAT4_DIAG_0   0
#define MAT4_DIAG_1   5
#define MAT4_DIAG_2  10
#define MAT4_DIAG_3  15

/* ── Section 1: Verify SDL linking ──────────────────────────────────────── */

static void demo_sdl_linked(void)
{
    SDL_Log("--- 1. SDL is linked ---");

    /* If target_link_libraries did not include SDL3::SDL3, none of these
     * SDL functions would be available.  The linker would report:
     *
     *   undefined reference to 'SDL_GetVersion'
     *   undefined reference to 'SDL_Log'
     *   undefined reference to 'SDL_GetNumVideoDrivers'
     *
     * The fix: add SDL3::SDL3 to your target_link_libraries call. */

    int version = SDL_GetVersion();
    int major = version / SDL_VERSION_MAJOR_DIV;
    int minor = (version / SDL_VERSION_MINOR_DIV) % SDL_VERSION_PART_MOD;
    int patch = version % SDL_VERSION_PART_MOD;
    SDL_Log("  SDL version: %d.%d.%d", major, minor, patch);

    /* SDL_GetNumVideoDrivers is another SDL function — calling it proves
     * that the full SDL library is linked, not just a header stub. */
    int num_drivers = SDL_GetNumVideoDrivers();
    SDL_Log("  Video drivers available: %d", num_drivers);
    for (int i = 0; i < num_drivers; i++) {
        SDL_Log("    [%d] %s", i, SDL_GetVideoDriver(i));
    }

    SDL_Log("  -> target_link_libraries(... SDL3::SDL3) is working");
    SDL_Log(" ");
}

/* ── Section 2: Verify multi-source build ───────────────────────────────── */

static void demo_multi_source(void)
{
    SDL_Log("--- 2. Multiple source files compiled and linked ---");

    /* get_greeting() and get_lesson_topic() are defined in greeting.c.
     * For the linker to find them, greeting.c must be listed in
     * add_executable:
     *
     *   add_executable(02-cmake-fundamentals main.c greeting.c)
     *
     * If greeting.c were missing from that list, the compiler would still
     * succeed (it can see the declarations in greeting.h), but the linker
     * would fail:
     *
     *   undefined reference to 'get_greeting'
     *   undefined reference to 'get_lesson_topic'
     *
     * This is the classic "compiles but doesn't link" error.  The compiler
     * trusts the header's promise that the function exists somewhere.  The
     * linker's job is to find the actual definition — and if the .c file
     * containing it was never compiled, the definition does not exist. */

    SDL_Log("  Greeting: %s", get_greeting());
    SDL_Log("  Topic:    %s", get_lesson_topic());
    SDL_Log("  -> add_executable(... main.c greeting.c) is working");
    SDL_Log(" ");
}

/* ── Section 3: Verify include directories ──────────────────────────────── */

static void demo_include_dirs(void)
{
    SDL_Log("--- 3. Include directories are configured ---");

    /* This code uses types and functions from common/math/forge_math.h.
     * The compiler found that header because our CMakeLists.txt set:
     *
     *   target_include_directories(02-cmake-fundamentals PRIVATE ${FORGE_COMMON_DIR})
     *
     * Without this line, the compiler would report:
     *
     *   fatal error: 'math/forge_math.h' file not found
     *
     * PRIVATE means this include path applies only to our target.  If we
     * were building a library that other targets depend on, we might use
     * PUBLIC (both us and our dependents) or INTERFACE (only our dependents).
     */

    /* Create some vectors using the forge math library. */
    vec3 a = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 b = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 c = vec3_add(a, b);
    float d = vec3_dot(a, b);

    SDL_Log("  vec3 a = (%.1f, %.1f, %.1f)", a.x, a.y, a.z);
    SDL_Log("  vec3 b = (%.1f, %.1f, %.1f)", b.x, b.y, b.z);
    SDL_Log("  a + b  = (%.1f, %.1f, %.1f)", c.x, c.y, c.z);
    SDL_Log("  a . b  = %.1f (dot product)", d);

    /* Create a 4x4 identity matrix — the starting point for every
     * transform in a 3D pipeline. */
    mat4 identity = mat4_identity();
    SDL_Log("  mat4 identity diagonal: (%.0f, %.0f, %.0f, %.0f)",
            identity.m[MAT4_DIAG_0], identity.m[MAT4_DIAG_1],
            identity.m[MAT4_DIAG_2], identity.m[MAT4_DIAG_3]);

    SDL_Log("  -> target_include_directories(... ${FORGE_COMMON_DIR}) is working");
    SDL_Log(" ");
}

/* ── Section 4: Explain the CMake target model ──────────────────────────── */

static void demo_target_model(void)
{
    SDL_Log("--- 4. The CMake target model ---");
    SDL_Log(" ");
    SDL_Log("  A CMake 'target' is anything CMake knows how to build:");
    SDL_Log("    - An executable    (add_executable)");
    SDL_Log("    - A library        (add_library)");
    SDL_Log("    - An imported lib  (find_package creates these)");
    SDL_Log(" ");
    SDL_Log("  Every target has 'properties' -- settings attached to it:");
    SDL_Log("    - INCLUDE_DIRECTORIES  -> where to find headers");
    SDL_Log("    - LINK_LIBRARIES       -> what to link against");
    SDL_Log("    - COMPILE_DEFINITIONS  -> preprocessor #defines");
    SDL_Log("    - COMPILE_OPTIONS      -> compiler flags");
    SDL_Log(" ");
    SDL_Log("  You set properties with target_* commands:");
    SDL_Log("    target_include_directories(mytarget PRIVATE path/)");
    SDL_Log("    target_link_libraries(mytarget PRIVATE SDL3::SDL3)");
    SDL_Log("    target_compile_definitions(mytarget PRIVATE DEBUG=1)");
    SDL_Log(" ");
    SDL_Log("  The PRIVATE / PUBLIC / INTERFACE keywords control");
    SDL_Log("  who inherits the property:");
    SDL_Log("    PRIVATE   -> only this target");
    SDL_Log("    PUBLIC    -> this target AND targets that link to it");
    SDL_Log("    INTERFACE -> ONLY targets that link to it (not this one)");
    SDL_Log(" ");
    SDL_Log("  For executables (not libraries), PRIVATE is almost always");
    SDL_Log("  correct -- nothing links against an executable.");
    SDL_Log(" ");
}

/* ── Section 5: Explain generator expressions ───────────────────────────── */

static void demo_generator_expressions(void)
{
    SDL_Log("--- 5. Generator expressions ---");
    SDL_Log(" ");
    SDL_Log("  Generator expressions use the $<...> syntax.");
    SDL_Log("  They are evaluated at build/generate time, not when");
    SDL_Log("  you run cmake.");
    SDL_Log(" ");
    SDL_Log("  Common generator expressions in this project:");
    SDL_Log(" ");
    SDL_Log("  $<TARGET_FILE:SDL3::SDL3-shared>");
    SDL_Log("    -> Full path to the SDL3 shared library (.dll/.so/.dylib)");
    SDL_Log("    -> Used to copy the DLL next to the executable on Windows");
    SDL_Log(" ");
    SDL_Log("  $<TARGET_FILE_DIR:02-cmake-fundamentals>");
    SDL_Log("    -> Directory containing this executable after building");
    SDL_Log("    -> The destination for DLL copies");
    SDL_Log(" ");
    SDL_Log("  $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>");
    SDL_Log("    -> Links the math library (-lm) on GCC/Clang only");
    SDL_Log("    -> MSVC does not need a separate math library");
    SDL_Log("    -> This is a conditional: if NOT MSVC, then link 'm'");
    SDL_Log(" ");
    SDL_Log("  $<$<CONFIG:Debug>:SLOW_CHECKS>");
    SDL_Log("    -> Adds a compile definition only in Debug builds");
    SDL_Log("    -> The value is empty (and ignored) in Release builds");
    SDL_Log(" ");
    SDL_Log("  Generator expressions replace platform-specific if() blocks");
    SDL_Log("  with a single portable expression that works everywhere.");
    SDL_Log(" ");
}

/* ── Section 6: Explain common errors ───────────────────────────────────── */

static void demo_common_errors(void)
{
    SDL_Log("--- 6. Common CMake errors and what they mean ---");
    SDL_Log(" ");

    SDL_Log("  ERROR: 'undefined reference to ...'");
    SDL_Log("    Cause:  A function was declared (header) but its");
    SDL_Log("            definition (.c file) was not compiled or linked.");
    SDL_Log("    Fix:    Check target_link_libraries and add_executable.");
    SDL_Log(" ");

    SDL_Log("  ERROR: 'fatal error: file not found'");
    SDL_Log("    Cause:  The compiler cannot find a #include'd header.");
    SDL_Log("    Fix:    Add target_include_directories with the right path.");
    SDL_Log(" ");

    SDL_Log("  ERROR: 'target ... not found'");
    SDL_Log("    Cause:  target_link_libraries names a target that does");
    SDL_Log("            not exist (typo, or missing find_package/FetchContent).");
    SDL_Log("    Fix:    Check the spelling. Run cmake to see available targets.");
    SDL_Log(" ");

    SDL_Log("  ERROR: 'Cannot specify link libraries for target ...");
    SDL_Log("          which is not built by this project'");
    SDL_Log("    Cause:  The target name in target_link_libraries does");
    SDL_Log("            not match any add_executable or add_library.");
    SDL_Log("    Fix:    Make sure the target name is spelled exactly right.");
    SDL_Log(" ");

    SDL_Log("  ERROR: 'SDL3::SDL3-shared library not found' (runtime)");
    SDL_Log("    Cause:  The .dll/.so is not next to the executable.");
    SDL_Log("    Fix:    Use the POST_BUILD copy command with a generator");
    SDL_Log("            expression (see the CMakeLists.txt in this lesson).");
    SDL_Log(" ");
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* SDL_Init(0) initializes no subsystems — just core SDL state and error
     * handling.  This gives us SDL_Log and SDL_GetError without pulling in
     * video, audio, etc.  Pass SDL_INIT_VIDEO when you need a window. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 02: CMake Fundamentals ===");
    SDL_Log(" ");

    /* Each section demonstrates a CMake concept by showing that the
     * build configuration worked correctly.  If any section fails to
     * compile or link, that is the lesson — the error message tells
     * you exactly which CMake command is missing or wrong. */
    demo_sdl_linked();
    demo_multi_source();
    demo_include_dirs();
    demo_target_model();
    demo_generator_expressions();
    demo_common_errors();

    SDL_Log("=== Build configuration verified ===");
    SDL_Log("All CMake targets, properties, and links are working correctly.");
    SDL_Log(" ");
    SDL_Log("Read the README.md in this lesson's directory for the full");
    SDL_Log("explanation of each concept, with diagrams and exercises.");

    SDL_Quit();
    return 0;
}
