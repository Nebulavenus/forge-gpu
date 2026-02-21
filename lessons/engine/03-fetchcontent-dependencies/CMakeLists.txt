# ── Engine Lesson 03 — FetchContent & Dependencies ──────────────────────────
#
# This is a simple lesson build — one source file, linked against SDL3 and
# the forge math library.  The *interesting* FetchContent configuration lives
# in the root CMakeLists.txt.  This lesson explains what that configuration
# does and how to extend it.

add_executable(03-fetchcontent-dependencies main.c)

# Include the common/ directory so we can #include "math/forge_math.h".
target_include_directories(03-fetchcontent-dependencies PRIVATE ${FORGE_COMMON_DIR})

# Link SDL3.  Whether SDL3::SDL3 was created by find_package, FetchContent,
# or the shim, the target name is the same.  This is the layered strategy
# explained in the lesson — your code links against SDL3::SDL3 and does not
# need to know where it came from.
target_link_libraries(03-fetchcontent-dependencies PRIVATE
    SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)

# Copy the SDL3 shared library next to the executable (needed on Windows).
# The if(TARGET ...) guard handles static builds where there is no .dll.
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 03-fetchcontent-dependencies POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:03-fetchcontent-dependencies>
    )
endif()
