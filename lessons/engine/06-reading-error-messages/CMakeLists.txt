# ── Engine Lesson 06 — Reading Error Messages ──────────────────────────────
#
# A console program that walks through compiler errors, linker errors,
# and runtime crashes — explaining how to read and fix each type.

add_executable(06-reading-error-messages main.c)
target_include_directories(06-reading-error-messages PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(06-reading-error-messages PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 06-reading-error-messages POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:06-reading-error-messages>
    )
endif()
