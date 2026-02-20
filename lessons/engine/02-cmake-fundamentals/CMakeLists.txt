# ── Engine Lesson 02 — CMake Fundamentals ─────────────────────────────────
#
# This CMakeLists.txt is the lesson.  Every line demonstrates a concept
# explained in the README.  Read the comments here alongside the README
# for the full picture.

# ── add_executable: create a target ───────────────────────────────────────
#
# add_executable creates a *target* — an executable that CMake knows how
# to build.  The first argument is the target name; the rest are source
# files.
#
# Every .c file listed here becomes its own *translation unit*.  The
# compiler turns each one into an object file (.o / .obj), and the
# linker combines them into the final executable.
#
# If you forget to list greeting.c here, the compiler will still succeed
# (main.c only needs the header), but the linker will fail:
#
#   undefined reference to 'get_greeting'
#
# The fix: add the missing .c file to this list.
add_executable(02-cmake-fundamentals main.c greeting.c)

# ── target_include_directories: where to find headers ─────────────────────
#
# The compiler searches a list of directories when resolving #include
# directives.  By default, it checks system headers and the source file's
# own directory.  To include headers from *other* directories, you must
# tell CMake where to look.
#
# ${FORGE_COMMON_DIR} is a variable set in the root CMakeLists.txt.  It
# points to the common/ directory containing shared libraries (math,
# obj parser, gltf parser).  Without this line, the compiler cannot find
# "math/forge_math.h".
#
# PRIVATE means this include path is only used when compiling this target.
# Other targets that link against us (if any) would NOT inherit this path.
# For executables, PRIVATE is almost always the right choice.
target_include_directories(02-cmake-fundamentals PRIVATE ${FORGE_COMMON_DIR})

# ── target_link_libraries: connect dependencies ──────────────────────────
#
# SDL3::SDL3 is an *imported target* created by FetchContent in the root
# CMakeLists.txt.  It carries its own properties: include directories,
# compile definitions, and the actual library to link.
#
# When you write:
#   target_link_libraries(02-cmake-fundamentals PRIVATE SDL3::SDL3)
#
# CMake does several things automatically:
#   1. Adds SDL3's include directories to the compiler search path
#   2. Adds SDL3's compile definitions (if any)
#   3. Tells the linker to link the SDL3 library
#
# This is the power of *target-based* CMake: dependencies carry their
# own configuration.  You don't need to manually specify -I flags or
# -L paths — linking the target brings everything along.
#
# The math library link ($<$<NOT:$<C_COMPILER_ID:MSVC>>:m>) is a
# *generator expression*.  It evaluates to "m" on GCC/Clang (which
# need -lm for math functions like sinf, cosf) and to nothing on MSVC
# (which includes math in the C runtime automatically).
target_link_libraries(02-cmake-fundamentals PRIVATE
    SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)

# ── POST_BUILD: copy the shared library ──────────────────────────────────
#
# On Windows, the executable needs SDL3.dll in the same directory to run.
# This custom command copies it there after every build.
#
# Generator expressions used:
#   $<TARGET_FILE:SDL3::SDL3-shared>       — full path to the SDL3 DLL/SO
#   $<TARGET_FILE_DIR:02-cmake-fundamentals> — directory of our executable
#
# The if(TARGET ...) guard handles the case where SDL3 is built as a
# static library — there is no shared library to copy.
#
# On Linux and macOS, shared libraries are typically found via rpath
# or system paths, but the copy still works and avoids path issues
# during development.
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 02-cmake-fundamentals POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:02-cmake-fundamentals>
    )
endif()
