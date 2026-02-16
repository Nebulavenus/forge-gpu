cmake_minimum_required(VERSION 3.24)
project(forge-gpu LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Fetch SDL3 from source ──────────────────────────────────────────────────
# This downloads and builds SDL3 as part of our project so learners don't
# need to install it separately — just clone, configure, and build.
include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# ── Common include path ─────────────────────────────────────────────────────
# Header-only helpers shared across lessons live in common/
set(FORGE_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

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

# ── GPU Lessons ─────────────────────────────────────────────────────────────
add_subdirectory(lessons/gpu/01-hello-window)
add_subdirectory(lessons/gpu/02-first-triangle)
add_subdirectory(lessons/gpu/03-uniforms-and-motion)
add_subdirectory(lessons/gpu/04-textures-and-samplers)
add_subdirectory(lessons/gpu/05-mipmaps)
add_subdirectory(lessons/gpu/06-depth-and-3d)
add_subdirectory(lessons/gpu/07-camera-and-input)

# ── Tests ───────────────────────────────────────────────────────────────────
add_subdirectory(tests/math)
