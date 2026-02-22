# ── Engine Lesson 07 — Using a Debugger ──────────────────────────────────────
#
# A console program with intentional bugs that the reader fixes using
# breakpoints, stepping, and variable inspection in GDB, LLDB, or
# Visual Studio's debugger.

add_executable(07-using-a-debugger main.c)
target_include_directories(07-using-a-debugger PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(07-using-a-debugger PRIVATE SDL3::SDL3 m)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 07-using-a-debugger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:07-using-a-debugger>
    )
endif()
