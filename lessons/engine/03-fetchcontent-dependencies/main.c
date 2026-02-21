/*
 * Engine Lesson 03 — FetchContent & Dependencies
 *
 * Demonstrates: How CMake's FetchContent module downloads, configures, and
 *               builds external libraries automatically.  This is the
 *               mechanism forge-gpu uses to provide SDL3 without requiring
 *               the learner to install it manually.
 *
 * What this program proves when it runs:
 *   1. SDL3 was obtained automatically (FetchContent, pre-installed, or shim)
 *   2. CMake's find_package / FetchContent layered strategy works
 *   3. Version pinning keeps builds reproducible
 *   4. The SDL3::SDL3 imported target carries all necessary properties
 *
 * SPDX-License-Identifier: Zlib
 */

#include <stdlib.h>

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── SDL version encoding ────────────────────────────────────────────────
 * SDL_GetVersion() returns a single integer encoding major.minor.patch as:
 *   version = major * 1000000 + minor * 1000 + patch
 * These constants decode it back into its three components. */
#define SDL_VERSION_MAJOR_DIV  1000000
#define SDL_VERSION_MINOR_DIV  1000
#define SDL_VERSION_PART_MOD   1000

/* SDL_Init(0) initialises no subsystems — just core SDL state and error
 * handling.  We give the literal a name so the intent is explicit. */
#define INIT_FLAGS  0

/* Forward declarations */
static void demo_how_sdl_arrived(void);
static void demo_fetchcontent_lifecycle(void);
static void demo_version_pinning(void);
static void demo_imported_targets(void);
static void demo_multiple_dependencies(void);
static void demo_offline_builds(void);

/* ── Section 1: How SDL3 arrived ──────────────────────────────────────── */

static void demo_how_sdl_arrived(void)
{
    SDL_Log("--- 1. How SDL3 arrived in this build ---");
    SDL_Log(" ");

    /* The fact that this code compiles and runs proves that SDL3 was
     * obtained successfully.  But *how* it was obtained depends on
     * the build configuration:
     *
     *   Path A — find_package(SDL3) found a pre-installed SDL3
     *   Path B — FetchContent downloaded and built SDL3 from source
     *   Path C — The SDL3 shim provided a minimal stand-in
     *
     * All three paths create the same target: SDL3::SDL3.
     * That is the key insight — your CMakeLists.txt links against
     * SDL3::SDL3 regardless of where the library came from. */

#ifndef FORGE_USE_SHIM
    /* SDL_GetVersion() is only available with the real SDL3 library.
     * The shim does not provide it, so skip version reporting when
     * building with -DFORGE_USE_SHIM=ON. */
    int version = SDL_GetVersion();
    int major = version / SDL_VERSION_MAJOR_DIV;
    int minor = (version / SDL_VERSION_MINOR_DIV) % SDL_VERSION_PART_MOD;
    int patch = version % SDL_VERSION_PART_MOD;

    SDL_Log("  SDL version: %d.%d.%d", major, minor, patch);
    SDL_Log(" ");
#endif
    SDL_Log("  The root CMakeLists.txt tries three paths in order:");
    SDL_Log("    1. find_package(SDL3) -- use a pre-installed SDL3");
    SDL_Log("    2. FORGE_USE_SHIM=ON  -- use minimal SDL3 shim");
    SDL_Log("    3. FetchContent       -- download and build from source");
    SDL_Log(" ");
    SDL_Log("  All three create the same target: SDL3::SDL3");
    SDL_Log("  Your lesson code never needs to know which path was taken.");
    SDL_Log(" ");
}

/* ── Section 2: FetchContent lifecycle ────────────────────────────────── */

static void demo_fetchcontent_lifecycle(void)
{
    SDL_Log("--- 2. FetchContent lifecycle ---");
    SDL_Log(" ");

    /* FetchContent works in two stages:
     *
     * Stage 1: FetchContent_Declare()
     *   Registers a dependency with a name, URL/repo, and version tag.
     *   No download happens yet — this is just a declaration.
     *
     * Stage 2: FetchContent_MakeAvailable()
     *   If the dependency has not been fetched yet:
     *     1. Downloads the source (git clone, URL fetch, etc.)
     *     2. Adds it as a subdirectory (like add_subdirectory)
     *     3. Configures and builds it alongside your project
     *   If it was already fetched, it reuses the cached copy.
     *
     * The downloaded sources live in your build directory:
     *   build/_deps/<name>-src/     source code
     *   build/_deps/<name>-build/   build artifacts
     *   build/_deps/<name>-subbuild/ download machinery
     *
     * This means:
     *   - Clean builds redownload (unless cached)
     *   - Different build directories are independent
     *   - The source tree stays clean */

    SDL_Log("  FetchContent has two stages:");
    SDL_Log(" ");
    SDL_Log("  Stage 1: FetchContent_Declare(name ...)");
    SDL_Log("    - Registers a dependency by name");
    SDL_Log("    - Specifies WHERE to get it (git repo, URL, etc.)");
    SDL_Log("    - Specifies WHICH version (git tag, commit hash)");
    SDL_Log("    - No download happens at this point");
    SDL_Log(" ");
    SDL_Log("  Stage 2: FetchContent_MakeAvailable(name)");
    SDL_Log("    - Downloads the source if not already cached");
    SDL_Log("    - Runs add_subdirectory() on the fetched source");
    SDL_Log("    - Builds it as part of your project");
    SDL_Log(" ");
    SDL_Log("  Downloaded sources live in the build directory:");
    SDL_Log("    build/_deps/<name>-src/       (source code)");
    SDL_Log("    build/_deps/<name>-build/     (build artifacts)");
    SDL_Log("    build/_deps/<name>-subbuild/  (download machinery)");
    SDL_Log(" ");
}

