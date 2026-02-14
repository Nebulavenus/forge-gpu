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

# ── Testing ─────────────────────────────────────────────────────────────────
enable_testing()

# ── Math Lessons ────────────────────────────────────────────────────────────
add_subdirectory(lessons/math/01-vectors)
add_subdirectory(lessons/math/02-coordinate-spaces)

# ── GPU Lessons ─────────────────────────────────────────────────────────────
add_subdirectory(lessons/gpu/01-hello-window)
add_subdirectory(lessons/gpu/02-first-triangle)
add_subdirectory(lessons/gpu/03-uniforms-and-motion)

# ── Tests ───────────────────────────────────────────────────────────────────
add_subdirectory(tests/math)
