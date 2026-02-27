cmake_minimum_required(VERSION 3.24)
project(forge-gpu LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
endif()

# ── UI Lessons ─────────────────────────────────────────────────────────────
add_subdirectory(lessons/ui/01-ttf-parsing)
add_subdirectory(lessons/ui/02-glyph-rasterization)
add_subdirectory(lessons/ui/03-font-atlas)
add_subdirectory(lessons/ui/04-text-layout)

# ── Tests ───────────────────────────────────────────────────────────────────
add_subdirectory(tests/math)
add_subdirectory(tests/ui)
add_subdirectory(tests/raster)
if(NOT FORGE_USE_SHIM)
    add_subdirectory(tests/obj)
    add_subdirectory(tests/gltf)
endif()