/* ── Section 3: Version pinning ──────────────────────────────────────── */

static void demo_version_pinning(void)
{
    SDL_Log("--- 3. Version pinning ---");
    SDL_Log(" ");

    /* forge-gpu pins SDL3 to a specific release tag:
     *
     *   FetchContent_Declare(SDL3
     *       GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
     *       GIT_TAG        release-3.4.0
     *       GIT_SHALLOW    TRUE
     *   )
     *
     * GIT_TAG is the version pin.  It can be:
     *   - A release tag:    release-3.4.0      (recommended)
     *   - A commit hash:    a1b2c3d4e5f6...    (most precise)
     *   - A branch name:    main               (NOT recommended)
     *
     * Why pin?
     *   - Reproducibility: everyone builds the same version
     *   - Stability: upstream changes do not break your build
     *   - Debugging: you know exactly which code you are running
     *
     * GIT_SHALLOW TRUE tells git to clone only the tagged commit,
     * not the entire repository history.  For SDL, this saves
     * hundreds of megabytes and significant time. */

    SDL_Log("  forge-gpu pins SDL3 with GIT_TAG:");
    SDL_Log("    GIT_TAG  release-3.4.0");
    SDL_Log(" ");
    SDL_Log("  GIT_TAG options (from safest to riskiest):");
    SDL_Log("    Commit hash  :  a1b2c3d...   (exact, never changes)");
    SDL_Log("    Release tag  :  release-3.4.0 (stable, rarely changes)");
    SDL_Log("    Branch name  :  main          (AVOID -- changes constantly)");
    SDL_Log(" ");
    SDL_Log("  Why pin versions?");
    SDL_Log("    - Reproducibility: same version for every developer");
    SDL_Log("    - Stability: upstream changes cannot break your build");
    SDL_Log("    - Debugging: you know exactly which code is running");
    SDL_Log(" ");
    SDL_Log("  GIT_SHALLOW TRUE skips full history (saves time + space)");
    SDL_Log(" ");
}

/* ── Section 4: Imported targets ─────────────────────────────────────── */

static void demo_imported_targets(void)
{
    SDL_Log("--- 4. Imported targets carry everything ---");
    SDL_Log(" ");

    /* Whether SDL3 came from find_package or FetchContent, you get
     * the same target: SDL3::SDL3.  This is an *imported target* —
     * a CMake target that represents a pre-built or externally-built
     * library.
     *
     * Linking against SDL3::SDL3 gives you:
     *   - Include directories  (SDL3/SDL.h becomes findable)
     *   - Compile definitions  (platform-specific defines)
     *   - The actual library    (libSDL3.so, SDL3.dll, etc.)
     *
     * You do not need to set these manually.  This is the power of
     * target-based CMake: the dependency carries its own configuration. */

    /* Prove the imported target works by calling SDL and forge_math */
    vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 right = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 forward = vec3_cross(right, up);

    SDL_Log("  Imported target SDL3::SDL3 provides:");
    SDL_Log("    - Include directories (SDL3/SDL.h found by compiler)");
    SDL_Log("    - Compile definitions (platform-specific)");
    SDL_Log("    - The library itself  (linked by the linker)");
    SDL_Log(" ");
    SDL_Log("  Proof: vec3_cross(right, up) = (%.1f, %.1f, %.1f)",
            forward.x, forward.y, forward.z);
    SDL_Log("  Both SDL3 and forge_math are linked and working.");
    SDL_Log(" ");
    SDL_Log("  This is the key benefit of target-based CMake:");
    SDL_Log("  link one target and get everything it needs.");
    SDL_Log(" ");
}

/* ── Section 5: Adding your own dependency ────────────────────────────── */

