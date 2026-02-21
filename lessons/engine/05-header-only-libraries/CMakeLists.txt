# ── Engine Lesson 05 — Header-Only Libraries ──────────────────────────────
#
# This lesson has TWO source files (main.c and physics.c) to demonstrate
# the one-definition rule.  Both files include my_vec.h, and the 'static
# inline' pattern prevents linker conflicts.

add_executable(05-header-only-libraries main.c physics.c)
target_include_directories(05-header-only-libraries PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # my_vec.h, physics.h
    ${FORGE_COMMON_DIR}           # math/forge_math.h
)
target_link_libraries(05-header-only-libraries PRIVATE
    SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 05-header-only-libraries POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:05-header-only-libraries>
    )
endif()
