cmake_minimum_required(VERSION 3.24)

# meshoptimizer is C++, so we need CXX enabled for this subdirectory.
enable_language(CXX)

# Dependencies
include(FetchContent)
FetchContent_Declare(meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        v0.22
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(meshoptimizer)

# MikkTSpace (single .c file — no CMake project)
FetchContent_Declare(mikktspace
    GIT_REPOSITORY https://github.com/mmikk/MikkTSpace.git
    GIT_TAG        3e895b4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(mikktspace)

add_executable(forge_mesh_tool main.c
    ${mikktspace_SOURCE_DIR}/mikktspace.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(forge_mesh_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
    ${mikktspace_SOURCE_DIR})
target_link_libraries(forge_mesh_tool PRIVATE
    SDL3::SDL3 meshoptimizer m)
