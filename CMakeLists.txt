cmake_minimum_required(VERSION 3.24)
project(forge-gpu LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Generate compile_commands.json for clangd / editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── SDL3 ──────────────────────────────────────────────────────────────────────
# Try a pre-installed SDL3 first (e.g. -DCMAKE_PREFIX_PATH=/opt/sdl3).
# If not found, fall back to building from source via FetchContent so learners
# don't need to install it separately — just clone, configure, and build.
#
# For environments without network access or a GPU, pass -DFORGE_USE_SHIM=ON
# to use a minimal SDL3 shim.  This builds math and engine lessons (console
# programs) but skips GPU lessons, which require the real SDL3 library.
option(FORGE_USE_SHIM "Use minimal SDL3 shim (console-only lessons)" OFF)

find_package(SDL3 QUIET)
if(SDL3_FOUND)
    message(STATUS "Using pre-installed SDL3: ${SDL3_DIR}")
elseif(FORGE_USE_SHIM)
    message(STATUS "SDL3 shim enabled — building console-only lessons")
    add_library(sdl3_shim INTERFACE)
    target_include_directories(sdl3_shim INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdl3_shim)
    target_compile_definitions(sdl3_shim INTERFACE FORGE_USE_SHIM)
    add_library(SDL3::SDL3 ALIAS sdl3_shim)
else()
    message(STATUS "SDL3 not found — fetching from source")
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# ── Basis Universal transcoder (C++ static lib, used by compressed texture pipeline)
add_subdirectory(third_party/basis_universal)

# ── Common include path ─────────────────────────────────────────────────────
# Header-only helpers shared across lessons live in common/
set(FORGE_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(FORGE_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)

# ── Frame capture (optional) ───────────────────────────────────────────────
# When enabled, lessons accept --screenshot / --capture-dir flags to save
# rendered frames as BMP files.  Used by scripts/capture_lesson.py.
option(FORGE_CAPTURE "Enable frame capture for screenshots and GIFs" OFF)

# ── Testing ─────────────────────────────────────────────────────────────────
enable_testing()

# Fuzz iteration count (override with -DFORGE_FUZZ_ITERATIONS=N)
set(FORGE_FUZZ_ITERATIONS 100000 CACHE STRING
    "Number of iterations for the text-input fuzz harness")

# ── Math Lessons ────────────────────────────────────────────────────────────
add_subdirectory(lessons/math/01-vectors)
add_subdirectory(lessons/math/02-coordinate-spaces)
add_subdirectory(lessons/math/03-bilinear-interpolation)
add_subdirectory(lessons/math/04-mipmaps-and-lod)
add_subdirectory(lessons/math/05-matrices)
add_subdirectory(lessons/math/06-projections)
add_subdirectory(lessons/math/07-floating-point)
add_subdirectory(lessons/math/08-orientation)
add_subdirectory(lessons/math/09-view-matrix)
add_subdirectory(lessons/math/10-anisotropy)
add_subdirectory(lessons/math/11-color-spaces)
add_subdirectory(lessons/math/12-hash-functions)
add_subdirectory(lessons/math/13-gradient-noise)
add_subdirectory(lessons/math/14-blue-noise-sequences)
add_subdirectory(lessons/math/15-bezier-curves)
add_subdirectory(lessons/math/16-density-functions)
add_subdirectory(lessons/math/17-implicit-curves)
add_subdirectory(lessons/math/18-scalar-field-gradients)

# ── Engine Lessons ──────────────────────────────────────────────────────────
add_subdirectory(lessons/engine/01-intro-to-c)
add_subdirectory(lessons/engine/02-cmake-fundamentals)
add_subdirectory(lessons/engine/03-fetchcontent-dependencies)
add_subdirectory(lessons/engine/04-pointers-and-memory)
add_subdirectory(lessons/engine/05-header-only-libraries)
add_subdirectory(lessons/engine/06-reading-error-messages)
add_subdirectory(lessons/engine/07-using-a-debugger)
add_subdirectory(lessons/engine/08-renderdoc)
add_subdirectory(lessons/engine/09-hlsl-shared-headers)
add_subdirectory(lessons/engine/10-cpu-rasterization)
add_subdirectory(lessons/engine/11-git-version-control)
add_subdirectory(lessons/engine/12-memory-arenas)

# ── GPU Lessons (require real SDL3) ─────────────────────────────────────────
if(NOT FORGE_USE_SHIM)
    add_subdirectory(lessons/gpu/01-hello-window)
    add_subdirectory(lessons/gpu/02-first-triangle)
    add_subdirectory(lessons/gpu/03-uniforms-and-motion)
    add_subdirectory(lessons/gpu/04-textures-and-samplers)
    add_subdirectory(lessons/gpu/05-mipmaps)
    add_subdirectory(lessons/gpu/06-depth-and-3d)
    add_subdirectory(lessons/gpu/07-camera-and-input)
    add_subdirectory(lessons/gpu/08-mesh-loading)
    add_subdirectory(lessons/gpu/09-scene-loading)
    add_subdirectory(lessons/gpu/10-basic-lighting)
    add_subdirectory(lessons/gpu/11-compute-shaders)
    add_subdirectory(lessons/gpu/12-shader-grid)
    add_subdirectory(lessons/gpu/13-instanced-rendering)
    add_subdirectory(lessons/gpu/14-environment-mapping)
    add_subdirectory(lessons/gpu/15-cascaded-shadow-maps)
    add_subdirectory(lessons/gpu/16-blending)
    add_subdirectory(lessons/gpu/17-normal-maps)
    add_subdirectory(lessons/gpu/18-blinn-phong-materials)
    add_subdirectory(lessons/gpu/19-debug-lines)
    add_subdirectory(lessons/gpu/20-linear-fog)
    add_subdirectory(lessons/gpu/21-hdr-tone-mapping)
    add_subdirectory(lessons/gpu/22-bloom)
    add_subdirectory(lessons/gpu/23-point-light-shadows)
    add_subdirectory(lessons/gpu/24-gobo-spotlight)
    add_subdirectory(lessons/gpu/25-shader-noise)
    add_subdirectory(lessons/gpu/26-procedural-sky)
    add_subdirectory(lessons/gpu/27-ssao)
    add_subdirectory(lessons/gpu/28-ui-rendering)
    add_subdirectory(lessons/gpu/29-screen-space-reflections)
    add_subdirectory(lessons/gpu/30-planar-reflections)
    add_subdirectory(lessons/gpu/31-transform-animations)
    add_subdirectory(lessons/gpu/32-skinning-animations)
    add_subdirectory(lessons/gpu/33-vertex-pulling)
    add_subdirectory(lessons/gpu/34-stencil-testing)
    add_subdirectory(lessons/gpu/35-decals)
    add_subdirectory(lessons/gpu/36-edge-detection)
    add_subdirectory(lessons/gpu/37-3d-picking)
    add_subdirectory(lessons/gpu/38-indirect-drawing)
    add_subdirectory(lessons/gpu/39-pipeline-processed-assets)
    add_subdirectory(lessons/gpu/40-scene-renderer)
    add_subdirectory(lessons/gpu/41-scene-model-loading)
    add_subdirectory(lessons/gpu/42-pipeline-texture-compression)
    add_subdirectory(lessons/gpu/43-pipeline-skinned-animations)
endif()

# ── UI Lessons ─────────────────────────────────────────────────────────────
add_subdirectory(lessons/ui/01-ttf-parsing)
add_subdirectory(lessons/ui/02-glyph-rasterization)
add_subdirectory(lessons/ui/03-font-atlas)
add_subdirectory(lessons/ui/04-text-layout)
add_subdirectory(lessons/ui/05-immediate-mode-basics)
add_subdirectory(lessons/ui/06-checkboxes-and-sliders)
add_subdirectory(lessons/ui/07-text-input)
add_subdirectory(lessons/ui/08-layout)
add_subdirectory(lessons/ui/09-panels-and-scrolling)
add_subdirectory(lessons/ui/10-windows)
add_subdirectory(lessons/ui/11-widget-id-system)
add_subdirectory(lessons/ui/12-font-scaling-and-spacing)
add_subdirectory(lessons/ui/13-theming-and-color-system)
add_subdirectory(lessons/ui/14-game-ui)
add_subdirectory(lessons/ui/15-dev-ui)

# ── Physics Lessons ────────────────────────────────────────────────────────
if(NOT FORGE_USE_SHIM)
    add_subdirectory(lessons/physics/01-point-particles)
    add_subdirectory(lessons/physics/02-springs-and-constraints)
    add_subdirectory(lessons/physics/03-particle-collisions)
    add_subdirectory(lessons/physics/04-rigid-body-state)
    add_subdirectory(lessons/physics/05-forces-and-torques)
    add_subdirectory(lessons/physics/06-resting-contacts-and-friction)
endif()

# ── Audio Lessons ─────────────────────────────────────────────────────────
if(NOT FORGE_USE_SHIM)
    add_subdirectory(lessons/audio/01-audio-basics)
endif()

# ── Asset Pipeline Tools ───────────────────────────────────────────────────
# Reusable C tools for the asset pipeline (invoked as subprocesses by Python
# plugins).  These live at the project level — not scoped to individual
# lessons — so the pipeline and any lesson can use them.
if(NOT FORGE_USE_SHIM)
    add_subdirectory(tools/mesh)
    add_subdirectory(tools/anim)
    add_subdirectory(tools/scene)
    add_subdirectory(tools/texture)
endif()

# ── Basis Universal CLI (basisu) ──────────────────────────────────────────
# The basisu command-line tool compresses textures to UASTC/ETC1S KTX2 at
# build time.  Built from source via FetchContent so every dev environment
# and CI server gets it automatically — no manual install required.
option(FORGE_BUILD_BASISU "Build the basisu CLI for texture compression" ON)
if(FORGE_BUILD_BASISU AND NOT FORGE_USE_SHIM)
    include(FetchContent)
    FetchContent_Declare(
        basis_universal_full
        GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal.git
        GIT_TAG        28e76f6e340e688188c94386ff068e437e1222ca
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(basis_universal_full)
    # Note: basisu binary goes to _deps/basis_universal_full-src/bin/ by
    # default.  Do NOT override RUNTIME_OUTPUT_DIRECTORY — the upstream
    # CMakeLists has a post-build strip command that references the default
    # path and would fail if redirected.
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
add_subdirectory(tests/arena)
add_subdirectory(tests/math)
add_subdirectory(tests/ui)
add_subdirectory(tests/raster)
add_subdirectory(tests/shapes)
add_subdirectory(tests/physics)
add_subdirectory(tests/scene)
if(NOT FORGE_USE_SHIM)
    add_subdirectory(tests/obj)
    add_subdirectory(tests/gltf)
    add_subdirectory(tests/audio)
    add_subdirectory(tests/pipeline)
endif()