static void demo_multiple_dependencies(void)
{
    SDL_Log("--- 5. Adding your own dependency ---");
    SDL_Log(" ");

    /* To add a new dependency via FetchContent, you need three pieces:
     *
     * 1. include(FetchContent)           -- load the module
     * 2. FetchContent_Declare(name ...)  -- register the dependency
     * 3. FetchContent_MakeAvailable(name) -- download and build
     *
     * Example — adding cJSON (a JSON parser):
     *
     *   include(FetchContent)
     *
     *   FetchContent_Declare(
     *       cJSON
     *       GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
     *       GIT_TAG        v1.7.18
     *       GIT_SHALLOW    TRUE
     *   )
     *   set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
     *   FetchContent_MakeAvailable(cJSON)
     *
     *   target_link_libraries(my_program PRIVATE cjson)
     *
     * The set() call before MakeAvailable configures cJSON's build
     * options *before* its CMakeLists.txt runs.  This is how you
     * control a dependency's behavior (disable tests, set build type,
     * choose static vs shared, etc.).
     *
     * forge-gpu's glTF loader (Lesson 09) uses exactly this pattern
     * to include cJSON for parsing .gltf scene files. */

    SDL_Log("  Adding a dependency via FetchContent:");
    SDL_Log(" ");
    SDL_Log("  include(FetchContent)");
    SDL_Log(" ");
    SDL_Log("  FetchContent_Declare(");
    SDL_Log("      cJSON");
    SDL_Log("      GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git");
    SDL_Log("      GIT_TAG        v1.7.18");
    SDL_Log("      GIT_SHALLOW    TRUE");
    SDL_Log("  )");
    SDL_Log("  FetchContent_MakeAvailable(cJSON)");
    SDL_Log(" ");
    SDL_Log("  target_link_libraries(my_app PRIVATE cjson)");
    SDL_Log(" ");
    SDL_Log("  Set options BEFORE MakeAvailable to configure the dependency:");
    SDL_Log("    set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL \"\" FORCE)");
    SDL_Log(" ");
}

/* ── Section 6: Offline builds ───────────────────────────────────────── */

static void demo_offline_builds(void)
{
    SDL_Log("--- 6. Strategies for offline builds ---");
    SDL_Log(" ");

    /* FetchContent requires network access on the first configure.
     * There are several strategies for environments without internet:
     *
     * Strategy 1: Pre-install the dependency
     *   Install SDL3 system-wide or to a prefix, then:
     *     cmake -B build -DCMAKE_PREFIX_PATH=/path/to/sdl3
     *   find_package(SDL3) will find it before FetchContent runs.
     *
     * Strategy 2: Use FETCHCONTENT_FULLY_DISCONNECTED
     *   If you have already configured once (build/_deps/ is populated):
     *     cmake -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
     *   CMake skips all downloads and uses the cached sources.
     *
     * Strategy 3: Use FETCHCONTENT_SOURCE_DIR_<NAME>
     *   Point to a local copy of the source:
     *     cmake -B build -DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL
     *   CMake uses the local source instead of downloading.
     *
     * Strategy 4: Use a shim (forge-gpu specific)
     *   For engine/math lessons that only need SDL_Log and basic APIs:
     *     cmake -B build -DFORGE_USE_SHIM=ON
     *   This uses a minimal header-only stand-in. GPU lessons are
     *   skipped because they require the real SDL3. */

    SDL_Log("  FetchContent needs network access on first configure.");
    SDL_Log("  Strategies for offline or restricted environments:");
    SDL_Log(" ");
    SDL_Log("  1. Pre-install the dependency:");
    SDL_Log("     cmake -B build -DCMAKE_PREFIX_PATH=/path/to/sdl3");
    SDL_Log("     -> find_package() finds it before FetchContent runs");
    SDL_Log(" ");
    SDL_Log("  2. Use FETCHCONTENT_FULLY_DISCONNECTED:");
    SDL_Log("     cmake -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON");
    SDL_Log("     -> Reuses previously downloaded sources from build/_deps/");
    SDL_Log(" ");
    SDL_Log("  3. Use FETCHCONTENT_SOURCE_DIR_<NAME>:");
    SDL_Log("     cmake -B build -DFETCHCONTENT_SOURCE_DIR_SDL3=/local/SDL");
    SDL_Log("     -> Uses a local source directory instead of downloading");
    SDL_Log(" ");
    SDL_Log("  4. Use a shim (forge-gpu specific):");
    SDL_Log("     cmake -B build -DFORGE_USE_SHIM=ON");
    SDL_Log("     -> Minimal stand-in for console-only lessons");
    SDL_Log(" ");
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* INIT_FLAGS is 0 — no subsystems, just core SDL state and error handling.
     * This gives us SDL_Log and SDL_GetError without pulling in video, audio,
     * etc.  Pass SDL_INIT_VIDEO when you need a window. */
    if (!SDL_Init(INIT_FLAGS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Log("=== Engine Lesson 03: FetchContent & Dependencies ===");
    SDL_Log(" ");

    /* Each section explains a facet of CMake's dependency management.
     * The fact that this program compiles and runs is itself proof that
     * the dependency system worked — SDL3 was obtained, configured,
     * built, and linked without the learner doing anything beyond
     * running cmake. */
    demo_how_sdl_arrived();
    demo_fetchcontent_lifecycle();
    demo_version_pinning();
    demo_imported_targets();
    demo_multiple_dependencies();
    demo_offline_builds();

    SDL_Log("=== Dependency management verified ===");
    SDL_Log("SDL3 was obtained, configured, and linked automatically.");
    SDL_Log(" ");
    SDL_Log("Read the README.md in this lesson's directory for the full");
    SDL_Log("explanation with diagrams and exercises.");

    SDL_Quit();
    return EXIT_SUCCESS;
}
